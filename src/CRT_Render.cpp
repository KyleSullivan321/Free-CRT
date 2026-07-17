/*
	CRT_Render.cpp

	The CRT look, as a CPU pipeline over an interleaved float RGBA buffer. This
	file is AE-agnostic apart from the shared structs in FreeCRT.h: it does
	plain math and heap buffers so it can later be ported to a GPU kernel
	(docs/roadmap.md) or unit-tested on its own.

	Pipeline order mirrors docs/features.md.
*/

#include "FreeCRT.h"
#include <new>
#include <cmath>
#include <cstring>
#include <thread>
#include <vector>

namespace {

/* Fork-join a pass over contiguous blocks of [0,total) across CPU cores. Every
   heavy loop here is row- or index-independent (the resample reads a separate
   src copy; the screen loop touches only its own pixel), so this needs no
   locks: the caller thread runs block 0, spawned threads run the rest, join.
   The pixel math is unchanged — this only spreads it over cores. */
template <class F>
inline void parallelBlocks(int total, F body) {
	if (total <= 0) return;
	unsigned hw = std::thread::hardware_concurrency();
	int nthreads = hw ? (int)hw : 1;
	if (nthreads > total) nthreads = total;
	if (nthreads <= 1) { body(0, total); return; }
	int chunk = (total + nthreads - 1) / nthreads;
	std::vector<std::thread> pool;
	pool.reserve(nthreads - 1);
	for (int t = 1; t < nthreads; ++t) {
		int lo = t * chunk; if (lo >= total) break;
		int hi = lo + chunk < total ? lo + chunk : total;
		pool.emplace_back([&body, lo, hi] { body(lo, hi); });
	}
	body(0, chunk < total ? chunk : total);		/* caller thread does block 0 */
	for (auto& th : pool) th.join();
}

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t)    { return a + (b - a) * t; }
inline float luma(const CRT_FPixel& p)           { return 0.2126f * p.r + 0.7152f * p.g + 0.0722f * p.b; }

inline float srgb_to_lin(float c) { return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f); }
inline float lin_to_srgb(float c) { return c <= 0.0031308f ? c * 12.92f : 1.055f * powf(c, 1.f / 2.4f) - 0.055f; }

/* cheap deterministic hash -> [0,1) */
inline float hash2(int x, int y) {
	unsigned int h = (unsigned int)(x * 374761393) + (unsigned int)(y * 668265263);
	h = (h ^ (h >> 13)) * 1274126177u;
	return (h & 0x00FFFFFF) / (float)0x01000000;
}

inline CRT_FPixel sampleBilinear(const CRT_FPixel* src, int W, int H, float fx, float fy) {
	if (fx < 0) fx = 0; if (fx > W - 1.f) fx = W - 1.f;
	if (fy < 0) fy = 0; if (fy > H - 1.f) fy = H - 1.f;
	int x0 = (int)fx, y0 = (int)fy;
	int x1 = x0 + 1 < W ? x0 + 1 : x0;
	int y1 = y0 + 1 < H ? y0 + 1 : y0;
	float tx = fx - x0, ty = fy - y0;
	const CRT_FPixel& a = src[y0 * W + x0]; const CRT_FPixel& b = src[y0 * W + x1];
	const CRT_FPixel& c = src[y1 * W + x0]; const CRT_FPixel& d = src[y1 * W + x1];
	CRT_FPixel o;
	o.r = lerpf(lerpf(a.r, b.r, tx), lerpf(c.r, d.r, tx), ty);
	o.g = lerpf(lerpf(a.g, b.g, tx), lerpf(c.g, d.g, tx), ty);
	o.b = lerpf(lerpf(a.b, b.b, tx), lerpf(c.b, d.b, tx), ty);
	o.a = lerpf(lerpf(a.a, b.a, tx), lerpf(c.a, d.a, tx), ty);
	return o;
}

/* Variable box sample where out-of-bounds taps are BLACK/transparent (not
   clamped), so the curved-away edges of the bulged "screen" don't smear/stretch.
   rx/ry are blur radii in pixels (0 = a single bilinear tap). */
