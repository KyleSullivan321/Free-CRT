# Free CRT

An Adobe After Effects effect plugin that recreates the look of retro CRT and
monitor screens: scanlines, glass "bulge", an RGB phosphor mask, glow/bloom,
chromatic aberration, posterize, flicker, and a full color-grade — plus a preset
menu. GPU (OpenCL) accelerated.

> An independent, from-scratch effect built for learning/creative use. It ships
> no third-party code or assets.

## Status — iteration 2 (GPU)

Built against the **AE 25.6 SDK** as a **Smart Render** effect with two paths:
a full-quality **CPU** pipeline (8/16/32-bpc float) and an **OpenCL GPU** path
(32-bpc float). Registers under **Effect → Free CRT → Free CRT**.

What works:
- Full parameter UI grouped into Pixels / Screen / Blurring / Aberrations / Glow
  plus top-level brightness, glow, saturation, gamma, tonemap.
- 7 presets (Midnight Arcade, Broadcast '83, Dead Channel, Pocket LCD,
  Hi-Fi Grille, Acid Wash, Night Shift) via the **Presets** popup.
- CPU pipeline (`CRT_Render.cpp`) and OpenCL kernel (`CRT_Kernel_CL.h`).

GPU notes: the OpenCL path approximates glow/blur with bounded multi-tap
sampling (single pass) and does not apply Auto Exposure (a whole-image average);
the CPU path is full quality. See [`docs/roadmap.md`](docs/roadmap.md).

## Build & install (Windows, GPU)

```powershell
# from a VS x64 developer prompt, at the repo root:
win\build_gpu.cmd
# -> build\Release\FreeCRT.aex   then run Install_FreeCRT.cmd (approve UAC)
```

`build_gpu.cmd` builds against the AE 25.6 SDK in `aesdk/`, the vendored Khronos
`CL` headers in `vendor/`, and an `OpenCL.lib` generated from `OpenCL.dll`.
More detail + verification steps: [`docs/build.md`](docs/build.md).

## Layout

```
src/        plugin source (AE plumbing, CPU render, OpenCL kernel)
resources/  PiPL (how AE discovers the effect)
win/        build_gpu.cmd + FreeCRT.rc
vendor/     Khronos CL headers + generated OpenCL.lib
docs/       feature spec, preset spec, build guide, roadmap, reference imgs
skills/     vendored Karpathy coding guidelines this project follows
GOAL.md     the success criteria driving the current iteration
```

Key files: [`src/FreeCRT.cpp`](src/FreeCRT.cpp) (AE integration: Smart Render,
GPU device setup, dispatch), [`src/CRT_Render.cpp`](src/CRT_Render.cpp) (CPU
look), [`src/CRT_Kernel_CL.h`](src/CRT_Kernel_CL.h) (OpenCL kernel),
[`src/CRT_Presets.cpp`](src/CRT_Presets.cpp) (preset tables). The parameter enum
in [`src/FreeCRT.h`](src/FreeCRT.h) is the single source of truth that the PiPL,
UI, presets, CPU render, and GPU kernel all key off.

## How it's built (for contributors)

This project follows the vendored
[Karpathy guidelines](skills/karpathy-guidelines/SKILL.md): simplest code that
solves the problem, surgical changes, explicit assumptions, and goal-driven
verification (`GOAL.md`).

## License

Plugin source: MIT (see `LICENSE`). The Adobe After Effects SDK is the property
of Adobe and is not included.
