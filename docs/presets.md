# Presets

Free CRT ships its own presets (original names, not the reference product's).
The popup is "None" plus the entries below. Values live in
`src/CRT_Presets.cpp` (`g_presets`) and the names in the `StrID_Preset_Choices`
string in `src/CRT_Strings.cpp` — keep the two in the same order, and keep
`CRT_PRESET_COUNT` in `src/FreeCRT.h` equal to `1 + number of presets`.

| # (popup) | Preset | Look |
|---|---|---|
| 1 | None | Leaves current params untouched. |
| 2 | Midnight Arcade | Saturated RGB-triad cabinet glow with strong bulge. |
| 3 | Broadcast '83 | Warm slot-mask tube, heavy scanlines, slight green cast. |
| 4 | Dead Channel | Broken signal: flicker, vertical hold, aberration, static. |
| 5 | Pocket LCD | Chunky grid-LED handheld, low scanlines, flat. |
| 6 | Hi-Fi Grille | Clean aperture-grille monitor, fine pitch, rich color. |
| 7 | Acid Wash | Blown-out saturation, big bloom, fish-eye bulge. |
| 8 | Night Shift | Dim warm night filter, soft glow, low brightness. |

## How loading works

Selecting a preset fires `PF_Cmd_USER_CHANGED_PARAM`. The handler in
`FreeCRT.cpp` copies that preset's values into the other params, flagging
each with `PF_ChangeFlag_CHANGED_VALUE` so After Effects commits them (and makes
them undoable). The preset popup intentionally stays on the chosen name — to
re-apply after tweaking, set it back to **None** and pick the preset again.

> Note: the triggering popup must **not** be flagged changed inside its own
> change event — doing so makes AE coalesce and drop the whole batch (this was
> the original "presets don't load" bug).
