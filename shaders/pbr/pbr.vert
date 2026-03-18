#version 450

layout (set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

layout (push_constant) uniform Push {
    mat4 model;
    vec4 baseColorFactor;
} pc;

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;
layout (location = 3) in vec3 inColor;
layout (location = 4) in vec4 inTangent;

layout (location = 0) out vec3 fragPos;
layout (location = 1) out vec3 fragNormal;
layout (location = 2) out vec3 fragColor;
layout (location = 3) out vec2 fragTexCoord;
layout (location = 4) out vec4 fragTangent;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;

    fragPos = worldPos.xyz;

    mat3 normalMatrix = mat3(transpose(inverse(pc.model)));
    fragNormal = normalize(normalMatrix * inNormal);

    vec3 T = normalize(normalMatrix * inTangent.xyz);
    fragTangent = vec4(T, inTangent.w);

    fragColor = inColor;
    fragTexCoord = vec2(inTexCoord.x, 1.0 - inTexCoord.y);
}
