// cyberline — Oral Chamber tract render harness (no Rack dependency).
// Feeds a synthetic LF-pulse source (with optional noise) through
// CyberlineTract and writes a WAV. Parameter sweep on the command line.
//
//   g++ -O2 -std=c++11 -o render_tract tests/render_tract.cpp
//   ./render_tract out.wav 2.0 "f=110 t=0.6 tongue=19,3.5 velum=0.01 constr=25,3 fric=0 vtl=17.5"
//
// Expected: tongue position changes vowel color; velum > 0.3 nasalizes.

#include "../src/CyberlineTract.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>

namespace render2 {

struct WavWriter {
	FILE* f = nullptr;
	uint32_t dataBytes = 0;
	uint32_t sr = 44100;

	bool open(const char* path, uint32_t sampleRate) {
		sr = sampleRate;
		f = std::fopen(path, "wb");
		if (!f) return false;
		std::fputs("RIFF", f);
		std::fseek(f, 4, SEEK_CUR);
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
		std::fseek(f, 4, SEEK_CUR);
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

struct Rng {
	uint32_t s = 0x9e3779b9u;
	float uniform() {
		s ^= s << 13; s ^= s >> 17; s ^= s << 5;
		return (float)(s & 0xFFFFFF) / 16777215.f;
	}
};

// Minimal LF-pulse source (same two-branch form, coarser than the module's).
struct PulseSource {
	float sr = 44100.f;
	float f0 = 110.f;
	float tenseness = 0.6f;
	float phase = 0.f;
	float period() const { return 1.f / f0; }
	// one sample: normalized LF-ish derivative pulse
	float process() {
		phase += 1.f / sr;
		if (phase >= period()) phase -= period();
		float t = phase * f0;      // 0..1 within period
		// crude LF: rising sinusoid to Te, then exponential recovery
		float Rd = 3.f * (1.f - tenseness);
		if (Rd < 0.5f) Rd = 0.5f;
		if (Rd > 2.7f) Rd = 2.7f;
		float Te = 0.3f + 0.12f * Rd;   // approximate open quotient
		if (t < Te) {
			return std::sin(M_PI * t / Te);
		}
		float e = (t - Te) / (1.f - Te);
		return -std::exp(-6.f * e) * (1.f - e);
	}
};

} // namespace render2

int main(int argc, char** argv) {
	if (argc < 3) {
		std::fprintf(stderr, "usage: %s out.wav seconds k=v ...\n", argv[0]);
		return 1;
	}
	const char* outPath = argv[1];
	float seconds = std::atof(argv[2]);

	std::map<std::string, float> kv;
	for (int i = 3; i < argc; i++) {
		std::string arg = argv[i];
		// split on spaces so one quoted arg can hold "f=110 tongue=22,2.8"
		size_t start = 0;
		while (start < arg.size()) {
			size_t end = arg.find(' ', start);
			if (end == std::string::npos) end = arg.size();
			std::string tok = arg.substr(start, end - start);
			start = end + 1;
			if (tok.empty()) continue;
			auto eq = tok.find('=');
			if (eq == std::string::npos) continue;
			std::string key = tok.substr(0, eq);
			std::string val = tok.substr(eq + 1);
			auto comma = val.find(',');
			kv[key] = std::atof(val.substr(0, comma).c_str());
			if (comma != std::string::npos)
				kv[key + "2"] = std::atof(val.substr(comma + 1).c_str());
		}
	}

	float f0        = kv.count("f") ? kv["f"] : 110.f;
	float tenseness = kv.count("t") ? kv["t"] : 0.6f;
	float tongueIdx = kv.count("tongue") ? kv["tongue"] : 19.f;
	float tongueDia = kv.count("tongue2") ? kv["tongue2"] : 3.5f;
	float velum     = kv.count("velum") ? kv["velum"] : 0.01f;
	float constrPos = kv.count("constr") ? kv["constr"] : -1.f;
	float constrWid = kv.count("constr2") ? kv["constr2"] : 3.f;
	float fric      = kv.count("fric") ? kv["fric"] : 0.f;
	float vtl       = kv.count("vtl") ? kv["vtl"] : 17.5f;
	float speed     = kv.count("speed") ? kv["speed"] : 15.f;
	float wall      = kv.count("wall") ? kv["wall"] : 0.999f;
	float injectPos = kv.count("inj") ? kv["inj"] : 20.f;
	float injAmt    = kv.count("injamt") ? kv["injamt"] : 0.f;

	const float sr = 44100.f;
	cyberline::CyberlineTract tract;	tract.setGeometry(vtl, sr);
	tract.setTongue(tongueIdx, tongueDia);
	if (kv.count("flat")) {
		// uniform-tube validation mode: constant diameter everywhere
		float d = kv["flat"];
		for (int i = 0; i < tract.n; i++)
			tract.restDiameter[i] = tract.targetDiameter[i] = tract.diameter[i] = d;
	}
	tract.setVelum(velum);
	tract.setConstriction(constrPos, constrWid, fric);
	tract.muscleSpeed = speed;
	tract.wallDamping = wall;

	render2::PulseSource src;
	src.sr = sr; src.f0 = f0; src.tenseness = tenseness;
	render2::Rng rng;

	render2::WavWriter w;
	if (!w.open(outPath, (uint32_t)sr)) {
		std::fprintf(stderr, "cannot open %s\n", outPath);
		return 1;
	}

	const size_t n = (size_t)(seconds * sr);
	float* buf = new float[n];
	float peak = 0.f;
	// allow transients/geometry to settle before recording
	const size_t warmup = (size_t)(0.2f * sr);
	for (size_t i = 0; i < n + warmup; i++) {
		tract.applyConstriction();

		float noise = rng.uniform() * 2.f - 1.f;
		float exc = src.process() * 0.5f;
		float inj = injAmt > 0.f ? noise * injAmt : 0.f;

		// shape update + rate-adaptive waveguide stepping
		float lip = tract.process(1.f / sr, exc, inj, injectPos, noise);
		float nose = tract.lastNoseOutput;

		if (i >= warmup) {
			float x = (lip + nose * 0.5f) * 0.25f;
			buf[i - warmup] = x;
			float a = std::fabs(x);
			if (a > peak) peak = a;
		}
	}
	float scale = peak > 1e-6f ? 0.9f / peak : 0.f;
	for (size_t i = 0; i < n; i++) w.write(buf[i] * scale);
	w.close();
	delete[] buf;

	std::printf("%s: %.1fs f0=%.1f t=%.2f tongue=%.1f,%.1f velum=%.2f "
		"constr=%.1f,%.1f fric=%.2f vtl=%.1f peak=%.3f\n",
		outPath, seconds, f0, tenseness, tongueIdx, tongueDia, velum,
		constrPos, constrWid, fric, vtl, peak);
	return 0;
}
