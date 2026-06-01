/*
	FreeCRT.cpp — Free CRT.

	Smart Render effect with a CPU path (full-quality, reuses CRT_RenderImage)
	and an OpenCL GPU path (CRT_Kernel_CL.h). Structure follows Adobe's
	SDK_Invert_ProcAmp GPU sample. Built against the AE 25.6 SDK.
*/

#include "FreeCRT.h"
#include "CRT_Strings.h"
#include "CRT_Presets.h"
#include "CRT_Kernel_CL.h"

#include <new>
#include <cmath>
#include <cstring>
#include <cstdlib>

inline PF_Err CL2Err(cl_int r) { return r == CL_SUCCESS ? PF_Err_NONE : PF_Err_INTERNAL_STRUCT_DAMAGED; }
#define CL_ERR(FUNC) ERR(CL2Err(FUNC))

struct OpenCLGPUData { cl_kernel crt_kernel; };

static void UnionLRect(const PF_LRect* src, PF_LRect* dst)
{
	if (dst->left == dst->right || dst->top == dst->bottom) {
		*dst = *src;
	} else if (!(src->left == src->right || src->top == src->bottom)) {
		if (src->left   < dst->left)   dst->left   = src->left;
		if (src->top    < dst->top)    dst->top    = src->top;
		if (src->right  > dst->right)  dst->right  = src->right;
		if (src->bottom > dst->bottom) dst->bottom = src->bottom;
	}
}

static size_t RoundUp(size_t v, size_t m) { return v ? ((v + m - 1) / m) * m : 0; }

/* ----------------------------------------------------------------- About -- */
static PF_Err About(PF_InData* in_data, PF_OutData* out_data)
{
	PF_SPRINTF(out_data->return_msg, "%s v%d.%d\r%s",
		CRT_NAME, CRT_MAJOR_VERSION, CRT_MINOR_VERSION, CRT_DESCRIPTION);
	return PF_Err_NONE;
}

/* ---------------------------------------------------------- GlobalSetup -- */
static PF_Err GlobalSetup(PF_InData* in_data, PF_OutData* out_data)
{
	out_data->my_version = PF_VERSION(CRT_MAJOR_VERSION, CRT_MINOR_VERSION,
		CRT_BUG_VERSION, CRT_STAGE_VERSION, CRT_BUILD_VERSION);

	/* time-varying (flicker/scanline roll), 16-bit aware, samples neighbours
	   (so NOT pixel-independent), and drives presets via UPDATE_PARAMS_UI. */
	out_data->out_flags  = PF_OutFlag_DEEP_COLOR_AWARE |
						   PF_OutFlag_NON_PARAM_VARY |
						   PF_OutFlag_SEND_UPDATE_PARAMS_UI;

	out_data->out_flags2 = PF_OutFlag2_FLOAT_COLOR_AWARE |
						   PF_OutFlag2_SUPPORTS_SMART_RENDER |
						   PF_OutFlag2_SUPPORTS_THREADED_RENDERING |
						   PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG;

	if (in_data->appl_id != 'PrMr') {
		out_data->out_flags2 |= PF_OutFlag2_SUPPORTS_GPU_RENDER_F32;
	}
	return PF_Err_NONE;
}

