#version 450

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform ShadowData {
    mat4 lightSpaceMatrix; // Projection * View
    mat4 model;
} PushConstants;

void main() {
    // Standard transformation to Light Clip Space
    gl_Position = PushConstants.lightSpaceMatrix * PushConstants.model * vec4(inPosition, 1.0);
}