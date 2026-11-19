// cyberline-midimap — MIDI Map: a labeled grid of CV inputs that drive an
// external synthesizer directly over MIDI. Unrelated to the vocal suite;
// ships here because it shares the house panel/neon helpers.
//
// The module OWNS a MIDI output (driver/device/channel picked on the panel
// like Core's MIDI modules) and generates the controller bytes itself, so
// it speaks both plain CC and — when a spec row carries an NRPN address —
// full NRPN sequences (CC99 MSB, CC98 LSB, CC6 data), which is how the
// Novation UltraNova reaches its NRPN-only parameters (Filter Env, Env
// 3-6, LFOs, FX engine).
//
// Each of the 16 cells is one synthesizer parameter: patch a Rack CV
// source (LFO, envelope, sequencer...) into the cell's IN jack and the
// module continuously transmits that parameter. The parameter list comes
// from a "synth spec" table {cc-or-nrpn, name, min, max, orientation,
// group}: three specs are built in (Waldorf Blofeld, Novation UltraNova,
// Generic CC 0-127) and arbitrary specs load from JSON via the context
// menu, so new synths need no code.
//
// Input voltage convention (orientation-aware, documented on the panel):
//   standard orientation: 0..+10 V sweeps the parameter min..max
//   centered orientation: -5..+5 V sweeps min..max (patch an LFO straight
//     into a pan/detune cell and it swings the full bipolar range)
// With the IN jack unpatched the cell rests at the parameter's range
// midpoint — its neutral position — and transmits that once.
// Values transmit only on change (change-gated like Core's CV-CC) and are
// rate-limited to 200 Hz, so slow CV dithering doesn't flood the MIDI
// line. Every transmitted value is an integer CC/NRPN data byte 0..127
// (or the parameter's own documented sub-range, e.g. Blofeld ARP length
// 0..15, OSC octave 16..112).

#pragma once
#include "CyberlineQuality.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>
#ifdef USING_CARDINAL_NOT_RACK
// CardinalPluginContext::writeMidiMessage (the host MIDI-out path) — the
// same include halfagiraf's Entwine uses for its CardinalMidiOut adapter.
#include <plugincontext.hpp>
#endif
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <atomic>
#include <map>
#include <string>
#include <vector>

namespace cyberline {

// ---- Synth spec model ----

struct MidiMapParam {
	int cc = -1;         // MIDI CC number (plain CC target)
	int nrpnMsb = -1;    // NRPN address (used when nrpnLsb >= 0)
	int nrpnLsb = -1;
	std::string name;    // human-readable parameter name (menu + panel label)
	float min = 0.f;     // documented value range
	float max = 127.f;
	bool centered = false; // true: bipolar parameter -> -5..+5 V input
	std::string group;   // menu grouping ("" -> "General")

	bool isNrpn() const { return nrpnLsb >= 0; }
	// Stable key for change-gating (per-target, not per-cell, so two cells
	// on the same target don't double-send).
	int key() const { return isNrpn() ? 0x4000 + nrpnMsb * 128 + nrpnLsb : cc; }
	// Short tag for the panel label ("CC 74" / "NRPN 0:74").
	std::string tag() const {
		char buf[24];
		if (isNrpn()) {
			std::snprintf(buf, sizeof(buf), "NRPN %d:%d", nrpnMsb, nrpnLsb);
			return buf;
		}
		std::snprintf(buf, sizeof(buf), "CC %d", cc);
		return buf;
	}
};

struct MidiMapSpec {
	std::string synth;   // display name, e.g. "Waldorf Blofeld"
	std::vector<MidiMapParam> params;
	// Default cell assignment: defaults[c] = param index for cell c
	// (shorter lists leave the remaining cells to first-unused/spread
	// fallback). Empty = spread over the table order.
	std::vector<int> defaults;

