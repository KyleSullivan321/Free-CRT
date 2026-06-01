/*
	CRT_Presets.h — named parameter sets selectable from the Plugin Presets popup.

	Values are stored in the SAME UI units as the sliders/colors in FreeCRT.cpp
	(brightness 0..200, scanlines 0..100, tint 0..1 per channel, etc.) so applying
	a preset is a direct copy into PF_ParamDef values.
*/
#pragma once

#include "FreeCRT.h"

typedef struct {
	float	brightness;
	float	glow_intensity;

	int		pixel_type;			/* CRT_PixelType (1-based popup) */
	float	pixel_size;
	float	pixel_sharpness;
	float	pixel_brightness;

	float	bulge_strength;
	float	scanlines_intensity;
	float	scanlines_speed;
	float	flicker_intensity;
	float	posterize;
	float	bulb_reflections;
	float	tint_r, tint_g, tint_b;

	float	blur_amount;
	float	blur_hbias;

	float	aberr_amount;
	float	aberr_angle;		/* degrees */
	float	noise;
	float	vertical_hold;

	float	glow_radius;
	float	glow_threshold;
	float	glow_saturation;

	float	boost_saturation;
	float	input_gamma;
	float	tonemapping;
} CRT_Preset;

/* Returns the preset for popup value 1..CRT_PRESET_COUNT-1 (popup 1 == "None"). */
const CRT_Preset* CRT_GetPreset(int popup_value);

/* Writes a preset's values into the live params[] and marks them changed. */
void CRT_ApplyPreset(PF_ParamDef* params[], const CRT_Preset* p);
