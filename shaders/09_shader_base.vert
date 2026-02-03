#version 450

layout (set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec2 inTexCoord;


layout (location = 0) out vec3 fragColor;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);

    // 1. Transform normal to World Space
    // We use the inverse transpose of the model matrix to keep normals perpendicular if scaled
    mat3 normalMatrix = mat3(transpose(inverse(ubo.model)));
    vec3 worldNormal = normalize(normalMatrix * inNormal);

    // 2. Simple Light Settings (You can later move these to a UBO)
    vec3 lightPos = vec3(-30.0, -30.0, 30.0);
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    float ambientStrength = 0.2;

    // 3. Calculate Light Direction and Diffuse factor
    vec3 worldPos = vec4(ubo.model * vec4(inPosition, 1.0)).xyz;
    vec3 lightDir = normalize(lightPos - worldPos);

    // Dot product determines how much light hits the surface (0.0 to 1.0)
    float diff = max(dot(worldNormal, lightDir), 0.0);

    // 4. Combine results
    vec3 ambient = ambientStrength * lightColor;
    vec3 diffuse = diff * lightColor;

    // Pass the calculated lighting applied to the vertex color to the fragment shader
    fragColor = (ambient + diffuse) * inColor;
}
//void main() {
//    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
//    fragColor = inColor;
//}