/* --------------------------------------------------------- ParamsSetup -- */
static PF_Err ParamsSetup(PF_InData* in_data, PF_OutData* out_data,
						  PF_ParamDef* params[], PF_LayerDef* output)
{
	PF_Err		err = PF_Err_NONE;
	PF_ParamDef	def;

	/* SUPERVISE is REQUIRED: without it AE never sends PF_Cmd_USER_CHANGED_PARAM,
	   so the preset popup handler never runs (the "presets don't load" bug). */
	AEFX_CLR_STRUCT(def);
	def.flags = PF_ParamFlag_SUPERVISE;
	PF_ADD_POPUP(CRT_GetString(StrID_Preset), CRT_PRESET_COUNT, 1,
		CRT_GetString(StrID_Preset_Choices), DID_PRESET);

	AEFX_CLR_STRUCT(def);
	PF_ADD_CHECKBOX(CRT_GetString(StrID_IgnoreBounds), "", FALSE, 0, DID_IGNORE_BOUNDS);
	AEFX_CLR_STRUCT(def);
	PF_ADD_CHECKBOX(CRT_GetString(StrID_LinearWorkflow), "", FALSE, 0, DID_LINEAR_WORKFLOW);
	AEFX_CLR_STRUCT(def);
	PF_ADD_CHECKBOX(CRT_GetString(StrID_AutoExposure), "", TRUE, 0, DID_AUTO_EXPOSURE);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_Brightness), 0, 1000, 0, 200, 0, 100, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_BRIGHTNESS);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_GlowIntensity), 0, 400, 0, 200, 0, 70, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_GLOW_INTENSITY);

	AEFX_CLR_STRUCT(def);
	PF_ADD_TOPIC(CRT_GetString(StrID_Pixels), DID_PIXELS_START);
	AEFX_CLR_STRUCT(def);
	PF_ADD_POPUP(CRT_GetString(StrID_PixelType), CRT_PIXEL_TYPE_COUNT, PIX_RGB_TRIAD, CRT_GetString(StrID_PixelType_Choices), DID_PIXEL_TYPE);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_PixelSize), 1, 256, 1, 64, 0, 4, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_PIXEL_SIZE);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_PixelSharpness), 0, 100, 0, 100, 0, 60, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_PIXEL_SHARPNESS);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_PixelBrightness), 0, 400, 0, 200, 0, 120, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_PIXEL_BRIGHTNESS);
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(DID_PIXELS_END);

	AEFX_CLR_STRUCT(def);
	PF_ADD_TOPIC(CRT_GetString(StrID_Screen), DID_SCREEN_START);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_BulgeStrength), 0, 1000, 0, 400, 0, 254, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_BULGE_STRENGTH);
	AEFX_CLR_STRUCT(def);
	PF_ADD_POINT(CRT_GetString(StrID_BulgeCenter), 50, 50, FALSE, DID_BULGE_CENTER);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_ScanlinesIntensity), 0, 100, 0, 100, 0, 15, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_SCANLINES_INTENSITY);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_ScanlinesSpeed), 0, 1000, 0, 200, 0, 80, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_SCANLINES_SPEED);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_FlickerIntensity), 0, 100, 0, 100, 0, 6, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_FLICKER_INTENSITY);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_Posterize), 2, 256, 2, 64, 0, 20, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_POSTERIZE);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_BulbReflections), 0, 100, 0, 100, 0, 30, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_BULB_REFLECTIONS);
	AEFX_CLR_STRUCT(def);
	PF_ADD_COLOR(CRT_GetString(StrID_ScreenTint), 255, 255, 255, DID_SCREEN_TINT);
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(DID_SCREEN_END);

	AEFX_CLR_STRUCT(def);
	PF_ADD_TOPIC(CRT_GetString(StrID_Blurring), DID_BLUR_START);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_BlurAmount), 0, 200, 0, 50, 0, 0, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_BLUR_AMOUNT);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_BlurHBias), 0, 100, 0, 100, 0, 50, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_BLUR_HBIAS);
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(DID_BLUR_END);

	AEFX_CLR_STRUCT(def);
	PF_ADD_TOPIC(CRT_GetString(StrID_Aberrations), DID_ABERR_START);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_AberrAmount), 0, 200, 0, 50, 0, 0, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_ABERR_AMOUNT);
	AEFX_CLR_STRUCT(def);
	PF_ADD_ANGLE(CRT_GetString(StrID_AberrAngle), 0, DID_ABERR_ANGLE);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_Noise), 0, 100, 0, 100, 0, 0, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_NOISE);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_VerticalHold), 0, 100, 0, 100, 0, 0, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_VERTICAL_HOLD);
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(DID_ABERR_END);

	AEFX_CLR_STRUCT(def);
	PF_ADD_TOPIC(CRT_GetString(StrID_Glow), DID_GLOW_START);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_GlowRadius), 0, 500, 0, 200, 0, 24, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_GLOW_RADIUS);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_GlowThreshold), 0, 100, 0, 100, 0, 60, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_GLOW_THRESHOLD);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_GlowSaturation), 0, 400, 0, 200, 0, 120, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_GLOW_SATURATION);
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(DID_GLOW_END);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_BoostSaturation), 0, 400, 0, 400, 0, 197, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_BOOST_SATURATION);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_InputGamma), 1, 1000, 1, 300, 0, 100, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_INPUT_GAMMA);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(CRT_GetString(StrID_Tonemapping), 0, 100, 0, 100, 0, 31, 1, PF_ValueDisplayFlag_NONE, FALSE, DID_TONEMAPPING);

	out_data->num_params = CRT_NUM_PARAMS;
	return err;
}

