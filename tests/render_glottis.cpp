// cyberline — DSP verification harness (no Rack dependency).
// Renders CyberlineGlottis to a WAV file for offline inspection.
//
//   g++ -O2 -std=c++11 -o render tests/render_glottis.cpp   (header-only DSP)
//   ./render out.wav 2.0 "t=0.6 p=0.8 f=110 fold=14 vib=6 drift=0.3 fb=0"
//
// Params: t tenseness(0-1), p pressure/amp(0-1), f base F0 (Hz, overrides
// allometry if given), fold fold length mm (sets base F0 by allometry when
// f not given), vib vibrato Hz, drift 0-1, fb constant feedback drive 0-1.

#include "../src/CyberlineGlottis.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <map>

namespace render {

struct WavWriter {
	FILE* f = nullptr;
	uint32_t dataBytes = 0;
	uint32_t sr = 44100;

	bool open(const char* path, uint32_t sampleRate) {
		sr = sampleRate;
		f = std::fopen(path, "wb");
		if (!f) return false;
		std::fputs("RIFF", f);
		std::fseek(f, 4, SEEK_CUR);           // riff size, patched on close
		std::fputs("WAVEfmt ", f);
		uint32_t fmtSize = 16; std::fwrite(&fmtSize, 4, 1, f);
		uint16_t pcm = 1, ch = 1;
		uint16_t bits = 16;
		uint32_t byteRate = sr * ch * bits / 8;
		uint16_t align = ch * bits / 8;
		std::fwrite(&pcm, 2, 1, f);
		std::fwrite(&ch, 2, 1, f);
		std::fwrite(&sr, 4, 1, f);
		std::fwrite(&byteRate, 4, 1, f);
		std::fwrite(&align, 2, 1, f);
		std::fwrite(&bits, 2, 1, f);
		std::fputs("data", f);
		std::fseek(f, 4, SEEK_CUR);           // data size, patched on close
		return true;
	}

	void write(float x) {
		if (x > 1.f) x = 1.f;
		if (x < -1.f) x = -1.f;
		int16_t s = (int16_t)(x * 32767.f);
		std::fwrite(&s, 2, 1, f);
		dataBytes += 2;
	}

	void close() {
		uint32_t riff = dataBytes + 36;
		std::fseek(f, 4, SEEK_SET); std::fwrite(&riff, 4, 1, f);
		std::fseek(f, 40, SEEK_SET); std::fwrite(&dataBytes, 4, 1, f);
		std::fclose(f);
	}
};

// Cheap deterministic white noise (xorshift), so renders are reproducible.
struct Rng {
	uint32_t s = 0x12345678u;
	float uniform() {
		s ^= s << 13; s ^= s >> 17; s ^= s << 5;
		return (float)(s & 0xFFFFFF) / 16777215.f;
	}
};

} // namespace render

int main(int argc, char** argv) {
	if (argc < 3) {
		std::fprintf(stderr, "usage: %s out.wav seconds [k=v ...]\n", argv[0]);
		return 1;
	}
	const char* outPath = argv[1];
	float seconds = std::atof(argv[2]);

	std::map<std::string, float> kv;
	for (int i = 3; i < argc; i++) {
		std::string a = argv[i];
		auto eq = a.find('=');
		if (eq == std::string::npos) continue;
		kv[a.substr(0, eq)] = std::atof(a.substr(eq + 1).c_str());
	}

	float tenseness = kv.count("t") ? kv["t"] : 0.6f;
	float pressure  = kv.count("p") ? kv["p"] : 0.8f;
	float foldLen   = kv.count("fold") ? kv["fold"] : 14.f;
	float vib       = kv.count("vib") ? kv["vib"] : 6.f;
	float drift     = kv.count("drift") ? kv["drift"] : 0.3f;
	float fb        = kv.count("fb") ? kv["fb"] : 0.f;
	float f0 = kv.count("f") ? kv["f"] : -1.f;

	const float sr = 44100.f;
	cyberline::CyberlineGlottis g;
	g.setSampleRate(sr);
	g.reset();

	// Allometric base frequency (same law as the module), then scale by
	// tension (string law) and explicit f override if provided.
	float allo = std::pow(10.f, 4.01f - 1.43f * std::log10(foldLen));
	float base = (f0 > 0.f) ? f0 : allo;
	float fEff = base * std::sqrt(std::fmax(tenseness, 0.01f) / 0.6f);
	g.setTargets(fEff, tenseness, pressure);

	render::Rng rng;
	render::WavWriter w;
	if (!w.open(outPath, (uint32_t)sr)) {
		std::fprintf(stderr, "cannot open %s\n", outPath);
		return 1;
	}

	// Normalize: track peak, scale at end (glottis output is roughly +-1 but
	// varies with settings; normalization keeps comparisons honest).
	const size_t n = (size_t)(seconds * sr);
	float* buf = new float[n];
	float peak = 0.f;
	for (size_t i = 0; i < n; i++) {
		float noise = rng.uniform() * 2.f - 1.f;
		float fbDrive = fb * 0.6f;
		float x = g.process(noise, fbDrive, drift, vib);
		buf[i] = x;
		float a = std::fabs(x);
		if (a > peak) peak = a;
	}
	float scale = peak > 1e-6f ? 0.9f / peak : 0.f;
	for (size_t i = 0; i < n; i++)
		w.write(buf[i] * scale);
	w.close();
	delete[] buf;

	std::printf("%s: %.1fs sr=%.0f t=%.2f p=%.2f fold=%.1f f0eff=%.1f peak=%.3f\n",
		outPath, seconds, sr, tenseness, pressure, foldLen, fEff, peak);
	return 0;
}
