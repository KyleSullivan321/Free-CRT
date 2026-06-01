/*
	CRT_Kernel_CL.h

	The Free CRT pipeline as an OpenCL kernel, embedded as a C string so the host
	can hand it straight to clCreateProgramWithSource. GPU buffers are BGRA, 32-bit
	float per channel (PF_PixelFormat_GPU_BGRA128).

	Mirrors the per-pixel math in CRT_Render.cpp, including the macro-lens bulge
	(convex sphere + zoom + pan + out-of-bounds→black) and the radial depth-of-
	field tied to the bulge. Glow uses bounded multi-tap sampling (single pass)
	and Auto Exposure (a whole-image average) is CPU-only. See docs/roadmap.md.
*/
#pragma once

static const char* kCRT_Kernel_OpenCLString = R"CLK(

float3 readRGB(__global const float4* img, int pitch, int w, int h, int x, int y) {
	if (x < 0) x = 0; if (x > w - 1) x = w - 1;
	if (y < 0) y = 0; if (y > h - 1) y = h - 1;
	float4 p = img[y * pitch + x];   /* BGRA */
	return (float3)(p.z, p.y, p.x);  /* -> RGB */
}

float3 sampleRGB(__global const float4* img, int pitch, int w, int h, float fx, float fy) {
	if (fx < 0.0f) fx = 0.0f; if (fx > w - 1.0f) fx = w - 1.0f;
	if (fy < 0.0f) fy = 0.0f; if (fy > h - 1.0f) fy = h - 1.0f;
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

/* RGBA sample, out-of-bounds -> 0 (black/transparent). Returns (r,g,b,a). */
float4 sampleRGBA(__global const float4* img, int pitch, int w, int h, float fx, float fy) {
	if (fx < 0.0f || fx > w - 1.0f || fy < 0.0f || fy > h - 1.0f) return (float4)(0.0f, 0.0f, 0.0f, 0.0f);
	int x0 = (int)fx, y0 = (int)fy;
	int x1 = x0 + 1 < w ? x0 + 1 : x0;
	int y1 = y0 + 1 < h ? y0 + 1 : y0;
	float tx = fx - x0, ty = fy - y0;
	float4 a = img[y0 * pitch + x0]; float4 b = img[y0 * pitch + x1];
	float4 c = img[y1 * pitch + x0]; float4 d = img[y1 * pitch + x1];
	float4 p = mix(mix(a, b, tx), mix(c, d, tx), ty);  /* BGRA */
	return (float4)(p.z, p.y, p.x, p.w);               /* RGBA */
}

/* Variable box sample (depth of field), out-of-bounds taps read black. */
float4 sampleBoxRGBA(__global const float4* img, int pitch, int w, int h, float fx, float fy, float rx, float ry) {
	int kx = min((int)(rx + 0.5f), 12);
	int ky = min((int)(ry + 0.5f), 12);
	int sx = kx > 4 ? kx / 4 : 1;
	int sy = ky > 4 ? ky / 4 : 1;
	float4 acc = (float4)(0.0f); float cnt = 0.0f;
	for (int j = -ky; j <= ky; j += sy) {
		for (int i = -kx; i <= kx; i += sx) {
			acc += sampleRGBA(img, pitch, w, h, fx + i, fy + j);
			cnt += 1.0f;
		}
	}
	return cnt > 0.0f ? acc / cnt : (float4)(0.0f);
}

float lum(float3 c) { return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z; }

float hashf(int x, int y) {
	uint h = (uint)(x * 374761393) + (uint)(y * 668265263);
	h = (h ^ (h >> 13)) * 1274126177u;
	return (h & 0x00FFFFFF) / (float)0x01000000;
}

float srgb2lin(float c) { return c <= 0.04045f ? c / 12.92f : pow((c + 0.055f) / 1.055f, 2.4f); }
float lin2srgb(float c) { return c <= 0.0031308f ? c * 12.92f : 1.055f * pow(c, 1.0f / 2.4f) - 0.055f; }

float3 pixelMask(int x, int y, int type, float cell, float sharp, float pbright) {
	float3 m = (float3)(1.0f, 1.0f, 1.0f);
	if (type == 1) return m;                 /* None */
	float lo = 1.0f - clamp(sharp, 0.0f, 1.0f);
	float fx = fmod((float)x, cell) / cell;
	float fy = fmod((float)y, cell) / cell;
	int sub = ((int)(fx * 3.0f)) % 3;
	if (type == 2) {                          /* RGB triad */
		m = (float3)(sub == 0 ? 1.0f : lo, sub == 1 ? 1.0f : lo, sub == 2 ? 1.0f : lo);
		float gap = fy > 0.85f ? lo : 1.0f; m *= gap;
	} else if (type == 3) {                   /* aperture grille */
		m = (float3)(sub == 0 ? 1.0f : lo, sub == 1 ? 1.0f : lo, sub == 2 ? 1.0f : lo);
	} else if (type == 4) {                   /* slot mask */
		int rowShift = (((int)(y / cell)) & 1) ? 1 : 0;
		sub = ((int)((fx + rowShift * 0.5f / 3.0f) * 3.0f)) % 3;
		m = (float3)(sub == 0 ? 1.0f : lo, sub == 1 ? 1.0f : lo, sub == 2 ? 1.0f : lo);
		float gap = fy > 0.8f ? lo : 1.0f; m *= gap;
	} else if (type == 5) {                   /* grid LED */
		float dx = fx - 0.5f, dy = fy - 0.5f;
		float d = sqrt(dx * dx + dy * dy) * 2.0f;
		float lit = clamp(1.0f - (d - (1.0f - sharp)) * 4.0f, lo, 1.0f);
		m = (float3)(lit, lit, lit);
	}
	return m * pbright;
}

__kernel void CRTKernel(
	__global const float4* inImg,
	__global float4* outImg,
	int inPitch, int outPitch, int in16f, int width, int height,
	float brightness, float glowIntensity,
	int pixelType, float pixelSize, float pixelSharp, float pixelBright,
	float bulgeStrength, float bulgeCx, float bulgeCy,
	float scanInt, float scanSpeed, float flickerInt, float posterize, float bulb,
	float tintR, float tintG, float tintB,
	float blurAmt, float blurHBias,
	float aberrAmt, float aberrAngle, float noise, float vhold,
	float glowRadius, float glowThr, float glowSat,
	float boostSat, float inputGamma, float tonemap, int linearWF, float timeSecs)
{
	int x = get_global_id(0);
	int y = get_global_id(1);
	if (x >= width || y >= height) return;

	float hw = width * 0.5f, hh = height * 0.5f;
	float hmin = fmin(hw, hh);
	float k = bulgeStrength * 0.6f;          /* sphere curvature */
	/* Zoom enough that the most-curved corners still sample inside the image,
	   so the frame stays filled (no black corners), with headroom to pan. */
	float r2corner = (hw * hw + hh * hh) / (hmin * hmin);
	float curveCorner = 1.0f + k * r2corner;
	float zoom = 1.0f + (curveCorner - 1.0f) * 1.3f + bulgeStrength * 0.5f;
	float mx = hw * curveCorner / zoom, my = hh * curveCorner / zoom;
	float cxp = clamp(bulgeCx * width,  mx, width  - mx);   /* panned focus */
	float cyp = clamp(bulgeCy * height, my, height - my);
	float vshift = vhold > 0.0f ? sin(timeSecs * 2.4f) * vhold * height * 0.25f : 0.0f;
	float ax = cos(aberrAngle), ay = sin(aberrAngle);
	float hb = clamp(blurHBias, 0.0f, 1.0f);

	/* macro-lens bulge: convex sphere + zoom + pan, OOB -> black */
	float nx = (x - hw) / hmin, ny = (y - hh) / hmin;
	float r2 = nx * nx + ny * ny;
	float curve = 1.0f + k * r2;
	float sx = cxp + nx * curve / zoom * hmin;
	float sy = cyp + ny * curve / zoom * hmin + vshift;

	/* radial depth of field: sharp at focus, blurrier toward the edges */
	float dof = blurAmt * r2 * 0.5f;
	float rx = dof * (0.5f + hb), ry = dof * (1.5f - hb);

	float4 base;
	if (aberrAmt > 0.01f) {
		float ox = ax * aberrAmt, oy = ay * aberrAmt;
		float rr = sampleBoxRGBA(inImg, inPitch, width, height, sx + ox, sy + oy, rx, ry).x;
		float4 g = sampleBoxRGBA(inImg, inPitch, width, height, sx, sy, rx, ry);
		float bb = sampleBoxRGBA(inImg, inPitch, width, height, sx - ox, sy - oy, rx, ry).z;
		base = (float4)(rr, g.y, bb, g.w);
	} else {
		base = sampleBoxRGBA(inImg, inPitch, width, height, sx, sy, rx, ry);
	}
	float3 c = base.xyz;
	float alpha = base.w;

	/* input gamma + linearise */
	float ig = inputGamma > 0.001f ? 1.0f / inputGamma : 1.0f;
	c = pow(max(c, (float3)(0.0f)), (float3)(ig));
	if (linearWF) c = (float3)(srgb2lin(c.x), srgb2lin(c.y), srgb2lin(c.z));

	/* approximate glow: bright-pass ring taps around the (zoomed) source point */
	float3 glow = (float3)(0.0f);
	if (glowIntensity > 0.001f && glowRadius > 0.5f) {
		float gr = min(glowRadius, 64.0f);
		float3 acc = (float3)(0.0f); float wsum = 0.0f;
		for (int t = 0; t < 12; ++t) {
			float ang = (t / 12.0f) * 6.2831853f;
			for (int ring = 1; ring <= 2; ++ring) {
				float rad = gr * (ring * 0.5f);
				float3 s = sampleRGB(inImg, inPitch, width, height, sx + cos(ang) * rad, sy + sin(ang) * rad);
				float l = lum(s);
				float bp = l > glowThr ? (l - glowThr) / (1.0f - glowThr + 1e-4f) : 0.0f;
				acc += s * bp; wsum += 1.0f;
			}
		}
		if (wsum > 0.0f) {
			glow = acc / wsum;
			float gl = lum(glow);
			glow = mix((float3)(gl), glow, glowSat);
		}
	}

	/* scanlines + flicker */
	float scanRoll = timeSecs * scanSpeed * 8.0f;
	float sc = 0.5f + 0.5f * sin(6.2831853f * ((y + scanRoll) / 3.0f));
	float scanMul = 1.0f - scanInt * (1.0f - sc);
	float flick = flickerInt > 0.0f ? 1.0f - flickerInt * hashf((int)(timeSecs * 60.0f), 17) * 0.6f : 1.0f;
	c *= scanMul * flick;

	/* phosphor mask */
	c *= pixelMask(x, y, pixelType, fmax(pixelSize, 1.0f), pixelSharp, pixelBright);

	/* noise / static */
	if (noise > 0.0f) {
		float n = (hashf(x * 7 + 1, y * 13 + (int)(timeSecs * 60.0f)) - 0.5f) * noise;
		c += (float3)(n);
	}

	/* posterize */
	if (posterize >= 2.0f && posterize < 256.0f) {
		float L = posterize - 1.0f;
		c = floor(clamp(c, 0.0f, 1.0f) * L + 0.5f) / L;
	}

	/* add glow */
	c += glow * glowIntensity;

	/* bulb reflections: vignette + soft top highlight */
	if (bulb > 0.0f) {
		float bx = (x - hw) / hw, by = (y - hh) / hh;
		float brr = bx * bx + by * by;
		float vign = 1.0f - bulb * 0.6f * brr;
		float hx = bx, hy = by + 0.5f;
		float hl = bulb * 0.25f * exp(-(hx * hx + hy * hy) * 3.0f);
		c = c * vign + (float3)(hl);
	}

	/* screen tint */
	c *= (float3)(tintR, tintG, tintB);

	/* boost saturation */
	if (boostSat != 1.0f) {
		float g = lum(c);
		c = mix((float3)(g), c, boostSat);
	}

	/* brightness */
	c *= brightness;

	/* tonemapping */
	if (tonemap > 0.0f) {
		c = mix(c, c / (1.0f + c), tonemap);
	}

	/* linear -> sRGB */
	if (linearWF) {
		c = clamp(c, 0.0f, 4.0f);
		c = (float3)(lin2srgb(c.x), lin2srgb(c.y), lin2srgb(c.z));
	}

	outImg[y * outPitch + x] = (float4)(c.z, c.y, c.x, alpha);  /* RGB -> BGRA */
}

)CLK";
