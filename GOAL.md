# GOAL — Free CRT

> A from-scratch After Effects effect plugin that recreates the look of retro
> CRT / monitor screens: scanlines, glass bulge, RGB phosphor mask, glow,
> chromatic aberration, posterize, flicker, and color grading. Visual targets
> are the reference screenshots in `docs/reference/`.

## Success criteria (current — iteration 2, GPU)

1. ✅ Builds against the AE 25.6 SDK into `FreeCRT.aex` (Smart Render effect).
2. ✅ Loads in After Effects under **Effect → Free CRT → Free CRT**.
3. ✅ Full grouped parameter UI (Pixels / Screen / Blurring / Aberrations / Glow
   + brightness, glow, saturation, gamma, tonemap).
4. ✅ CPU pipeline (8/16/32-bpc) **and** an OpenCL GPU path produce the CRT look.
5. ✅ Presets load their values into the params (popup flagged `SUPERVISE`).
6. ✅ Bulge is a convex spherical wrap (image appears wrapped onto a sphere).
7. ✅ No third-party brand names anywhere in the project.

## Out of scope for now (tracked in docs/roadmap.md)

- CUDA / Metal GPU kernels (currently OpenCL-only; CPU is the fallback).
- True multi-pass GPU bloom + GPU Auto Exposure (GPU glow is approximate).
- Buffer-expanding glow for "Ignore Bounds".
- Premiere Pro drop-in transition variant.

## How to verify (developer)

```
1. win\build_gpu.cmd                  -> verify: build\Release\FreeCRT.aex exists
2. Install_FreeCRT.cmd (approve UAC)  -> verify: effect appears in AE menu
3. Apply to a layer, scrub params     -> verify: CRT look renders
4. Pick a preset                      -> verify: sliders jump to the preset
5. Project Settings -> Mercury GPU (OpenCL) -> verify: GPU render matches CPU
```