	const MidiMapParam* paramAt(int index) const {
		if (index < 0 || index >= (int) params.size())
			return NULL;
		return &params[index];
	}
};

// Built-in spec tables (compiled in; JSON import builds the same struct).

// Fill s.defaults by parameter NAME (first match wins; unmatched names
// are skipped, leaving those cells to the fallback).
inline void specDefaultsByName(MidiMapSpec& s, std::initializer_list<const char*> names) {
	for (const char* nm : names) {
		for (int i = 0; i < (int) s.params.size(); i++) {
			if (s.params[i].name == nm) {
				s.defaults.push_back(i);
				break;
			}
		}
	}
}

inline MidiMapSpec makeBlofeldSpec() {
	MidiMapSpec s;
	s.synth = "Waldorf Blofeld";
	// Data: Waldorf Blofeld MIDI CC chart (captainblacks.app; matches the
	// factory implementation chart). centered = "centered" orientation.
	// The Blofeld reaches everything else (LFO-free deep params) via
	// Waldorf SysEx, not NRPN — out of scope here.
	auto add = [&](int cc, const char* name, float min, float max, bool centered, const char* group) {
		MidiMapParam p;
		p.cc = cc;
		p.name = name;
		p.min = min;
		p.max = max;
		p.centered = centered;
		p.group = group;
		s.params.push_back(p);
	};
	add(1, "Mod wheel", 0, 127, false, "Standard");
	add(2, "Breath control", 0, 127, false, "Standard");
	add(4, "Foot control", 0, 127, false, "Standard");
	add(5, "Glide rate", 0, 127, false, "Standard");
	add(7, "Channel volume", 0, 127, false, "Standard");
	add(10, "Pan", 0, 127, true, "Standard");
	add(12, "Arpeggiator range", 0, 9, false, "Arpeggiator");
	add(13, "Arpeggiator length", 0, 15, false, "Arpeggiator");
	add(14, "Arpeggiator active", 0, 3, false, "Arpeggiator"); // 0 Off 1 On 2 One shot 3 Hold
	add(15, "LFO 1 shape", 0, 5, false, "LFO 1"); // 0 Sin 1 Tri 2 Sqr 3 Saw 4 Rnd 5 S&H
	add(16, "LFO 1 speed", 0, 127, false, "LFO 1");
	add(17, "LFO 1 sync", 0, 1, false, "LFO 1");
	add(18, "LFO 1 delay", 0, 127, false, "LFO 1");
	add(19, "LFO 2 shape", 0, 5, false, "LFO 2");
	add(20, "LFO 2 speed", 0, 127, false, "LFO 2");
	add(21, "LFO 2 sync", 0, 1, false, "LFO 2");
	add(22, "LFO 2 delay", 0, 127, false, "LFO 2");
	add(23, "LFO 3 shape", 0, 5, false, "LFO 3");
	add(24, "LFO 3 speed", 0, 127, false, "LFO 3");
	add(25, "LFO 3 sync", 0, 1, false, "LFO 3");
	add(26, "LFO 3 delay", 0, 127, false, "LFO 3");
	add(27, "OSC 1 octave", 16, 112, true, "Osc 1"); // 128' .. 1/2'
	add(28, "OSC 1 semitone", 52, 76, true, "Osc 1"); // -12..+12
	add(29, "OSC 1 detune", 0, 127, true, "Osc 1");
	add(30, "OSC 1 FM", 0, 127, false, "Osc 1");
	add(31, "OSC 1 shape", 0, 5, false, "Osc 1"); // 0 Puls 1 Saw 2 Tri 3 Sin 4 A1 5 A2
	add(33, "OSC 1 pulsewidth", 0, 127, false, "Osc 1");
	add(34, "OSC 1 PWM", 0, 127, true, "Osc 1");
	add(35, "OSC 2 octave", 16, 112, true, "Osc 2");
	add(36, "OSC 2 semitone", 52, 76, true, "Osc 2");
	add(37, "OSC 2 detune", 0, 127, true, "Osc 2");
	add(38, "OSC 2 FM", 0, 127, false, "Osc 2");
	add(39, "OSC 2 shape", 0, 5, false, "Osc 2");
	add(40, "OSC 2 pulsewidth", 0, 127, false, "Osc 2");
	add(41, "OSC 2 PWM", 0, 127, false, "Osc 2");
	add(42, "OSC 3 octave", 16, 112, true, "Osc 3");
	add(43, "OSC 3 semitone", 52, 76, true, "Osc 3");
	add(44, "OSC 3 detune", 0, 127, true, "Osc 3");
	add(45, "OSC 3 FM", 0, 127, false, "Osc 3");
	add(46, "OSC 3 shape", 0, 5, false, "Osc 3");
	add(47, "OSC 3 pulsewidth", 0, 127, false, "Osc 3");
	add(48, "OSC 3 PWM", 0, 127, true, "Osc 3");
	add(49, "Sync", 0, 1, false, "Mixer");
	add(50, "Pitchmod", 0, 127, true, "Mixer");
	add(51, "Glide mode", 0, 9, false, "Standard");
	add(52, "OSC 1 level", 0, 127, false, "Mixer");
	add(53, "OSC 1 balance", 0, 127, true, "Mixer");
	add(54, "Ringmod level", 0, 127, false, "Mixer");
	add(55, "Ringmod balance", 0, 127, true, "Mixer");
	add(56, "OSC 2 level", 0, 127, false, "Mixer");
	add(57, "OSC 2 balance", 0, 127, true, "Mixer");
	add(58, "OSC 3 level", 0, 127, false, "Mixer");
	add(59, "OSC 3 balance", 0, 127, true, "Mixer");
	add(60, "Noise level", 0, 127, false, "Mixer");
	add(61, "Noise balance", 0, 127, true, "Mixer");
	add(62, "Noise color", 0, 127, false, "Mixer");
	add(65, "Glide active", 0, 127, false, "Standard");
	add(67, "Filter routing", 0, 1, false, "Filter 1"); // 0 Par 1 Ser
	add(68, "Filter 1 type", 0, 10, false, "Filter 1"); // 0 Byp 1 LP24 2 LP12 3 BP24 4 BP12 5 HP24 6 HP12 7 N24 8 N12 9 Comb 10 PPG LP
	add(69, "Filter 1 cutoff", 0, 127, false, "Filter 1");
	add(70, "Filter 1 resonance", 0, 127, false, "Filter 1");
	add(71, "Filter 1 drive", 0, 127, false, "Filter 1");
	add(72, "Filter 1 keytrack", 0, 127, false, "Filter 1");
	add(73, "Filter 1 env amount", 0, 127, true, "Filter 1");
	add(74, "Filter 1 env velocity", 0, 127, true, "Filter 1");
	add(75, "Filter 1 cutoff mod", 0, 127, true, "Filter 1");
	add(76, "Filter 1 FM", 0, 127, false, "Filter 1");
	add(77, "Filter 1 pan", 0, 127, true, "Filter 1");
	add(78, "Filter 1 pan mod", 0, 127, true, "Filter 1");
	add(79, "Filter 2 type", 0, 10, false, "Filter 2");
	add(80, "Filter 2 cutoff", 0, 127, false, "Filter 2");
	add(81, "Filter 2 resonance", 0, 127, false, "Filter 2");
	add(82, "Filter 2 drive", 0, 127, false, "Filter 2");
	add(83, "Filter 2 keytrack", 0, 127, false, "Filter 2");
	add(84, "Filter 2 env amount", 0, 127, true, "Filter 2");
	add(85, "Filter 2 env velocity", 0, 127, true, "Filter 2");
	add(86, "Filter 2 cutoff mod", 0, 127, true, "Filter 2");
	add(87, "Filter 2 FM", 0, 127, false, "Filter 2");
	add(88, "Filter 2 pan", 0, 127, true, "Filter 2");
	add(89, "Filter 2 pan mod", 0, 127, true, "Filter 2");
	add(90, "Amp volume", 0, 127, false, "Amp & FX");
	add(91, "Amp velocity", 0, 127, true, "Amp & FX");
	add(92, "Amp modulation", 0, 127, true, "Amp & FX");
	add(93, "FX 1 mix", 0, 127, false, "Amp & FX");
	add(94, "FX 2 mix", 0, 127, false, "Amp & FX");
	add(95, "Filter env attack", 0, 127, false, "Env Filter");
	add(96, "Filter env decay", 0, 127, false, "Env Filter");
	add(97, "Filter env sustain", 0, 127, false, "Env Filter");
	add(98, "Filter env decay 2", 0, 127, false, "Env Filter");
	add(99, "Filter env sustain 2", 0, 127, false, "Env Filter");
	add(100, "Filter env release", 0, 127, false, "Env Filter");
	add(101, "Amp env attack", 0, 127, false, "Env Amp");
	add(102, "Amp env decay", 0, 127, false, "Env Amp");
	add(103, "Amp env sustain", 0, 127, false, "Env Amp");
	add(104, "Amp env decay 2", 0, 127, false, "Env Amp");
	add(105, "Amp env sustain 2", 0, 127, false, "Env Amp");
	add(106, "Amp env release", 0, 127, false, "Env Amp");
	add(107, "Envelope 3 attack", 0, 127, false, "Env 3");
	add(108, "Envelope 3 decay", 0, 127, false, "Env 3");
	add(109, "Envelope 3 sustain", 0, 127, false, "Env 3");
	add(110, "Envelope 3 decay 2", 0, 127, false, "Env 3");
	add(111, "Envelope 3 sustain 2", 0, 127, false, "Env 3");
	add(112, "Envelope 3 release", 0, 127, false, "Env 3");
	add(113, "Envelope 4 attack", 0, 127, false, "Env 4");
	add(114, "Envelope 4 decay", 0, 127, false, "Env 4");
	add(115, "Envelope 4 sustain", 0, 127, false, "Env 4");
	add(116, "Envelope 4 decay 2", 0, 127, false, "Env 4");
	add(117, "Envelope 4 sustain 2", 0, 127, false, "Env 4");
	add(118, "Envelope 4 release", 0, 127, false, "Env 4");
	specDefaultsByName(s, {
		"Mod wheel", "Breath control", "Channel volume", "Pan",
		"Filter 1 cutoff", "Filter 1 resonance",
		"OSC 1 level", "OSC 2 level", "OSC 3 level", "Noise level",
		"Amp env attack", "Amp env decay", "Amp env sustain", "Amp env release",
		"Filter env attack", "Filter env release",
	});
	return s;
}

inline MidiMapSpec makeUltraNovaSpec() {
	MidiMapSpec s;
	s.synth = "Novation UltraNova";
	// Data: "UltraNova Program Parameter MIDI Assignments" (novation
	// ultranova-midi-implementation2.pdf). Two transports exist:
	//   - plain CC (the "MIDI Controller List"): flat 0..127 for every
	//     listed parameter; standard-MIDI CCs (bank, data entry, pedals,
	//     RPN/NRPN selectors 96..101, channel modes) are not parameter
	//     targets and are omitted
	//   - NRPN (msb:lsb pairs in the "MIDI NRPN LIST"): how the Filter
	//     Env, Env 3..6, LFOs, FX engine and mod matrix are reached.
	//     Transmitted as CC99 (NRPN MSB) -> CC98 (NRPN LSB) -> CC6 (data);
	//     the synth applies the value after CC6 lands. We always send the
	//     full 3-message sequence (sending NRPN MSB=0 explicitly is safe
	//     per the implementation notes).
	auto addcc = [&](int cc, const char* name, bool centered, const char* group) {
		MidiMapParam p;
		p.cc = cc;
		p.name = name;
		p.centered = centered;
		p.group = group;
		s.params.push_back(p);
	};
	auto addnrpn = [&](int msb, int lsb, const char* name, bool centered, const char* group) {
		MidiMapParam p;
		p.nrpnMsb = msb;
		p.nrpnLsb = lsb;
		p.name = name;
		p.centered = centered;
		p.group = group;
		s.params.push_back(p);
	};
	// --- plain CC parameters ---
	addcc(1, "Modwheel", false, "Standard");
	addcc(2, "Breath", false, "Standard");
	addcc(7, "Channel volume", false, "Standard");
	addcc(10, "Pan position", true, "Standard");
	addcc(3, "Polyphony mode", false, "Global");
	addcc(5, "Portamento rate", false, "Global");
	addcc(8, "Dry:wet balance", true, "Global");
	addcc(9, "Pre-glide", false, "Global");
	addcc(12, "Portamento expo/linear", false, "Global");
	addcc(13, "Keyboard octave offset", true, "Global");
	addcc(14, "Unison count", false, "Global");
	addcc(15, "Unison detune", false, "Global");
	addcc(16, "Osc123 drift", false, "Osc");
	addcc(17, "Osc123 start phase", false, "Osc");
	addcc(18, "Osc123 fixed note", false, "Osc");
	addcc(19, "Osc1 waveform/WT/audio", false, "Osc");
	addcc(20, "Osc1 WT interpolate", false, "Osc");
	addcc(21, "Osc1 pulsewidth/WT index", false, "Osc");
	addcc(22, "Osc1 sync depth", false, "Osc");
	addcc(23, "Osc1 hardness", false, "Osc");
	addcc(24, "Osc1 density", false, "Osc");
	addcc(25, "Osc1 density detune", false, "Osc");
	addcc(26, "Osc1 semitone offset", true, "Osc");
	addcc(27, "Osc1 cents offset", true, "Osc");
	addcc(28, "Osc1 bendwheel>pitch", true, "Osc");
	addcc(29, "Osc2 waveform/WT/audio", false, "Osc");
	addcc(30, "Osc2 WT interpolate", false, "Osc");
	addcc(31, "Osc2 pulsewidth/WT index", false, "Osc");
	addcc(33, "Osc2 sync depth", false, "Osc");
	addcc(34, "Osc2 hardness", false, "Osc");
	addcc(35, "Osc2 density", false, "Osc");
	addcc(36, "Osc2 density detune", false, "Osc");
	addcc(37, "Osc2 semitone offset", true, "Osc");
	addcc(39, "Osc2 cents offset", true, "Osc");
	addcc(40, "Osc2 bendwheel>pitch", true, "Osc");
	addcc(41, "Osc3 waveform/WT/audio", false, "Osc");
	addcc(42, "Osc3 WT interpolate", false, "Osc");
	addcc(43, "Osc3 pulsewidth/WT index", false, "Osc");
	addcc(44, "Osc3 sync depth", false, "Osc");
	addcc(45, "Osc3 hardness", false, "Osc");
	addcc(46, "Osc3 density", false, "Osc");
	addcc(47, "Osc3 density detune", false, "Osc");
	addcc(48, "Osc3 semitone offset", true, "Osc");
	addcc(49, "Osc3 cents offset", true, "Osc");
	addcc(50, "Osc3 bendwheel>pitch", true, "Osc");
	addcc(51, "Osc1 mix level", false, "Mix");
	addcc(52, "Osc2 mix level", false, "Mix");
	addcc(53, "Osc3 mix level", false, "Mix");
	addcc(54, "Osc13 ringmod mix level", false, "Mix");
	addcc(55, "Osc23 ringmod mix level", false, "Mix");
	addcc(56, "Noise mix level", false, "Mix");
	addcc(57, "Noise type", false, "Mix");
	addcc(58, "Pre-FX level", false, "Mix");
	addcc(59, "Post-FX level", false, "Mix");
	addcc(60, "Filter configuration", false, "Filter");
	addcc(61, "Filter balance", true, "Filter");
	addcc(62, "Filter packed data", false, "Filter");
	addcc(63, "Filter1 drive", false, "Filter");
	addcc(65, "Filter1 drive type", false, "Filter");
	addcc(68, "Filter1 type", false, "Filter");
	addcc(69, "Filter1 track", false, "Filter");
	addcc(70, "AmpEnv sustain level", false, "Amp Env");
	addcc(71, "Filter1 resonance", false, "Filter");
	addcc(72, "AmpEnv release", false, "Amp Env");
	addcc(73, "AmpEnv attack", false, "Amp Env");
	addcc(74, "Filter1 frequency", false, "Filter");
	addcc(75, "AmpEnv decay", false, "Amp Env");
	addcc(76, "Easy vibrato rate", false, "Global");
	addcc(77, "Easy vibrato depth", false, "Global");
	addcc(78, "Filter1 Q-normalise", false, "Filter");
	addcc(79, "FiltEnv>Filter1 freq", true, "Filter");
	addcc(80, "Filter2 drive", false, "Filter");
	addcc(81, "Filter2 drive type", false, "Filter");
	addcc(82, "Filter2 type", false, "Filter");
	addcc(83, "Filter2 frequency", false, "Filter");
	addcc(84, "Filter2 track", false, "Filter");
	addcc(85, "Filter2 resonance", false, "Filter");
	addcc(86, "Filter2 Q-normalise", false, "Filter");
	addcc(87, "FiltEnv>Filter2 freq", true, "Filter");
	addcc(88, "Pan mod rate", false, "Pan");
	addcc(89, "Pan mod rate sync", false, "Pan");
	addcc(90, "Pan mod depth", false, "Pan");
	addcc(91, "FxSlot1 level (reverb)", false, "FX");
	addcc(92, "FxSlot2 level (tremolo)", false, "FX");
	addcc(93, "FxSlot3 level (chorus)", false, "FX");
	addcc(94, "FxSlot4 level (celeste)", false, "FX");
	addcc(95, "FxSlot5 level (phaser)", false, "FX");
	addcc(102, "Pan L/R phase offset", false, "Pan");
	addcc(103, "L/R width enhance", false, "Pan");
	addcc(104, "Pan spare", false, "Pan");
	addcc(105, "Env packed mono/multi", false, "Env misc");
	addcc(106, "Env level track centre", true, "Env misc");
	addcc(107, "Drum time", false, "Env misc");
	addcc(108, "AmpEnv velocity", true, "Amp Env");
	addcc(109, "AmpEnv sustain rate", false, "Amp Env");
	addcc(110, "AmpEnv sustain time", false, "Amp Env");
	addcc(111, "AmpEnv A/D repeats", false, "Amp Env");
	addcc(112, "AmpEnv attack track", false, "Amp Env");
	addcc(113, "AmpEnv decay track", false, "Amp Env");
	addcc(114, "AmpEnv level track", false, "Amp Env");
	addcc(115, "AmpEnv attack slope", false, "Amp Env");
	addcc(116, "AmpEnv decay slope", false, "Amp Env");
	addcc(117, "AmpEnv touch trigger", false, "Amp Env");
	// --- NRPN parameters (the rest of the engine) ---
	// Filter Env
	addnrpn(0, 1, "FiltEnv attack", false, "Filter Env (NRPN)");
	addnrpn(0, 2, "FiltEnv decay", false, "Filter Env (NRPN)");
	addnrpn(0, 3, "FiltEnv sustain level", false, "Filter Env (NRPN)");
	addnrpn(0, 4, "FiltEnv release", false, "Filter Env (NRPN)");
	addnrpn(0, 0, "FiltEnv velocity", true, "Filter Env (NRPN)");
	// Env 3..6 (ADSR core)
	addnrpn(0, 15, "Env3 attack", false, "Env 3 (NRPN)");
	addnrpn(0, 16, "Env3 decay", false, "Env 3 (NRPN)");
	addnrpn(0, 17, "Env3 sustain level", false, "Env 3 (NRPN)");
	addnrpn(0, 18, "Env3 release", false, "Env 3 (NRPN)");
	addnrpn(0, 29, "Env4 attack", false, "Env 4 (NRPN)");
	addnrpn(0, 30, "Env4 decay", false, "Env 4 (NRPN)");
	addnrpn(0, 31, "Env4 sustain level", false, "Env 4 (NRPN)");
	addnrpn(0, 32, "Env4 release", false, "Env 4 (NRPN)");
	addnrpn(0, 43, "Env5 attack", false, "Env 5 (NRPN)");
	addnrpn(0, 44, "Env5 decay", false, "Env 5 (NRPN)");
	addnrpn(0, 45, "Env5 sustain level", false, "Env 5 (NRPN)");
	addnrpn(0, 46, "Env5 release", false, "Env 5 (NRPN)");
	addnrpn(0, 57, "Env6 attack", false, "Env 6 (NRPN)");
	addnrpn(0, 58, "Env6 decay", false, "Env 6 (NRPN)");
	addnrpn(0, 59, "Env6 sustain level", false, "Env 6 (NRPN)");
	addnrpn(0, 60, "Env6 release", false, "Env 6 (NRPN)");
	// LFOs 1..3 (shape / delay / rate)
	addnrpn(0, 73, "Lfo1 shape", false, "LFO 1 (NRPN)");
	addnrpn(0, 74, "Lfo1 delay", false, "LFO 1 (NRPN)");
	addnrpn(0, 76, "Lfo1 rate", false, "LFO 1 (NRPN)");
	addnrpn(0, 82, "Lfo2 shape", false, "LFO 2 (NRPN)");
	addnrpn(0, 83, "Lfo2 delay", false, "LFO 2 (NRPN)");
	addnrpn(0, 85, "Lfo2 rate", false, "LFO 2 (NRPN)");
	addnrpn(0, 91, "Lfo3 shape", false, "LFO 3 (NRPN)");
	addnrpn(0, 92, "Lfo3 delay", false, "LFO 3 (NRPN)");
	addnrpn(0, 94, "Lfo3 rate", false, "LFO 3 (NRPN)");
	// FX engine core params
	addnrpn(1, 6, "Delay1 time", false, "FX (NRPN)");
	addnrpn(1, 8, "Delay1 feedback", false, "FX (NRPN)");
	addnrpn(1, 12, "Delay2 time", false, "FX (NRPN)");
	addnrpn(1, 14, "Delay2 feedback", false, "FX (NRPN)");
	addnrpn(1, 19, "Reverb1 decay", false, "FX (NRPN)");
	addnrpn(1, 20, "Reverb1 damping", false, "FX (NRPN)");
	addnrpn(1, 22, "Reverb2 decay", false, "FX (NRPN)");
	addnrpn(1, 23, "Reverb2 damping", false, "FX (NRPN)");
	addnrpn(1, 25, "Phas/Chor1 rate", false, "FX (NRPN)");
	addnrpn(1, 27, "Phas/Chor1 feedback", false, "FX (NRPN)");
	addnrpn(1, 28, "Phas/Chor1 mod depth", false, "FX (NRPN)");
	addnrpn(1, 31, "Phas/Chor2 rate", false, "FX (NRPN)");
	addnrpn(1, 33, "Phas/Chor2 feedback", false, "FX (NRPN)");
	addnrpn(1, 34, "Phas/Chor2 mod depth", false, "FX (NRPN)");
	addnrpn(1, 37, "Phas/Chor3 rate", false, "FX (NRPN)");
	addnrpn(1, 39, "Phas/Chor3 feedback", false, "FX (NRPN)");
	addnrpn(1, 40, "Phas/Chor3 mod depth", false, "FX (NRPN)");
	addnrpn(1, 43, "Phas/Chor4 rate", false, "FX (NRPN)");
	addnrpn(1, 45, "Phas/Chor4 feedback", false, "FX (NRPN)");
	addnrpn(1, 46, "Phas/Chor4 mod depth", false, "FX (NRPN)");
	addnrpn(1, 0, "Distort1 type", false, "FX (NRPN)");
	addnrpn(1, 2, "Distort1 output level", false, "FX (NRPN)");
	// EQ
	addnrpn(0, 104, "Bass EQ frequency", false, "EQ (NRPN)");
	addnrpn(0, 105, "Bass EQ level", true, "EQ (NRPN)");
	addnrpn(0, 106, "Mid EQ frequency", false, "EQ (NRPN)");
	addnrpn(0, 107, "Mid EQ level", true, "EQ (NRPN)");
	addnrpn(0, 108, "Treble EQ frequency", false, "EQ (NRPN)");
	addnrpn(0, 109, "Treble EQ level", true, "EQ (NRPN)");
	// Arpeggiator (NRPN-only params)
	addnrpn(1, 62, "Arp octaves", false, "Arp (NRPN)");
	addnrpn(1, 64, "Arp gate time", false, "Arp (NRPN)");
	addnrpn(1, 65, "Arp mode", false, "Arp (NRPN)");
	addnrpn(1, 66, "Arp pattern", false, "Arp (NRPN)");
	addnrpn(1, 67, "Arp velocity", false, "Arp (NRPN)");
	specDefaultsByName(s, {
		"Modwheel", "Breath", "Channel volume", "Pan position",
		"Filter1 frequency", "Filter1 resonance",
		"Osc1 mix level", "Osc2 mix level", "Osc3 mix level", "Noise mix level",
		"AmpEnv attack", "AmpEnv decay", "AmpEnv sustain level", "AmpEnv release",
		"FiltEnv attack", "FiltEnv release",
	});
	return s;
}

inline MidiMapSpec makeGenericSpec() {
	MidiMapSpec s;
	s.synth = "Generic CC 0-127";
	for (int cc = 0; cc <= 127; cc++) {
		MidiMapParam p;
		p.cc = cc;
		char buf[16];
		std::snprintf(buf, sizeof(buf), "CC %d", cc);
		p.name = buf;
		p.group = (cc < 32) ? "CC 0-31" : (cc < 96) ? "CC 32-95" : "CC 96-127";
		s.params.push_back(p);
		s.defaults.push_back(cc); // sequential default: CC 0..15 on cells 1..16
	}
	return s;
}

// Parse a spec JSON document:
//   { "synth": "...", "params": [
//       { "cc": 69, "name": "Filter 1 cutoff", "min": 0, "max": 127,
//         "centered": false, "group": "Filter 1" },
//       { "nrpn": "1:6", "name": "Delay1 time", ... }, ... ] }
// NRPN rows use either "nrpn": "msb:lsb" or "nrpnMsb"/"nrpnLsb" ints.
// min/max default 0..127, centered/group optional. Returns false (and
// fills *error) on malformed input. Used by the widget's "Load spec
// JSON..." menu action so new synths need no code.
inline bool parseMidiMapSpecJson(const char* text, MidiMapSpec* out, std::string* error) {
	json_error_t err;
	json_t* root = json_loads(text, 0, &err);
	if (!root) {
		if (error)
			*error = std::string("JSON: ") + err.text;
		return false;
	}
	json_t* synthJ = json_object_get(root, "synth");
	if (!json_is_string(synthJ)) {
		if (error)
			*error = "missing \"synth\" string";
		json_decref(root);
		return false;
	}
	json_t* paramsJ = json_object_get(root, "params");
	if (!json_is_array(paramsJ)) {
		if (error)
			*error = "missing \"params\" array";
		json_decref(root);
		return false;
	}
	MidiMapSpec s;
	s.synth = json_string_value(synthJ);
	size_t i;
	json_t* v;
	json_array_foreach(paramsJ, i, v) {
		MidiMapParam p;
		json_t* ccJ = json_object_get(v, "cc");
		json_t* nrpnJ = json_object_get(v, "nrpn");
		json_t* nrpnMsbJ = json_object_get(v, "nrpnMsb");
		json_t* nrpnLsbJ = json_object_get(v, "nrpnLsb");
		json_t* nameJ = json_object_get(v, "name");
		if (!json_is_string(nameJ))
			continue; // skip malformed rows rather than rejecting the file
		p.name = json_string_value(nameJ);
		if (json_is_integer(ccJ)) {
			p.cc = (int) json_integer_value(ccJ);
		} else if (json_is_string(nrpnJ)) {
			int msb = 0, lsb = -1;
			if (std::sscanf(json_string_value(nrpnJ), "%d:%d", &msb, &lsb) == 2) {
				p.nrpnMsb = msb;
				p.nrpnLsb = lsb;
			} else {
				continue;
			}
		} else if (json_is_integer(nrpnLsbJ)) {
			p.nrpnMsb = json_is_integer(nrpnMsbJ) ? (int) json_integer_value(nrpnMsbJ) : 0;
			p.nrpnLsb = (int) json_integer_value(nrpnLsbJ);
		} else {
			continue; // row needs a cc or nrpn address
		}
		json_t* minJ = json_object_get(v, "min");
		json_t* maxJ = json_object_get(v, "max");
		p.min = json_is_number(minJ) ? (float) json_number_value(minJ) : 0.f;
		p.max = json_is_number(maxJ) ? (float) json_number_value(maxJ) : 127.f;
		json_t* centeredJ = json_object_get(v, "centered");
		p.centered = json_is_true(centeredJ);
		json_t* groupJ = json_object_get(v, "group");
		p.group = json_is_string(groupJ) ? json_string_value(groupJ) : "General";
		if (p.max <= p.min)
			p.max = p.min + 1.f; // guard degenerate ranges
		s.params.push_back(p);
	}
	if (s.params.empty()) {
		if (error)
			*error = "no valid params rows";
		json_decref(root);
		return false;
	}
	// Optional default cell assignment: array of parameter names or
	// indices. e.g. "defaults": ["Filter 1 cutoff", "Mod wheel", 7]
	json_t* defJ = json_object_get(root, "defaults");
	if (json_is_array(defJ)) {
		size_t di;
		json_t* dv;
		json_array_foreach(defJ, di, dv) {
			if (json_is_string(dv)) {
				std::string nm = json_string_value(dv);
				for (int i = 0; i < (int) s.params.size(); i++) {
					if (s.params[i].name == nm) {
						s.defaults.push_back(i);
						break;
					}
				}
			} else if (json_is_integer(dv)) {
				int idx = (int) json_integer_value(dv);
				if (idx >= 0 && idx < (int) s.params.size())
					s.defaults.push_back(idx);
			}
		}
	}
	*out = s;
	json_decref(root);
	return true;
}

// ---- Module ----

struct CyberlineMidiMap;

// Conflict-detection key for a cell target: transport + address +
// effective MIDI channel. Two cells conflict when they hold the same key
// (they would write the same parameter on the same channel and fight —
// last writer wins at 200 Hz). The same CC on a different channel is a
// different target (multi-timbral setups are legitimate), and CC vs NRPN
// never conflict even at the same number (different transports).
struct MidiMapTargetKey {
	bool nrpn;
	int addr;     // CC number, or nrpnMsb*128+nrpnLsb
	int channel;  // effective channel: port channel < 0 means 0 (ch 1)

