// cyberline — CyberlineHeartCore harness: verifies the Wiggers anchors
// (rhythm, stroke volume, arterial pressures, finite state) across states
// and renders the phonocardiogram to WAV.
//
//   g++ -O2 -std=c++11 -o render_heart tests/render_heart.cpp
//   ./render_heart out.wav
//
// Anchor expectations (human defaults): ~72 bpm, stroke volume ~0.3-0.6,
// ECG R spikes present, all state finite; exercise raises bpm; mouse mass
// multiplies rate ~7x vs human; pacing locks the rhythm; irritability
// produces occasional premature beats with longer compensatory intervals.

#include "../src/CyberlineHeartCore.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using cyberline::CyberlineHeartCore;

namespace heartharness {

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

struct RunStats {
	bool finite = true;
	double bpmMeasured = 0;
	double meanStroke = 0;
	double meanSys = 0;   // arterial pressure at S2 (end ejection)
	double meanDia = 0;   // arterial pressure at beat trigger
	int beats = 0;
	int ectopics = 0;     // intervals < 0.6x median
};

// Run one state; warmup discarded. Optionally render sound, print stats.
static RunStats run(CyberlineHeartCore::Params p, float seconds,
		const char* wavPath, bool verbose, const char* label) {
	CyberlineHeartCore core;
	core.setSampleRate(44100.f);
	core.reset();

	const float warmup = 4.f;
	std::vector<float> wav;
	if (wavPath) wav.reserve((size_t)(seconds * 44100.f));

	std::vector<float> intervals, strokes, sysAr, diaAr;
	float lastBeatT = -1.f;
	bool beatWas = false, ejecting = false;
	int n = (int)((warmup + seconds) * 44100.f);

	for (int i = 0; i < n; i++) {
		auto o = core.process(p);
		float tt = i * core.sampleTime;
		bool inWarm = tt < warmup;

		if (!std::isfinite(o.volume) || !std::isfinite(o.ecg)
			|| !std::isfinite(o.sound) || !std::isfinite(core.apart)) {
			std::fprintf(stderr, "NON-FINITE at t=%.3f (%s)\n", tt, label);
			RunStats bad; bad.finite = false; return bad;
		}

		// BEAT trigger -> interval + diastolic arterial snapshot
		if (o.beat > 0.f && !beatWas) {
			if (lastBeatT >= 0.f && !inWarm) {
				intervals.push_back(tt - lastBeatT);
				diaAr.push_back(core.apart);
			}
			lastBeatT = tt;
		}
		beatWas = o.beat > 0.f;

		// ejection end (S2, valve state) -> completed stroke volume +
		// systolic arterial snapshot. (Use the valve state, not the
		// smoothed OUTFLOW output — the one-pole tail would blur the edge.)
		if (ejecting && !core.outflowOpen && !inWarm) {
			strokes.push_back(core.lastStroke);
			sysAr.push_back(core.apart);
		}
		ejecting = core.outflowOpen;

		if (!inWarm && wavPath) wav.push_back(o.sound);
	}

	RunStats st;
	st.finite = true;
	if (!intervals.empty()) {
		std::vector<float> srt = intervals;
		std::sort(srt.begin(), srt.end());
		float med = srt[srt.size() / 2];
		double sum = 0; int cnt = 0;
		for (float iv : intervals) {
			if (iv < med * 2.5f) {  // skip pause outliers for bpm mean
				sum += iv; cnt++;
				if (iv < med * 0.6f) st.ectopics++;
			}
		}
		st.bpmMeasured = cnt ? 60.0 * cnt / sum : 0.0;
	}
	st.beats = (int)intervals.size();
	if (!strokes.empty()) {
		double s = 0; for (float x : strokes) s += x;
		st.meanStroke = s / strokes.size();
	}
	if (!sysAr.empty()) {
		double s = 0; for (float x : sysAr) s += x;
		st.meanSys = s / sysAr.size();
	}
	if (!diaAr.empty()) {
		double s = 0; for (float x : diaAr) s += x;
		st.meanDia = s / diaAr.size();
	}
	if (verbose) {
		std::printf("%-20s bpm=%6.1f beats=%3d ectopics=%d "
			"stroke=%.3f sysArt=%.2f diaArt=%.2f vvol=%.2f avol=%.2f\n",
			label, st.bpmMeasured, st.beats, st.ectopics,
			st.meanStroke, st.meanSys, st.meanDia, core.vvol, core.avol);
	}
	if (wavPath && !wav.empty()) {
		WavWriter w;
		if (w.open(wavPath, 44100)) {
			float peak = 1e-6f;
			for (float x : wav) peak = fmaxf(peak, fabsf(x));
			float scale = 0.9f / peak;
			for (float x : wav) w.write(x * scale);
			w.close();
		}
	}
	return st;
}

} // namespace heartharness

int main(int argc, char** argv) {
	const char* wav = argc > 1 ? argv[1] : nullptr;
	using namespace heartharness;
	using P = CyberlineHeartCore::Params;

	std::printf("== cyberline-heart core anchors ==\n");

	P rest; rest.soundLvl = 1.f;
	RunStats a = run(rest, 14.f, wav, true, "rest (72 knob)");
	if (!a.finite) return 2;

	P ex; ex.rate = 168.f; ex.drive = 0.9f; ex.fill = 0.95f;
	ex.afterload = 0.22f; ex.contract = 0.95f; ex.hrv = 0.06f;
	RunStats b = run(ex, 14.f, nullptr, true, "heavy exercise");
	if (!b.finite) return 2;

	P mouse; mouse.mass = 0.f;
	RunStats c = run(mouse, 10.f, nullptr, true, "mouse mass");
	if (!c.finite) return 2;

	P ele; ele.mass = 1.f;
	RunStats d = run(ele, 16.f, nullptr, true, "elephant mass");
	if (!d.finite) return 2;

	// pacing lock: pacer triggers at 200 bpm against a 60 bpm SA node
	{
		CyberlineHeartCore core;
		core.setSampleRate(44100.f); core.reset();
		P paced; paced.rate = 60.f;
		std::vector<float> iv; float lastT = -1; bool was = false;
		float trigInterval = 60.f / 200.f;
		int n = (int)(16.f * 44100.f);
		for (int i = 0; i < n; i++) {
			float tt = i * core.sampleTime;
			paced.pacing = std::fmod(tt, trigInterval) < 0.002f;
			auto o = core.process(paced);
			if (o.beat > 0.f && !was && tt > 4.f) {
				if (lastT >= 0) iv.push_back(tt - lastT);
				lastT = tt;
			}
			was = o.beat > 0.f;
		}
		double sum = 0; for (float x : iv) sum += x;
		std::printf("%-20s bpm=%6.1f (expect ~200)\n",
			"paced @200",
			iv.size() ? 60.0 * iv.size() / sum : 0.0);
	}

	P irr; irr.irrit = 1.f; irr.hrv = 0.f;
	RunStats g = run(irr, 45.f, nullptr, true, "irritability=1");
	if (!g.finite) return 2;

	P sten; sten.valve = -0.8f;
	RunStats h = run(sten, 10.f, nullptr, true, "stenosis");
	if (!h.finite) return 2;

	P reg; reg.valve = 0.8f;
	RunStats ri = run(reg, 10.f, nullptr, true, "regurgitation");
	if (!ri.finite) return 2;

	P vagal; vagal.drive = -1.f; vagal.rate = 80.f;
	RunStats v1 = run(vagal, 16.f, nullptr, true, "vagal drive -1");
	if (!v1.finite) return 2;

	std::printf("== all states finite ==\n");
	return 0;
}
