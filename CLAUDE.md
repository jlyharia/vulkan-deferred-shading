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
| `GpuTimestamps` | Double-buffered VkQueryPool pair; `writeBegin`/`writeEnd` bracket each pass; `readback` converts tick deltas → ms |

### Descriptor layout
| Set | Binding | Type | Stage | Content |
|-----|---------|------|-------|---------|
| 0 | 0 | UBO | vert+frag | GlobalUBO: view, proj, invView, invProj, cameraPos, dirLightSpaceMatrices[4], cascadeSplitDepths, dirLightDir, pointLights[24] |
| 0 | 1 | SSBO | vert | InstanceData[] for instanced sphere draws |
| 1 | 0-2 | CombinedImageSampler | frag | Albedo, Normal, MetallicRoughness |

### GPU instancing (light spheres)
- 4 spheres → single `drawIndexed(..., instanceCount)` call
- Per-instance matrix+color packed into SSBO each frame, read via `gl_InstanceIndex`
- `sphereMesh_` ptr in Renderer identifies sphere instances by pointer equality
- Non-sphere objects drawn in a separate loop with push constants

### Folder layout
```
src/
  app/          — App orchestration
  assets/       — AssetManager, GltfLoader
  common/       — GPU structs, Config, enums
  external/     — VMA/vendor impl files
  renderer/     — Renderer, UserInterface, GpuTimestamps
    passes/     — ForwardPass, GeometryPass, LightingPass, OverlayPass, DirShadowPass, SsaoPass, SsaoBlurPass
      graph/    — RenderGraph, RGPass, RGTexture, RGTextureAccess, TextureState, CompiledPass
  scene/        — Camera, Mesh, MeshInstance, Texture, etc.
  vulkan/       — VulkanContext, SwapChain, GBuffer, GraphicsPipeline, etc.
```

### Pass architecture (`src/renderer/passes/`)
Each pass is a plain struct with an `execute(vk::CommandBuffer, ...)` method. No virtual base.
| Pass | File | Role |
|------|------|------|
| `ForwardPass` | ForwardPass.hpp/cpp | Full forward: PBR + unlit + instanced spheres |
| `GeometryPass` | GeometryPass.hpp/cpp | G-buffer fill + gbuffer barrier (producer-side) |
| `LightingPass` | LightingPass.hpp/cpp | Fullscreen BRDF triangle, writes to swapchain |
| `OverlayPass` | OverlayPass.hpp/cpp | Light sphere visualization (depth-test-read-only) |
| `DirShadowPass` | DirShadowPass.hpp/cpp | Loops over NUM_CASCADES, renders scene into each cascade layer view with that cascade's light-space matrix |
| `SsaoPass` | SsaoPass.hpp/cpp | SSAO occlusion at half resolution (width/2 × height/2), 16-sample kernel; owns noise texture + kernel buffer |
| `SsaoBlurPass` | SsaoBlurPass.hpp/cpp | Bilateral blur over the half-res SSAO buffer |

### Render graph (`src/renderer/passes/graph/`)
Frame-level DAG that owns barrier derivation. Wired into the render loop — `renderGraph_->execute(cmd)` is the primary draw call in `recordCommandBuffer()`.
| Type | Role |
|------|------|
| `RenderGraph` | Owns texture registry + pass list; runs `compile()` then `execute()` each frame |
| `RGPass` | Graph node: declares `readTextures`/`writeTextures` + `execute` lambda |
| `RGTexture` | Handle: `vk::Image` + `vk::ImageView` + format + initial layout |
| `RGTextureAccess` | Per-pass texture declaration: name + expected layout + stage + access flags |
| `TextureState` | Mutable tracking state during `compile()`: current layout/stage/access |
| `CompiledPass` | Compile output: `RGPass` + pre-baked `vk::ImageMemoryBarrier2` list |

`Renderer::recordCommandBuffer()` calls `renderGraph_->execute(cmd, gpuTimestamps_.get(), currentFrame)`, then OverlayPass (also timed) and ImGui outside the graph.

