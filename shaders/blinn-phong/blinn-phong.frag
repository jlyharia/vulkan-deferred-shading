#version 450

layout (location = 0) in vec3 fragPos;
layout (location = 1) in vec3 fragNormal;
layout (location = 2) in vec3 fragColor;

layout (location = 0) out vec4 outColor;

// FIX: Must match the Vertex shader Push Constant block exactly
layout (push_constant) uniform Push {
    mat4 model;
} pc;

// FIX: Must match the Vertex shader UBO block exactly
layout (set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
} ubo;

float screen_dither(vec2 uv) {
    return (fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
}

void main() {
    vec3 N = normalize(fragNormal);

    // Simple directional light logic
    vec3 L = normalize(vec3(-10.0, -10.0, 30.0));

    // Extract camera position from view matrix
    vec3 cameraPos = inverse(ubo.view)[3].xyz;
    vec3 V = normalize(cameraPos - fragPos);
    vec3 H = normalize(L + V);

    vec3 ambient = 0.1 * vec3(1.0);
    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    vec3 lighting = (ambient + diff + spec) * fragColor;
    float d = screen_dither(gl_FragCoord.xy);

    outColor = vec4(lighting + d, 1.0);
}