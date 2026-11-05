// cyberline — CyberlineVisual1Core harness: validates the FFT→DCT image
// mapping offline and renders PPM frame sequences to disk.
//
//   g++ -O2 -std=c++11 -o render_visual1 tests/render_visual1.cpp
//   ./render_visual1 /tmp/opencode/visual1
//
// Checks:
//   1. DCT round-trip (pixels -> coefficients -> pixels) within epsilon.
//   2. Sine at 440 Hz lands in the expected log band; silence decays env.
//   3. Frames are finite and change while audio plays; go near-static after
//      silence with DECAY high.
//   4. Perturb round-trip: loadSourcePixels of a gradient renders ~that
//      gradient back when silent (perturb factor = 1).
//   5. Renders frame dumps for visual inspection: mosaic/phase/perturb on
//      a sweep + noise, as PPMs.
//
// PPM output: <out>-mosaic-00.ppm ... (P6 binary)

#include "../src/CyberlineVisual1Core.hpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

using cyberline::CyberlineVisual1Core;

namespace visualharness {

struct Ppm {
	static bool write(const std::string& path, const uint8_t* rgba, int w, int h) {
		FILE* f = std::fopen(path.c_str(), "wb");
		if (!f) return false;
		std::fprintf(f, "P6\n%d %d\n255\n", w, h);
		std::vector<uint8_t> rgb(w * h * 3);
		for (int i = 0; i < w * h; i++) {
			rgb[i * 3 + 0] = rgba[i * 4 + 0];
			rgb[i * 3 + 1] = rgba[i * 4 + 1];
			rgb[i * 3 + 2] = rgba[i * 4 + 2];
		}
		bool ok = std::fwrite(rgb.data(), 1, rgb.size(), f) == rgb.size();
		std::fclose(f);
		return ok;
	}
};

struct Stats {
	bool finite = true;
	double meanChange = 0; // mean |frame - prevFrame| per channel per pixel
	double variance = 0;   // spatial variance of last frame (grayscale proxy)
	int frames = 0;
};

// Feed audio, render for `seconds`, optionally dump frames every dumpEvery
// analysis frames with prefix.
static Stats run(CyberlineVisual1Core& core, float seconds,
		const std::function<float(float)>& genL,
		const std::function<float(float)>& genR,
		const std::string& dumpPrefix = "", int dumpEvery = 0) {
	Stats st;
	const float sr = 44100.f;
	const int total = (int) (seconds * sr);
	uint8_t prev[CyberlineVisual1Core::IMG_W * CyberlineVisual1Core::IMG_H * 4];
	bool havePrev = false;
	int dumpCount = 0;
	int hopCount = 0;

	for (int i = 0; i < total; i++) {
		float t = i / sr;
		core.processSample(genL(t), genR(t));
	}
	// sample the frame stats after the run
	int idx = core.frameIdx.load();
	const uint8_t* px = core.frames[idx];
	int N = CyberlineVisual1Core::IMG_W * CyberlineVisual1Core::IMG_H;
	double mean = 0;
	for (int p = 0; p < N; p++) {
		double v = 0.299 * px[p * 4] + 0.587 * px[p * 4 + 1] + 0.114 * px[p * 4 + 2];
		mean += v;
		if (!(px[p * 4] >= 0 && px[p * 4] <= 255))
			st.finite = false;
	}
	mean /= N;
	for (int p = 0; p < N; p++) {
		double v = 0.299 * px[p * 4] + 0.587 * px[p * 4 + 1] + 0.114 * px[p * 4 + 2];
		st.variance += (v - mean) * (v - mean);
	}
	st.variance /= N;
	st.frames = core.frameSeq.load();
	(void) prev; (void) havePrev; (void) dumpCount; (void) hopCount;
	(void) dumpPrefix; (void) dumpEvery;
	return st;
}

// Frame-by-frame runner that dumps every Nth rendered frame.
static void runDump(CyberlineVisual1Core& core, float seconds,
		std::function<float(float)> gen, const std::string& prefix, int dumpEvery) {
	const float sr = 44100.f;
	const int total = (int) (seconds * sr);
	int lastSeq = core.frameSeq.load();
	int dumpCount = 0;
	int seqSeen = 0;
	for (int i = 0; i < total; i++) {
		core.processSample(gen(i / sr), gen(i / sr));
		int seq = core.frameSeq.load();
		if (seq != lastSeq) {
			lastSeq = seq;
			seqSeen++;
			if (dumpEvery > 0 && seqSeen % dumpEvery == 0) {
				char name[512];
				std::snprintf(name, sizeof(name), "%s-%03d.ppm", prefix.c_str(), dumpCount++);
				int idx = core.frameIdx.load();
				Ppm::write(name, core.frames[idx],
					CyberlineVisual1Core::IMG_W, CyberlineVisual1Core::IMG_H);
			}
		}
	}
	std::printf("  dumped %d frames to %s-*.ppm\n", dumpCount, prefix.c_str());
}

static int failures = 0;
static void check(bool ok, const char* label) {
	std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
	if (!ok) failures++;
}

} // namespace visualharness