/* -------------------------------------------------- preset popup handler -- */
static PF_Err UserChangedParam(PF_InData* in_data, PF_OutData* out_data,
							   PF_ParamDef* params[], const PF_UserChangedParamExtra* which)
{
	if (which->param_index == CRT_PRESET) {
		int popup = params[CRT_PRESET]->u.pd.value;
		if (popup > 1) {
			CRT_ApplyPreset(params, CRT_GetPreset(popup));
			out_data->out_flags |= PF_OutFlag_REFRESH_UI;
		}
	}
	return PF_Err_NONE;
}

/* ------------------------------------------------------ settings extract -- */
static float rdf(PF_ParamDef* p) { return (float)p->u.fs_d.value; }

static void ExtractSettings(PF_InData* in_data, PF_ParamDef* params[], CRT_Settings* s)
{
	const float dsx = (float)in_data->downsample_x.num / (float)in_data->downsample_x.den;
	const float dsy = (float)in_data->downsample_y.num / (float)in_data->downsample_y.den;
	const float ds  = 0.5f * (dsx + dsy);
	const int   lw  = in_data->width  > 0 ? in_data->width  : 1;
	const int   lh  = in_data->height > 0 ? in_data->height : 1;

	s->ignore_bounds	= params[CRT_IGNORE_BOUNDS]->u.bd.value != 0;
	s->linear_workflow	= params[CRT_LINEAR_WORKFLOW]->u.bd.value != 0;
	s->auto_exposure	= params[CRT_AUTO_EXPOSURE]->u.bd.value != 0;
	s->brightness		= rdf(params[CRT_BRIGHTNESS]) / 100.f;
	s->glow_intensity	= rdf(params[CRT_GLOW_INTENSITY]) / 100.f;

	s->pixel_type		= params[CRT_PIXEL_TYPE]->u.pd.value;
	s->pixel_size		= rdf(params[CRT_PIXEL_SIZE]) * ds; if (s->pixel_size < 1.f) s->pixel_size = 1.f;
	s->pixel_sharpness	= rdf(params[CRT_PIXEL_SHARPNESS]) / 100.f;
	s->pixel_brightness	= rdf(params[CRT_PIXEL_BRIGHTNESS]) / 100.f;

	s->bulge_strength	= rdf(params[CRT_BULGE_STRENGTH]) / 400.f;
	s->bulge_cx			= ((float)params[CRT_BULGE_CENTER]->u.td.x_value / 65536.f) / lw;
	s->bulge_cy			= ((float)params[CRT_BULGE_CENTER]->u.td.y_value / 65536.f) / lh;
	s->scanlines_intensity = rdf(params[CRT_SCANLINES_INTENSITY]) / 100.f;
	s->scanlines_speed	= rdf(params[CRT_SCANLINES_SPEED]) / 100.f;
	s->flicker_intensity = rdf(params[CRT_FLICKER_INTENSITY]) / 100.f;
	s->posterize_levels	= rdf(params[CRT_POSTERIZE]);
	s->bulb_reflections	= rdf(params[CRT_BULB_REFLECTIONS]) / 100.f;
	s->tint_r			= params[CRT_SCREEN_TINT]->u.cd.value.red   / 255.f;
	s->tint_g			= params[CRT_SCREEN_TINT]->u.cd.value.green / 255.f;
	s->tint_b			= params[CRT_SCREEN_TINT]->u.cd.value.blue  / 255.f;

	s->blur_amount		= rdf(params[CRT_BLUR_AMOUNT]) * ds;
	s->blur_hbias		= rdf(params[CRT_BLUR_HBIAS]) / 100.f;

	s->aberr_amount		= rdf(params[CRT_ABERR_AMOUNT]) * ds;
	s->aberr_angle		= (float)((params[CRT_ABERR_ANGLE]->u.ad.value / 65536.0) * 3.14159265358979 / 180.0);
	s->noise			= rdf(params[CRT_NOISE]) / 100.f;
	s->vertical_hold	= rdf(params[CRT_VERTICAL_HOLD]) / 100.f;

	s->glow_radius		= rdf(params[CRT_GLOW_RADIUS]) * ds;
	s->glow_threshold	= rdf(params[CRT_GLOW_THRESHOLD]) / 100.f;
	s->glow_saturation	= rdf(params[CRT_GLOW_SATURATION]) / 100.f;

	s->boost_saturation	= rdf(params[CRT_BOOST_SATURATION]) / 100.f;
	s->input_gamma		= rdf(params[CRT_INPUT_GAMMA]) / 100.f;
	s->tonemapping		= rdf(params[CRT_TONEMAPPING]) / 100.f;

	s->time_secs		= (float)in_data->current_time / (float)in_data->time_scale;
	s->width = 0; s->height = 0;
}

