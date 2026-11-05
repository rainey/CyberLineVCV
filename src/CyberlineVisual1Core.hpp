// CyberlineVisual1Core — audio→image visualizer DSP (rack-free, testable
// like HeartCore). Spec: docs/spec.md §5e.
//
// The image is treated as a grid of 8x8 JPEG-style DCT blocks. The audio
// FFT magnitude spectrum (63 log-spaced bands, 35 Hz..~16 kHz) is injected
// into the blocks' 2D DCT coefficients by zigzag position — audio bin k
// drives the AC coefficient at zigzag slot k+1 — then every block is
// inverse-DCTed into RGBA pixels. "Frequency domain audio → frequency
// domain image", literally.
//
// Modes:
//   MOSAIC — every block gets the same spectrum: a pure audio→image map.
//   PHASE  — per-block zigzag band offsets + deterministic ±1 coefficient
//            signs: the same spectrum scrambles into evolving mosaics.
//   PERTURB — a loaded image (decoded once, DCTed into coefficient space)
//            is multiplied band-by-band by the spectrum: the picture
//            breathes/deforms with the audio, and relaxes back to the
//            original when the input goes silent.
//
// Stereo (L+R): SUM averages the spectra; INTLV feeds even zigzag slots
// from L and odd from R; SPLIT drives the left/right image halves.
//
// Frame plumbing: double-buffered RGBA + atomics (audio thread renders,
// widget thread blits via nvgUpdateImage). All render work happens on hop
// boundaries in processSample(); the widget never allocates or computes.
//
// Suite conventions: NaN guards, state floors, no hidden constants. The
// core is rack-free so tests/render_visual1.cpp can exercise the mapping
// headlessly.

#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <atomic>

namespace cyberline {

struct CyberlineVisual1Core {
	// ---- geometry ----
	static const int FFT_N = 2048;
	static const int FFT_H = FFT_N / 2;
	static const int HOP = 512;
	static const int IMG_W = 256;
	static const int IMG_H = 192;
	static const int BX = IMG_W / 8;   // 32 blocks across
	static const int BY = IMG_H / 8;   // 24 blocks down
	static const int NBLOCKS = BX * BY; // 768
	static const int BANDS = 63;        // AC zigzag slots (band k -> slot k+1)

	enum Mode { MOSAIC = 0, PHASE = 1, PERTURB = 2 };
	enum Stereo { ST_SUM = 0, ST_INTLV = 1, ST_SPLIT = 2 };

	struct Params {
		float gain = 0.5f;    // 0..1 input sensitivity
		float band = 0.65f;   // 0..1 spectral spread (zigzag reach)
		float decay = 0.4f;   // 0..1 band envelope release
		int mode = MOSAIC;
		int stereo = ST_SUM;
	};

	Params params;
	float sampleRate = 44100.f;

	// ---- audio window ring (L/R) ----
	float winL[FFT_N];
	float winR[FFT_N];
	int winPos = 0;

	// ---- FFT work area ----
	float hann[FFT_N];
	float twCos[FFT_H];
	float twSin[FFT_H];
	uint16_t bitrev[FFT_N];
	float fre[FFT_N];
	float fim[FFT_N];

	// ---- spectrum / envelopes ----
	int bandBin[BANDS + 1];   // bin edges per band
	float env[BANDS];         // combined band envelope 0..1
	float envL[BANDS];
	float envR[BANDS];
	float centroid = 0.5f;    // smoothed spectral centroid 0..1 (palette hue)
	float activity = 0.f;     // mean envelope (panel light)

	// ---- perturb source (RGB planes, DCT coefficient domain) ----
	float srcC[3][NBLOCKS][64];
	bool hasSource = false;

	// ---- output frames (RGBA double buffer) ----
	std::atomic<int> frameIdx;   // index of the COMPLETE frame for the widget
	std::atomic<int> frameSeq;   // increments per rendered frame (FRAME trig)
	std::atomic<int> activity1000;
	uint8_t frames[2][IMG_W * IMG_H * 4];
	int writeIdx = 1;

	// ---- 8x8 DCT basis ----
	float dctM[8][8]; // M[x][u] = a(u) cos((2x+1) u pi / 16), orthonormal

	// zigzag: zigzag index -> row-major raster index (v*8+u), standard JPEG
	static const uint8_t* zigzag() {
		static const uint8_t t[64] = {
			0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
			12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
			35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
			58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
		};
		return t;
	}

	CyberlineVisual1Core() : frameIdx(0), frameSeq(0), activity1000(0) {
		init();
	}

