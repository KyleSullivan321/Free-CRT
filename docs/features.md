# Feature & parameter specification

Reverse-engineered from the reference screenshots (see
`docs/reference/`). This is the contract that `src/FreeCRT.h` (`enum
CRT_ParamID`), the PiPL, the preset tables, and the render pipeline implement.

## Top-level controls

| Param | Type | Range / default | Meaning |
|---|---|---|---|
| **Plugin Presets** | Popup | see `presets.md` | Loads a named parameter set. "None" = no change. |
| **Ignore Bounds** | Checkbox | off | Render the effect over the full comp, ignoring the layer's alpha bounds (lets glow/bulge spill past edges). |
| **Linear Workflow** | Checkbox | off | Convert sRGB→linear before processing and back after (physically correct glow/blur). |
| **Auto Exposure** | Checkbox | on | Normalise overall brightness so output sits in a pleasing range regardless of input. |
| **Brightness** | Float | 0–200, def 100 | Overall gain (100 = unity). |
| **Glow Intensity** | Float | 0–200, def 70 | Master multiplier for the bloom added back in the Glow stage. |

## Pixels (group) — the CRT phosphor mask

| Param | Type | Range / default | Meaning |
|---|---|---|---|
| Pixel Type | Popup | None / RGB Triad / Aperture Grille (vertical) / Slot Mask / Grid LED | Shape of the sub-pixel structure. |
| Pixel Size | Float | 1–64 px, def 4 | Size of one phosphor cell in pixels. |
| Pixel Sharpness | Float | 0–100, def 60 | Hardness of the gap between cells (0 = soft, 100 = hard edges). |
| Pixel Brightness | Float | 0–200, def 120 | Compensating gain for the energy lost to the mask. |

## Screen (group)

| Param | Type | Range / default | Meaning |
|---|---|---|---|
| **Bulge Strength** | Float | 0–400, def 254 | Macro-lens bulge: zooms into a convex sphere of the screen (higher = closer / more curved) so you only see the bulged part — no stretched edges. |
| **Bulge Center** | Point | layer center | Where the lens is focused; panning moves you around the sphere (clamped to stay in-frame). |
| **Scanlines Intensity** | Float | 0–100, def 15 | Depth of the dark horizontal scanline modulation. |
| **Scanlines Speed** | Float | 0–200, def 80 | Vertical roll speed of the scanlines over time. |
| **Flicker Intensity** | Float | 0–100, def 6 | Per-frame brightness jitter (refresh flicker). |
| **Posterize** | Float | 2–256 levels, def 20 | Quantises each channel to N levels (limited bit-depth look). |
| **Bulb Reflections** | Float | 0–100, def 30 | Soft elliptical highlight/vignette emulating glass reflection. |
| **Screen Tint** | Color | white | Multiplies output by this color (phosphor color cast). |

## Defocus (group) — depth of field tied to the bulge

| Param | Type | Range / default | Meaning |
|---|---|---|---|
| Lens Defocus (edges) | Float | 0–50 px, def 0 | Blur that grows with distance from the focus centre — the parts of the curved screen receding from the lens go soft (macro DOF). 0 = everything sharp. |
| Defocus Bias | Float | 0–100, def 50 | Biases the defocus toward horizontal/vertical. |

## Aberrations (group)

| Param | Type | Range / default | Meaning |
|---|---|---|---|
| Chromatic Aberration | Float | 0–50 px, def 0 | Radial RGB channel separation. |
| Aberration Angle | Angle | 0° | Direction bias of the channel split. |
| Noise / Static | Float | 0–100, def 0 | Additive luma grain. |
| Vertical Hold | Float | 0–100, def 0 | Occasional vertical roll/tear. |

## Glow (group)

| Param | Type | Range / default | Meaning |
|---|---|---|---|
| Glow Radius | Float | 0–200 px, def 24 | Blur radius of the bloom buffer. |
| Glow Threshold | Float | 0–100, def 60 | Luma above which pixels feed the bloom. |
| Glow Saturation | Float | 0–200, def 120 | Saturation of the bloom (colored glow). |

> Note: top-level **Glow Intensity** scales the final add-back; this group shapes it.

## Final color grade (top-level, after groups)

| Param | Type | Range / default | Meaning |
|---|---|---|---|
| **Boost Saturation** | Float | 0–400, def 197 | Saturation gain on the final image. |
| **Input Gamma** | Float | 1–300, def 100 | Gamma applied to input (100 = 1.0). |
| **Tonemapping** | Float | 0–100, def 31 | Filmic/Reinhard rolloff to tame highlights. |

## Pipeline order

```
input
  → (Linear Workflow: sRGB→linear)
  → Input Gamma
  → Bulge (geometric resample)
  → Chromatic Aberration (per-channel radial offset)
  → Blurring (separable)
  → Glow prepass (bright-pass → blur → buffer)
  → Scanlines + Flicker + Vertical Hold
  → Pixel Mask
  → Posterize
  → add Glow * Glow Intensity
  → Bulb Reflections (vignette/highlight)
  → Screen Tint
  → Boost Saturation
  → Brightness, Auto Exposure
  → Tonemapping
  → (Linear Workflow: linear→sRGB)
output
```