/* ----------------------------------------------- CPU world <-> float buf -- */
static PF_Err RenderWorldCPU(PF_InData* in_data, PF_PixelFormat fmt,
							 PF_EffectWorld* input, PF_EffectWorld* output, CRT_Settings s)
{
	const A_long W = output->width, H = output->height;
	const A_long iW = input->width, iH = input->height;
	s.width = W; s.height = H;

	CRT_FPixel* buf = new (std::nothrow) CRT_FPixel[(size_t)W * H];
	if (!buf) return PF_Err_OUT_OF_MEMORY;

	for (A_long y = 0; y < H; ++y) {
		A_long sy = y < iH ? y : iH - 1;
		for (A_long x = 0; x < W; ++x) {
			A_long sx = x < iW ? x : iW - 1;
			CRT_FPixel* d = &buf[(size_t)y * W + x];
			if (fmt == PF_PixelFormat_ARGB128) {
				PF_PixelFloat* p = (PF_PixelFloat*)((char*)input->data + sy * input->rowbytes) + sx;
				d->a = p->alpha; d->r = p->red; d->g = p->green; d->b = p->blue;
			} else if (fmt == PF_PixelFormat_ARGB64) {
				PF_Pixel16* p = (PF_Pixel16*)((char*)input->data + sy * input->rowbytes) + sx;
				d->a = p->alpha / 32768.f; d->r = p->red / 32768.f; d->g = p->green / 32768.f; d->b = p->blue / 32768.f;
			} else {
				PF_Pixel8* p = (PF_Pixel8*)((char*)input->data + sy * input->rowbytes) + sx;
				d->a = p->alpha / 255.f; d->r = p->red / 255.f; d->g = p->green / 255.f; d->b = p->blue / 255.f;
			}
		}
	}

	CRT_RenderImage(buf, s);

	for (A_long y = 0; y < H; ++y) {
		for (A_long x = 0; x < W; ++x) {
			CRT_FPixel* sp = &buf[(size_t)y * W + x];
			float r = sp->r, g = sp->g, b = sp->b, a = sp->a;
			if (fmt == PF_PixelFormat_ARGB128) {
				PF_PixelFloat* dp = (PF_PixelFloat*)((char*)output->data + y * output->rowbytes) + x;
				dp->alpha = a; dp->red = r; dp->green = g; dp->blue = b;
			} else {
				if (r < 0) r = 0; if (r > 1) r = 1;
				if (g < 0) g = 0; if (g > 1) g = 1;
				if (b < 0) b = 0; if (b > 1) b = 1;
				if (a < 0) a = 0; if (a > 1) a = 1;
				if (fmt == PF_PixelFormat_ARGB64) {
					PF_Pixel16* dp = (PF_Pixel16*)((char*)output->data + y * output->rowbytes) + x;
					dp->alpha = (A_u_short)(a * 32768 + 0.5f); dp->red = (A_u_short)(r * 32768 + 0.5f);
					dp->green = (A_u_short)(g * 32768 + 0.5f); dp->blue = (A_u_short)(b * 32768 + 0.5f);
				} else {
					PF_Pixel8* dp = (PF_Pixel8*)((char*)output->data + y * output->rowbytes) + x;
					dp->alpha = (A_u_char)(a * 255 + 0.5f); dp->red = (A_u_char)(r * 255 + 0.5f);
					dp->green = (A_u_char)(g * 255 + 0.5f); dp->blue = (A_u_char)(b * 255 + 0.5f);
				}
			}
		}
	}
	delete[] buf;
	return PF_Err_NONE;
}

