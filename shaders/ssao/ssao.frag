#version 450

layout (location = 0) in vec2 inUV;
// ssao output a singel float
layout (location = 0) out float outColor;

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
layout (set = 2, binding = 1) uniform sampler2D gbNormalRoughness;
layout (set = 2, binding = 2) uniform sampler2D gbDepth;


layout (set = 3, binding = 0) uniform SSAOKernelUBO {
    vec4 samples[64];
} kernel;

layout (set = 3, binding = 1) uniform sampler2D SSAONoise;
float radius = 0.5;
float bias = 0.025; // incase of self-occlusion

/// Reconstruct view-space position from depth buffer + inverse matrices.
/// Avoids storing position as a G-buffer render target (saves 16 bytes/pixel).
vec3 reconstructViewPos(vec2 uv, float depth) {
    // UV [0,1] -> NDC [-1,1]
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos = ubo.invProj * clipPos;
    viewPos /= viewPos.w;
    return viewPos.xyz;
}


void main() {
    // --- Sample G-buffer ---
    vec3 normalInView = normalize(mat3(ubo.view) * texture(gbNormalRoughness, inUV).xyz);
    vec3 randomVec = texture(SSAONoise, inUV * 4.0).xyz; // tile noise texture
    // Gramm-Schmidt orthogonalization
    // TBN [XYZ], cross(T,B) = N, cross(N, T) = B
    vec3 tangent = normalize(randomVec - normalInView * dot(normalInView, randomVec));
    // generate occlusion factor

    vec3 bitangent = cross(normalInView, tangent);
    mat3 TBN = mat3(tangent, bitangent, normalInView);
    vec3 actualPosInView = reconstructViewPos(inUV, texture(gbDepth, inUV).r);
    float occlusion = 0.0f;
    for (int i = 0; i < 64; i++) {
        vec3 dirVec = TBN * kernel.samples[i].xyz;
        vec3 samplePosInView = actualPosInView + dirVec * radius;
        vec4 offset = ubo.proj * vec4(samplePosInView, 1.0);
        offset.xyz /= offset.w; // go to clip space
        offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 ~ 1.0

        // depth both in view space, so they are comparable
        vec3 offsetViewPos = reconstructViewPos(offset.xy, texture(gbDepth, offset.xy).r);
        // check if occluded


        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(actualPosInView.z - offsetViewPos.z));
        occlusion += (offsetViewPos.z - samplePosInView.z > bias ? 1.0 : 0.0) * rangeCheck;
    }
    // occlusion factor, the smaller, the more occlude it get
    float occlusionFactor = 1.0 - occlusion / 64.0;
//    outColor = vec4(vec3(occlusionFactor), 1.0);
    outColor = occlusionFactor;
}