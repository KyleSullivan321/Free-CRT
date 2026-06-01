# Free CRT — project instructions

An Adobe After Effects effect plugin (C++, AE SDK) that recreates the look of
retro CRT / monitor screens. See `GOAL.md` for scope.

## Working principles (vendored from Karpathy guidelines)

This project follows `skills/karpathy-guidelines/SKILL.md`. In short:

1. **Think before coding** — state assumptions; surface tradeoffs; ask when unclear.
2. **Simplicity first** — minimum code that solves the problem; no speculative
   abstractions. (The render pipeline favours readable per-pixel math over a
   premature GPU port.)
3. **Surgical changes** — touch only what the task needs; match existing style.
4. **Goal-driven execution** — every change traces to a success criterion in
   `GOAL.md`; verify before claiming done.

## Architecture

- `src/FreeCRT.cpp` — AE plumbing: entry point, command dispatch, parameter
  setup, sequence data, preset application.
- `src/CRT_Render.cpp` — the CRT image pipeline (CPU). Pure functions over a
  float RGBA buffer; AE-agnostic where practical.
- `src/CRT_Presets.cpp` — named preset parameter tables.
- `src/CRT_Strings.cpp` — all UI-facing strings (one place to localise).
- `src/CRT_Kernel_CL.h` — the OpenCL kernel (GPU path), embedded as a C string.
- `resources/FreeCRT_PiPL.r` — the PiPL that AE reads to register the effect.
- `win/build_gpu.cmd` — the build (SDK 25.6 + OpenCL). `Install_FreeCRT.cmd` deploys.

Parameter indices live in `src/FreeCRT.h` (`enum CRT_ParamID`). The PiPL,
the param setup, presets, and render all key off that enum — keep them in sync.

## Build

From a VS x64 developer prompt: `win\build_gpu.cmd` → `build\Release\FreeCRT.aex`,
then `Install_FreeCRT.cmd` (admin) deploys it. Builds against the AE 25.6 SDK in
`aesdk/`, vendored Khronos `CL` headers + generated `OpenCL.lib` in `vendor/`.
Full instructions: `docs/build.md`.

## Conventions

- C++17. No exceptions across the AE boundary; return `PF_Err`.
- Render math in 32-bit float internally; convert at the 8/16/32-bit edges.
- Keep the param enum, PiPL count, and preset tables consistent — there are
  asserts/comments marking these coupling points.
