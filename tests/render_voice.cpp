// cyberline — end-to-end chain harness: CyberlineGlottis -> CyberlineTract.
// This is the audio path of the Voice module (and of the atoms patch).
// Also validates the semi-coupled feedback loop (fb>0) for stability.
//
//   g++ -O2 -std=c++11 -o render_voice tests/render_voice.cpp
//   ./render_voice out.wav 2.0 "f=110 t=0.6 p=0.8 fold=14 tongue=22,2.8 vtl=17.5 fb=0"
//
// Vowel tests: tongue index/diameter shape formants. fb tests: 0 vs 0.4
// (loaded voice: slight pitch drop + timbre change expected).

#include "../src/CyberlineGlottis.hpp"
#include "../src/CyberlineTract.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <map>

namespace render3 {

struct WavWriter {
	FILE* f = nullptr;
	uint32_t dataBytes = 0;
	uint32_t sr = 44100;
	bool open(const char* path, uint32_t sampleRate) {
		sr = sampleRate;
		f = std::fopen(path, "wb");
		if (!f) return false;
		std::fputs("RIFF", f); std::fseek(f, 4, SEEK_CUR);
		std::fputs("WAVEfmt ", f);
		uint32_t fmtSize = 16; std::fwrite(&fmtSize, 4, 1, f);
		uint16_t pcm = 1, ch = 1; uint16_t bits = 16;
		uint32_t byteRate = sr * ch * bits / 8;
		uint16_t align = ch * bits / 8;
		std::fwrite(&pcm, 2, 1, f); std::fwrite(&ch, 2, 1, f);
		std::fwrite(&sr, 4, 1, f); std::fwrite(&byteRate, 4, 1, f);
		std::fwrite(&align, 2, 1, f); std::fwrite(&bits, 2, 1, f);
		std::fputs("data", f); std::fseek(f, 4, SEEK_CUR);
		return true;
	}
	void write(float x) {
		if (x > 1.f) x = 1.f;
		if (x < -1.f) x = -1.f;
		int16_t s = (int16_t)(x * 32767.f);
		std::fwrite(&s, 2, 1, f); dataBytes += 2;
	}
	void close() {
		uint32_t riff = dataBytes + 36;
		std::fseek(f, 4, SEEK_SET); std::fwrite(&riff, 4, 1, f);
		std::fseek(f, 40, SEEK_SET); std::fwrite(&dataBytes, 4, 1, f);
		std::fclose(f);
	}
};

struct Rng {
	uint32_t s = 0xabcdef01u;
	float uniform() {
		s ^= s << 13; s ^= s >> 17; s ^= s << 5;
		return (float)(s & 0xFFFFFF) / 16777215.f;
	}
};

} // namespace render3

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
	float pressure  = kv.count("p") ? kv["p"] : 0.8f;
	float foldLen   = kv.count("fold") ? kv["fold"] : 14.f;
	float vib       = kv.count("vib") ? kv["vib"] : 4.f;
	float drift     = kv.count("drift") ? kv["drift"] : 0.25f;
	float fbGain    = kv.count("fb") ? kv["fb"] : 0.f;
	float tongueIdx = kv.count("tongue") ? kv["tongue"] : 22.f;
	float tongueDia = kv.count("tongue2") ? kv["tongue2"] : 2.8f;
	float velum     = kv.count("velum") ? kv["velum"] : 0.01f;
	float vtl       = kv.count("vtl") ? kv["vtl"] : 17.5f;
	float fric      = kv.count("fric") ? kv["fric"] : 0.f;
	float constrPos = kv.count("constr") ? kv["constr"] : -1.f;
	float constrWid = kv.count("constr2") ? kv["constr2"] : 3.f;

	const float sr = 44100.f;
	cyberline::CyberlineGlottis g;
	g.setSampleRate(sr);
	g.reset();

	cyberline::CyberlineTract tract;
	tract.setGeometry(vtl, sr);
	tract.setTongue(tongueIdx, tongueDia);
	tract.setVelum(velum);
	tract.setConstriction(constrPos, constrWid, fric);
	tract.muscleSpeed = 15.f;

	// allometric fold scaling (same law as module)
	float allo = std::pow(10.f, 4.01f - 1.43f * std::log10(foldLen));
	float fEff = (f0 > 0.f) ? f0 : allo;
	fEff *= std::sqrt(std::fmax(tenseness, 0.01f) / 0.6f);
	g.setTargets(fEff, tenseness, pressure, foldLen);

	render3::Rng rng;
	render3::WavWriter w;
	if (!w.open(outPath, (uint32_t)sr)) {
		std::fprintf(stderr, "cannot open %s\n", outPath);
		return 1;
	}

	const size_t n = (size_t)(seconds * sr);
	float* buf = new float[n];
	float peak = 0.f;
	const size_t warmup = (size_t)(0.25f * sr);
	bool blewUp = false;

	for (size_t i = 0; i < n + warmup; i++) {
		tract.applyConstriction();
		tract.reshape(1.f / sr);
		tract.calculateReflectionsPublic();

		// semi-coupled loop: probe pressure -> glottis fb drive (smoothed in glottis)
		float probeP = (tract.R[tract.n / 3] + tract.L[tract.n / 3]) * fbGain;
		if (!std::isfinite(probeP)) probeP = 0.f;

		float noise = rng.uniform() * 2.f - 1.f;
		float voiced = g.process(noise, probeP, drift, vib);

		float exc = voiced;
		float lip = tract.step(exc, 0.f, 0.f, noise, 0.5f);
		lip = tract.step(exc, 0.f, 0.f, noise, 1.0f);
		float nose = tract.lastNoseOutput;

		if (i >= warmup) {
			float x = (lip + nose * 0.3f) * 0.35f;
			if (!std::isfinite(x)) { blewUp = true; x = 0.f; }
			buf[i - warmup] = x;
			float a = std::fabs(x);
			if (a > peak) peak = a;
		}
	}
	float scale = (peak > 1e-6f && !blewUp) ? 0.9f / peak : 0.f;
	for (size_t i = 0; i < n; i++) w.write(buf[i] * scale);
	w.close();
	delete[] buf;

	std::printf("%s: %.1fs f0eff=%.1f fold=%.1f tongue=%.1f,%.1f vtl=%.1f "
		"fb=%.2f velum=%.2f peak=%.3f%s\n",
		outPath, seconds, fEff, foldLen, tongueIdx, tongueDia, vtl, fbGain,
		velum, peak, blewUp ? "  ***BLEW UP***" : "");
	return blewUp ? 2 : 0;
}
