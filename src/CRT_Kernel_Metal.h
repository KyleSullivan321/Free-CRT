/*
	CRT_Kernel_Metal.h

	The Free CRT pipeline as a Metal compute kernel (macOS GPU path), embedded as
	a C string compiled at GPU-device setup via newLibraryWithSource. Mirrors the
	OpenCL kernel (CRT_Kernel_CL.h) and the CPU pipeline (CRT_Render.cpp). GPU
	buffers are BGRA, 32-bit float per channel.

	The CRTParams struct below MUST match the host-side struct in FreeCRT.cpp
	(same field order, all 4-byte scalars).

	NOTE: this Mac/Metal path is written modelled on Adobe's SDK_Invert_ProcAmp
	sample but has NOT been compiled or tested on macOS yet.
*/
#pragma once

static const char* kCRT_Kernel_MetalString = R"MSL(
#include <metal_stdlib>
using namespace metal;

typedef struct {
	int   inPitch, outPitch, in16f, width, height;
	float brightness, glowIntensity;
	int   pixelType;
	float pixelSize, pixelSharp, pixelBright;
	float bulgeStrength, bulgeCx, bulgeCy;
	float scanInt, scanSpeed, flickerInt, posterize, bulb;
	float tintR, tintG, tintB;
	float blurAmt, blurHBias;
	float aberrAmt, aberrAngle, noise, vhold;
	float glowRadius, glowThr, glowSat;
	float boostSat, inputGamma, tonemap;
	int   linearWF;
	float timeSecs;
} CRTParams;

float3 readRGB(device const float4* img, int pitch, int w, int h, int x, int y) {
	x = clamp(x, 0, w - 1); y = clamp(y, 0, h - 1);
	float4 p = img[y * pitch + x];   /* BGRA */
	return float3(p.z, p.y, p.x);
}

float3 sampleRGB(device const float4* img, int pitch, int w, int h, float fx, float fy) {
	fx = clamp(fx, 0.0f, (float)(w - 1));
	fy = clamp(fy, 0.0f, (float)(h - 1));
	int x0 = (int)fx, y0 = (int)fy;
	int x1 = x0 + 1 < w ? x0 + 1 : x0;
	int y1 = y0 + 1 < h ? y0 + 1 : y0;
	float tx = fx - x0, ty = fy - y0;
	float3 a = readRGB(img, pitch, w, h, x0, y0);
	float3 b = readRGB(img, pitch, w, h, x1, y0);
	float3 c = readRGB(img, pitch, w, h, x0, y1);
	float3 d = readRGB(img, pitch, w, h, x1, y1);
	return mix(mix(a, b, tx), mix(c, d, tx), ty);
}

float4 sampleRGBA(device const float4* img, int pitch, int w, int h, float fx, float fy) {
	if (fx < 0.0f || fx > w - 1.0f || fy < 0.0f || fy > h - 1.0f) return float4(0.0f);
	int x0 = (int)fx, y0 = (int)fy;
	int x1 = x0 + 1 < w ? x0 + 1 : x0;
	int y1 = y0 + 1 < h ? y0 + 1 : y0;
	float tx = fx - x0, ty = fy - y0;
	float4 a = img[y0 * pitch + x0]; float4 b = img[y0 * pitch + x1];
	float4 c = img[y1 * pitch + x0]; float4 d = img[y1 * pitch + x1];
	float4 p = mix(mix(a, b, tx), mix(c, d, tx), ty);  /* BGRA */
	return float4(p.z, p.y, p.x, p.w);                 /* RGBA */
}

float4 sampleBoxRGBA(device const float4* img, int pitch, int w, int h, float fx, float fy, float rx, float ry) {
	int kx = min((int)(rx + 0.5f), 12);
	int ky = min((int)(ry + 0.5f), 12);
	int sx = kx > 4 ? kx / 4 : 1;
	int sy = ky > 4 ? ky / 4 : 1;
	float4 acc = float4(0.0f); float cnt = 0.0f;
	for (int j = -ky; j <= ky; j += sy)
		for (int i = -kx; i <= kx; i += sx) { acc += sampleRGBA(img, pitch, w, h, fx + i, fy + j); cnt += 1.0f; }
	return cnt > 0.0f ? acc / cnt : float4(0.0f);
}