/* ------------------------------------------------------- GPU device setup -- */
static PF_Err GPUDeviceSetup(PF_InData* in_data, PF_OutData* out_data, PF_GPUDeviceSetupExtra* extraP)
{
	PF_Err err = PF_Err_NONE;

	PF_GPUDeviceInfo device_info;
	AEFX_CLR_STRUCT(device_info);

	AEFX_SuiteScoper<PF_HandleSuite1> handle_suite =
		AEFX_SuiteScoper<PF_HandleSuite1>(in_data, kPFHandleSuite, kPFHandleSuiteVersion1, out_data);
	AEFX_SuiteScoper<PF_GPUDeviceSuite1> gpu_suite =
		AEFX_SuiteScoper<PF_GPUDeviceSuite1>(in_data, kPFGPUDeviceSuite, kPFGPUDeviceSuiteVersion1, out_data);

	gpu_suite->GetDeviceInfo(in_data->effect_ref, extraP->input->device_index, &device_info);

	if (extraP->input->what_gpu == PF_GPU_Framework_OPENCL) {
		PF_Handle gpu_dataH = handle_suite->host_new_handle(sizeof(OpenCLGPUData));
		OpenCLGPUData* cl_gpu_data = reinterpret_cast<OpenCLGPUData*>(*gpu_dataH);

		cl_int result = CL_SUCCESS;
		size_t size = strlen(kCRT_Kernel_OpenCLString);
		char const* strings[] = { kCRT_Kernel_OpenCLString };
		size_t sizes[] = { size };
		cl_context context = (cl_context)device_info.contextPV;
		cl_device_id device = (cl_device_id)device_info.devicePV;

		cl_program program = clCreateProgramWithSource(context, 1, strings, sizes, &result);
		CL_ERR(result);
		CL_ERR(clBuildProgram(program, 1, &device, "-cl-single-precision-constant -cl-fast-relaxed-math", 0, 0));

		if (!err) {
			cl_gpu_data->crt_kernel = clCreateKernel(program, "CRTKernel", &result);
			CL_ERR(result);
		}
		extraP->output->gpu_data = gpu_dataH;
		out_data->out_flags2 = PF_OutFlag2_SUPPORTS_GPU_RENDER_F32;
	}
	return err;
}