inline CRT_FPixel sampleBoxOOB(const CRT_FPixel* src, int W, int H, float fx, float fy, float rx, float ry) {
	int kx = (int)(rx + 0.5f); if (kx > 12) kx = 12;
	int ky = (int)(ry + 0.5f); if (ky > 12) ky = 12;
	int stepx = kx > 4 ? kx / 4 : 1;
	int stepy = ky > 4 ? ky / 4 : 1;
	CRT_FPixel acc = { 0,0,0,0 }; float cnt = 0.f;
	for (int j = -ky; j <= ky; j += stepy) {
		for (int i = -kx; i <= kx; i += stepx) {
			float X = fx + i, Y = fy + j;
			cnt += 1.f;
			if (X < 0.f || X > W - 1.f || Y < 0.f || Y > H - 1.f) continue;	/* OOB -> black */
			int x0 = (int)X, y0 = (int)Y;
			int x1 = x0 + 1 < W ? x0 + 1 : x0;
			int y1 = y0 + 1 < H ? y0 + 1 : y0;
			float tx = X - x0, ty = Y - y0;
			const CRT_FPixel& a = src[y0 * W + x0]; const CRT_FPixel& b = src[y0 * W + x1];
			const CRT_FPixel& c = src[y1 * W + x0]; const CRT_FPixel& d = src[y1 * W + x1];
			acc.r += lerpf(lerpf(a.r, b.r, tx), lerpf(c.r, d.r, tx), ty);
			acc.g += lerpf(lerpf(a.g, b.g, tx), lerpf(c.g, d.g, tx), ty);
			acc.b += lerpf(lerpf(a.b, b.b, tx), lerpf(c.b, d.b, tx), ty);
			acc.a += lerpf(lerpf(a.a, b.a, tx), lerpf(c.a, d.a, tx), ty);
		}
	}
	if (cnt > 0.f) { acc.r /= cnt; acc.g /= cnt; acc.b /= cnt; acc.a /= cnt; }
	return acc;
}

/* Separable box blur (3 passes ≈ gaussian). rx/ry are radii in pixels.
   Each pass is parallel: the horizontal pass over independent rows, the
   vertical pass over independent columns. */
void boxBlur(CRT_FPixel* buf, CRT_FPixel* tmp, int W, int H, float rx, float ry) {
	for (int pass = 0; pass < 3; ++pass) {
		int kx = (int)(rx + 0.5f);
		if (kx > 0) {
			float inv = 1.f / (2 * kx + 1);
			parallelBlocks(H, [=](int ylo, int yhi) {
				for (int y = ylo; y < yhi; ++y) {
					const CRT_FPixel* row = &buf[y * W];
					CRT_FPixel* out = &tmp[y * W];
					for (int x = 0; x < W; ++x) {
						CRT_FPixel s = { 0,0,0,0 };
						for (int k = -kx; k <= kx; ++k) {
							int xx = (int)clampf((float)(x + k), 0, (float)(W - 1));
							const CRT_FPixel& p = row[xx];
							s.r += p.r; s.g += p.g; s.b += p.b; s.a += p.a;
						}
						out[x].r = s.r * inv; out[x].g = s.g * inv; out[x].b = s.b * inv; out[x].a = s.a * inv;
					}
				}
			});
			memcpy(buf, tmp, (size_t)W * H * sizeof(CRT_FPixel));
		}
		int ky = (int)(ry + 0.5f);
		if (ky > 0) {
			float inv = 1.f / (2 * ky + 1);
			parallelBlocks(W, [=](int xlo, int xhi) {
				for (int x = xlo; x < xhi; ++x) {
					for (int y = 0; y < H; ++y) {
						CRT_FPixel s = { 0,0,0,0 };
						for (int k = -ky; k <= ky; ++k) {
							int yy = (int)clampf((float)(y + k), 0, (float)(H - 1));
							const CRT_FPixel& p = buf[yy * W + x];
							s.r += p.r; s.g += p.g; s.b += p.b; s.a += p.a;
						}
						CRT_FPixel* o = &tmp[y * W + x];
						o->r = s.r * inv; o->g = s.g * inv; o->b = s.b * inv; o->a = s.a * inv;
					}
				}
			});
			memcpy(buf, tmp, (size_t)W * H * sizeof(CRT_FPixel));
		}
	}
}

