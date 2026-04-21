# Render Graph Design

A frame-level DAG that owns all image layout transitions and pipeline barriers for the deferred rendering pipeline.

---

## Problem

Without a render graph, every pass is responsible for its own barriers:

```cpp
// GeometryPass.cpp — before render graph
cmd.endRendering();
barriers = { colorAttachmentToShaderRead(albedo), colorAttachmentToShaderRead(normal), depthToShaderRead(depth) };
cmd.pipelineBarrier2(...);

// SsaoPass.cpp
cmd.endRendering();
cmd.pipelineBarrier2(colorAttachmentToShaderRead(ssaoBuffer));

// LightingPass.cpp — manually coordinated with all of the above
```

This couples passes to each other, makes the barrier logic invisible at the call site, and guarantees stale barriers after any resource recreate (e.g. window resize).

---

## Architecture

```
src/renderer/passes/graph/
├── RenderGraph.hpp/.cpp     — registry + topo sort + barrier derivation + execute
├── RGPass.hpp               — graph node: read/write declarations + execute lambda
├── RGTexture.hpp            — texture handle: image, view, format, initial layout
├── RGTextureAccess.hpp      — per-pass texture declaration: name + expected layout + stage + access
├── TextureState.hpp         — mutable tracking state during compile(): current layout/stage/access
└── CompiledPass.hpp         — compile output: RGPass + pre-baked ImageMemoryBarrier2 list
```

### Type roles

| Type | Role |
|---|---|
| `RGTexture` | Imported resource handle. Owns nothing — just a `vk::Image` + `vk::ImageView` + format + initial layout. |
| `RGTextureAccess` | What a single pass requires of a texture: the layout it expects, the pipeline stage that touches it, and the access mask. |
| `RGPass` | Graph node. Declares `readTextures` / `writeTextures` as `RGTextureAccess` lists, plus an `execute` lambda that records GPU work. |
| `TextureState` | Scratch state during `compile()`. Tracks current layout/stage/access as we walk sorted passes. Never stored past compile. |
| `CompiledPass` | Output of `compile()`. A sorted `RGPass` plus the pre-baked `vk::ImageMemoryBarrier2` list to emit before it runs. |
| `RenderGraph` | Owns the texture registry and pass list. Runs `compile()` once at init (and after resource recreate), then `execute()` every frame. |

---

## compile()

Called once at startup and after any swapchain/G-buffer recreate. Never called per-frame.

### Step 1 — Topological sort (Kahn's algorithm)

Build a `writerMap` (texture → pass that writes it) and an `inDegree` map (pass → number of graph-internal producers it depends on). Passes whose reads come from externally-managed resources (e.g. a texture owned outside the graph) get in-degree 0 and sort to the front.

Kahn's is chosen over DFS because:
- It naturally produces a stable linear order (queue-based, deterministic)
- It detects cycles via the final size check (`assert(sorted.size() == passes.size())`)
- It's easier to reason about when adding future features like pass culling

### Step 2 — Barrier derivation

Walk sorted passes in order. For each pass, iterate its `readTextures` and `writeTextures`. For each access, call `makeBarrier()`:

```
makeBarrier(tAccess, textureStateMap):
  cur = textureStateMap[tAccess.name]          // current layout/stage/access
  if cur matches tAccess exactly → no barrier needed
  emit ImageMemoryBarrier2(cur → tAccess)
  update textureStateMap[tAccess.name] = tAccess
```

The key invariant: **`textureStateMap` is updated in place as we walk, so each pass sees the state left by the previous pass that touched that texture.** This means read-after-write transitions are correctly derived without any explicit edge declaration between passes — the sort order and access declarations contain all the information needed.

Initial state is set from `RGTexture::initialLayout` with `srcStage = eNone, srcAccess = eNone` (correct sync2 idiom for "no prior producer").

### Step 3 — Store as CompiledPass

Each sorted pass plus its pre-baked barrier list is stored in `compiledPass`. The execute lambdas are captured at registration time and stored by value — they hold a `this` pointer to `Renderer` and read frame-scoped state (`graphicsPipeline_`, `meshInstances_`, etc.) that is set at the top of `drawFrame()`.

---

## execute()

Called every frame. Zero allocation, zero string lookups.

```cpp
for (auto &[pass, barriers] : compiledPass) {
    if (!barriers.empty())
        cmd.pipelineBarrier2(vk::DependencyInfo{}.setImageMemoryBarriers(barriers));
    pass.execute(cmd);
}
```

One `pipelineBarrier2` call per pass (barriers pre-grouped by pass). The GPU sees a well-formed sync2 command stream with correctly-scoped src/dst stage and access masks.

---

## Pass declarations (Renderer.cpp)

