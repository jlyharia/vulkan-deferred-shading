# Bug: sRGB Double Conversion in Deferred Lighting Pass

## Summary

The deferred lighting pass was applying a manual `pow(albedo, 2.2)` sRGB-to-linear decode
on albedo values that had already been decoded by hardware. This caused albedo to be
approximately 4× too dark on mid-tone surfaces, requiring artificially high light intensities
to compensate — and producing incorrect PBR output.

## Root Cause

The G-buffer geometry pass samples albedo textures loaded with format
`VK_FORMAT_R8G8B8A8_SRGB`. When Vulkan samples a `_SRGB` format texture, the hardware
automatically converts sRGB → linear at sample time. The G-buffer RT0 (`R8G8B8A8_UNORM`)
therefore stores **linear** albedo values after the geometry pass writes them.

The lighting pass (`shaders/lighting/lighting.frag`) originally did:

```glsl
// WRONG — albedo is already linear; this applies the curve twice
vec3 albedo = pow(albedoMetallic.rgb, vec3(2.2));
```

Applying `pow(x, 2.2)` to an already-linear value squares the gamma correction. For a
mid-tone surface with linear albedo `0.32`, the result was `0.32^2.2 ≈ 0.08` — roughly
4× too dark.

## How It Was Diagnosed

Using RenderDoc: sampled a pixel on Sponza's stone floor in the G-buffer RT0 texture view
(`R8G8B8A8_UNORM`). The stored R/G/B values read approximately `(0.32, 0.27, 0.20)`.

For sRGB-encoded data, those same surfaces would read `(0.60, 0.55, 0.48)` — noticeably
brighter raw values. The low linear-looking values confirmed the G-buffer was already storing
decoded linear albedo, meaning the `pow(2.2)` in the shader was a second decode.

**Key rule**: `pow(linear_value, 2.2)` makes it darker. If the G-buffer had been sRGB-encoded,
the albedo values would have been higher (closer to 0.5–0.7 for mid-tones). Values in the
0.2–0.3 range for a mid-gray surface are the fingerprint of linear data.

## Fix

```glsl
// CORRECT — hardware decoded sRGB at sample time in the geometry pass
vec3 albedo = albedoMetallic.rgb;
```

## Side Effect: Scene Appeared Too Bright

After the fix, albedo values were 3–4× higher than the incorrectly darkened values the
light intensities had been tuned against. The scene appeared washed out.

Resolution: halved all point light intensities in `src/app/App.cpp` and reduced the ambient
constant in the lighting shader from `0.03` to `0.01`.

## Lessons

- Always verify the format of your texture views (`_SRGB` vs `_UNORM`). Hardware sRGB decode
  is invisible in code — it happens at the sampler level.
- In a deferred pipeline, document explicitly at each G-buffer write/read boundary what color
  space the data is in. A comment in the geometry pass shader ("writes linear albedo — hardware
  decoded from sRGB texture") prevents this class of bug.
- When tuning light intensities, know whether your albedo is linear. Intensities tuned against
  wrong albedo will need full rescaling after a correctness fix.
- RenderDoc's texture viewer is the fastest way to confirm: sample a pixel you know the
  expected value of (white wall, gray stone) and check whether the raw float value matches
  the linear or sRGB expectation.
