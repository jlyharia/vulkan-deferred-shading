#version 450

layout (location = 0) in vec3 fragNormal;
layout (location = 1) in vec3 fragColor;
layout (location = 2) in vec2 fragTexCoord;
layout (location = 3) in vec4 inTangent;

// MRT outputs — two render targets for the G-buffer
layout (location = 0) out vec4 outAlbedoMetallic;   // RT0: albedo.rgb + metallic in alpha
layout (location = 1) out vec4 outNormalRoughness;   // RT1: worldNormal.xyz + roughness in alpha

layout (set = 1, binding = 0) uniform sampler2D albedoMap;
layout (set = 1, binding = 1) uniform sampler2D normalMap;
layout (set = 1, binding = 2) uniform sampler2D metallicRoughnessMap;

layout (push_constant) uniform Push {
    mat4 model;
    vec4 baseColorFactor;
} pc;

void main() {
    // --- Sample textures ---
    vec4 albedoSample = texture(albedoMap, fragTexCoord) * pc.baseColorFactor;
    if (albedoSample.a < 0.1)
        discard;

    // Store sRGB albedo as-is; decode to linear in the lighting pass
    vec3 albedo = albedoSample.rgb;

    // glTF: R=occlusion, G=roughness, B=metallic
    vec4 mrSample = texture(metallicRoughnessMap, fragTexCoord);
    float roughness = clamp(mrSample.g, 0.04, 1.0);
    float metallic  = clamp(mrSample.b, 0.0, 1.0);

    // --- Normal mapping (TBN) ---
    vec3 N = normalize(fragNormal);
    if (!gl_FrontFacing) N = -N;

    vec3 T = normalize(inTangent.xyz);
    vec3 B = cross(N, T) * inTangent.w;
    mat3 TBN = mat3(T, B, N);

    vec3 localNormal = texture(normalMap, fragTexCoord).rgb * 2.0 - 1.0;
    N = normalize(TBN * normalize(localNormal));

    // --- Write G-buffer ---
    outAlbedoMetallic  = vec4(albedo, metallic);
    outNormalRoughness = vec4(N, roughness); // RT1 is R16G16B16A16_SFLOAT, stores [-1,1] directly
}