	bool operator<(const MidiMapTargetKey& o) const {
		if (nrpn != o.nrpn)
			return nrpn < o.nrpn;
		if (addr != o.addr)
			return addr < o.addr;
		return channel < o.channel;
	}
	bool operator==(const MidiMapTargetKey& o) const {
		return nrpn == o.nrpn && addr == o.addr && channel == o.channel;
	}
};

// Who holds a target (for the "(cell n)" grey-out hints in the menu).
struct MidiMapTargetUse {
	MidiMapTargetKey key;
	int64_t moduleId = -1;
	int cell = -1;
	std::string synth; // owning module's spec name (hint text)
};

// One module's own uses (pure, testable headless). Defined after
// CyberlineMidiMap (needs the complete type).
inline void collectSelfUses(CyberlineMidiMap* m, int64_t moduleId,
		std::vector<MidiMapTargetUse>* out);

// ParamQuantity for a cell-target param: displays the assigned parameter
// name + CC/NRPN tag instead of a bare list index (context menu, tooltip,
// Ctrl-click entry all use the same string). Defined after CyberlineMidiMap
// (needs the complete type).
struct CellParamQuantity : rack::engine::ParamQuantity {
	std::string getDisplayValueString() override;
};

// Change-gated MIDI CC / NRPN sender with two compile-time backends:
//
// - Rack: subclasses rack::midi::Output (driver/device/channel picked on
//   the panel like Core's MIDI modules). sendMessage applies the port's
//   selected channel automatically when it is >= 0; with the channel
//   unset (-1) the message's own low nibble (0) is transmitted = MIDI ch 1.
// - Cardinal (USING_CARDINAL_NOT_RACK): the MIDI driver stack is removed
//   there (include/midi.hpp keeps only Message), so this is a standalone
//   struct forwarding to the host via
//   CardinalPluginContext::writeMidiMessage — the host IS the device, and
//   the channel is the only selector (0-based, -1 = ch 1, matching the
//   Rack port semantics our conflict keys use). Frame is block-relative
//   (host expects the offset within the current process block), computed
//   by CyberlineMidiMap::process from Engine::getBlockFrame.
//   Precedent: CardinalMidiOut in halfagiraf's osdialog-midi patch, and
//   core HostMIDI's midiOutput.
struct MidiMapOutput
#ifdef USING_CARDINAL_NOT_RACK
    /* standalone (Cardinal) */
#else
    : rack::midi::Output
#endif
{
	int64_t frame = -1;
	std::map<int, uint8_t> lastValues; // keyed by MidiMapParam::key()

	void resetValues() {
		lastValues.clear();
	}

#ifdef USING_CARDINAL_NOT_RACK
	// Channel: 0-based, -1 = unset (transmits as ch 1). The channel menu
	// under Cardinal stores 0..15 here.
	int channel = -1;

	void sendController(int cc, uint8_t value) {
		rack::midi::Message m;
		m.setStatus(0xb);
		m.setNote(cc);
		m.setValue(value);
		m.setFrame(frame);
		auto* pc = static_cast<CardinalPluginContext*>(APP);
		if (pc)
			pc->writeMidiMessage(m, channel < 0 ? 0 : (uint8_t) channel);
	}

	void reset() {
		resetValues();
	}

	json_t* toJson() const {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "channel", json_integer(channel));
		return rootJ;
	}