	void init() {
		// Hann window
		for (int i = 0; i < FFT_N; i++)
			hann[i] = 0.5f * (1.f - std::cos(2.f * M_PI * i / FFT_N));
		// Twiddles for forward FFT (e^-j2pi k / N)
		for (int k = 0; k < FFT_H; k++) {
			float a = -2.f * (float) M_PI * k / FFT_N;
			twCos[k] = std::cos(a);
			twSin[k] = std::sin(a);
		}
		// Bit-reversal permutation
		for (int i = 0; i < FFT_N; i++) {
			unsigned r = 0, x = (unsigned) i;
			for (int b = 0; b < 11; b++) { // log2(2048)
				r = (r << 1) | (x & 1);
				x >>= 1;
			}
			bitrev[i] = (uint16_t) r;
		}
		// DCT basis
		for (int x = 0; x < 8; x++) {
			for (int u = 0; u < 8; u++) {
				float au = (u == 0) ? std::sqrt(1.f / 8.f) : std::sqrt(2.f / 8.f);
				dctM[x][u] = au * std::cos((2 * x + 1) * u * (float) M_PI / 16.f);
			}
		}
		// Band edges: log-spaced 35 Hz .. 16 kHz (capped below Nyquist)
		setSampleRate(sampleRate);
		// Neutral startup frame: dark cyan-gray
		for (int f = 0; f < 2; f++) {
			uint8_t* px = frames[f];
			for (int i = 0; i < IMG_W * IMG_H; i++) {
				px[i * 4 + 0] = 10; px[i * 4 + 1] = 14; px[i * 4 + 2] = 18; px[i * 4 + 3] = 255;
			}
		}
		writeIdx = 1;
		frameIdx = 0;
	}

	void setSampleRate(float sr) {
		sampleRate = sr;
		float fLo = 35.f;
		float fHi = std::fmin(16000.f, 0.44f * sr);
		for (int k = 0; k <= BANDS; k++) {
			float f = fLo * std::pow(fHi / fLo, k / (float) BANDS);
			int bin = (int) std::lround(f / sr * FFT_N);
			if (k > 0 && bin <= bandBin[k - 1])
				bin = bandBin[k - 1] + 1;
			if (bin > FFT_H) bin = FFT_H;
			bandBin[k] = bin;
		}
	}

	void reset() {
		winPos = 0;
		for (int i = 0; i < FFT_N; i++) { winL[i] = 0.f; winR[i] = 0.f; }
		for (int k = 0; k < BANDS; k++) { env[k] = 0.f; envL[k] = 0.f; envR[k] = 0.f; }
		centroid = 0.5f;
		activity = 0.f;
		activity1000 = 0;
		frameSeq = 0;
	}

	// ---- main entry: one audio sample pair ----
	void processSample(float l, float r) {
		if (!std::isfinite(l)) l = 0.f;
		if (!std::isfinite(r)) r = 0.f;
		winL[winPos] = l;
		winR[winPos] = r;
		winPos = (winPos + 1) & (FFT_N - 1);
		if ((winPos & (HOP - 1)) == 0)
			analyze();
	}

	// ---- analysis + render on hop boundaries ----
	void analyze() {
		// Input sensitivity: map gain 0..1 onto a magnitude multiplier.
		float g = 3.f + 37.f * params.gain;

		analyzeChannel(winL, g, envL);
		analyzeChannel(winR, g, envR);

		for (int k = 0; k < BANDS; k++) {
			if (params.stereo == ST_SUM)
				env[k] = 0.5f * (envL[k] + envR[k]);
			else
				env[k] = std::fmax(envL[k], envR[k]);
		}

		// Spectral centroid (over band envelopes) -> smoothed palette hue
		float wsum = 0.f, ksum = 0.f;
		for (int k = 0; k < BANDS; k++) {
			wsum += env[k];
			ksum += env[k] * k;
		}
		float target = (wsum > 1e-5f) ? ksum / wsum / (float) BANDS : 0.5f;
		centroid += 0.25f * (target - centroid);

		float a = 0.f;
		for (int k = 0; k < BANDS; k++)
			a += env[k];
		activity = a / BANDS;
		activity1000 = (int) (activity * 1000.f);

		renderFrame();
	}

