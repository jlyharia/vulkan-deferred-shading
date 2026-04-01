# Deferred Shading — Understanding Assessment

## Strengths

**Asks the right questions.**
Probing the reasoning behind decisions — why two color images, what RT0 means,
why the attenuation denominator needs +1 — not just accepting the output.

**Diagnosed a real bug with real tools.**
The sRGB double-conversion was found by opening RenderDoc, sampling a pixel on Sponza
stone, reading `(0.32, 0.27, 0.20)`, and knowing that mid-tone linear values read darker
than sRGB-encoded values. That is the actual production debugging workflow.

**Understands the bandwidth argument.**
G-buffer bandwidth math: ~32MB write (geometry pass) + ~32MB read (lighting pass) = ~64MB
per frame. This is the core trade-off of deferred vs forward — why deferred wins when you
have many lights, and where it loses (transparent geometry, bandwidth on mobile).

**Good architecture instincts.**
Pushed back on monolithic `recordCommandBuffer`, asked for `renderDeferred()` extraction.
Noticed the unused `radius` field. Caught the `worldPos` round-trip in `loadPointLights`.
Senior engineering instincts applied to graphics problems.

---

## Gaps to Close Before Interviews

**Depth reconstruction math.**
The pipeline works but be able to derive it on a whiteboard:
`clipPos → invProj * clipPos → perspective divide (/ w) → invView → worldPos`

Each step has a reason. Know why the perspective divide is necessary and what breaks
without it. Know why you pass `depth` directly (Vulkan depth is [0,1] in NDC, not [-1,1]).

**G-buffer layout decisions.**
Know not just *what* is stored but *why*:
- Why `R16G16B16A16_SFLOAT` for normals and not `R8G8B8A8_UNORM`?
  (Normals are in [-1,1], UNORM can't represent negatives without encoding tricks)
- Why not store world position as a 4th render target?
  (16 bytes/pixel extra bandwidth — depth reconstruction is free by comparison)
- Why `R8G8B8A8_UNORM` for albedo+metallic and not `_SRGB`?
  (G-buffer is an intermediate buffer, not a display surface — store linear, decode at write)

**Warp divergence on the radius early-out.**
`if (dist >= radius) continue` saves ALU when lights don't overlap. But on the GPU,
threads execute in warps of 32. When some threads take the branch and others don't,
both paths execute — divergent threads are masked, not skipped. The early-out helps
when *all* threads in a warp agree (e.g. a screen region with no nearby lights).
This is why tiled deferred is more effective — it eliminates divergence entirely by
only running lights that provably affect the tile.

---

## Key Trade-offs to Articulate

| Decision | Why |
|----------|-----|
| Deferred over forward | Many lights — O(pixels × lights) becomes O(pixels + lights) |
| Depth reconstruction over world-pos RT | Saves 16 bytes/pixel bandwidth |
| SFLOAT for normals | Range [-1,1] without encoding, precision for specular |
| Nearest sampler for G-buffer | Texels map 1:1 to pixels — no interpolation needed |
| `eDontCare` loadOp in lighting pass | Fullscreen triangle overwrites every pixel |
| Depth read-only in overlay pass | Spheres depth-test against scene but don't write |