	void fromJson(json_t* rootJ) {
		json_t* j = json_object_get(rootJ, "channel");
		if (j)
			channel = rack::math::clamp((int) json_integer_value(j), 0, 15);
		resetValues();
	}
#else
	void sendController(int cc, uint8_t value) {
		rack::midi::Message m;
		m.setStatus(0xb);
		m.setNote(cc);
		m.setValue(value);
		m.setFrame(frame);
		sendMessage(m);
	}
#endif

	// Transmit `value` for one parameter (CC or NRPN), gated on change.
	void sendParam(const MidiMapParam& p, uint8_t value) {
		int k = p.key();
		auto it = lastValues.find(k);
		if (it != lastValues.end() && it->second == value)
			return;
		lastValues[k] = value;
		if (p.isNrpn()) {
			// NRPN sequence: CC99 MSB, CC98 LSB, CC6 data MSB. The synth
			// applies the value after CC6 (per the implementation notes,
			// sending NRPN MSB=0 explicitly is safe).
			sendController(99, (uint8_t) p.nrpnMsb);
			sendController(98, (uint8_t) p.nrpnLsb);
			sendController(6, value);
		} else {
			sendController(p.cc, value);
		}
	}
};

struct CyberlineMidiMap : rack::engine::Module {
	static constexpr int NUM_CELLS = 16;