float lum3(float3 c) { return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z; }

float hashf(int x, int y) {
	uint h = (uint)(x * 374761393) + (uint)(y * 668265263);
	h = (h ^ (h >> 13)) * 1274126177u;
	return (h & 0x00FFFFFF) / (float)0x01000000;
}

float srgb2lin(float c) { return c <= 0.04045f ? c / 12.92f : pow((c + 0.055f) / 1.055f, 2.4f); }
float lin2srgb(float c) { return c <= 0.0031308f ? c * 12.92f : 1.055f * pow(c, 1.0f / 2.4f) - 0.055f; }

float3 pixelMask(int x, int y, int type, float cell, float sharp, float pbright) {
	float3 m = float3(1.0f);
	if (type == 1) return m;
	float lo = 1.0f - clamp(sharp, 0.0f, 1.0f);
	float fx = fmod((float)x, cell) / cell;
	float fy = fmod((float)y, cell) / cell;
	int sub = ((int)(fx * 3.0f)) % 3;
	if (type == 2) {
		m = float3(sub == 0 ? 1.0f : lo, sub == 1 ? 1.0f : lo, sub == 2 ? 1.0f : lo);
		float gap = fy > 0.85f ? lo : 1.0f; m *= gap;
	} else if (type == 3) {
		m = float3(sub == 0 ? 1.0f : lo, sub == 1 ? 1.0f : lo, sub == 2 ? 1.0f : lo);
	} else if (type == 4) {
		int rowShift = (((int)(y / cell)) & 1) ? 1 : 0;
		sub = ((int)((fx + rowShift * 0.5f / 3.0f) * 3.0f)) % 3;
		m = float3(sub == 0 ? 1.0f : lo, sub == 1 ? 1.0f : lo, sub == 2 ? 1.0f : lo);
		float gap = fy > 0.8f ? lo : 1.0f; m *= gap;
	} else if (type == 5) {
		float dx = fx - 0.5f, dy = fy - 0.5f;
		float d = sqrt(dx * dx + dy * dy) * 2.0f;
		float lit = clamp(1.0f - (d - (1.0f - sharp)) * 4.0f, lo, 1.0f);
		m = float3(lit, lit, lit);
	}
	return m * pbright;
}

