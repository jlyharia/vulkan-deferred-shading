#version 450

struct PointLight {
    vec4 position;
    vec4 color;
};

layout (set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    mat4 invView;
    mat4 invProj;
    vec4 cameraPos;
    PointLight pointLights[24];
} ubo;

struct InstanceData {
    mat4 modelMatrix;
    vec4 color;
};

layout (set = 0, binding = 1) readonly buffer InstanceBuffer {
    InstanceData instances[];
};

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;
layout (location = 3) in vec3 inColor;
layout (location = 4) in vec4 inTangent;

layout (location = 0) out vec4 fragColor;

void main() {
    InstanceData inst = instances[gl_InstanceIndex];
    gl_Position = ubo.proj * ubo.view * inst.modelMatrix * vec4(inPosition, 1.0);
    fragColor = inst.color;
}