```cpp
renderGraph_->addPass({
    .name = "Geometry",
    .readTextures  = {},
    .writeTextures = {
        { texName::albedoMetallic,  eColorAttachmentOptimal,        eColorAttachmentOutput, eColorAttachmentWrite },
        { texName::normalRoughness, eColorAttachmentOptimal,        eColorAttachmentOutput, eColorAttachmentWrite },
        { texName::gbufferDepth,    eDepthStencilAttachmentOptimal, eEarlyFragmentTests|eLateFragmentTests, eDepthStencilAttachmentWrite },
    },
    .execute = [this](vk::CommandBuffer cmd) {
        geometryPass_->execute(cmd, ...);
    },
});
```

Texture names are `constexpr std::string_view` constants in `namespace texName` — a typo is a compile error, not a silent graph mis-wire at runtime.

---

## Frame execution order

```
prepareFrameImages()          — swapchain color + depth: Undefined → attachment (outside graph)
│
├── [graph barrier] shadowMap: Undefined → DepthStencilAttachment
├── DirShadow pass
│
├── [graph barrier] albedoMetallic/normalRoughness/gbufferDepth: Undefined → ColorAttachment/Depth
├── Geometry pass
│
├── [graph barrier] albedoMetallic/normalRoughness: ColorAttachment → ShaderReadOnly
│                   gbufferDepth: DepthStencilAttachment → DepthReadOnly
│                   ssaoBuffer: Undefined → ColorAttachment
├── SSAO pass
│
├── [graph barrier] ssaoBuffer: ColorAttachment → ShaderReadOnly
│                   ssaoBlur: Undefined → ColorAttachment
├── SSAO Blur pass
│
├── [graph barrier] ssaoBlur: ColorAttachment → ShaderReadOnly
│                   shadowMap: DepthStencilAttachment → ShaderReadOnly
├── Lighting pass
│
overlayPass_->execute()       — outside graph: reads depth (already DepthReadOnly), writes swapchain color
userInterface.recordCommands()
finalizeFrameImages()         — swapchain color: ColorAttachment → PresentSrc
```

---

## Design decisions

**Why compile/execute split?**
Barrier derivation is graph work — it only needs to run when the graph changes (startup or resize). Separating it from the per-frame execute path means `execute()` has no allocation, no string lookups, and no logic — just iterate a pre-sorted array and call lambdas. This is the same split used in Frostbite's render graph.

**Why Kahn's over DFS topological sort?**
DFS-based topo sort requires marking visited nodes and handling back-edge detection separately. Kahn's naturally detects cycles (unsorted passes remain at the end), produces a deterministic BFS-order result, and is easier to extend with pass culling (passes with in-degree 0 that are not consumed → dead pass elimination).

**Why `initialLayout = eUndefined` for most textures?**
`eUndefined` as old layout in a barrier tells the driver it may discard existing content — no cache flush needed for the source. G-buffer attachments, SSAO buffer, and shadow map are fully overwritten each frame, so `eUndefined` is correct and allows the driver to avoid unnecessary readbacks. The gbuffer depth is the exception: `prepareFrameImages()` transitions it to `eDepthStencilAttachmentOptimal` before the graph runs, so its initial state is registered as that layout.

**Why are barriers emitted per-pass rather than batched into one call?**
Correctness first — each pass's barriers depend on the state left by the previous pass. Batching all barriers into a single call at the start of the frame would require tracking which state each pass needs before any pass runs, which is correct only if all passes are independent (they're not — Lighting reads what Geometry wrote). Per-pass emission is correct and simple. For 5–6 passes the cost is negligible.

**What the graph intentionally omits:**
- Buffer resource tracking (SSBOs, indirect draw buffers) — not needed for this pass set
- Transient resource aliasing — would require VMA memory aliasing; left for future work
- Async compute / cross-queue sync — requires separate queue + semaphore insertion
- Dead pass culling — straightforward to add (reference-count writes, skip unreferenced passes)
- Subresource tracking (per-mip, per-layer) — all resources are single mip/layer

---

## Rebuilding on resize

`recreateSwapChain()` destroys and recreates the G-buffer, SSAO, and swapchain depth images. After recreation, the `vk::Image` handles stored in the texture registry are stale. `rebuildRenderGraph()` is called at the end of `recreateSwapChain()` — it calls `reset()` then re-registers all textures with fresh handles and recompiles. The execute lambdas are re-added unchanged since they capture `this`, not raw handles.

---

## Key files

| File | What to read |
|---|---|
| `src/renderer/passes/graph/RenderGraph.cpp` | `compile()` and `makeBarrier()` — the full barrier derivation logic |
| `src/renderer/Renderer.cpp` — `rebuildRenderGraph()` | How passes and textures are declared; `texName` constants |
| `src/renderer/Renderer.cpp` — `recordCommandBuffer()` | How `execute()` fits into the frame loop |