	// Windowed FFT of one channel ring + per-band envelope update.
	void analyzeChannel(const float* win, float g, float* outEnv) {
		// Load window (winPos points at the OLDEST sample)
		for (int i = 0; i < FFT_N; i++) {
			float x = win[(winPos + i) & (FFT_N - 1)] * hann[i];
			int j = bitrev[i];
			fre[j] = x;
			fim[j] = 0.f;
		}
		// Iterative radix-2 in-place FFT
		for (int len = 2; len <= FFT_N; len <<= 1) {
			int half = len >> 1;
			int step = FFT_N / len;
			for (int i = 0; i < FFT_N; i += len) {
				for (int k = 0; k < half; k++) {
					float wr = twCos[k * step];
					float wi = twSin[k * step];
					int a0 = i + k;
					int b0 = a0 + half;
					float xr = fre[b0] * wr - fim[b0] * wi;
					float xi = fre[b0] * wi + fim[b0] * wr;
					fre[b0] = fre[a0] - xr;
					fim[b0] = fim[a0] - xi;
					fre[a0] += xr;
					fim[a0] += xi;
				}
			}
		}
		// Band magnitudes
		float dt = HOP / sampleRate;
		float tau = 0.05f + 4.f * params.decay * params.decay; // s
		float rel = std::exp(-dt / tau);
		for (int k = 0; k < BANDS; k++) {
			float m = 0.f;
			for (int bin = bandBin[k]; bin < bandBin[k + 1]; bin++) {
				float re2 = fre[bin] * fre[bin] + fim[bin] * fim[bin];
				m += std::sqrt(re2);
			}
			m *= 2.f / FFT_N;
			// dB map: full-scale sine (~0.5 amplitude) lands near 0.7-0.8
			float v = (20.f * std::log10(m * g + 1e-7f) + 66.f) / 48.f;
			v = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
			outEnv[k] = std::fmax(v, outEnv[k] * rel);
		}
	}

	// Zigzag slot j (0..63) -> (u, v) frequency indices
	static inline void zzUV(int j, int& u, int& v) {
		int r = zigzag()[j];
		u = r & 7;
		v = r >> 3;
	}

	// Deterministic per-block/sign hash for PHASE mode
	static inline int blockSign(int bx, int by, int j) {
		unsigned h = (unsigned) ((bx + 128) * 73856093u)
			^ (unsigned) ((by + 128) * 19349663u)
			^ (unsigned) ((j + 128) * 83492791u);
		h ^= h >> 13;
		h *= 0x5bd1e995u;
		h ^= h >> 15;
		return (int) (h & 1u) * 2 - 1;
	}

	// Band envelope for slot j at block (bx, by), honoring stereo mode and
	// the BAND knob reach. `rot` shifts the band window per block (spatial
	// structure); 0 = plain mapping.
	inline float slotEnv(int j, int bx, int by, int rot = 0) const {
		int reach = 4 + (int) (params.band * 59.f); // 4..63
		int kk;
		if (rot != 0)
			kk = (j - 1 + rot) % reach;
		else if (params.mode == PHASE)
			kk = (j - 1 + by * 11 + bx * 7) % reach;
		else
			kk = (j - 1) % reach;
		if (kk < 0) kk += reach;
		float l = envL[kk], r = envR[kk];
		switch (params.stereo) {
			case ST_SUM:   return 0.5f * (l + r);
			case ST_INTLV: return (j & 1) ? r : l;
			default:       return (bx < BX / 2) ? l : r; // SPLIT
		}
	}

