/*
	CRT_Strings.cpp — string table. Popup choices use '|' separators as AE expects.
*/
#include "CRT_Strings.h"

typedef struct { unsigned long index; const char* str; } TableString;

static TableString g_strs[StrID_NUMTYPES] = {
	{ StrID_NONE,				"" },
	{ StrID_Name,				"Free CRT" },
	{ StrID_Description,			"Free CRT: retro CRT / monitor screens with scanlines, glass bulge, RGB phosphor mask, glow, chromatic aberration and color grading." },

	{ StrID_Preset,				"Presets" },
	{ StrID_Preset_Choices,
		"None|"
		"Midnight Arcade|Broadcast '83|Dead Channel|Pocket LCD|"
		"Hi-Fi Grille|Acid Wash|Night Shift" },
	{ StrID_IgnoreBounds,		"Ignore Bounds" },
	{ StrID_LinearWorkflow,		"Linear Workflow" },
	{ StrID_AutoExposure,		"Auto Exposure" },
	{ StrID_Brightness,			"Brightness" },
	{ StrID_GlowIntensity,		"Glow Intensity" },

	{ StrID_Pixels,				"Pixels" },
	{ StrID_PixelType,			"Pixel Type" },
	{ StrID_PixelType_Choices,	"None|RGB Triad|Aperture Grille|Slot Mask|Grid LED" },
	{ StrID_PixelSize,			"Pixel Size" },
	{ StrID_PixelSharpness,		"Pixel Sharpness" },
	{ StrID_PixelBrightness,	"Pixel Brightness" },

	{ StrID_Screen,				"Screen" },
	{ StrID_BulgeStrength,		"Bulge Strength" },
	{ StrID_BulgeCenter,		"Bulge Center" },
	{ StrID_ScanlinesIntensity,	"Scanlines Intensity" },
	{ StrID_ScanlinesSpeed,		"Scanlines Speed" },
	{ StrID_FlickerIntensity,	"Flicker Intensity" },
	{ StrID_Posterize,			"Posterize" },
	{ StrID_BulbReflections,	"Bulb Reflections" },
	{ StrID_ScreenTint,			"Screen Tint" },

	{ StrID_Blurring,			"Defocus" },
	{ StrID_BlurAmount,			"Lens Defocus (edges)" },
	{ StrID_BlurHBias,			"Defocus Bias" },

	{ StrID_Aberrations,		"Aberrations" },
	{ StrID_AberrAmount,		"Chromatic Aberration" },
	{ StrID_AberrAngle,			"Aberration Angle" },
	{ StrID_Noise,				"Noise / Static" },
	{ StrID_VerticalHold,		"Vertical Hold" },

	{ StrID_Glow,				"Glow" },
	{ StrID_GlowRadius,			"Glow Radius" },
	{ StrID_GlowThreshold,		"Glow Threshold" },
	{ StrID_GlowSaturation,		"Glow Saturation" },

	{ StrID_BoostSaturation,	"Boost Saturation" },
	{ StrID_InputGamma,			"Input Gamma" },
	{ StrID_Tonemapping,		"Tonemapping" },

	{ StrID_GroupEnd,			"" }
};

const char* CRT_GetString(int strNum)
{
	if (strNum < 0 || strNum >= StrID_NUMTYPES) return "";
	return g_strs[strNum].str;
}