kernel void CRTKernel(
	device const float4* inImg   [[buffer(0)]],
	device float4*       outImg  [[buffer(1)]],
	constant CRTParams&  P        [[buffer(2)]],
	uint2 gid [[thread_position_in_grid]])
{
	int x = (int)gid.x, y = (int)gid.y;
	if (x >= P.width || y >= P.height) return;

	float hw = P.width * 0.5f, hh = P.height * 0.5f;
	float hmin = fmin(hw, hh);
	float k = P.bulgeStrength * 0.6f;
	float r2corner = (hw * hw + hh * hh) / (hmin * hmin);
	float curveCorner = 1.0f + k * r2corner;
	float zoom = 1.0f + (curveCorner - 1.0f) * 1.3f + P.bulgeStrength * 0.5f;
	float mx = hw * curveCorner / zoom, my = hh * curveCorner / zoom;
	float cxp = clamp(P.bulgeCx * P.width,  mx, P.width  - mx);
	float cyp = clamp(P.bulgeCy * P.height, my, P.height - my);
	float vshift = P.vhold > 0.0f ? sin(P.timeSecs * 2.4f) * P.vhold * P.height * 0.25f : 0.0f;
	float ax = cos(P.aberrAngle), ay = sin(P.aberrAngle);
	float hb = clamp(P.blurHBias, 0.0f, 1.0f);

	float nx = (x - hw) / hmin, ny = (y - hh) / hmin;
	float r2 = nx * nx + ny * ny;
	float curve = 1.0f + k * r2;
	float sx = cxp + nx * curve / zoom * hmin;
	float sy = cyp + ny * curve / zoom * hmin + vshift;

	float dof = P.blurAmt * r2 * 0.5f;
	float rx = dof * (0.5f + hb), ry = dof * (1.5f - hb);

	float4 base;
	if (P.aberrAmt > 0.01f) {
		float ox = ax * P.aberrAmt, oy = ay * P.aberrAmt;
		float rr = sampleBoxRGBA(inImg, P.inPitch, P.width, P.height, sx + ox, sy + oy, rx, ry).x;
		float4 g = sampleBoxRGBA(inImg, P.inPitch, P.width, P.height, sx, sy, rx, ry);
		float bb = sampleBoxRGBA(inImg, P.inPitch, P.width, P.height, sx - ox, sy - oy, rx, ry).z;
		base = float4(rr, g.y, bb, g.w);
	} else {
		base = sampleBoxRGBA(inImg, P.inPitch, P.width, P.height, sx, sy, rx, ry);
	}
	float3 c = base.xyz;
	float alpha = base.w;

	float ig = P.inputGamma > 0.001f ? 1.0f / P.inputGamma : 1.0f;
	c = pow(max(c, float3(0.0f)), float3(ig));
	if (P.linearWF) c = float3(srgb2lin(c.x), srgb2lin(c.y), srgb2lin(c.z));

	float3 glow = float3(0.0f);
	if (P.glowIntensity > 0.001f && P.glowRadius > 0.5f) {
		float gr = min(P.glowRadius, 64.0f);
		float3 acc = float3(0.0f); float wsum = 0.0f;
		for (int t = 0; t < 12; ++t) {
			float ang = (t / 12.0f) * 6.2831853f;
			for (int ring = 1; ring <= 2; ++ring) {
				float rad = gr * (ring * 0.5f);
				float3 s = sampleRGB(inImg, P.inPitch, P.width, P.height, sx + cos(ang) * rad, sy + sin(ang) * rad);
				float l = lum3(s);
				float bp = l > P.glowThr ? (l - P.glowThr) / (1.0f - P.glowThr + 1e-4f) : 0.0f;
				acc += s * bp; wsum += 1.0f;
			}
		}
		if (wsum > 0.0f) { glow = acc / wsum; float gl = lum3(glow); glow = mix(float3(gl), glow, P.glowSat); }
	}

	float scanRoll = P.timeSecs * P.scanSpeed * 8.0f;
	float sc = 0.5f + 0.5f * sin(6.2831853f * ((y + scanRoll) / 3.0f));
	float scanMul = 1.0f - P.scanInt * (1.0f - sc);
	float flick = P.flickerInt > 0.0f ? 1.0f - P.flickerInt * hashf((int)(P.timeSecs * 60.0f), 17) * 0.6f : 1.0f;
	c *= scanMul * flick;

	c *= pixelMask(x, y, P.pixelType, fmax(P.pixelSize, 1.0f), P.pixelSharp, P.pixelBright);

	if (P.noise > 0.0f) {
		float n = (hashf(x * 7 + 1, y * 13 + (int)(P.timeSecs * 60.0f)) - 0.5f) * P.noise;
		c += float3(n);
	}

	if (P.posterize >= 2.0f && P.posterize < 256.0f) {
		float L = P.posterize - 1.0f;
		c = floor(clamp(c, 0.0f, 1.0f) * L + 0.5f) / L;
	}

	c += glow * P.glowIntensity;

	if (P.bulb > 0.0f) {
		float bx = (x - hw) / hw, by = (y - hh) / hh;
		float brr = bx * bx + by * by;
		float vign = 1.0f - P.bulb * 0.6f * brr;
		float hx = bx, hy = by + 0.5f;
		float hl = P.bulb * 0.25f * exp(-(hx * hx + hy * hy) * 3.0f);
		c = c * vign + float3(hl);
	}

	c *= float3(P.tintR, P.tintG, P.tintB);

	if (P.boostSat != 1.0f) { float g = lum3(c); c = mix(float3(g), c, P.boostSat); }

	c *= P.brightness;

	if (P.tonemap > 0.0f) c = mix(c, c / (1.0f + c), P.tonemap);

	if (P.linearWF) { c = clamp(c, 0.0f, 4.0f); c = float3(lin2srgb(c.x), lin2srgb(c.y), lin2srgb(c.z)); }

	outImg[y * P.outPitch + x] = float4(c.z, c.y, c.x, alpha);  /* RGB -> BGRA */
}
)MSL";
