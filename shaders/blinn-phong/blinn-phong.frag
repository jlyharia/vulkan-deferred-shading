#version 450

// 1. Fixed Locations to match your C++ struct
layout (location = 0) in vec3 fragPos;
layout (location = 1) in vec3 fragNormal;
layout (location = 2) in vec3 fragColor;
layout (location = 3) in vec2 fragTexCoord;
layout (location = 4) in vec4 inTangent; // Corrected to 4

layout (location = 0) out vec4 outColor;

// update per frame
layout (set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

// Set 1: Matches your Renderer::createDescriptorSetLayout
layout (set = 1, binding = 0) uniform sampler2D texSampler;
layout (set = 1, binding = 1) uniform sampler2D normalSampler;
layout (set = 1, binding = 2) uniform sampler2D metalRoughMap; // The 3rd binding!


layout (push_constant) uniform Push {
    mat4 model;
    vec4 baseColorFactor;
} pc;


void main() {
    // Standard Alpha Cutout (Fixes the "Strange" Lion edges)
    vec4 albedoSample = texture(texSampler, fragTexCoord) * pc.baseColorFactor;
    if (albedoSample.a < 0.1) {
        discard;
    }

    // --- NORMAL MAPPING (TBN) ---
    vec3 N = normalize(fragNormal);
    if (!gl_FrontFacing) { N = -N; }

    // Reconstruct the TBN matrix
    vec3 T = normalize(inTangent.xyz);
    vec3 B = cross(N, T) * inTangent.w;
    mat3 TBN = mat3(T, B, N);

    // Sample normal map and remap from [0, 1] to [-1, 1]
    vec3 localNormal = texture(normalSampler, fragTexCoord).rgb * 2.0 - 1.0;

    // Transform normal to World Space
    // If the normal map is missing/black, this will effectively use N
    vec3 worldNormal = normalize(TBN * normalize(localNormal));

    // In glTF: B = Roughness, G = Metallic
    vec4 mrSample = texture(metalRoughMap, fragTexCoord);
    float roughness = mrSample.g;
    float metallic = mrSample.b;

    // --- LIGHTING ---
    vec3 L = normalize(vec3(-10.0, -10.0, 30.0));
    vec3 V = normalize(ubo.cameraPos.xyz - fragPos);
    vec3 H = normalize(L + V);

    // Diffuse
    float dotNL = max(dot(worldNormal, L), 0.0);
    vec3 diffuse = dotNL * albedoSample.rgb * (1.0 - metallic);

    // Specular (Roughness-dependent)
    // We map roughness to a more visible specular power
    float shininess = (1.0 - roughness) * 128.0;
    float spec = pow(max(dot(worldNormal, H), 0.0), shininess);
    vec3 specular = spec * vec3(0.3) * (metallic + 0.1);

    // 5. Final Composition
    vec3 ambient = 0.05 * albedoSample.rgb;
    vec3 color = ambient + diffuse + specular;

    // HDR Tonemapping (Simple Reindhard) + Gamma Correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
//    outColor = vec4(worldNormal * 0.5 + 0.5, 1.0);
}