	// Inverse 2D DCT of an 8x8 coefficient block (row-major [v][u]) into
	// pixels f[y][x]. M is orthonormal: f = M^T C M.
	void idct8(const float* C, float* f) const {
		float t[8][8];
		for (int y = 0; y < 8; y++) {
			for (int u = 0; u < 8; u++) {
				float s = 0.f;
				for (int v = 0; v < 8; v++)
					s += C[v * 8 + u] * dctM[y][v];
				t[y][u] = s;
			}
		}
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				float s = 0.f;
				for (int u = 0; u < 8; u++)
					s += t[y][u] * dctM[x][u];
				f[y * 8 + x] = s;
			}
		}
	}

	// Forward 2D DCT of an 8x8 pixel block (row-major [y][x]) into
	// coefficients C[v][u]. C = M^T f M.
	void fdct8(const float* f, float* C) const {
		float t[8][8];
		for (int v = 0; v < 8; v++) {
			for (int x = 0; x < 8; x++) {
				float s = 0.f;
				for (int y = 0; y < 8; y++)
					s += f[y * 8 + x] * dctM[y][v];
				t[v][x] = s;
			}
		}
		for (int v = 0; v < 8; v++) {
			for (int u = 0; u < 8; u++) {
				float s = 0.f;
				for (int x = 0; x < 8; x++)
					s += t[v][x] * dctM[x][u];
				C[v * 8 + u] = s;
			}
		}
	}

	// Palette hue: cyan (bass) -> violet (mid) -> coral (treble)
	static void paletteColor(float t, float& r, float& g, float& b) {
		t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
		float r0 = 0.25f, g0 = 0.88f, b0 = 1.00f;   // neon cyan
		float r1 = 0.78f, g1 = 0.63f, b1 = 0.97f;   // neon violet
		float r2 = 1.00f, g2 = 0.35f, b2 = 0.39f;   // neon coral
		if (t < 0.5f) {
			float s = t * 2.f;
			r = r0 + (r1 - r0) * s;
			g = g0 + (g1 - g0) * s;
			b = b0 + (b1 - b0) * s;
		} else {
			float s = (t - 0.5f) * 2.f;
			r = r1 + (r2 - r1) * s;
			g = g1 + (g2 - g1) * s;
			b = b1 + (b2 - b1) * s;
		}
	}

	void renderFrame() {
		uint8_t* out = frames[writeIdx];
		const float amp = 0.55f; // overall coefficient amplitude

		if (params.mode == PERTURB && hasSource) {
			renderPerturb(out);
		} else {
			renderSynthetic(out, amp);
		}

		frameSeq = frameSeq + 1;
		frameIdx = writeIdx;
		writeIdx ^= 1;
	}

	// MOSAIC / PHASE: grayscale DCT mosaic, palette-tinted. SPLIT stereo
	// tints the left half cyan and the right half coral. Per-block smooth
	// macro-modulation + directional band rotation give the mosaic
	// large-scale structure (otherwise all blocks are identical).
	void renderSynthetic(uint8_t* out, float amp) {
		float pr, pg, pb;
		paletteColor(centroid, pr, pg, pb);
		for (int by2 = 0; by2 < BY; by2++) {
			for (int bx2 = 0; bx2 < BX; bx2++) {
				// smooth macro window over block position (two slow cosines
				// with hashed phases) — varies luminance/texture across the
				// image without hard edges
				float ph1 = 0.7f + 0.3f * blockSign(bx2, 3, 5) * 0.5f;
				float ph2 = 1.1f + 0.4f * blockSign(7, by2, 9) * 0.5f;
				float macro = 1.f
					+ 0.38f * std::cos(2.f * (float) M_PI * bx2 / BX * 2.f + ph1)
					+ 0.30f * std::cos(2.f * (float) M_PI * by2 / BY * 2.f + ph2);
				macro = clampf(macro, 0.15f, 1.9f);
				// directional band rotation: neighbors see shifted spectra,
				// producing wave-like structure across rows/columns
				int rot = (by2 * 11 + bx2 * 7) % 63;
				int b = by2 * BX + bx2;
				float C[64];
				for (int j = 0; j < 64; j++) C[j] = 0.f;
				float dc = 0.5f * (envL[0] + envR[0]);
				C[0] = 0.25f * dc * macro;
				for (int j = 1; j < 64; j++) {
					float e = slotEnv(j, bx2, by2, rot);
					if (e <= 0.001f) continue;
					float c = e * amp * macro;
					if (params.mode == PHASE && (j & 1))
						c *= blockSign(bx2, by2, j);
					int u, v;
					zzUV(j, u, v);
					C[v * 8 + u] = c;
				}
				float f[64];
				idct8(C, f);
				// palette for this side
				float cr, cg, cb;
				if (params.stereo == ST_SPLIT) {
					if (bx2 < BX / 2) { cr = 0.25f; cg = 0.88f; cb = 1.0f; }
					else              { cr = 1.0f;  cg = 0.42f; cb = 0.46f; }
				} else {
					cr = pr; cg = pg; cb = pb;
				}
				for (int y = 0; y < 8; y++) {
					int py = by2 * 8 + y;
					for (int x = 0; x < 8; x++) {
						int pxx = bx2 * 8 + x;
						float gray = f[y * 8 + x];
						gray = gray < -1.f ? -1.f : (gray > 1.f ? 1.f : gray);
						float lum = 0.14f + 0.86f * (0.5f + 0.5f * gray);
						uint8_t* px = out + (py * IMG_W + pxx) * 4;
						px[0] = (uint8_t) (clampf(cr * lum, 0.f, 1.f) * 255.f);
						px[1] = (uint8_t) (clampf(cg * lum, 0.f, 1.f) * 255.f);
						px[2] = (uint8_t) (clampf(cb * lum, 0.f, 1.f) * 255.f);
						px[3] = 255;
					}
				}
			}
		}
	}

	// PERTURB: loaded image coefficients multiplied band-by-band by the
	// spectrum. Silence returns the original image (factor 1).
	void renderPerturb(uint8_t* out) {
		for (int by2 = 0; by2 < BY; by2++) {
			for (int bx2 = 0; bx2 < BX; bx2++) {
				int b = by2 * BX + bx2;
				float C[3][64];
				float F[3][64];
				for (int p = 0; p < 3; p++) {
					for (int j = 0; j < 64; j++) {
						int u, v;
						zzUV(j, u, v);
						float base = srcC[p][b][j];
						if (!std::isfinite(base)) base = 0.f;
						if (j == 0) {
							// DC breathes with overall loudness but never collapses
							float e = 0.5f * (envL[0] + envR[0]);
							C[p][0] = base * (1.f + 0.6f * e);
						} else {
							// Multiplicative breathing of existing image content,
							// plus a small hashed additive shimmer so even smooth
							// (low-AC) images visibly ripple with the spectrum.
							float e = slotEnv(j, bx2, by2);
							float mult = base * (1.f + 1.2f * e);
							float add = 0.30f * e * blockSign(bx2, by2, j);
							C[p][v * 8 + u] = mult + add;
						}
					}
					idct8(C[p], F[p]);
				}
				for (int y = 0; y < 8; y++) {
					int py = by2 * 8 + y;
					for (int x = 0; x < 8; x++) {
						int pxx = bx2 * 8 + x;
						uint8_t* px = out + (py * IMG_W + pxx) * 4;
						px[0] = (uint8_t) clampi((int) (128.f + 128.f * F[0][y * 8 + x]), 0, 255);
						px[1] = (uint8_t) clampi((int) (128.f + 128.f * F[1][y * 8 + x]), 0, 255);
						px[2] = (uint8_t) clampi((int) (128.f + 128.f * F[2][y * 8 + x]), 0, 255);
						px[3] = 255;
					}
				}
			}
		}
	}

	static inline float clampf(float x, float lo, float hi) {
		return x < lo ? lo : (x > hi ? hi : x);
	}
	static inline int clampi(int x, int lo, int hi) {
		return x < lo ? lo : (x > hi ? hi : x);
	}

	// ---- source image load (perturb mode) ----
	// rgb: tightly packed 3-channel pixels, any size; box-averaged into the
	// 256x192 block grid, level-shifted and DCTed into coefficient space.
	void loadSourcePixels(const uint8_t* rgb, int w, int h) {
		if (!rgb || w <= 0 || h <= 0)
			return;
		for (int by2 = 0; by2 < BY; by2++) {
			for (int bx2 = 0; bx2 < BX; bx2++) {
				int b = by2 * BX + bx2;
				float f[3][64];
				// box-average each target pixel
				for (int y = 0; y < 8; y++) {
					for (int x = 0; x < 8; x++) {
						int tx = bx2 * 8 + x;
						int ty = by2 * 8 + y;
						int x0 = (int) ((int64_t) tx * w / IMG_W);
						int x1 = (int) ((int64_t) (tx + 1) * w / IMG_W);
						int y0 = (int) ((int64_t) ty * h / IMG_H);
						int y1 = (int) ((int64_t) (ty + 1) * h / IMG_H);
						if (x1 <= x0) x1 = x0 + 1;
						if (y1 <= y0) y1 = y0 + 1;
						float acc[3] = {0.f, 0.f, 0.f};
						int n = 0;
						for (int sy = y0; sy < y1 && sy < h; sy++) {
							for (int sx = x0; sx < x1 && sx < w; sx++) {
								const uint8_t* s = rgb + (sy * w + sx) * 3;
								acc[0] += s[0]; acc[1] += s[1]; acc[2] += s[2];
								n++;
							}
						}
						if (n == 0) n = 1;
						for (int p = 0; p < 3; p++)
							f[p][y * 8 + x] = acc[p] / n / 128.f - 1.f;
					}
				}
				for (int p = 0; p < 3; p++)
					fdct8(f[p], srcC[p][b]);
			}
		}
		hasSource = true;
		frameSeq = frameSeq + 1; // widget picks the new frame on next render
	}

	// Widget-side access: pointer to the most recent complete frame.
	const uint8_t* frameRGBA() const {
		return frames[frameIdx.load(std::memory_order_acquire)];
	}
};

} // namespace cyberline