static PF_Err GPUDeviceSetdown(PF_InData* in_data, PF_OutData* out_data, PF_GPUDeviceSetdownExtra* extraP)
{
	PF_Err err = PF_Err_NONE;
	if (extraP->input->what_gpu == PF_GPU_Framework_OPENCL) {
		PF_Handle gpu_dataH = (PF_Handle)extraP->input->gpu_data;
		OpenCLGPUData* cl_gpu_data = reinterpret_cast<OpenCLGPUData*>(*gpu_dataH);
		(void)clReleaseKernel(cl_gpu_data->crt_kernel);

		AEFX_SuiteScoper<PF_HandleSuite1> handle_suite =
			AEFX_SuiteScoper<PF_HandleSuite1>(in_data, kPFHandleSuite, kPFHandleSuiteVersion1, out_data);
		handle_suite->host_dispose_handle(gpu_dataH);
	}
	return err;
}

/* ------------------------------------------------------------- PreRender -- */
static void DisposePreRenderData(void* p) { if (p) free(p); }

static PF_Err PreRender(PF_InData* in_data, PF_OutData* out_data, PF_PreRenderExtra* extraP)
{
	PF_Err err = PF_Err_NONE;
	PF_CheckoutResult in_result;
	PF_RenderRequest req = extraP->input->output_request;

	extraP->output->flags |= PF_RenderOutputFlag_GPU_RENDER_POSSIBLE;

	CRT_Settings* infoP = (CRT_Settings*)malloc(sizeof(CRT_Settings));
	if (!infoP) return PF_Err_OUT_OF_MEMORY;

	/* checkout every value param so we can flatten settings now */
	PF_ParamDef arr[CRT_NUM_PARAMS];
	PF_ParamDef* ptrs[CRT_NUM_PARAMS];
	AEFX_CLR_STRUCT(arr);
	for (int i = 1; i < CRT_NUM_PARAMS; ++i) {
		PF_ParamDef cur; AEFX_CLR_STRUCT(cur);
		ERR(PF_CHECKOUT_PARAM(in_data, i, in_data->current_time, in_data->time_step, in_data->time_scale, &cur));
		arr[i] = cur;
		ptrs[i] = &arr[i];
	}
	ptrs[0] = NULL;
	if (!err) ExtractSettings(in_data, ptrs, infoP);

	extraP->output->pre_render_data = infoP;
	extraP->output->delete_pre_render_data_func = DisposePreRenderData;

	ERR(extraP->cb->checkout_layer(in_data->effect_ref, CRT_INPUT, CRT_INPUT, &req,
		in_data->current_time, in_data->time_step, in_data->time_scale, &in_result));

	UnionLRect(&in_result.result_rect, &extraP->output->result_rect);
	UnionLRect(&in_result.max_result_rect, &extraP->output->max_result_rect);
	return err;
}

