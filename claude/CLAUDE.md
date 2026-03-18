# defer_render — Project Context for Claude

## What this is
A Vulkan 1.3 renderer in C++23, built as a learning/portfolio project focused on deferred shading, PBR, and GPU systems understanding. Uses dynamic rendering (no render passes).

## Build system
- CMake + Conan for package management
- `conan install . --output-folder=build --build=missing`
- Vulkan SDK at `~/VulkanSDK/1.4.335.0/x86_64`
- Shaders compiled to SPIR-V via `glslangValidator` at build time (output: `build/shaders/`)

## Key dependencies (via Conan)
- volk, VulkanHeaders, VulkanMemoryAllocator (VMA)
- glfw3, glm, glslang, stb, tinyobjloader, imgui, TinyGLTF

## Source layout (`src/`)
- `app/` — application entry/loop
- `vulkan/` — VulkanContext, swap chain, graphics pipeline, validation, utils
- `renderer/` — renderer, ImGui UI
- `scene/` — Camera, Model, RenderObject, Transform, Texture, TextureManager
- `system/` — GltfLoader
- `core/` — AssetManager
- `common/` — Vertex, Uniform, Material, PushConstant, VulkanInclude, config
- `external/` — VMA and vendor impl files

## Current branch: `pbr`
Working on PBR shader integration.

## Roadmap (priority order)
1. PBR (Cook-Torrance, Fresnel-Schlick) — current
2. Deferred rendering (G-buffer: normal/albedo/roughness/metallic + lighting pass)
3. Depth reconstruct (replace position buffer)
4. Shadow maps (directional)
5. Render graph / frame graph
6. Reversed Z + infinite far plane
7. SSAO

## CLion run config env vars
```
VK_LAYER_PATH=/home/johnny/VulkanSDK/1.4.335.0/x86_64/share/vulkan/explicit_layer.d
LD_LIBRARY_PATH=/home/johnny/VulkanSDK/1.4.335.0/x86_64/lib
VULKAN_SDK=~/VulkanSDK/1.4.335.0/x86_64
```