/* Per-pixel phosphor mask multiplier for the given output coordinate. */
CRT_FPixel pixelMask(int x, int y, const CRT_Settings& s) {
	CRT_FPixel m = { 1,1,1,1 };
	if (s.pixel_type == PIX_NONE) return m;

	const float cell = s.pixel_size;
	const float lo   = 1.f - clampf(s.pixel_sharpness, 0.f, 1.f);	/* dim level of "off" subpixel */
	float fx = fmodf((float)x, cell) / cell;						/* 0..1 across a cell */
	float fy = fmodf((float)y, cell) / cell;

	auto stripe = [&](float f)->int { return (int)(f * 3.f) % 3; };	/* 0=R 1=G 2=B */

	switch (s.pixel_type) {
		case PIX_RGB_TRIAD: {
			int sub = stripe(fx);
			m.r = (sub == 0) ? 1.f : lo; m.g = (sub == 1) ? 1.f : lo; m.b = (sub == 2) ? 1.f : lo;
			float gap = (fy > 0.85f) ? lo : 1.f;	/* thin horizontal grille gap */
			m.r *= gap; m.g *= gap; m.b *= gap;
		} break;
		case PIX_APERTURE_GRILLE: {
			int sub = stripe(fx);
			m.r = (sub == 0) ? 1.f : lo; m.g = (sub == 1) ? 1.f : lo; m.b = (sub == 2) ? 1.f : lo;
		} break;
		case PIX_SLOT_MASK: {
			int rowShift = ((int)(y / cell) & 1) ? 1 : 0;			/* brick offset */
			int sub = stripe(fx + rowShift * 0.5f / 3.f);
			m.r = (sub == 0) ? 1.f : lo; m.g = (sub == 1) ? 1.f : lo; m.b = (sub == 2) ? 1.f : lo;
			float gap = (fy > 0.8f) ? lo : 1.f;
			m.r *= gap; m.g *= gap; m.b *= gap;
		} break;
		case PIX_GRID_LED: {
			float dx = fx - 0.5f, dy = fy - 0.5f;
			float d = sqrtf(dx * dx + dy * dy) * 2.f;				/* 0 center .. ~1 edge */
			float lit = clampf(1.f - (d - (1.f - s.pixel_sharpness)) * 4.f, lo, 1.f);
			m.r = m.g = m.b = lit;
		} break;
	}
	m.r *= s.pixel_brightness; m.g *= s.pixel_brightness; m.b *= s.pixel_brightness;
	return m;
}

} // namespace