/* ----------------------------------------------------------- SmartRender -- */
static PF_Err SmartRenderGPU(PF_InData* in_data, PF_OutData* out_data, PF_PixelFormat fmt,
							 PF_EffectWorld* input, PF_EffectWorld* output,
							 PF_SmartRenderExtra* extraP, CRT_Settings* s)
{
	PF_Err err = PF_Err_NONE;

	AEFX_SuiteScoper<PF_GPUDeviceSuite1> gpu_suite =
		AEFX_SuiteScoper<PF_GPUDeviceSuite1>(in_data, kPFGPUDeviceSuite, kPFGPUDeviceSuiteVersion1, out_data);

	if (fmt != PF_PixelFormat_GPU_BGRA128) return PF_Err_UNRECOGNIZED_PARAM_TYPE;
	const A_long bpp = 16;

	PF_GPUDeviceInfo device_info;
	ERR(gpu_suite->GetDeviceInfo(in_data->effect_ref, extraP->input->device_index, &device_info));

	void* src_mem = 0; ERR(gpu_suite->GetGPUWorldData(in_data->effect_ref, input,  &src_mem));
	void* dst_mem = 0; ERR(gpu_suite->GetGPUWorldData(in_data->effect_ref, output, &dst_mem));

	int width  = input->width;
	int height = input->height;
	int srcPitch = input->rowbytes  / bpp;
	int dstPitch = output->rowbytes / bpp;
	int is16f = 0;

	if (!err && extraP->input->what_gpu == PF_GPU_Framework_OPENCL) {
		PF_Handle gpu_dataH = (PF_Handle)extraP->input->gpu_data;
		OpenCLGPUData* cl = reinterpret_cast<OpenCLGPUData*>(*gpu_dataH);
		cl_mem cl_src = (cl_mem)src_mem;
		cl_mem cl_dst = (cl_mem)dst_mem;
		cl_kernel k = cl->crt_kernel;

		cl_uint i = 0;
		#define ARG(SZ, PTR) CL_ERR(clSetKernelArg(k, i++, SZ, PTR))
		ARG(sizeof(cl_mem), &cl_src);
		ARG(sizeof(cl_mem), &cl_dst);
		ARG(sizeof(int), &srcPitch);
		ARG(sizeof(int), &dstPitch);
		ARG(sizeof(int), &is16f);
		ARG(sizeof(int), &width);
		ARG(sizeof(int), &height);
		ARG(sizeof(float), &s->brightness);
		ARG(sizeof(float), &s->glow_intensity);
		ARG(sizeof(int),   &s->pixel_type);
		ARG(sizeof(float), &s->pixel_size);
		ARG(sizeof(float), &s->pixel_sharpness);
		ARG(sizeof(float), &s->pixel_brightness);
		ARG(sizeof(float), &s->bulge_strength);
		ARG(sizeof(float), &s->bulge_cx);
		ARG(sizeof(float), &s->bulge_cy);
		ARG(sizeof(float), &s->scanlines_intensity);
		ARG(sizeof(float), &s->scanlines_speed);
		ARG(sizeof(float), &s->flicker_intensity);
		ARG(sizeof(float), &s->posterize_levels);
		ARG(sizeof(float), &s->bulb_reflections);
		ARG(sizeof(float), &s->tint_r);
		ARG(sizeof(float), &s->tint_g);
		ARG(sizeof(float), &s->tint_b);
		ARG(sizeof(float), &s->blur_amount);
		ARG(sizeof(float), &s->blur_hbias);
		ARG(sizeof(float), &s->aberr_amount);
		ARG(sizeof(float), &s->aberr_angle);
		ARG(sizeof(float), &s->noise);
		ARG(sizeof(float), &s->vertical_hold);
		ARG(sizeof(float), &s->glow_radius);
		ARG(sizeof(float), &s->glow_threshold);
		ARG(sizeof(float), &s->glow_saturation);
		ARG(sizeof(float), &s->boost_saturation);
		ARG(sizeof(float), &s->input_gamma);
		ARG(sizeof(float), &s->tonemapping);
		ARG(sizeof(int),   &s->linear_workflow);
		ARG(sizeof(float), &s->time_secs);
		#undef ARG

		size_t threadBlock[2] = { 16, 16 };
		size_t grid[2] = { RoundUp(width, threadBlock[0]), RoundUp(height, threadBlock[1]) };
		CL_ERR(clEnqueueNDRangeKernel((cl_command_queue)device_info.command_queuePV, k, 2, 0, grid, threadBlock, 0, 0, 0));
	}
	return err;
}