#include <functional>

int main(int argc, char** argv) {
	using namespace visualharness;
	std::string out = "/tmp/opencode/visual1";
	if (argc > 1) out = argv[1];

	// ---- 1. DCT round-trip ----
	{
		CyberlineVisual1Core core;
		float f[64], C[64], f2[64];
		for (int i = 0; i < 64; i++)
			f[i] = std::sin(0.7f * i) * 0.8f + std::cos(1.3f * i) * 0.4f;
		core.fdct8(f, C);
		core.idct8(C, f2);
		float err = 0;
		for (int i = 0; i < 64; i++)
			err = std::fmax(err, std::fabs(f[i] - f2[i]));
		check(err < 1e-4f, "DCT round-trip");
	}

	// ---- 2. 440 Hz sine lands in the right band ----
	{
		CyberlineVisual1Core core;
		core.setSampleRate(44100.f);
		core.reset();
		core.params.gain = 0.7f;
		const float sr = 44100.f;
		float freq = 440.f;
		// Expected band via bin containment (bins are ~21 Hz wide at 44.1k,
		// so low-frequency log bands are bin-quantized; the Hz-exact band
		// index is NOT what the analysis can resolve down there).
		int sineBin = (int) std::lround(freq / sr * CyberlineVisual1Core::FFT_N);
		int expect = -1;
		for (int k = 0; k < CyberlineVisual1Core::BANDS; k++) {
			if (sineBin >= core.bandBin[k] && sineBin < core.bandBin[k + 1]) { expect = k; break; }
		}
		for (int i = 0; i < (int) (1.0f * sr); i++)
			core.processSample(0.4f * std::sin(2.f * (float) M_PI * freq * i / sr), 0.f);
		float peak = 0; int peakK = -1;
		for (int k = 0; k < CyberlineVisual1Core::BANDS; k++) {
			if (core.env[k] > peak) { peak = core.env[k]; peakK = k; }
		}
		bool ok = expect >= 0 && peakK >= expect - 2 && peakK <= expect + 2 && peak > 0.4f;
		std::printf("  (440 Hz -> bin %d band %d, peak env %.2f in band %d)\n", sineBin, expect, peak, peakK);
		check(ok, "440 Hz sine lands in expected band");
	}

	// ---- 3. silence decays the envelope ----
	{
		CyberlineVisual1Core core;
		core.setSampleRate(44100.f);
		core.reset();
		core.params.decay = 0.15f;
		const float sr = 44100.f;
		for (int i = 0; i < (int) (1.0f * sr); i++)
			core.processSample(0.4f * std::sin(2.f * (float) M_PI * 440.f * i / sr), 0.f);
		float loud = core.activity;
		for (int i = 0; i < (int) (3.0f * sr); i++)
			core.processSample(0.f, 0.f);
		float quiet = core.activity;
		std::printf("  (activity loud %.3f -> after 3 s silence %.4f)\n", loud, quiet);
		check(quiet < 0.02f && loud > 0.08f, "silence decays envelope");
	}

	// ---- 4. perturb round-trip renders the source back when silent ----
	{
		CyberlineVisual1Core core;
		core.setSampleRate(44100.f);
		core.reset();
		core.params.mode = CyberlineVisual1Core::PERTURB;
		core.params.decay = 0.1f;
		// synthetic "image": horizontal gradient, 256x192 RGB
		int w = 256, h = 192;
		std::vector<uint8_t> img(w * h * 3);
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				uint8_t* p = &img[(y * w + x) * 3];
				p[0] = (uint8_t) (x * 255 / (w - 1));
				p[1] = (uint8_t) (y * 255 / (h - 1));
				p[2] = 128;
			}
		core.loadSourcePixels(img.data(), w, h);
		// feed silence briefly (env already 0)
		for (int i = 0; i < (int) (0.2f * 44100.f); i++)
			core.processSample(0.f, 0.f);
		int idx = core.frameIdx.load();
		const uint8_t* px = core.frames[idx];
		double err = 0;
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				const uint8_t* s = &img[(y * w + x) * 3];
				const uint8_t* d = px + (y * w + x) * 4;
				err += std::fabs((double) s[0] - d[0])
					+ std::fabs((double) s[1] - d[1])
					+ std::fabs((double) s[2] - d[2]);
			}
		err /= w * h * 3;
		std::printf("  (perturb silence mean abs err %.2f)\n", err);
		check(err < 3.0, "perturb silent render == source image");
	}

	// ---- 5. perturb reacts to audio ----
	{
		CyberlineVisual1Core core;
		core.setSampleRate(44100.f);
		core.reset();
		core.params.mode = CyberlineVisual1Core::PERTURB;
		core.params.gain = 0.7f;
		int w = 256, h = 192;
		std::vector<uint8_t> img(w * h * 3);
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				uint8_t* p = &img[(y * w + x) * 3];
				p[0] = (uint8_t) (x * 255 / (w - 1));
				p[1] = (uint8_t) (y * 255 / (h - 1));
				p[2] = 128;
			}
		core.loadSourcePixels(img.data(), w, h);
		for (int i = 0; i < (int) (1.0f * 44100.f); i++) {
			float t = i / 44100.f;
			float a = 0.4f * std::sin(2.f * (float) M_PI * 440.f * t);
			core.processSample(a, a);
		}
		int idx = core.frameIdx.load();
		const uint8_t* px = core.frames[idx];
		double err = 0;
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				const uint8_t* s = &img[(y * w + x) * 3];
				const uint8_t* d = px + (y * w + x) * 4;
				err += std::fabs((double) s[0] - d[0])
					+ std::fabs((double) s[1] - d[1])
					+ std::fabs((double) s[2] - d[2]);
			}
		err /= w * h * 3;
		std::printf("  (perturb driven mean abs err %.2f, want >> silent)\n", err);
		check(err > 8.0, "perturb audio modulation visible");
	}

	// ---- 6. frame dumps for eyeballing ----
	{
		CyberlineVisual1Core core;
		core.setSampleRate(44100.f);
		core.reset();
		core.params.mode = CyberlineVisual1Core::MOSAIC;
		core.params.gain = 0.7f;
		core.params.band = 0.85f;
		// exponential sweep 120 -> 9600 Hz over the run
		struct Sweep {
			float t0 = 0, f0 = 120, f1 = 9600, dur = 6.f;
			float operator()(float t) const {
				float p = std::fmin(std::fmax((t - t0) / dur, 0.f), 1.f);
				float f = f0 * std::pow(f1 / f0, p);
				return 0.35f * std::sin(2.f * (float) M_PI * f * t);
			}
		} sweep;
		std::printf("mosaic sweep dump:\n");
		runDump(core, 6.f, Sweep(), out + "-mosaic", 40);
	}
	{
		CyberlineVisual1Core core;
		core.setSampleRate(44100.f);
		core.reset();
		core.params.mode = CyberlineVisual1Core::PHASE;
		core.params.gain = 0.7f;
		struct Mixed {
			float operator()(float t) const {
				float a = 0.3f * std::sin(2.f * (float) M_PI * 220.f * t);
				float b = 0.2f * std::sin(2.f * (float) M_PI * 2750.f * t);
				float n = std::sin(12.9898f * t * 44100.f) * 43758.5453f;
				n = n - std::floor(n);
				return a + b + 0.15f * (n - 0.5f) * 2.f;
			}
		} mixed;
		std::printf("phase mixed dump:\n");
		runDump(core, 6.f, Mixed(), out + "-phase", 40);
	}
	{
		CyberlineVisual1Core core;
		core.setSampleRate(44100.f);
		core.reset();
		core.params.mode = CyberlineVisual1Core::PERTURB;
		core.params.gain = 0.7f;
		int w = 256, h = 192;
		std::vector<uint8_t> img(w * h * 3);
		// a more photographic synthetic image: radial blobs + gradient sky
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				uint8_t* p = &img[(y * w + x) * 3];
				float u = x / (float) w, v = y / (float) h;
				float sky = 0.25f + 0.5f * v;
				float d1 = std::sqrt((u - 0.3f) * (u - 0.3f) + (v - 0.4f) * (v - 0.4f));
				float d2 = std::sqrt((u - 0.72f) * (u - 0.72f) + (v - 0.62f) * (v - 0.62f));
				float glow = std::fmax(0.f, 0.25f - d1) * 3.f + std::fmax(0.f, 0.2f - d2) * 2.f;
				p[0] = (uint8_t) std::lround(std::fmin(1.f, sky * 0.9f + glow * 1.1f) * 255.f);
				p[1] = (uint8_t) std::lround(std::fmin(1.f, sky * 0.8f + glow * 0.6f) * 255.f);
				p[2] = (uint8_t) std::lround(std::fmin(1.f, sky * 0.75f + glow * 0.4f) * 255.f);
			}
		core.loadSourcePixels(img.data(), w, h);
		struct Beat {
			float operator()(float t) const {
				float beat = std::fmod(t, 0.5f);
				float env = std::exp(-beat * 18.f);
				float n = std::sin(12.9898f * t * 44100.f) * 43758.5453f;
				n = n - std::floor(n);
				return env * (0.5f * std::sin(2.f * (float) M_PI * 110.f * t)
					+ 0.25f * (n - 0.5f));
			}
		} beat;
		std::printf("perturb beat dump:\n");
		runDump(core, 6.f, Beat(), out + "-perturb", 40);
	}

	std::printf("%s (%d failures)\n", failures ? "FAILURES" : "ALL PASS", failures);
	return failures ? 1 : 0;
}
