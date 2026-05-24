#version 450

#define NUM_CASCADES 4
//#define DEBUG_CASCADES  // uncomment to tint each cascade a distinct color

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
    mat4 dirLightSpaceMatrices[NUM_CASCADES]; // 4 × (light proj * view) for CSM
    vec4 cascadeSplitDepths;                  // x,y,z,w = cascade far distances (positive metres from camera)
    vec4 dirLightDir;
    PointLight pointLights[24];
} ubo;

// G-buffer inputs (set 2)
layout (set = 2, binding = 0) uniform sampler2D gbAlbedoMetallic;
layout (set = 2, binding = 1) uniform sampler2D gbNormalRoughness;
layout (set = 2, binding = 2) uniform sampler2D gbDepth;
layout (set = 2, binding = 3) uniform sampler2D ssaoBuffer;
layout (set = 2, binding = 4) uniform sampler2DArrayShadow shadowMap; // hardware PCF, array for CSM

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

///// Returns x = shadow factor (1.0 lit, 0.0 shadowed), y = cascade index (0..NUM_CASCADES-1).
///// Selects the tightest cascade whose far plane exceeds the fragment's view-space depth, then
///// samples the corresponding layer of the 2D shadow array with hardware PCF.
//vec2 shadowFactor(vec3 worldPos) {
//    // Camera view-space Z is negative for in-front fragments (RH, camera looks down -Z).
//    // Negate to get a positive linear depth in metres from camera — same units as cascadeSplitDepths.
//    float fragDist = -(ubo.view * vec4(worldPos, 1.0)).z;
//
//    // Pick the finest cascade that covers this fragment; default to the coarsest.
//    int cascade = NUM_CASCADES - 1;
//    for (int i = 0; i < NUM_CASCADES; ++i) {
//        if (fragDist < ubo.cascadeSplitDepths[i]) {
//            cascade = i;
//            break;
//        }
//    }
//
//    vec4 lightClip = ubo.dirLightSpaceMatrices[cascade] * vec4(worldPos, 1.0);
//    vec3 proj = lightClip.xyz / lightClip.w;  // orthographic: w=1, but keep general
//    vec2 shadowUV = proj.xy * 0.5 + 0.5;     // NDC [-1,1] → UV [0,1]
//
//    // Outside this cascade's light frustum → treat as lit
//    if (any(lessThan(shadowUV, vec2(0.0))) || any(greaterThan(shadowUV, vec2(1.0))))
//        return vec2(1.0, float(cascade));
//    if (proj.z < 0.0 || proj.z > 1.0)
//        return vec2(1.0, float(cascade));
//
//    // Shader-side bias in NDC space. Scale with cascade index so that the world-space dead zone
//    // stays roughly constant: cascade 0 (~5m range) needs less NDC bias than cascade 3 (~140m).
//    // The shadow pass slope-scaled GPU bias handles most self-shadowing; this is just insurance.
//    const float biasScale[4] = float[4](0.0002, 0.0004, 0.0008, 0.0015);
//    float currentDepth = proj.z + biasScale[cascade];
//
//    // texture(sampler2DArrayShadow, vec4) — .z = array layer, .w = reference depth for compareOp.
//    // Hardware PCF returns fraction of the 2x2 footprint that passes the comparison (i.e. lit).
//    float s = texture(shadowMap, vec4(shadowUV, float(cascade), currentDepth));
//    return vec2(s, float(cascade));
//}

/// Define arrays at global scope
const float biasScale[4] = float[4](0.0002, 0.0004, 0.0008, 0.0015);

// Safely extracts dynamic scalar components from the ubo.cascadeSplitDepths vec4 container
float getCascadeSplitDepth(int index) {
    if (index == 0) return ubo.cascadeSplitDepths.x;
    if (index == 1) return ubo.cascadeSplitDepths.y;
    if (index == 2) return ubo.cascadeSplitDepths.z;
    return ubo.cascadeSplitDepths.w;
}

// Standard GLSL Helper Function
float getShadowAmount(vec3 worldPos, int cascadeIdx) {
    vec4 lightClip = ubo.dirLightSpaceMatrices[cascadeIdx] * vec4(worldPos, 1.0);
    vec3 proj = lightClip.xyz / lightClip.w;
    vec2 shadowUV = proj.xy * 0.5 + 0.5;

    // Clip bounds check
    if (any(lessThan(shadowUV, vec2(0.0))) || any(greaterThan(shadowUV, vec2(1.0))) ||
    proj.z < 0.0 || proj.z > 1.0) {
        return 1.0;
    }

    float currentDepth = proj.z - biasScale[cascadeIdx]; // Reverse-Z subtractive bias
    return texture(shadowMap, vec4(shadowUV, float(cascadeIdx), currentDepth));
}

vec2 shadowFactor(vec3 worldPos) {
    float fragDist = -(ubo.view * vec4(worldPos, 1.0)).z;

    if (fragDist > getCascadeSplitDepth(NUM_CASCADES - 1)) {
        return vec2(1.0, float(NUM_CASCADES - 1));
    }

    int cascade = NUM_CASCADES - 1;
    for (int i = 0; i < NUM_CASCADES; ++i) {
        if (fragDist < ubo.cascadeSplitDepths[i]) {
            cascade = i;
            break;
        }
    }

    // Call the global helper function
    float s = getShadowAmount(worldPos, cascade);

    // Cascade Blending
    if (cascade < NUM_CASCADES - 1) {
        float nextSplit = getCascadeSplitDepth(cascade);
        float distToNextSplit = nextSplit - fragDist;
        float blendRange = 3.0;

        if (distToNextSplit < blendRange) {
            float blendWeight = 1.0 - (distToNextSplit / blendRange);

            // Call it again for the next layer
            float nextCascadeShadow = getShadowAmount(worldPos, cascade + 1);
            s = mix(s, nextCascadeShadow, blendWeight);
        }
    }

    return vec2(s, float(cascade));
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

    // Sky/background pixels — no geometry wrote here (reverse-Z: clear = 0.0)
    if (depth <= 0.0) {
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
    int cascadeIdx = 0;  // set by shadowFactor; used by DEBUG_CASCADES tint below
    {
        vec3 L = normalize(-ubo.dirLightDir.xyz); // stored as light→scene; negate for frag→light
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float HdotV = max(dot(H, V), 0.0);

        const float dirLightIntensity = 4.0;
        vec3 radiance = vec3(dirLightIntensity);

        vec2 shadowResult = shadowFactor(worldPos);
        float shadow = shadowResult.x;
        cascadeIdx = int(shadowResult.y);
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

    #ifdef DEBUG_CASCADES
    // Tint: cascade 0 = red (nearest), 1 = green, 2 = blue, 3 = yellow
    vec3 cascadeColors[4] = vec3[4](
    vec3(1.0, 0.2, 0.2),
    vec3(0.2, 1.0, 0.2),
    vec3(0.2, 0.4, 1.0),
    vec3(1.0, 1.0, 0.2)
    );
    color = mix(color, cascadeColors[cascadeIdx], 0.4);
    #endif

    outColor = vec4(color, 1.0);
    //    outColor = vec4(vec3(ao), 1.0);
}