static PF_Err SmartRender(PF_InData* in_data, PF_OutData* out_data, PF_SmartRenderExtra* extraP, bool isGPU)
{
	PF_Err err = PF_Err_NONE, err2 = PF_Err_NONE;
	PF_EffectWorld* input = NULL;
	PF_EffectWorld* output = NULL;

	CRT_Settings* infoP = reinterpret_cast<CRT_Settings*>(extraP->input->pre_render_data);
	if (!infoP) return PF_Err_INTERNAL_STRUCT_DAMAGED;

	ERR(extraP->cb->checkout_layer_pixels(in_data->effect_ref, CRT_INPUT, &input));
	ERR(extraP->cb->checkout_output(in_data->effect_ref, &output));

	if (!err && input && output) {
		AEFX_SuiteScoper<PF_WorldSuite2> world_suite =
			AEFX_SuiteScoper<PF_WorldSuite2>(in_data, kPFWorldSuite, kPFWorldSuiteVersion2, out_data);
		PF_PixelFormat fmt = PF_PixelFormat_INVALID;
		ERR(world_suite->PF_GetPixelFormat(input, &fmt));

		if (isGPU) {
			ERR(SmartRenderGPU(in_data, out_data, fmt, input, output, extraP, infoP));
		} else {
			ERR(RenderWorldCPU(in_data, fmt, input, output, *infoP));
		}
	}
	ERR2(extraP->cb->checkin_layer_pixels(in_data->effect_ref, CRT_INPUT));
	return err;
}

/* ------------------------------------------------- classic render (8/16) -- */
static PF_Err Render(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
	CRT_Settings s;
	ExtractSettings(in_data, params, &s);
	PF_PixelFormat fmt = PF_WORLD_IS_DEEP(output) ? PF_PixelFormat_ARGB64 : PF_PixelFormat_ARGB32;
	return RenderWorldCPU(in_data, fmt, &params[CRT_INPUT]->u.ld, output, s);
}

/* -------------------------------------------------------------- entry --- */
extern "C" DllExport PF_Err PluginDataEntryFunction(
	PF_PluginDataPtr inPtr, PF_PluginDataCB inPluginDataCallBackPtr,
	SPBasicSuite* inSPBasicSuitePtr, const char* inHostName, const char* inHostVersion)
{
	PF_Err result = PF_Err_INVALID_CALLBACK;
	result = PF_REGISTER_EFFECT(inPtr, inPluginDataCallBackPtr,
		CRT_NAME, "Free CRT", "Free CRT", AE_RESERVED_INFO);
	return result;
}

PF_Err EffectMain(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data,
				  PF_ParamDef* params[], PF_LayerDef* output, void* extra)
{
	PF_Err err = PF_Err_NONE;
	try {
		switch (cmd) {
			case PF_Cmd_ABOUT:				err = About(in_data, out_data); break;
			case PF_Cmd_GLOBAL_SETUP:		err = GlobalSetup(in_data, out_data); break;
			case PF_Cmd_PARAMS_SETUP:		err = ParamsSetup(in_data, out_data, params, output); break;
			case PF_Cmd_GPU_DEVICE_SETUP:	err = GPUDeviceSetup(in_data, out_data, (PF_GPUDeviceSetupExtra*)extra); break;
			case PF_Cmd_GPU_DEVICE_SETDOWN:	err = GPUDeviceSetdown(in_data, out_data, (PF_GPUDeviceSetdownExtra*)extra); break;
			case PF_Cmd_RENDER:				err = Render(in_data, out_data, params, output); break;
			case PF_Cmd_SMART_PRE_RENDER:	err = PreRender(in_data, out_data, (PF_PreRenderExtra*)extra); break;
			case PF_Cmd_SMART_RENDER:		err = SmartRender(in_data, out_data, (PF_SmartRenderExtra*)extra, false); break;
			case PF_Cmd_SMART_RENDER_GPU:	err = SmartRender(in_data, out_data, (PF_SmartRenderExtra*)extra, true); break;
			case PF_Cmd_USER_CHANGED_PARAM:	err = UserChangedParam(in_data, out_data, params, (const PF_UserChangedParamExtra*)extra); break;
			default: break;
		}
	} catch (PF_Err& thrown) {
		err = thrown;
	} catch (...) {
		err = PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	return err;
}
