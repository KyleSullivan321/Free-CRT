/*
	CRT_Strings.h — all UI-facing text in one place.
*/
#pragma once

typedef enum {
	StrID_NONE = 0,
	StrID_Name,
	StrID_Description,

	StrID_Preset,				StrID_Preset_Choices,
	StrID_IgnoreBounds,
	StrID_LinearWorkflow,
	StrID_AutoExposure,
	StrID_Brightness,
	StrID_GlowIntensity,

	StrID_Pixels,
	StrID_PixelType,			StrID_PixelType_Choices,
	StrID_PixelSize,
	StrID_PixelSharpness,
	StrID_PixelBrightness,

	StrID_Screen,
	StrID_BulgeStrength,
	StrID_BulgeCenter,
	StrID_ScanlinesIntensity,
	StrID_ScanlinesSpeed,
	StrID_FlickerIntensity,
	StrID_Posterize,
	StrID_BulbReflections,
	StrID_ScreenTint,

	StrID_Blurring,
	StrID_BlurAmount,
	StrID_BlurHBias,

	StrID_Aberrations,
	StrID_AberrAmount,
	StrID_AberrAngle,
	StrID_Noise,
	StrID_VerticalHold,

	StrID_Glow,
	StrID_GlowRadius,
	StrID_GlowThreshold,
	StrID_GlowSaturation,

	StrID_BoostSaturation,
	StrID_InputGamma,
	StrID_Tonemapping,

	StrID_GroupEnd,
	StrID_NUMTYPES
} StrIDType;

/* Renamed from GetStringPtr to avoid clashing with the SDK's String_Utils.h. */
const char* CRT_GetString(int strNum);
