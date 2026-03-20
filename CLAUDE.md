# defer_render — Project Memory

## Project
Vulkan 1.3 deferred shading renderer in C++23. Portfolio/learning project targeting graphics career switch.
Build: Conan + CMake. Memory allocator: VMA. No render passes — uses dynamic rendering throughout.

## User
CS MS (graphics, 2013). Staff Engineer at eBay 10yr. Switching to graphics career.
Favor system-level insight (bandwidth, memory lifetime, GPU trade-offs) over visual polish.

## Workflow
- Spawn a **Plan agent** to review implementation steps before coding, and to find bugs after.
- Commit with detailed messages. Push after each feature.
- File naming: all source files **PascalCase** (App.cpp, Renderer.cpp, SwapChain.cpp, etc.)

## Code Style
- Indent: 4 spaces. Column limit: 120. K&R braces. Pointer right (`int *p`).
- See `.clang-format` for full rules.

## Architecture

### Key types
| Type | Role |
|------|------|
| `Mesh` | GPU geometry — owns vertex/index buffers + Materials |
| `MeshInstance` | Scene object — `shared_ptr<Mesh>` + Transform + color tint |
| `Material` | PBR surface — texture descriptor set, `unlit` flag |
| `Texture` | Self-owning GPU image (VMA + imageView) |
| `TextureManager` | `shared_ptr<Texture>` cache + fallbacks + sampler |
| `AssetManager` | GltfLoader → upload → cache Mesh |
| `GraphicsPipeline` | `pbrPipeline_` + `unlitPipeline_`, shared layout |
| `Renderer` | Selects pipeline per `material.unlit`, manages draw loop |
| `InstanceData` | Per-instance SSBO struct: mat4 + vec4 color (80 bytes, std430) |

### Descriptor layout
| Set | Binding | Type | Stage | Content |
|-----|---------|------|-------|---------|
| 0 | 0 | UBO | vert+frag | GlobalUBO: view, proj, cameraPos, point lights |
| 0 | 1 | SSBO | vert | InstanceData[] for instanced sphere draws |
| 1 | 0-2 | CombinedImageSampler | frag | Albedo, Normal, MetallicRoughness |

### GPU instancing (light spheres)
- 4 spheres → single `drawIndexed(..., instanceCount)` call
- Per-instance matrix+color packed into SSBO each frame, read via `gl_InstanceIndex`
- `sphereMesh_` ptr in Renderer identifies sphere instances by pointer equality
- Non-sphere objects drawn in a separate loop with push constants

### Render loop (renderScene)
1. Non-instanced pass — all objects except spheres, push constants, PBR or unlit pipeline
2. Instanced pass — spheres only, bind vertex buffer + unlit pipeline, one draw call

## Current State (2026-03-19)
- PBR Cook-Torrance BRDF ✓
- Point lights in GlobalUBO ✓
- GPU instancing for light spheres via SSBO ✓
- 1-second averaged frametime display ✓
- All files PascalCase ✓

## Roadmap
1. Deferred rendering (G-buffer + lighting pass)
2. Depth reconstruct
3. Shadow maps
4. Render graph
5. Reversed Z + infinite far plane
6. SSAO
