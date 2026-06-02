/*
	FreeCRT.h — Free CRT effect (Smart Render + OpenCL GPU).

	The parameter enum below is the single source of truth: the PiPL, parameter
	setup, presets, the CPU render, and the OpenCL kernel dispatch all key off
	these indices. Keep them in sync.
*/

#pragma once

#include "AEConfig.h"
#include "entry.h"
#include "AEFX_SuiteHelper.h"
#include "PrSDKAESupport.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectGPUSuites.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectSuites.h"

/* GPU framework per platform: Windows -> OpenCL, macOS -> Metal. Both fall back
   to the CPU Smart Render path when the host's GPU framework isn't supported. */
#ifdef AE_OS_WIN
	#define HAS_OPENCL 1
	#define HAS_METAL  0
	#include <Windows.h>
	#include <CL/cl.h>
#elif defined(AE_OS_MAC)
	#define HAS_OPENCL 0
	#define HAS_METAL  1
	/* <Metal/Metal.h> is imported only in FreeCRT.cpp (compiled as Obj-C++ on
	   macOS) so the CPU-only translation units stay plain C++. */
#endif
#ifndef HAS_OPENCL
	#define HAS_OPENCL 0
#endif
#ifndef HAS_METAL
	#define HAS_METAL 0
#endif

/* ---- Versioning (also written into the PiPL) ---- */
#define	CRT_MAJOR_VERSION	0
#define	CRT_MINOR_VERSION	2
#define	CRT_BUG_VERSION		1
#define	CRT_STAGE_VERSION	PF_Stage_DEVELOP
#define	CRT_BUILD_VERSION	1

#define	CRT_NAME			"Free CRT"
#define	CRT_DESCRIPTION		"Free CRT: retro CRT / monitor screens with scanlines, glass bulge, RGB phosphor mask, glow, chromatic aberration and color grading. GPU (OpenCL) accelerated."

/* ---- Parameter indices. Order MUST match PARAMS_SETUP in FreeCRT.cpp ---- */
enum CRT_ParamID {
	CRT_INPUT = 0,

	CRT_PRESET,
	CRT_IGNORE_BOUNDS,
	CRT_LINEAR_WORKFLOW,
	CRT_AUTO_EXPOSURE,
	CRT_BRIGHTNESS,
	CRT_GLOW_INTENSITY,

	CRT_PIXELS_START,
	CRT_PIXEL_TYPE,
	CRT_PIXEL_SIZE,
	CRT_PIXEL_SHARPNESS,
	CRT_PIXEL_BRIGHTNESS,
	CRT_PIXELS_END,

	CRT_SCREEN_START,
	CRT_BULGE_STRENGTH,
	CRT_BULGE_CENTER,
	CRT_SCANLINES_INTENSITY,
	CRT_SCANLINES_SPEED,
	CRT_FLICKER_INTENSITY,
	CRT_POSTERIZE,
	CRT_BULB_REFLECTIONS,
	CRT_SCREEN_TINT,
	CRT_SCREEN_END,

	CRT_BLUR_START,
	CRT_BLUR_AMOUNT,
	CRT_BLUR_HBIAS,
	CRT_BLUR_END,

	CRT_ABERR_START,
	CRT_ABERR_AMOUNT,
	CRT_ABERR_ANGLE,
	CRT_NOISE,
	CRT_VERTICAL_HOLD,
	CRT_ABERR_END,

	CRT_GLOW_START,
	CRT_GLOW_RADIUS,
	CRT_GLOW_THRESHOLD,
	CRT_GLOW_SATURATION,
	CRT_GLOW_END,

	CRT_BOOST_SATURATION,
	CRT_INPUT_GAMMA,
	CRT_TONEMAPPING,

	CRT_NUM_PARAMS
};

/* Disk IDs for serialization stability. Never renumber an existing one. */
enum CRT_DiskID {
	DID_PRESET = 1,
	DID_IGNORE_BOUNDS, DID_LINEAR_WORKFLOW, DID_AUTO_EXPOSURE,
	DID_BRIGHTNESS, DID_GLOW_INTENSITY,
	DID_PIXELS_START, DID_PIXEL_TYPE, DID_PIXEL_SIZE, DID_PIXEL_SHARPNESS,
	DID_PIXEL_BRIGHTNESS, DID_PIXELS_END,
	DID_SCREEN_START, DID_BULGE_STRENGTH, DID_BULGE_CENTER,
	DID_SCANLINES_INTENSITY, DID_SCANLINES_SPEED, DID_FLICKER_INTENSITY,
	DID_POSTERIZE, DID_BULB_REFLECTIONS, DID_SCREEN_TINT, DID_SCREEN_END,
	DID_BLUR_START, DID_BLUR_AMOUNT, DID_BLUR_HBIAS, DID_BLUR_END,
	DID_ABERR_START, DID_ABERR_AMOUNT, DID_ABERR_ANGLE, DID_NOISE,
	DID_VERTICAL_HOLD, DID_ABERR_END,
	DID_GLOW_START, DID_GLOW_RADIUS, DID_GLOW_THRESHOLD, DID_GLOW_SATURATION,
	DID_GLOW_END,
	DID_BOOST_SATURATION, DID_INPUT_GAMMA, DID_TONEMAPPING
};

#define CRT_PRESET_COUNT		8		/* None + 7 named presets */
#define CRT_PIXEL_TYPE_COUNT	5

enum CRT_PixelType {
	PIX_NONE = 1, PIX_RGB_TRIAD, PIX_APERTURE_GRILLE, PIX_SLOT_MASK, PIX_GRID_LED
};

/*
	Flattened, normalised parameter values. Built once in PreRender and consumed
	by both the CPU pipeline (CRT_Render.cpp) and the OpenCL kernel dispatch.
	Plain-old-data so it can be malloc'd into pre_render_data.
*/
typedef struct {
	int		ignore_bounds;
	int		linear_workflow;
	int		auto_exposure;
	float	brightness;
	float	glow_intensity;

	int		pixel_type;
	float	pixel_size;
	float	pixel_sharpness;
	float	pixel_brightness;

	float	bulge_strength;
	float	bulge_cx, bulge_cy;
	float	scanlines_intensity;
	float	scanlines_speed;
	float	flicker_intensity;
	float	posterize_levels;
	float	bulb_reflections;
	float	tint_r, tint_g, tint_b;

	float	blur_amount;
	float	blur_hbias;

	float	aberr_amount;
	float	aberr_angle;
	float	noise;
	float	vertical_hold;

	float	glow_radius;
	float	glow_threshold;
	float	glow_saturation;

	float	boost_saturation;
	float	input_gamma;
	float	tonemapping;

	float	time_secs;
	int		width, height;
} CRT_Settings;

typedef struct { float r, g, b, a; } CRT_FPixel;

/* AE-agnostic CPU core (CRT_Render.cpp): processes interleaved float RGBA. */
void CRT_RenderImage(CRT_FPixel* buf, const CRT_Settings& s);

extern "C" {
	DllExport PF_Err EffectMain(
		PF_Cmd			cmd,
		PF_InData		*in_data,
		PF_OutData		*out_data,
		PF_ParamDef		*params[],
		PF_LayerDef		*output,
		void			*extra);
}
