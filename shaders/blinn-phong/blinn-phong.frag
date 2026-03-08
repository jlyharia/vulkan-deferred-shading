#version 450

// Inputs from Vertex Shader
layout (location = 0) in vec3 fragPos;
layout (location = 1) in vec3 fragNormal;
layout (location = 2) in vec3 fragColor;    // The 0.588 gray stone color
layout (location = 3) in vec2 fragTexCoord;

layout (location = 0) out vec4 outColor;

// Global Data
layout (set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

// Texture Sampler
layout (set = 1, binding = 0) uniform sampler2D texSampler;

// Simple dither to reduce banding
float screen_dither(vec2 uv) {
    return (fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
}

void main() {
    // 1. Handle Normals (Two-Sided Lighting)
    // Sponza arches and banners often have inverted faces.
    // If we're looking at the back, flip the normal so lighting works.
    vec3 N = normalize(fragNormal);
    if (!gl_FrontFacing) {
        N = -N;
    }

    // 2. Lighting Vectors
    vec3 L = normalize(vec3(-10.0, -10.0, 30.0)); // Directional light
    vec3 V = normalize(ubo.cameraPos.xyz - fragPos);
    vec3 H = normalize(L + V);

    // 3. Texture Sampling & Fallback
    vec4 texColor = texture(texSampler, fragTexCoord);

    // DETERMINING BASE COLOR:
    // If texture is missing (alpha ~0) or the texture is purely black,
    // we use the fragColor (the stone gray from your logs).
    vec3 baseColor;
    if (texColor.a < 0.05 || length(texColor.rgb) < 0.01) {
        baseColor = fragColor;
    } else {
        // Standard glTF: Multiply texture by the material factor
        baseColor = texColor.rgb * fragColor;
    }

    // 4. Transparency (Sponza leaves/decals)
    // Only discard if the texture was actually supposed to be there (alpha > 0)
    if (texColor.a > 0.0 && texColor.a < 0.1) {
        discard;
    }

    // 5. Blinn-Phong Lighting Calculation
    vec3 lightColor = vec3(1.0);
    vec3 ambient = 0.15 * lightColor; // Slightly boosted ambient

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    vec3 finalRGB = (ambient + diff + spec) * baseColor;

    // 6. Final Output with Dithering
    float d = screen_dither(gl_FragCoord.xy);
    outColor = vec4(finalRGB + d, 1.0);
}