	enum ParamIds {
		CELL0_PARAM, // one param per cell: index into the spec's param list
		NUM_PARAMS = CELL0_PARAM + NUM_CELLS
	};
	enum InputIds {
		CELL0_INPUT,
		NUM_INPUTS = CELL0_INPUT + NUM_CELLS,
		SYNC_INPUT = NUM_INPUTS, // resend-all trigger (clocked flush)
		NUM_INPUTS_WITH_SYNC
	};
	enum LightIds {
		NUM_LIGHTS
	};

	// Built-in spec indices + the currently selected/loaded spec.
	static constexpr int SPEC_BLOFELD = 0;
	static constexpr int SPEC_ULTRANOVA = 1;
	static constexpr int SPEC_GENERIC = 2;
	static constexpr int SPEC_LOADED = 3;
	int specIndex = SPEC_BLOFELD;
	MidiMapSpec loadedSpec;   // used when specIndex == SPEC_LOADED
	std::string loadedPath;   // display name of the loaded file ("" = none)

	MidiMapOutput midiOutput;
	rack::dsp::Timer rateLimiterTimer; // 200 Hz send rate (Core CV-CC style)
	bool syncWasHigh = false;         // SYNC trigger edge detect
	CyberlineCpuMeter cpuMeter;   // plugin-side CPU % ring (see CyberlineModels.hpp)

