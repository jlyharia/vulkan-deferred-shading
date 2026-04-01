# SSAO Implementation Guide

## Workflow: How to implement SSAO with AI assistance

### 1. Learn first, code second

Read before writing any code:
- Mittring (2007) — "Finding Next Gen: CryEngine 2" (original Crytek SSAO)
- Filion/McNaughton — SSAO chapter in GPU Pro

Understand these before touching the keyboard:
- Why hemisphere samples in view space, not world space
- What the depth comparison is actually testing
- Why you need the random rotation texture and what it prevents
- Why blur must be bilateral, not Gaussian

Ask Claude to explain any part you don't understand. Do not ask for code yet.

---

### 2. Design it yourself first

Before implementation, write out on paper or in a doc:
- What render targets you need and why
- What the SSAO pass reads from the G-buffer
- What the blur pass inputs/outputs are
- How it connects to the existing GeometryPass → LightingPass pipeline

Show the design for review before writing any code. This is the most valuable step.

---

### 3. Write the core algorithm yourself

The SSAO fragment shader is the heart of it — hemisphere sampling, depth test,
occlusion accumulation. Write this yourself, even if it's wrong at first.
Debugging wrong code is where the knowledge locks in.

Use AI to:
- Answer "why is my AO bleeding across this edge?"
- Explain what a specific line of math is doing
- Review correctness after you have written it

Do not ask AI to write the shader for you.

---

### 4. Use AI for scaffolding, not logic

**AI should write (boilerplate, plumbing):**
- New render target creation (R8_UNORM, half-res)
- Descriptor set layout for the SSAO pass
- Pipeline setup and CMake additions
- Blur pass wiring into the pass architecture

**You write (core algorithm):**
- Hemisphere sample kernel generation (16–32 samples, cosine-weighted)
- SSAO fragment shader: sampling, depth comparison, occlusion accumulation
- Bilateral blur kernel

---

### 5. Implementation notes (from the panel)

- **Half-resolution SSAO** — render at half res, bilateral upsample. Full-res is 4× the
  cost for marginal quality gain. This is what shipping engines do.
- **16–32 samples** — industry sweet spot. More samples adds noise that blur has to hide.
  64+ samples is waste.
- **Bilateral blur, not Gaussian** — Gaussian bleeds AO across depth discontinuities.
  Columns get halos. Bilateral preserves edges. ~20 extra shader lines, obvious difference.
- **AO multiplies ambient only** — never apply AO to direct lighting or specular.
  Physically wrong and visually bad. A common mistake.
- **HBAO over classic SSAO** — same implementation complexity, better quality, better
  interview signal. Reference: Bavoil/Sainz (2008).

---

### 6. Pass architecture (where it fits)

```
GeometryPass
  → writes gbNormal (RT1) + gbDepth
SSAOPass          ← reads gbNormal + gbDepth, writes R8_UNORM AO target
SSAOBlurPass      ← bilateral blur on AO target
LightingPass      ← reads AO target, multiplies into ambient term only
OverlayPass
```

Controlled by `ssaoEnabled_` bool on Renderer (not a separate RenderPath).

---

### 7. The ownership test

Before adding SSAO to your resume, close everything and answer these without looking:

1. Why hemisphere samples and not full sphere?
2. Why view space and not world space?
3. What happens if you skip the random rotation texture?
4. Why bilateral blur over Gaussian?

If you can answer all four, you own it. If not, go back to step 1 for whatever you can't answer.
