#version 450

// 1. Global UBO: Only contains data that stays the same for the WHOLE frame
layout (set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

// 2. Push Constant: Data that changes per DRAW CALL
layout (push_constant) uniform Push {
    mat4 model;
} pc;


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inColor;

layout (location = 0) out vec3 fragPos;
layout (location = 1) out vec3 fragNormal;
layout (location = 2) out vec3 fragColor;
layout (location = 3) out vec2 fragTexCoord;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;

    fragPos = worldPos.xyz;

    // FIX: Use pc.model for normal transformation
    fragNormal = mat3(transpose(inverse(pc.model))) * inNormal;
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}