	// Cells disabled by default assignment: every target on this channel
	// is already held (by another instance or a sibling cell), so the cell
	// transmits nothing. A manual pick from the assign menu re-enables.
	// Persisted in the patch JSON.
	bool cellDisabled[NUM_CELLS] = {};
	// Per-cell last processed value (atomic: written by the audio thread
	// in process(), read by the UI thread in MidiMapGrid::draw for the
	// activity bars — replaces reading the change-gate map cross-thread).
	std::atomic<uint8_t> telValue[NUM_CELLS];

	CyberlineMidiMap() {
		config(NUM_PARAMS, NUM_INPUTS_WITH_SYNC, 0, 0);
		for (int c = 0; c < NUM_CELLS; c++)
			configParam<CellParamQuantity>(CELL0_PARAM + c, 0.f, 1.f, 0.f, "Cell target");
		configInput(CELL0_INPUT, "Cell 1 CV");
		configInput(SYNC_INPUT, "Sync (resend all values on trigger)");
		setSpec(SPEC_BLOFELD);
		for (int c = 0; c < NUM_CELLS; c++)
			telValue[c].store(0, std::memory_order_relaxed);
	}

	const MidiMapSpec* spec() const {
		switch (specIndex) {
			case SPEC_BLOFELD: {
				static MidiMapSpec blofeld; // built once, shared read-only
				static bool init = false;
				if (!init) {
					blofeld = makeBlofeldSpec();
					init = true;
				}
				return &blofeld;
			}
			case SPEC_ULTRANOVA: {
				static MidiMapSpec ultranova;
				static bool init = false;
				if (!init) {
					ultranova = makeUltraNovaSpec();
					init = true;
				}
				return &ultranova;
			}
			case SPEC_GENERIC: {
				static MidiMapSpec generic;
				static bool init = false;
				if (!init) {
					generic = makeGenericSpec();
					init = true;
				}
				return &generic;
			}
			default:
				return &loadedSpec;
		}
	}

