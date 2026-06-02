# Build & install guide

Free CRT is an After Effects **Smart Render** effect (C++) with a CPU path and an
**OpenCL GPU** path, built against the **AE 25.6 SDK**.

## What's in the repo

- `src/`, `resources/`, `win/build_gpu.cmd`, `Install_FreeCRT.cmd` — the plugin.
- `vendor/CL/` — Khronos OpenCL headers (committed; Apache-2.0).

## After cloning a fresh copy

Two build inputs are **git-ignored** (not redistributable / generated), so on a
fresh clone you must supply them once:

1. **Adobe AE 25.6 SDK** → extract it to `aesdk/AfterEffectsSDK_25.6_61_win/ae25.6_61.64bit.AfterEffectsSDK/`
   (so `aesdk/.../Examples/Headers/AE_Effect.h` exists), or set `AE_SDK_PATH`
   to wherever you put it. Download: <https://developer.adobe.com/after-effects/>.
2. **`vendor/OpenCL.lib`** → generate it from the system OpenCL runtime (see the
   bottom of this guide). Needs only `C:\Windows\System32\OpenCL.dll` (ships with
   GPU drivers).

You also need **Visual Studio Build Tools** with the **Desktop development with
C++** (x64) workload.

## Build — Windows (OpenCL GPU + CPU)

From a **VS x64 developer prompt** (so `cl.exe`, `rc.exe`, `link.exe`, `lib.exe`
are on PATH), at the repo root:

```bat
win\build_gpu.cmd
```

This: generates the PiPL (`cl /EP` → `PiPLtool.exe`), compiles the sources,
compiles `win\FreeCRT.rc`, and links `build\Release\FreeCRT.aex` against
`vendor\OpenCL.lib`. On Windows the Metal code is `#if`-compiled out.

> If `cl.exe` isn't found, open the **"x64 Native Tools Command Prompt for VS"**
> (or run `vcvars64.bat`) first.

## Build — macOS (Metal GPU + CPU) — ⚠️ untested

Requires Xcode command-line tools and the AE SDK. From the repo root:

```bash
AE_SDK_PATH=/path/to/AfterEffectsSDK ./mac/build_mac.sh
```

This compiles `FreeCRT.cpp` as **Objective-C++** (for the Metal device code) and
the others as C++, links a universal `.plugin` bundle against the Metal /
Foundation frameworks, compiles the PiPL with **Rez**, and assembles
`build/mac/FreeCRT.plugin`. On macOS the OpenCL code is `#if`-compiled out.

This path was written on Windows and **has not been built/run on macOS**. If it
needs adjusting, mirror the AE SDK's `SDK_Invert_ProcAmp` Xcode project
(`Examples/Effect/SDK_Invert_ProcAmp/Mac`).

## Install

Double-click **`Install_FreeCRT.cmd`** → approve the UAC prompt. It copies
`build\Release\FreeCRT.aex` into every detected After Effects `Plug-ins\Free CRT`
folder. Restart AE; the effect is under **Effect → Free CRT → Free CRT**.

Manual alternative: copy `build\Release\FreeCRT.aex` into
`C:\Program Files\Adobe\Adobe After Effects <ver>\Support Files\Plug-ins\`.

## Verify

1. New comp, add footage or a solid. Apply **Free CRT**.
2. You should see scanlines, an RGB phosphor mask, a glass bulge, and glow.
3. Open **Presets** and pick *Midnight Arcade* / *Dead Channel* / *Hi-Fi Grille* —
   the sliders below should jump to that look.
4. **GPU**: File → Project Settings → Video Rendering and Effects →
   Use **Mercury GPU Acceleration (OpenCL)**. Compare against **Software Only**;
   they should match.

## Rebuilding the OpenCL import lib (only if needed)

`vendor\OpenCL.lib` is generated from the system `OpenCL.dll`:

```bat
dumpbin /exports C:\Windows\System32\OpenCL.dll      REM list cl* exports
REM write them into vendor\OpenCL.def as "EXPORTS\n<name>\n..."
lib /def:vendor\OpenCL.def /machine:x64 /out:vendor\OpenCL.lib
```

## Continuous builds (GitHub Actions)

`.github/workflows/build.yml` builds **both** Windows (`.aex`) and macOS
(`.plugin`) on GitHub's runners, uploads each as a downloadable **artifact**, and
— when run on a `v*` tag — attaches them to that release. This is how macOS
binaries get produced for testers (no Mac needed locally).

**One-time setup:** add a repo **secret** `AE_SDK_URL` — a direct-download URL to
a `.zip` of the AE SDK whose contents include `Examples/Headers/AE_Effect.h`
(the SDK isn't redistributable, so host it privately and point the secret at it).

**Use it:** Actions tab → **build** → *Run workflow* to get test artifacts
without a release, or push a tag (e.g. `v0.2.1`) to also publish to that release.

> Status: the workflow and the macOS/Metal path are written but **not yet run** —
> the first CI run is the verification step.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `cannot open AE_Effect.h` | Not building via `build_gpu.cmd`, or `aesdk/` missing. |
| `cannot open CL/cl.h` | `vendor/CL/` missing from the include path. |
| Effect not in AE menu | `.aex` not in the Plug-ins folder; restart AE. |
| `outflags2 mismatch` warning | PiPL `AE_Effect_Global_OutFlags_2` ≠ GlobalSetup. |
| GPU looks wrong, Software OK | OpenCL kernel bug — compare and report. |
