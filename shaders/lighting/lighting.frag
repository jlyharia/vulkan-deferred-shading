#version 450

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

struct PointLight {
    vec4 position; // xyz = world pos, w = intensity
    vec4 color;    // xyz = RGB color, w = radius
};

// GlobalUBO must match C++ GlobalUBO struct layout (std140)
layout (set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    mat4 invView;
    mat4 invProj;
    vec4 cameraPos;
    mat4 dirLightSpaceMatrix;
    vec4 dirLightDir;
    PointLight pointLights[24];
} ubo;

// G-buffer inputs (set 2)
layout (set = 2, binding = 0) uniform sampler2D gbAlbedoMetallic;
layout (set = 2, binding = 1) uniform sampler2D gbNormalRoughness;
layout (set = 2, binding = 2) uniform sampler2D gbDepth;
layout (set = 2, binding = 3) uniform sampler2D ssaoBuffer;
layout (set = 2, binding = 4) uniform sampler2DShadow shadowMap; // hardware PCF

const float PI = 3.14159265359;

// Surface material unpacked from G-buffer
struct Surface {
    vec3 albedo;
    float metallic;
    float roughness;
    vec3 F0;       // base reflectivity: 0.04 for dielectrics, albedo-tinted for metals
};

// --- Cook-Torrance BRDF functions ---

// GGX / Trowbridge-Reitz normal distribution
float distributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// Smith's geometry function (Schlick-GGX)
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(float NdotV, float NdotL, float roughness) {
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

// Schlick Fresnel approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

/// Evaluates the full Cook-Torrance BRDF (diffuse + specular).
/// Returns the combined term — caller multiplies by radiance * NdotL.
vec3 evalBRDF(Surface s, float NdotV, float NdotL, float NdotH, float HdotV) {
    float D = distributionGGX(NdotH, s.roughness);
    float G = geometrySmith(NdotV, NdotL, s.roughness);
    vec3 F = fresnelSchlick(HdotV, s.F0);
    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kD = (vec3(1.0) - F) * (1.0 - s.metallic);
    return kD * s.albedo / PI + specular;
}

/// Returns 1.0 = fully lit, 0.0 = fully shadowed.
/// Hardware PCF: sampler2DShadow with Linear filter does 2x2 bilinear blend of depth comparisons.
/// Caller passes vec3(uv, refDepth) — GPU evaluates compareOp per tap and interpolates results.
float shadowFactor(vec3 worldPos) {
    vec4 lightClip = ubo.dirLightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 proj = lightClip.xyz / lightClip.w;  // orthographic: w=1, but keep general
    vec2 shadowUV = proj.xy * 0.5 + 0.5;     // NDC [-1,1] → UV [0,1]

    // Outside the light frustum → treat as lit
    if (any(lessThan(shadowUV, vec2(0.0))) || any(greaterThan(shadowUV, vec2(1.0))))
    return 1.0;
    if (proj.z < 0.0 || proj.z > 1.0)
    return 1.0;

    // Small bias to counteract residual acne after slope-scaled depth bias in the shadow pass
    float bias = 0.002;
    float currentDepth = proj.z - bias;

    // texture(sampler2DShadow, vec3) — .z is the reference depth fed to compareOp (eLess).
    // Returns fraction of the 2x2 footprint where frag depth < stored depth (i.e. lit).
    return texture(shadowMap, vec3(shadowUV, currentDepth));
}

/// Reconstruct world-space position from depth buffer + inverse matrices.
/// Avoids storing world position as a G-buffer render target (saves 16 bytes/pixel).
vec3 reconstructWorldPos(vec2 uv, float depth) {
    // UV [0,1] -> NDC [-1,1]
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos = ubo.invProj * clipPos;
    viewPos /= viewPos.w;
    vec4 worldPos = ubo.invView * viewPos;
    return worldPos.xyz;
}

void main() {
    // --- Sample G-buffer ---
    vec4 albedoMetallic = texture(gbAlbedoMetallic, inUV);
    vec4 normalRoughness = texture(gbNormalRoughness, inUV);
    float depth = texture(gbDepth, inUV).r;

    // Sky/background pixels — no geometry wrote here
    if (depth >= 1.0) {
        outColor = vec4(0.02, 0.02, 0.02, 1.0);
        return;
    }

    // --- Unpack G-buffer into surface ---
    vec3 N = normalize(normalRoughness.xyz);
    vec3 worldPos = reconstructWorldPos(inUV, depth);

    Surface surf;
    surf.albedo = albedoMetallic.rgb;
    surf.metallic = albedoMetallic.a;
    surf.roughness = normalRoughness.a;
    surf.F0 = mix(vec3(0.04), surf.albedo, surf.metallic);

    vec3 V = normalize(ubo.cameraPos.xyz - worldPos);
    float NdotV = max(dot(N, V), 0.0001);

    // --- Evaluate all point lights ---
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < 24; i++) {
        vec3 Ldir = ubo.pointLights[i].position.xyz - worldPos;
        float dist = length(Ldir);
        float radius = ubo.pointLights[i].color.w;

        // Early-out: fragment is outside light radius — skip full BRDF evaluation
        if (dist >= radius) continue;

        vec3 L = normalize(Ldir);
        vec3 H = normalize(V + L);

        // KHR_lights_punctual windowed attenuation — smoothly reaches 0 at radius, no pop
        float window = pow(max(1.0 - pow(dist / radius, 4.0), 0.0), 2.0);
        float intensity = ubo.pointLights[i].position.w;
        float attenuation = (intensity / (dist * dist + 1.0)) * window;
        vec3 radiance = ubo.pointLights[i].color.xyz * attenuation;

        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float HdotV = max(dot(H, V), 0.0);

        Lo += evalBRDF(surf, NdotV, NdotL, NdotH, HdotV) * radiance * NdotL;
    }

    // --- Directional light (shadow-mapped) ---
    {
        vec3 L = normalize(-ubo.dirLightDir.xyz); // stored as light→scene; negate for frag→light
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float HdotV = max(dot(H, V), 0.0);

        const float dirLightIntensity = 4.0;
        vec3 radiance = vec3(dirLightIntensity);

        float shadow = shadowFactor(worldPos);
        Lo += evalBRDF(surf, NdotV, NdotL, NdotH, HdotV) * radiance * NdotL * shadow;
    }

    // Ambient (simple approximation)
    float ao = texture(ssaoBuffer, inUV).r;
    vec3 ambient = vec3(0.05) * surf.albedo * ao;

    vec3 color = ambient + Lo;

    // Tone mapping + gamma correction
    // Reinhard:  color = color / (color + vec3(1.0));
    // ACES:
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    color = clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);


    color = pow(color, vec3(1.0 / 2.2));
    outColor = vec4(color, 1.0);
    //    outColor = vec4(vec3(ao), 1.0);
}