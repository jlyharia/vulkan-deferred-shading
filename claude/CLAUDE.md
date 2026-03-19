# defer_render — Project Context for Claude

## What this is
A Vulkan 1.3 renderer in C++23, built as a learning/portfolio project focused on deferred shading, PBR, and GPU systems understanding. Uses dynamic rendering (no render passes).

## Build system
- CMake + Conan for package management
- `conan install . --output-folder=build --build=missing`
- Vulkan SDK at `~/VulkanSDK/1.4.335.0/x86_64`
- Shaders compiled to SPIR-V via `glslangValidator` at build time (output: `cmake-build-debug/shaders/`)
- After adding new shader files, re-run `cmake -B cmake-build-debug` to pick them up (GLOB_RECURSE)

## Key dependencies (via Conan)
- volk, VulkanHeaders, VulkanMemoryAllocator (VMA)
- glfw3, glm, glslang, stb, tinyobjloader, imgui, TinyGLTF

## Source layout (`src/`)
- `app/` — App (owns all subsystems, scene loading, main loop)
- `vulkan/` — VulkanContext, SwapChain, GraphicsPipeline, Validation, VulkanUtils
- `renderer/` — Renderer (multi-pipeline draw), UserInterface (ImGui)
- `scene/` — Mesh, MeshInstance, Camera, Transform, Texture, TextureManager, MeshUtils
- `system/` — GltfLoader
- `core/` — AssetManager
- `common/` — Vertex, Uniform, Material, PushConstantConstant, VulkanInclude, config
- `external/` — VMA and vendor impl files

## Architecture (as of pbr branch)

### Key types
| Type | Role |
|------|------|
| `Mesh` | GPU geometry blueprint — owns vertex/index buffers + Materials. Format-agnostic (doesn't know about glTF) |
| `MeshInstance` | Scene instance — holds `shared_ptr<Mesh>`, `Transform`, `name`, `color` (per-instance tint) |
| `Material` | PBR surface — `shared_ptr<Texture>` refs, baked descriptor set, `baseColorFactor`, `unlit` flag |
| `Texture` | Self-owning GPU image — destructor destroys imageView + VMA image |
| `TextureManager` | VRAM vault — `shared_ptr<Texture>` cache keyed by URI, fallbacks, sampler |
| `AssetManager` | Orchestrates loading — calls GltfLoader, uploads GPU buffers, caches Meshes |
| `GltfLoader` | Format translator — knows tinygltf, returns `MeshData`. Does NOT know about Mesh class |
| `GraphicsPipeline` | Holds `pbrPipeline_` + `unlitPipeline_` (shared layout). `buildPipeline(vertSpv, fragSpv)` |
| `Renderer` | Draws scene — selects pipeline per object via `material.unlit`, multiplies `instance.color * mat.baseColorFactor` in push constants |

### Ownership chain (destruction order in App)
```
App (unique_ptr owners, LIFO destruction)
  → VulkanContext (last to die)
  → SwapChain
  → Renderer
  → GraphicsPipeline
  → TextureManager  ← shared_ptr<Texture> cache
  → AssetManager    ← shared_ptr<Mesh> cache
  → UserInterface
  → renderObjects_  ← vector<MeshInstance> → shared_ptr<Mesh> → Materials → shared_ptr<Texture>
```
Texture GPU resources free themselves via `Texture::~Texture()` when last shared_ptr drops.

### Pipelines
| Pipeline | Shader | Used for |
|----------|--------|----------|
| PBR | `shaders/pbr/pbr.vert/frag` | Scene geometry (Sponza etc.) |
| Unlit | `shaders/unlit/unlit.vert/frag` | Light indicator spheres |

Both share the same `pipelineLayout_` (Set 0: GlobalUBO, Set 1: albedo/normal/metalRough, Push: modelMatrix + baseColorFactor).

### Push constants
```cpp
struct MeshPushConstants {
    glm::mat4 modelMatrix;      // 64 bytes
    glm::vec4 baseColorFactor;  // 16 bytes — instance.color * mat.baseColorFactor
};
```

### Descriptor sets
- **Set 0** (Global): `view`, `proj`, `cameraPos` — bound once per frame
- **Set 1** (Material): albedo, normal, metallicRoughness samplers — bound per submesh

### PBR shader (Cook-Torrance BRDF)
- GGX normal distribution, Smith geometry, Schlick-Fresnel
- Single directional light for now — point lights to be added via GlobalUBO extension
- sRGB→linear conversion on albedo, Reinhard tone mapping + gamma on output

## Coding conventions
- See `.clang-format`: indent 4, col limit 120, K&R braces (Attach), pointer-right (`int *p`)
- File names: PascalCase (`Mesh.hpp`, `MeshInstance.hpp`)
- No file-format knowledge in `Mesh` — loading is AssetManager/GltfLoader's job
- Scene data (positions, colors) defined as local structs in `loadScene()`/`loadPointLights()` in app.cpp — no separate config files for now

## Current branch: `pbr`
PBR Cook-Torrance shader integrated. Point lights exist as `vector<PointLight>` in App but are not yet passed to the shader (hardcoded directional light in pbr.frag).

## Roadmap (priority order)
1. ~~PBR shader (Cook-Torrance)~~ — done
2. Pass point lights to PBR shader via GlobalUBO
3. Deferred rendering (G-buffer: normal/albedo/roughness/metallic + lighting pass)
4. Depth reconstruct (replace position buffer)
5. Shadow maps (directional)
6. Render graph / frame graph
7. Reversed Z + infinite far plane
8. SSAO

## CLion run config env vars
```
VK_LAYER_PATH=/home/johnny/VulkanSDK/1.4.335.0/x86_64/share/vulkan/explicit_layer.d
LD_LIBRARY_PATH=/home/johnny/VulkanSDK/1.4.335.0/x86_64/lib
VULKAN_SDK=~/VulkanSDK/1.4.335.0/x86_64
```
