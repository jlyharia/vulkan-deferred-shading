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
    PointLight pointLights[24];
} ubo;

// G-buffer inputs (set 2)
layout (set = 2, binding = 0) uniform sampler2D gbAlbedoMetallic;
layout (set = 2, binding = 1) uniform sampler2D gbNormalRoughness;
layout (set = 2, binding = 2) uniform sampler2D gbDepth;
layout (set = 2, binding = 3) uniform sampler2D ssaoBuffer;

const float PI = 3.14159265359;

// --- Cook-Torrance BRDF functions (same as forward PBR) ---

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

    // --- Unpack G-buffer ---
    vec3 albedo = albedoMetallic.rgb;
    float metallic = albedoMetallic.a;
    vec3 N = normalize(normalRoughness.xyz);
    float roughness = normalRoughness.a;

    // --- Reconstruct world position from depth ---
    vec3 worldPos = reconstructWorldPos(inUV, depth);

    // --- Lighting setup ---
    vec3 V = normalize(ubo.cameraPos.xyz - worldPos);
    float NdotV = max(dot(N, V), 0.0001);

    // F0: base reflectivity (dielectric = 0.04, metal uses albedo)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

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
        float intensity = ubo.pointLights[i].position.w;
        vec3 lightColor = ubo.pointLights[i].color.xyz;

        // KHR_lights_punctual windowed attenuation — smoothly reaches 0 at radius, no pop
        float window = pow(max(1.0 - pow(dist / radius, 4.0), 0.0), 2.0);
        float attenuation = (intensity / (dist * dist + 1.0)) * window;
        vec3 radiance = lightColor * attenuation;

        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float HdotV = max(dot(H, V), 0.0);

        // --- Cook-Torrance BRDF ---
        float D = distributionGGX(NdotH, roughness);
        float G = geometrySmith(NdotV, NdotL, roughness);
        vec3 F = fresnelSchlick(HdotV, F0);

        vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        vec3 diffuse = kD * albedo / PI;

        Lo += (diffuse + specular) * radiance * NdotL;
    }

    // Ambient (simple approximation)
    float ao = texture(ssaoBuffer, inUV).r;
    vec3 ambient = vec3(0.05) * albedo * ao;

    vec3 color = ambient + Lo;

    // Tone mapping (Reinhard) + gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(vec3(ao), 1.0);
}