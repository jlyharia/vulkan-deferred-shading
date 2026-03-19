# Session Notes — PBR Branch

## Current State (branch: `pbr`)
- Shader: `shaders/blinn-phong/` — single hardcoded directional light, Blinn-Phong math
- `GlobalUBO` in `Uniform.hpp` still has only `view`, `proj`, `vec3 cameraPos`
- No point lights yet

## What We Planned / In Progress

### Step 1: Expand GlobalUBO with point lights
**`src/common/Uniform.hpp`** — add `PointLight` struct, expand `GlobalUBO`:
```cpp
struct PointLight {
    glm::vec4 position;  // w unused
    glm::vec4 color;     // rgb = color, w = intensity
};

struct GlobalUBO {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec4 cameraPos;       // was vec3 — promoted to vec4
    PointLight lights[4];
    glm::vec4 lightCount;                  // x = count, yzw unused (avoids padding)
};
```

**`src/renderer/renderer.cpp`** — `updateUniformBuffer()`: populate 4 hardcoded lights:
```cpp
GlobalUBO ubo{
    .view = camera.getViewMatrix(),
    .proj = camera.getProjectionMatrix(...),
    .cameraPos = glm::vec4(camera.position, 1.0f),
    .lights = {
        PointLight{.position = {0.0f,  0.0f, 4.0f, 0.0f}, .color = {1.0f, 0.95f, 0.9f, 200.0f}},
        PointLight{.position = {4.0f,  0.0f, 2.0f, 0.0f}, .color = {1.0f, 0.7f,  0.4f, 100.0f}},
        PointLight{.position = {-4.0f, 0.0f, 2.0f, 0.0f}, .color = {0.4f, 0.7f,  1.0f, 100.0f}},
        PointLight{.position = {0.0f,  0.0f, 1.0f, 0.0f}, .color = {1.0f, 1.0f,  1.0f,  50.0f}},
    },
    .lightCount = {4.0f, 0.0f, 0.0f, 0.0f},
};
```

**`shaders/blinn-phong/blinn-phong.frag`** — update UBO struct declaration and replace hardcoded light with loop:
```glsl
struct PointLight {
    vec4 position;
    vec4 color;  // rgb = color, w = intensity
};

layout (set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    PointLight lights[4];
    vec4 lightCount;
} ubo;
```
Lighting loop in `main()`:
```glsl
vec3 V = normalize(ubo.cameraPos.xyz - fragPos);
float shininess = (1.0 - roughness) * 128.0;

vec3 Lo = vec3(0.0);
int numLights = int(ubo.lightCount.x);
for (int i = 0; i < numLights; i++) {
    vec3 lightPos   = ubo.lights[i].position.xyz;
    vec3 lightColor = ubo.lights[i].color.rgb;
    float intensity = ubo.lights[i].color.w;

    vec3 toLight = lightPos - fragPos;
    float dist   = length(toLight);
    vec3 L       = toLight / dist;
    vec3 H       = normalize(L + V);
    float attenuation = intensity / (dist * dist);

    float dotNL  = max(dot(worldNormal, L), 0.0);
    vec3 diffuse = dotNL * albedoSample.rgb * (1.0 - metallic) * lightColor * attenuation;

    float spec    = pow(max(dot(worldNormal, H), 0.0), shininess);
    vec3 specular = spec * lightColor * attenuation * (metallic + 0.1);

    Lo += diffuse + specular;
}
vec3 ambient = 0.05 * albedoSample.rgb;
vec3 color = ambient + Lo;
```

### Goal of Step 1
Verify 4 point lights work in Blinn-Phong before moving to Cook-Torrance PBR shader.

---

## Next Steps After Verification
1. Create `shaders/pbr/pbr.vert` (copy of blinn-phong.vert)
2. Create `shaders/pbr/pbr.frag` with Cook-Torrance BRDF:
   - GGX/Trowbridge-Reitz NDF
   - Smith-Schlick-GGX geometry term
   - Schlick Fresnel, F0 = mix(vec3(0.04), albedo, metallic)
   - No IBL needed for now
3. Point pipeline creation in `app.cpp` to new pbr shaders

## Key Design Decisions
- `lightCount` as `vec4` (not int) to avoid std140 alignment/padding issues
- Light `color.w` = intensity, using inverse-square attenuation: `intensity / (dist * dist)`
- Light positions may need tuning once we see them in Sponza — coordinate system TBD
- IBL skipped for now — not needed to demonstrate Cook-Torrance math
