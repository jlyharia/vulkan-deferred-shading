#version 450

layout (location = 0) in vec3 fragPos;
layout (location = 1) in vec3 fragNormal;
layout (location = 2) in vec3 fragColor;
layout (location = 3) in vec2 fragTexCoord;
layout (location = 4) in vec4 inTangent;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

layout (set = 1, binding = 0) uniform sampler2D albedoMap;
layout (set = 1, binding = 1) uniform sampler2D normalMap;
layout (set = 1, binding = 2) uniform sampler2D metallicRoughnessMap;

layout (push_constant) uniform Push {
    mat4 model;
    vec4 baseColorFactor;
} pc;

const float PI = 3.14159265359;

// GGX / Trowbridge-Reitz normal distribution
float distributionGGX(float NdotH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
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

void main() {
    // --- Sample textures ---
    vec4 albedoSample = texture(albedoMap, fragTexCoord) * pc.baseColorFactor;
    if (albedoSample.a < 0.1)
        discard;

    vec3 albedo = pow(albedoSample.rgb, vec3(2.2)); // sRGB → linear

    // glTF: R=occlusion, G=roughness, B=metallic
    vec4 mrSample  = texture(metallicRoughnessMap, fragTexCoord);
    float roughness = clamp(mrSample.g, 0.04, 1.0);
    float metallic  = clamp(mrSample.b, 0.0,  1.0);

    // --- Normal mapping ---
    vec3 N = normalize(fragNormal);
    if (!gl_FrontFacing) N = -N;

    vec3 T = normalize(inTangent.xyz);
    vec3 B = cross(N, T) * inTangent.w;
    mat3 TBN = mat3(T, B, N);

    vec3 localNormal = texture(normalMap, fragTexCoord).rgb * 2.0 - 1.0;
    N = normalize(TBN * normalize(localNormal));

    // --- Lighting setup ---
    vec3 V   = normalize(ubo.cameraPos.xyz - fragPos);
    vec3 L   = normalize(vec3(-1.0, -2.0, 3.0)); // directional light direction
    vec3 H   = normalize(V + L);
    vec3 lightColor = vec3(3.0); // white directional light, intensity 3

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0001);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    // --- Cook-Torrance BRDF ---
    // F0: base reflectivity (dialectric = 0.04, metal uses albedo)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float D = distributionGGX(NdotH, roughness);
    float G = geometrySmith(NdotV, NdotL, roughness);
    vec3  F = fresnelSchlick(HdotV, F0);

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);

    // Energy conservation: diffuse only on non-metal
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    vec3 Lo = (diffuse + specular) * lightColor * NdotL;

    // Ambient (simple approximation)
    vec3 ambient = vec3(0.03) * albedo;

    vec3 color = ambient + Lo;

    // Tone mapping (Reinhard) + gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