void CRT_RenderImage(CRT_FPixel* buf, const CRT_Settings& s) {
	const int W = s.width, H = s.height;
	if (W <= 0 || H <= 0) return;
	const size_t N = (size_t)W * H;
	const int Ni = (int)N;

	/* 0. input gamma + optional linearisation */
	const float invGamma = (s.input_gamma > 0.001f) ? (1.f / s.input_gamma) : 1.f;
	parallelBlocks(Ni, [&](int lo, int hi) {
		for (int i = lo; i < hi; ++i) {
			buf[i].r = powf(clampf(buf[i].r, 0.f, 4.f), invGamma);
			buf[i].g = powf(clampf(buf[i].g, 0.f, 4.f), invGamma);
			buf[i].b = powf(clampf(buf[i].b, 0.f, 4.f), invGamma);
			if (s.linear_workflow) {
				buf[i].r = srgb_to_lin(buf[i].r); buf[i].g = srgb_to_lin(buf[i].g); buf[i].b = srgb_to_lin(buf[i].b);
			}
		}
	});

	/* scratch buffers */
	CRT_FPixel* src  = new (std::nothrow) CRT_FPixel[N];
	CRT_FPixel* tmp  = new (std::nothrow) CRT_FPixel[N];
	if (!src || !tmp) { delete[] src; delete[] tmp; return; }

	/* 1+2+3. Macro-lens bulge: convex sphere + zoom into the bulged part + pan
	   (Bulge Center) + chromatic aberration + radial depth-of-field (Blurring),
	   all in one resample. Out-of-bounds samples read black so the curved-away
	   edges don't stretch. */
	memcpy(src, buf, N * sizeof(CRT_FPixel));
	const float halfW = W * 0.5f, halfH = H * 0.5f;
	const float hmin  = halfW < halfH ? halfW : halfH;			/* round bulge */
	const float k     = s.bulge_strength * 0.6f;				/* sphere curvature */
	/* Zoom enough that even the (most-curved) corners sample inside the image,
	   so the frame stays filled — no black corners — with headroom left to pan. */
	const float r2corner    = (halfW * halfW + halfH * halfH) / (hmin * hmin);
	const float curveCorner = 1.f + k * r2corner;
	const float zoom = 1.f + (curveCorner - 1.f) * 1.3f + s.bulge_strength * 0.5f;
	const float mx = halfW * curveCorner / zoom, my = halfH * curveCorner / zoom;
	const float cxp = clampf(s.bulge_cx * W, mx, W - mx);		/* panned focus */
	const float cyp = clampf(s.bulge_cy * H, my, H - my);
	const float vshift = s.vertical_hold > 0.f ? sinf(s.time_secs * 2.4f) * s.vertical_hold * H * 0.25f : 0.f;
	const float ax = cosf(s.aberr_angle), ay = sinf(s.aberr_angle);
	const float hbf = clampf(s.blur_hbias, 0.f, 1.f);

	if (s.bulge_strength > 0.0005f || s.aberr_amount > 0.01f || s.blur_amount > 0.01f || vshift != 0.f) {
		parallelBlocks(H, [&](int ylo, int yhi) {
			for (int y = ylo; y < yhi; ++y) {
				for (int x = 0; x < W; ++x) {
					float nx = (x - halfW) / hmin, ny = (y - halfH) / hmin;
					float r2 = nx * nx + ny * ny;
					float curve = 1.f + k * r2;							/* convex: edges curve away */
					float sx = cxp + nx * curve / zoom * hmin;
					float sy = cyp + ny * curve / zoom * hmin + vshift;
					/* depth of field: sharp at the focus centre, blurrier toward the
					   edges (which recede from the lens), scaled by Blur Amount. */
					float dof = s.blur_amount * r2 * 0.5f;
					float rx = dof * (0.5f + hbf), ry = dof * (1.5f - hbf);
					CRT_FPixel* d = &buf[y * W + x];
					if (s.aberr_amount > 0.01f) {
						float ox = ax * s.aberr_amount, oy = ay * s.aberr_amount;
						d->r = sampleBoxOOB(src, W, H, sx + ox, sy + oy, rx, ry).r;
						CRT_FPixel g = sampleBoxOOB(src, W, H, sx, sy, rx, ry);
						d->g = g.g; d->a = g.a;
						d->b = sampleBoxOOB(src, W, H, sx - ox, sy - oy, rx, ry).b;
					} else {
						*d = sampleBoxOOB(src, W, H, sx, sy, rx, ry);
					}
				}
			}
		});
	}

	/* 4. glow prepass: bright-pass -> blur -> saturate */
	CRT_FPixel* glow = new (std::nothrow) CRT_FPixel[N];
	bool haveGlow = glow && s.glow_intensity > 0.001f && s.glow_radius > 0.5f;
	if (haveGlow) {
		float thr = s.glow_threshold;
		parallelBlocks(Ni, [&](int lo, int hi) {
			for (int i = lo; i < hi; ++i) {
				float l = luma(buf[i]);
				float bp = (l > thr) ? (l - thr) / (1.f - thr + 1e-4f) : 0.f;
				glow[i].r = buf[i].r * bp; glow[i].g = buf[i].g * bp; glow[i].b = buf[i].b * bp; glow[i].a = 0;
			}
		});
		boxBlur(glow, tmp, W, H, s.glow_radius, s.glow_radius);
		parallelBlocks(Ni, [&](int lo, int hi) {	/* glow saturation */
			for (int i = lo; i < hi; ++i) {
				float gl = luma(glow[i]);
				glow[i].r = lerpf(gl, glow[i].r, s.glow_saturation);
				glow[i].g = lerpf(gl, glow[i].g, s.glow_saturation);
				glow[i].b = lerpf(gl, glow[i].b, s.glow_saturation);
			}
		});
	}

	/* 5-13. per-pixel screen + grade */
	const float flick = (s.flicker_intensity > 0.f)
		? 1.f - s.flicker_intensity * hash2((int)(s.time_secs * 60.f), 17) * 0.6f : 1.f;
	const float scanRoll = s.time_secs * s.scanlines_speed * 8.f;

	parallelBlocks(H, [&](int ylo, int yhi) {
		for (int y = ylo; y < yhi; ++y) {
			/* scanline modulation for this row */
			float sc = 0.5f + 0.5f * sinf(6.2831853f * ((y + scanRoll) / 3.f));
			float scanMul = 1.f - s.scanlines_intensity * (1.f - sc);

			for (int x = 0; x < W; ++x) {
				size_t i = (size_t)y * W + x;
				CRT_FPixel c = buf[i];

				/* scanlines + flicker */
				c.r *= scanMul * flick; c.g *= scanMul * flick; c.b *= scanMul * flick;

				/* phosphor mask */
				CRT_FPixel m = pixelMask(x, y, s);
				c.r *= m.r; c.g *= m.g; c.b *= m.b;

				/* noise / static */
				if (s.noise > 0.f) {
					float n = (hash2(x * 7 + 1, y * 13 + (int)(s.time_secs * 60.f)) - 0.5f) * s.noise;
					c.r += n; c.g += n; c.b += n;
				}

				/* posterize */
				if (s.posterize_levels >= 2.f && s.posterize_levels < 256.f) {
					float L = s.posterize_levels - 1.f;
					c.r = floorf(clampf(c.r,0,1) * L + 0.5f) / L;
					c.g = floorf(clampf(c.g,0,1) * L + 0.5f) / L;
					c.b = floorf(clampf(c.b,0,1) * L + 0.5f) / L;
				}

				/* add glow */
				if (haveGlow) {
					c.r += glow[i].r * s.glow_intensity;
					c.g += glow[i].g * s.glow_intensity;
					c.b += glow[i].b * s.glow_intensity;
				}

				/* bulb reflections: vignette + soft top highlight */
				if (s.bulb_reflections > 0.f) {
					float nx = (x - W * 0.5f) / (W * 0.5f), ny = (y - H * 0.5f) / (H * 0.5f);
					float rr = nx * nx + ny * ny;
					float vign = 1.f - s.bulb_reflections * 0.6f * rr;
					float hx = nx, hy = ny + 0.5f;
					float hl = s.bulb_reflections * 0.25f * expf(-(hx * hx + hy * hy) * 3.f);
					c.r = c.r * vign + hl; c.g = c.g * vign + hl; c.b = c.b * vign + hl;
				}

				/* screen tint */
				c.r *= s.tint_r; c.g *= s.tint_g; c.b *= s.tint_b;

				/* boost saturation */
				if (s.boost_saturation != 1.f) {
					float g = luma(c);
					c.r = lerpf(g, c.r, s.boost_saturation);
					c.g = lerpf(g, c.g, s.boost_saturation);
					c.b = lerpf(g, c.b, s.boost_saturation);
				}

				/* brightness */
				c.r *= s.brightness; c.g *= s.brightness; c.b *= s.brightness;

				buf[i] = c;
			}
		}
	});

	/* 12b. auto exposure (global) */
	if (s.auto_exposure) {
		double sum = 0; for (size_t i = 0; i < N; ++i) sum += luma(buf[i]);
		float mean = (float)(sum / N);
		if (mean > 1e-4f) {
			float f = clampf(0.45f / mean, 0.5f, 2.0f);
			parallelBlocks(Ni, [&](int lo, int hi) {
				for (int i = lo; i < hi; ++i) { buf[i].r *= f; buf[i].g *= f; buf[i].b *= f; }
			});
		}
	}

	/* 13. tonemapping + linear->sRGB */
	parallelBlocks(Ni, [&](int lo, int hi) {
		for (int i = lo; i < hi; ++i) {
			if (s.tonemapping > 0.f) {
				float t = s.tonemapping;
				buf[i].r = lerpf(buf[i].r, buf[i].r / (1.f + buf[i].r), t);
				buf[i].g = lerpf(buf[i].g, buf[i].g / (1.f + buf[i].g), t);
				buf[i].b = lerpf(buf[i].b, buf[i].b / (1.f + buf[i].b), t);
			}
			if (s.linear_workflow) {
				buf[i].r = lin_to_srgb(clampf(buf[i].r,0,4)); buf[i].g = lin_to_srgb(clampf(buf[i].g,0,4)); buf[i].b = lin_to_srgb(clampf(buf[i].b,0,4));
			}
		}
	});

	delete[] src; delete[] tmp; delete[] glow;
}
