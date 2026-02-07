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
    // 1. Basic Position Math
    vec4 worldPos4 = ubo.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos4;
    vec3 worldPos = worldPos4.xyz;

    // 2. Transform Normal to World Space
    mat3 normalMatrix = mat3(transpose(inverse(ubo.model)));
    vec3 worldNormal = normalize(normalMatrix * inNormal);

    // 3. Light and View Settings
    vec3 lightPos = vec3(-10.0, -10.0, 30.0);
    vec3 lightColor = vec3(1.0, 1.0, 1.0);

    // Camera position in world space is needed for specular
    // Extracting cam pos from the inverse of the view matrix
    vec3 viewPos = inverse(ubo.view)[3].xyz;

    // --- Lighting Calculations ---

    // A. Ambient
    float ambientStrength = 0.05;
    vec3 ambient = ambientStrength * lightColor;

    // B. Diffuse
    vec3 lightDir = normalize(lightPos - worldPos);
    float diff = max(dot(worldNormal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * 0.4;

    // C. Specular (The new part)
    float specularStrength = 0.7;
    vec3 viewDir = normalize(viewPos - worldPos);
    vec3 reflectDir = reflect(-lightDir, worldNormal);

    // 1. Calculate the Halfway Vector
    vec3 halfwayDir = normalize(lightDir + viewDir);

    // 2. Use the dot product of the Normal and the Halfway vector
    // Note: You typically need a higher power (shininess) for Blinn-Phong
    // to match the "tightness" of standard Phong.
    float spec = pow(max(dot(worldNormal, halfwayDir), 0.0), 64.0);
    vec3 specular = specularStrength * spec * lightColor;

    // 4. Final Output
    fragColor = (ambient + diffuse + specular) * inColor;
}