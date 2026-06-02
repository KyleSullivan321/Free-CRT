# Free CRT

An Adobe After Effects effect plugin that recreates the look of retro CRT and
monitor screens: scanlines, glass "bulge" (a macro-lens sphere), an RGB phosphor
mask, glow/bloom, chromatic aberration, posterize, flicker, and a full
color-grade — plus a preset menu. **GPU-accelerated** (OpenCL on Windows, Metal
on macOS) with a CPU fallback.

> An independent, from-scratch effect built for learning/creative use. It ships
> no third-party code or assets.

Registers in After Effects under **Effect → Free CRT → Free CRT**.

## Install (new users)

**Option A — prebuilt binary (recommended for non-developers).**
Grab the build for your OS from the repo's **[Releases](../../releases)** page,
then drop it into After Effects' Plug-ins folder and restart AE:

| OS | File | Copy to |
|----|------|---------|
| Windows | `FreeCRT.aex` | `C:\Program Files\Adobe\Adobe After Effects <ver>\Support Files\Plug-ins\` |
| macOS | `FreeCRT.plugin` | `/Applications/Adobe After Effects <ver>/Plug-ins/` |

*(No Releases yet? Then build it from source — below. The repo itself does not
contain a compiled plugin.)*

**Option B — build it yourself.** See **Build from source**.

## Build from source

You need the **Adobe After Effects SDK** (free, Adobe ID) — it is not
redistributable, so it isn't in this repo. Download it from
<https://developer.adobe.com/after-effects/> and point `AE_SDK_PATH` at it (the
folder containing `Examples/Headers`). Full details + verification:
[`docs/build.md`](docs/build.md).

### Windows (OpenCL GPU + CPU)
```bat
:: from an "x64 Native Tools Command Prompt for VS", at the repo root:
win\build_gpu.cmd                  :: -> build\Release\FreeCRT.aex
Install_FreeCRT.cmd                :: copies it into AE's Plug-ins (approve UAC)
```
Uses the vendored Khronos `CL` headers in `vendor/` and an `OpenCL.lib` generated
from the system `OpenCL.dll` (see `docs/build.md`).

### macOS (Metal GPU + CPU) — ⚠️ untested
```bash
AE_SDK_PATH=/path/to/AfterEffectsSDK ./mac/build_mac.sh   # -> build/mac/FreeCRT.plugin
# copy FreeCRT.plugin into /Applications/Adobe After Effects <ver>/Plug-ins/
```
The Mac/Metal path is written but has **not** been compiled on macOS yet; if the
script needs adjusting, the AE SDK's `SDK_Invert_ProcAmp` Xcode project is the
reference.

## How GPU is selected

The effect is a Smart Render plugin. After Effects renders it on the GPU when the
project uses **Mercury GPU Acceleration** (File → Project Settings → Video
Rendering and Effects): **OpenCL** on Windows, **Metal** on macOS. If the host's
GPU framework isn't one of those (e.g. CUDA-mode AE), it falls back to the
full-quality **CPU** path automatically.

## Layout

```
src/        plugin source — FreeCRT.cpp (AE plumbing + GPU dispatch),
            CRT_Render.cpp (CPU), CRT_Kernel_CL.h (OpenCL), CRT_Kernel_Metal.h (Metal),
            CRT_Presets.*, CRT_Strings.*
resources/  PiPL (effect registration) + Info.plist (macOS bundle)
win/        build_gpu.cmd + FreeCRT.rc        mac/  build_mac.sh
vendor/CL/  Khronos OpenCL headers
docs/       feature/preset specs, build guide, roadmap
```

The parameter enum in [`src/FreeCRT.h`](src/FreeCRT.h) is the single source of
truth that the PiPL, parameter UI, presets, CPU render, and both GPU kernels key
off.

## License

Plugin source: MIT (see `LICENSE`). The Adobe After Effects SDK is the property
of Adobe and is not included.
