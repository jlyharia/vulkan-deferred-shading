#version 450

layout (location = 0) in vec2 inUV;
// ssao output a singel float
layout (location = 0) out float outColor;


layout (set = 0, binding = 0) uniform sampler2D depthTexture;
layout (set = 0, binding = 1) uniform sampler2D ssaoTexture;

// Controls how aggressively depth discontinuities suppress blur bleeding.
// Higher = harder AO edges; lower = softer depth-edge transitions.
const float DEPTH_SCALE = 50.0;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoTexture, 0));
    float centerDepth = texture(depthTexture, inUV).r;

    float result = 0.0;
    float totalWeight = 0.0;

    for (int x = -2; x < 2; ++x)
    {
        for (int y = -2; y < 2; ++y)
        {
            vec2 sampleUV = inUV + vec2(float(x), float(y)) * texelSize;

            float tapDepth = texture(depthTexture, sampleUV).r;
            // Linear falloff: cheaper than exp(), same edge-preservation behavior.
            float weight = max(0.0, 1.0 - abs(centerDepth - tapDepth) * DEPTH_SCALE);

            result += texture(ssaoTexture, sampleUV).r * weight;
            totalWeight += weight;
        }
    }

    outColor = result / totalWeight;
}