	// Select a spec. Re-targets every cell by parameter NAME where possible
	// (a cell assigned "Filter 1 cutoff" finds the same name in the new
	// spec; unmatched cells keep their index clamped into the new list).
	// Select a spec. With applyDefaults (user picked a spec, fresh module,
	// loaded a JSON spec), cells are assigned the spec's default parameter
	// set — skipping targets already held by any other MIDI Map instance
	// or cell on the same effective channel. With applyDefaults=false
	// (patch load), the engine-restored cell targets are kept as-is and
	// only clamped into the new spec's range.
	void setSpec(int index, bool applyDefaults = true) {
		specIndex = rack::math::clamp(index, 0, SPEC_LOADED);
		if (specIndex == SPEC_LOADED && loadedSpec.params.empty())
			specIndex = SPEC_BLOFELD; // nothing loaded yet
		const MidiMapSpec* s = spec();
		float maxIdx = (float) (s->params.size() - 1);
		for (int c = 0; c < NUM_CELLS; c++) {
			// Set the quantity range directly (NOT applyParamRange, which
			// never widens past the configured 0..1 config range — that
			// clamped every target into 0/1 and collapsed all cells onto
			// the first spec entries). Cell targets legitimately span the
			// whole spec, and the spec width changes on every switch.
			auto* pq = paramQuantities[CELL0_PARAM + c];
			if (pq) {
				pq->minValue = 0.f;
				pq->maxValue = maxIdx;
			}
			// Keep the raw value inside the range (patch-load preserve
			// path; the defaults path overwrites below anyway).
			float v = params[CELL0_PARAM + c].getValue();
			if (v > maxIdx)
				params[CELL0_PARAM + c].setValue(maxIdx);
		}
		if (applyDefaults)
			applySpecDefaultsLive();
		// Targets (re)assigned: force a re-send of everything.
		midiOutput.resetValues();
	}

	// Assign the spec's default parameter set to the cells, avoiding
	// targets already in `uses` (other instances) AND targets picked by
	// earlier cells of this loop (recorded into a local copy — without
	// that, every fallback cell would converge on the same first-free
	// parameter and fight each other). Pure (testable headless); the live
	// wrapper scans the engine.
	//
	// Pick order: cell c prefers the spec's c-th default; when absent or
	// taken, the first parameter in ascending target order (CC 0 up, then
	// NRPNs by address) that no one holds. When every target on this
	// channel is taken, the cell is DISABLED (transmits nothing) instead
	// of colliding — the assign menu can still pick a target, which
	// re-enables it (user trades places with whoever holds that target).
	void applySpecDefaults(const std::vector<MidiMapTargetUse>& uses) {
		const MidiMapSpec* s = spec();
		int n = (int) s->params.size();
		int channel = effectiveChannel();
		auto keyOf = [&](int idx) {
			MidiMapTargetKey key;
			const MidiMapParam& p = s->params[idx];
			key.nrpn = p.isNrpn();
			key.addr = p.isNrpn() ? p.nrpnMsb * 128 + p.nrpnLsb : p.cc;
			key.channel = channel;
			return key;
		};
		std::vector<MidiMapTargetUse> taken = uses;
		auto isTaken = [&](int idx) {
			MidiMapTargetKey k = keyOf(idx);
			for (auto& u : taken)
				if (u.key == k)
					return true;
			return false;
		};
		// Params in ascending target order (CC 0.. first, then NRPNs).
		std::vector<int> order(n);
		for (int i = 0; i < n; i++)
			order[i] = i;
		std::sort(order.begin(), order.end(),
			[&](int a, int b) { return keyOf(a) < keyOf(b); });

		for (int c = 0; c < NUM_CELLS; c++) {
			cellDisabled[c] = false;
			int pick = -1;
			if (c < (int) s->defaults.size()) {
				int cand = s->defaults[c];
				if (cand >= 0 && cand < n && !isTaken(cand))
					pick = cand;
			}
			if (pick < 0) {
				for (int oi = 0; oi < n; oi++) {
					int i = order[oi];
					if (!isTaken(i)) {
						pick = i;
						break;
					}
				}
			}
			if (pick < 0) {
				// Every target on this channel is held: disable the cell.
				cellDisabled[c] = true;
				params[CELL0_PARAM + c].setValue(0.f);
				continue;
			}
			params[CELL0_PARAM + c].setValue((float) pick);
			// Record this pick so later cells don't claim the same target.
			MidiMapTargetUse u;
			u.key = keyOf(pick);
			u.moduleId = this->id;
			u.cell = c;
			u.synth = s->synth;
			taken.push_back(u);
		}
	}

	// Live wrapper: scan the engine for OTHER instances' uses (self is
	// excluded — a fresh module re-claiming its own defaults is fine).
	void applySpecDefaultsLive() {
		std::vector<MidiMapTargetUse> uses;
		if (rack::contextGet() && rack::contextGet()->engine) {
			for (int64_t id : rack::contextGet()->engine->getModuleIds()) {
				if (id == this->id)
					continue;
				auto* mod = rack::contextGet()->engine->getModule(id);
				auto* mm = dynamic_cast<CyberlineMidiMap*>(mod);
				if (mm)
					collectSelfUses(mm, id, &uses);
			}
		}
		applySpecDefaults(uses);
	}

	bool loadSpecJson(const char* text, const std::string& pathLabel, std::string* error) {
		MidiMapSpec s;
		if (!parseMidiMapSpecJson(text, &s, error))
			return false;
		loadedSpec = s;
		loadedPath = pathLabel;
		setSpec(SPEC_LOADED, true);
		return true;
	}

	// CV volts -> 7-bit value for one parameter. Orientation-aware:
	// standard = 0..+10 V, centered = -5..+5 V (LFO-friendly).
	static uint8_t voltsToValue(const MidiMapParam& p, float volts) {
		float norm = p.centered
			? rack::math::clamp((volts + 5.f) / 10.f, 0.f, 1.f)
			: rack::math::clamp(volts / 10.f, 0.f, 1.f);
		float v = p.min + norm * (p.max - p.min);
		long lo = (long) std::lround(p.min), hi = (long) std::lround(p.max);
		long lv = std::lround(v);
		if (lv < lo)
			lv = lo;
		if (lv > hi)
			lv = hi;
		return (uint8_t) lv;
	}

	// Neutral position of a parameter (unpatched cell rest value).
	static uint8_t neutralValue(const MidiMapParam& p) {
		return voltsToValue(p, p.centered ? 0.f : 5.f);
	}

