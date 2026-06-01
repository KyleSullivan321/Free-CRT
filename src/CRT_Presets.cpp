/*
	CRT_Presets.cpp — preset value tables.

	Recreations of the looks described on the product page (see docs/presets.md).
	Tune these visually; the structure is what matters for iteration 1.
*/
#include "CRT_Presets.h"

/* Field order matches CRT_Preset:
   bright, glow, pixType, pSize, pSharp, pBright,
   bulge, scanI, scanSpd, flick, post, bulb, tR,tG,tB,
   blur, hbias, aberr, aAng, noise, vhold,
   gRad, gThr, gSat, boostSat, inGamma, tone               */
static const CRT_Preset g_presets[CRT_PRESET_COUNT - 1] = {
	/* Midnight Arcade */ {110, 90, PIX_RGB_TRIAD,       5, 70,135, 300,22, 80,10, 16,40, 1.00f,0.96f,0.90f,  1,55, 1, 0,  4, 0, 36,55,150, 230,100,45},
	/* Broadcast '83   */ { 95, 70, PIX_SLOT_MASK,       7, 60,128, 240,35, 80, 8, 10,40, 0.85f,1.00f,0.88f,  1,55, 2, 0,  8, 0, 30,55,110, 185,105,35},
	/* Dead Channel    */ { 95,100, PIX_RGB_TRIAD,       5, 65,130, 260,30,160,55, 12,45, 0.95f,0.97f,1.00f,  2,60, 6, 0, 28,40, 40,50,140, 210,100,40},
	/* Pocket LCD      */ {100, 60, PIX_GRID_LED,       12, 85,140,  80, 8, 40, 4, 24,18, 0.96f,1.00f,0.98f,  0,50, 0, 0,  2, 0, 40,65,110, 175,100,28},
	/* Hi-Fi Grille    */ {100, 80, PIX_APERTURE_GRILLE, 3, 82,122, 120,10, 80, 4, 48,18, 1.00f,1.00f,1.00f,  0,50, 1, 0,  1, 0, 26,70,140, 205,100,32},
	/* Acid Wash       */ {120,120, PIX_RGB_TRIAD,       5, 80,140, 320,16,120, 8, 14,45, 1.00f,0.96f,1.00f,  1,55, 2, 0,  4, 0, 48,45,180, 320,100,55},
	/* Night Shift     */ { 70, 55, PIX_RGB_TRIAD,       4, 60,118, 220,20, 80, 6, 20,35, 1.00f,0.90f,0.70f,  2,55, 1, 0,  6, 0, 30,62,100, 150,110,28},
};

const CRT_Preset* CRT_GetPreset(int popup_value)
{
	/* popup_value: 1 == "None"; 2..8 map to g_presets[0..6] */
	int idx = popup_value - 2;
	if (idx < 0 || idx >= (CRT_PRESET_COUNT - 1)) return nullptr;
	return &g_presets[idx];
}

static void set_float(PF_ParamDef* p, float v)
{
	p->u.fs_d.value = v;
	p->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
}

void CRT_ApplyPreset(PF_ParamDef* params[], const CRT_Preset* p)
{
	if (!p) return;

	set_float(params[CRT_BRIGHTNESS],			p->brightness);
	set_float(params[CRT_GLOW_INTENSITY],		p->glow_intensity);

	params[CRT_PIXEL_TYPE]->u.pd.value = (A_short)p->pixel_type;
	params[CRT_PIXEL_TYPE]->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
	set_float(params[CRT_PIXEL_SIZE],			p->pixel_size);
	set_float(params[CRT_PIXEL_SHARPNESS],		p->pixel_sharpness);
	set_float(params[CRT_PIXEL_BRIGHTNESS],		p->pixel_brightness);

	set_float(params[CRT_BULGE_STRENGTH],		p->bulge_strength);
	set_float(params[CRT_SCANLINES_INTENSITY],	p->scanlines_intensity);
	set_float(params[CRT_SCANLINES_SPEED],		p->scanlines_speed);
	set_float(params[CRT_FLICKER_INTENSITY],	p->flicker_intensity);
	set_float(params[CRT_POSTERIZE],			p->posterize);
	set_float(params[CRT_BULB_REFLECTIONS],		p->bulb_reflections);

	params[CRT_SCREEN_TINT]->u.cd.value.red   = (A_u_char)(p->tint_r * 255.f + 0.5f);
	params[CRT_SCREEN_TINT]->u.cd.value.green = (A_u_char)(p->tint_g * 255.f + 0.5f);
	params[CRT_SCREEN_TINT]->u.cd.value.blue  = (A_u_char)(p->tint_b * 255.f + 0.5f);
	params[CRT_SCREEN_TINT]->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;

	set_float(params[CRT_BLUR_AMOUNT],			p->blur_amount);
	set_float(params[CRT_BLUR_HBIAS],			p->blur_hbias);

	set_float(params[CRT_ABERR_AMOUNT],			p->aberr_amount);
	params[CRT_ABERR_ANGLE]->u.ad.value = (A_long)(p->aberr_angle * 65536.0f); /* fixed 16.16 */
	params[CRT_ABERR_ANGLE]->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
	set_float(params[CRT_NOISE],				p->noise);
	set_float(params[CRT_VERTICAL_HOLD],		p->vertical_hold);

	set_float(params[CRT_GLOW_RADIUS],			p->glow_radius);
	set_float(params[CRT_GLOW_THRESHOLD],		p->glow_threshold);
	set_float(params[CRT_GLOW_SATURATION],		p->glow_saturation);

	set_float(params[CRT_BOOST_SATURATION],		p->boost_saturation);
	set_float(params[CRT_INPUT_GAMMA],			p->input_gamma);
	set_float(params[CRT_TONEMAPPING],			p->tonemapping);
}