### Render loop
1. `renderGraph_->execute(cmd)` — drives DirShadowPass, GeometryPass, SsaoPass, SsaoBlurPass, LightingPass with auto-derived barriers
2. OverlayPass — instanced light spheres with depth test (read-only), runs after graph
3. ImGui UI pass

### `DirLightView` — cascade frustum computation (`src/scene/DirLightView.hpp/cpp`)
Fields: `position`, `target`, `shadowFar` (default 200m). No single-cascade ortho fields remain.
`computeCascades(cameraView, cameraProj, lambda=0.9)` returns `std::array<CascadeData, NUM_CASCADES>`.
- Extracts FOV and `cameraNear` from projection matrix (`proj[1][1]`, `proj[0][0]`, `proj[3][2]`)
- Splits camera frustum using Practical Split Scheme (blend of log and uniform via `lambda`)
- `shadowFar` is independent of the camera's infinite far plane
- Per cascade: 8 view-space corners → world space → light space → AABB → texel-snap XY → `orthoRH_ZO` + Y-flip + reverse-Z
- `CascadeData`: `lightSpaceMatrix` (mat4) + `splitDepth` (positive meters from camera)

### `ShadowMap` — cascade array image (`src/vulkan/ShadowMap.hpp/cpp`)
Owns a single `e2DArray` depth image with `NUM_CASCADES` layers (2048×2048×4, D32Sfloat).
- `getDepthView()` — full array view (`e2DArray`), bound to lighting shader as `sampler2DArrayShadow`
- `getLayerView(int i)` — per-layer `e2D` view, used as depth attachment in `DirShadowPass` per cascade
- Sampler: hardware PCF (`compareEnable=true`, `eGreaterOrEqual`), clamp-to-edge, reverse-Z
- `AttachmentImage` not used here — image managed directly via VMA (needs `arrayLayers = NUM_CASCADES`)

## Current State (2026-05-12)
- PBR Cook-Torrance BRDF ✓
- Deferred rendering (G-buffer + lighting pass) ✓
- Point lights in GlobalUBO ✓
- GPU instancing for light spheres via SSBO ✓
- 1-second averaged frametime display ✓
- All files PascalCase ✓
- Pass extraction into `src/renderer/passes/` ✓
- `src/assets/` (was `core/` + `system/`) ✓
- Directional shadow map with PCF hardware filtering ✓
- SSAO with bilateral blur pass ✓
- Render graph with compile-time barrier derivation — wired into render loop ✓
- Tone mapping: ACES (Narkowicz 2015 fitted curve) active; Reinhard commented out in `lighting.frag`
- Reversed-Z with infinite far plane ✓
- SSAO at half resolution (16-sample kernel) ✓
- Debug labels via `VK_EXT_debug_utils` — all graph passes + Overlay + ImGui labeled for RenderDoc/Nsight ✓
- GPU timestamp queries per pass — `GpuTimestamps` double-buffered query pools, 1-second averaged ImGui table ✓
- CSM complete (branch `csm`): all 9 steps done
  - `GlobalUBO` carries `dirLightSpaceMatrices[4]` + `cascadeSplitDepths`; `DirShadowPass` renders 4 cascade layers
  - `lighting.frag`: `sampler2DArrayShadow`, cascade selection by camera-view-space depth, array sample with hardware PCF
  - Descriptor binding 4 uses `depthArrayView_` (e2DArray) + compare sampler; `eCombinedImageSampler` unchanged

## Roadmap
1. ~~Deferred rendering (G-buffer + lighting pass)~~ ✓
2. ~~Shadow maps~~ ✓
3. ~~SSAO~~ ✓
4. ~~Render graph~~ ✓
5. ~~Reversed Z + infinite far plane~~ ✓
6. ~~Debug labels (RenderDoc/Nsight pass naming)~~ ✓
7. ~~GPU timestamp queries per pass~~ ✓
8. Cascaded shadow maps (in progress)
9. Technical README