	void process(const ProcessArgs& args) override {
		bool measure = cpuMeter.frameStart();
		// SYNC trigger: a rising edge clears the change-gate so every cell
		// retransmits on the next rate-limiter tick (clocked resync, e.g.
		// after hand-editing hardware knobs or a preset change). Edge is
		// detected per-sample so short pulses can't slip between ticks.
		bool syncHigh = inputs[SYNC_INPUT].getVoltage() >= 1.f;
		if (syncHigh && !syncWasHigh) {
			midiOutput.resetValues();
		}
		syncWasHigh = syncHigh;
		// 200 Hz send rate (matches Core's CV-CC rate limiter); CV is
		// sampled and change-gated there so the MIDI line isn't flooded.
		const float rateLimiterPeriod = 1 / 200.f;
		bool rateLimiterTriggered = (rateLimiterTimer.process(args.sampleTime) >= rateLimiterPeriod);
		if (rateLimiterTriggered)
			rateLimiterTimer.time -= rateLimiterPeriod;
		else {
			cpuMeter.frameEnd(measure, args.sampleRate);
			return;
		}

		midiOutput.frame = args.frame;
#ifdef USING_CARDINAL_NOT_RACK
		// Cardinal: the host expects block-relative event frames; compute
		// the offset from the current process block (CardinalCommon's own
		// InputQueue does the same with engine->getBlockFrame()).
		{
			auto* pc = static_cast<CardinalPluginContext*>(APP);
			if (pc && pc->engine) {
				int64_t blockStart = pc->engine->getBlockFrame();
				midiOutput.frame = args.frame > blockStart ? args.frame - blockStart : 0;
			} else
				midiOutput.frame = 0;
		}
#endif
		const MidiMapSpec* s = spec();
		for (int c = 0; c < NUM_CELLS; c++) {
			if (cellDisabled[c]) {
				// Disabled cell transmits nothing; bar shows empty.
				telValue[c].store(0, std::memory_order_relaxed);
				continue;
			}
			const MidiMapParam* p = s->paramAt((int) std::lround(params[CELL0_PARAM + c].getValue()));
			if (!p)
				continue;
			auto& in = inputs[CELL0_INPUT + c];
			uint8_t value = in.isConnected()
				? voltsToValue(*p, in.getVoltage())
				: neutralValue(*p);
			midiOutput.sendParam(*p, value);
			// Atomic cell meter for the UI thread (also covers the
			// change-gated case: value equals what was last sent).
			telValue[c].store(value, std::memory_order_relaxed);
		}
		cpuMeter.frameEnd(measure, args.sampleRate);
	}

	// Force every cell to retransmit its current value (after patch load
	// or a synth state change). Effect is deferred to the next process
	// tick: change-gating is reset, so each cell re-sends once.
	void resendAll() {
		midiOutput.resetValues();
	}

	void onReset() override {
		midiOutput.reset();
		syncWasHigh = false;
		resendAll();
	}

	// Effective MIDI channel the port transmits on (port channel < 0 means
	// the message's own nibble, which sendController leaves at 0 = ch 1).
	int effectiveChannel() const {
		return midiOutput.channel < 0 ? 0 : midiOutput.channel;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "specIndex", json_integer(specIndex));
		if (specIndex == SPEC_LOADED)
			json_object_set_new(rootJ, "loadedPath", json_string(loadedPath.c_str()));
		json_object_set_new(rootJ, "midi", midiOutput.toJson());
		json_t* disJ = json_array();
		for (int c = 0; c < NUM_CELLS; c++)
			json_array_append_new(disJ, json_boolean(cellDisabled[c]));
		json_object_set_new(rootJ, "cellDisabled", disJ);
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* j;
		j = json_object_get(rootJ, "specIndex");
		if (j && json_is_integer(j))
			specIndex = rack::math::clamp((int) json_integer_value(j), 0, SPEC_LOADED);
		j = json_object_get(rootJ, "loadedPath");
		if (j && json_is_string(j))
			loadedPath = json_string_value(j);
		// Note: a *loaded* spec's param table itself cannot round-trip
		// through the patch JSON (we only keep its file label). Cells
		// re-target by name against the built-ins; for a truly portable
		// patch, use a built-in spec. Documented in docs/spec.md.
		if (specIndex == SPEC_LOADED && loadedSpec.params.empty()) {
			// Spec file not (yet) re-loaded: fall back to Blofeld so the
			// labels are at least sane; the user can re-load the file.
			specIndex = SPEC_BLOFELD;
		}
		setSpec(specIndex, false);
		j = json_object_get(rootJ, "cellDisabled");
		if (j && json_is_array(j)) {
			size_t di;
			json_t* dv;
			json_array_foreach(j, di, dv) {
				if (di >= NUM_CELLS)
					break;
				cellDisabled[di] = json_is_true(dv);
			}
		}
		j = json_object_get(rootJ, "midi");
		if (j)
			midiOutput.fromJson(j);
		resendAll();
	}
};

inline std::string CellParamQuantity::getDisplayValueString() {
	auto* m = dynamic_cast<CyberlineMidiMap*>(module);
	if (!m)
		return ParamQuantity::getDisplayValueString();
	const MidiMapSpec* s = m->spec();
	const MidiMapParam* p = s->paramAt((int) std::lround(getValue()));
	if (!p)
		return "?";
	return p->name + " (" + p->tag() + ")";
}

// One module's own uses (pure, testable headless). Non-const module: Rack's
// Param::getValue is not const-qualified.
inline void collectSelfUses(CyberlineMidiMap* m, int64_t moduleId,
		std::vector<MidiMapTargetUse>* out) {
	if (!m)
		return;
	const MidiMapSpec* s = m->spec();
	int channel = m->effectiveChannel();
	for (int c = 0; c < CyberlineMidiMap::NUM_CELLS; c++) {
		if (m->cellDisabled[c])
			continue; // disabled cells hold no target
		const MidiMapParam* p = s->paramAt(
			(int) std::lround(m->params[CyberlineMidiMap::CELL0_PARAM + c].getValue()));
		if (!p)
			continue;
		MidiMapTargetUse u;
		u.key.nrpn = p->isNrpn();
		u.key.addr = p->isNrpn() ? p->nrpnMsb * 128 + p->nrpnLsb : p->cc;
		u.key.channel = channel;
		u.moduleId = moduleId;
		u.cell = c;
		u.synth = s->synth;
		out->push_back(u);
	}
}

// All CyberlineMidiMap instances in the engine (live app only; headless
// contexts without an engine get an empty scan). Used by the assign menu
// to grey out targets already held by any cell of any instance.
inline std::vector<MidiMapTargetUse> collectMidiMapUses(const CyberlineMidiMap* self) {
	std::vector<MidiMapTargetUse> uses;
	if (!rack::contextGet() || !rack::contextGet()->engine)
		return uses;
	auto ids = rack::contextGet()->engine->getModuleIds();
	for (int64_t id : ids) {
		auto* mod = rack::contextGet()->engine->getModule(id);
		auto* mm = dynamic_cast<CyberlineMidiMap*>(mod);
		if (!mm)
			continue;
		collectSelfUses(mm, id, &uses);
	}
	return uses;
}

extern rack::plugin::Model* modelCyberlineMidiMap;

} // namespace cyberline
