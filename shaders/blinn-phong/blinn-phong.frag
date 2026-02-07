#version 450

layout (location = 0) in vec3 fragPos;
layout (location = 1) in vec3 fragNormal;
layout (location = 2) in vec3 fragColor; // Color from Vertex Shader

layout (location = 0) out vec4 outColor;

layout (push_constant) uniform Push {
    int shadingMode; // 0 = Gouraud, 1 = Phong
} pc;

layout (set = 0, binding = 0) uniform UBO {
    mat4 model; mat4 view; mat4 proj;
} ubo;

void main() {
    if (pc.shadingMode == 1) {
        // --- PHONG SHADING (Per-Pixel) ---
        vec3 N = normalize(fragNormal);
        vec3 L = normalize(vec3(-10.0, -10.0, 30.0) - fragPos);
        vec3 V = normalize(inverse(ubo.view)[3].xyz - fragPos);
        vec3 H = normalize(L + V);

        vec3 ambient = 0.05 * vec3(1.0);
        float diff = max(dot(N, L), 0.0) * 0.2;
        float spec = pow(max(dot(N, H), 0.0), 64.0);

        vec3 lighting = (ambient + diff + spec) * fragColor;
        outColor = vec4(lighting, 1.0);
    } else {
        // --- GOURAUD SHADING (Per-Vertex) ---
        // Just use the color that was already lit in the vertex shader
        outColor = vec4(fragColor, 1.0);
    }
}