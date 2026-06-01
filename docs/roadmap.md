# Roadmap

Iteration 1 was a CPU, 8/16-bit, classic-render plugin. **Iteration 2 (done)**
moved to Smart Render against the AE 25.6 SDK with a CPU path (8/16/32-bpc) and
an **OpenCL GPU** path (`PF_Cmd_GPU_DEVICE_SETUP` + `CRT_Kernel_CL.h`).

## Done in iteration 2
- [x] **Smart Render + 32bpc float** (`SMART_PRE_RENDER` / `SMART_RENDER`).
- [x] **OpenCL GPU** kernel + device setup/dispatch (BGRA128 float).
- [x] Threaded rendering flag (`PF_OutFlag2_SUPPORTS_THREADED_RENDERING`).

## Next steps
- [ ] Add **CUDA** (`.cu`) and **Metal** kernels so GPU render works on machines
      where AE selects those frameworks (currently OpenCL-only).
- [ ] Make the GPU glow/blur a true multi-pass bloom (intermediate GPU buffers)
      to match the CPU path, and add Auto Exposure on GPU.
- [ ] Replace the CPU 3-pass box blur with a gaussian / dual-Kawase bloom.
- [ ] **Ignore Bounds** → expand the output buffer so glow/bulge spill past edges.

## Iteration 4 — parity polish
- [ ] Visually tune preset tables against the reference screenshots.
- [ ] Add any sub-parameters revealed by the original's collapsed groups
      (Pixels / Blurring / Aberrations / Glow) once confirmed.
- [ ] Premiere Pro drop-in transition variant.
- [ ] Save/Load custom presets to disk.

## Known simplifications in iteration 1
- Auto Exposure is a single global gain, not a rolling/animated metering.
- Vertical Hold is a simple sinusoidal roll, not a randomised tear.
- Bulb Reflections is a vignette + one soft highlight, not a full glass model.
- Preset values are best-effort recreations, not measured from the original.
