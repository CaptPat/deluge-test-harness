// Tests for MIDIFollow pure-logic: CC-to-param mapping tables, bidirectional
// consistency, clipForLastNoteReceived array management, and getCCFromParam
// lookup logic. The real MidiFollow class cannot be compiled in the harness
// (too many GUI/audio deps), so we reimplement the extractable logic and
// verify its invariants and edge cases.

#include "CppUTest/TestHarness.h"

#include "modulation/params/param.h"
#include <array>
#include <cstdint>
#include <cstring>

namespace params = deluge::modulation::params;

// ── Constants mirroring midi_follow.cpp ─────────────────────────────────
// kMaxMIDIValue and MIDI_CC_NONE come from definitions_cxx.hpp (force-included)

static constexpr int32_t PARAM_ID_NONE = 255;

// ── Reimplemented mapping tables ────────────────────────────────────────
// These reproduce initDefaultMappings() exactly, so we can verify invariants
// without pulling in the full MidiFollow class.

struct MidiFollowMappings {
	std::array<uint8_t, kMaxMIDIValue + 1> ccToSoundParam;
	std::array<uint8_t, kMaxMIDIValue + 1> ccToGlobalParam;
	std::array<uint8_t, params::UNPATCHED_START + params::UNPATCHED_SOUND_MAX_NUM> soundParamToCC;
	std::array<uint8_t, params::UNPATCHED_GLOBAL_MAX_NUM> globalParamToCC;

	void clear() {
		ccToSoundParam.fill(PARAM_ID_NONE);
		ccToGlobalParam.fill(PARAM_ID_NONE);
		soundParamToCC.fill(MIDI_CC_NONE);
		globalParamToCC.fill(MIDI_CC_NONE);
	}

	void initDefaults() {
		clear();

		// SOUND PARAMS — exact copy from midi_follow.cpp initDefaultMappings()
		ccToSoundParam[3] = params::LOCAL_PITCH_ADJUST;
		soundParamToCC[params::LOCAL_PITCH_ADJUST] = 3;
		ccToSoundParam[5] = params::UNPATCHED_START + params::UNPATCHED_PORTAMENTO;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_PORTAMENTO] = 5;
		ccToSoundParam[7] = params::GLOBAL_VOLUME_POST_FX;
		soundParamToCC[params::GLOBAL_VOLUME_POST_FX] = 7;
		ccToSoundParam[10] = params::LOCAL_PAN;
		soundParamToCC[params::LOCAL_PAN] = 10;
		ccToSoundParam[12] = params::LOCAL_OSC_A_PITCH_ADJUST;
		soundParamToCC[params::LOCAL_OSC_A_PITCH_ADJUST] = 12;
		ccToSoundParam[13] = params::LOCAL_OSC_B_PITCH_ADJUST;
		soundParamToCC[params::LOCAL_OSC_B_PITCH_ADJUST] = 13;
		ccToSoundParam[14] = params::LOCAL_MODULATOR_0_PITCH_ADJUST;
		soundParamToCC[params::LOCAL_MODULATOR_0_PITCH_ADJUST] = 14;
		ccToSoundParam[15] = params::LOCAL_MODULATOR_1_PITCH_ADJUST;
		soundParamToCC[params::LOCAL_MODULATOR_1_PITCH_ADJUST] = 15;
		ccToSoundParam[16] = params::GLOBAL_MOD_FX_RATE;
		soundParamToCC[params::GLOBAL_MOD_FX_RATE] = 16;
		ccToSoundParam[17] = params::UNPATCHED_START + params::UNPATCHED_MOD_FX_FEEDBACK;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_MOD_FX_FEEDBACK] = 17;
		ccToSoundParam[18] = params::UNPATCHED_START + params::UNPATCHED_MOD_FX_OFFSET;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_MOD_FX_OFFSET] = 18;
		ccToSoundParam[19] = params::LOCAL_FOLD;
		soundParamToCC[params::LOCAL_FOLD] = 19;
		ccToSoundParam[20] = params::UNPATCHED_START + params::UNPATCHED_STUTTER_RATE;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_STUTTER_RATE] = 20;
		ccToSoundParam[21] = params::LOCAL_OSC_A_VOLUME;
		soundParamToCC[params::LOCAL_OSC_A_VOLUME] = 21;
		ccToSoundParam[23] = params::LOCAL_OSC_A_PHASE_WIDTH;
		soundParamToCC[params::LOCAL_OSC_A_PHASE_WIDTH] = 23;
		ccToSoundParam[24] = params::LOCAL_CARRIER_0_FEEDBACK;
		soundParamToCC[params::LOCAL_CARRIER_0_FEEDBACK] = 24;
		ccToSoundParam[25] = params::LOCAL_OSC_A_WAVE_INDEX;
		soundParamToCC[params::LOCAL_OSC_A_WAVE_INDEX] = 25;
		ccToSoundParam[26] = params::LOCAL_OSC_B_VOLUME;
		soundParamToCC[params::LOCAL_OSC_B_VOLUME] = 26;
		ccToSoundParam[27] = params::UNPATCHED_START + params::UNPATCHED_COMPRESSOR_THRESHOLD;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_COMPRESSOR_THRESHOLD] = 27;
		ccToSoundParam[28] = params::LOCAL_OSC_B_PHASE_WIDTH;
		soundParamToCC[params::LOCAL_OSC_B_PHASE_WIDTH] = 28;
		ccToSoundParam[29] = params::LOCAL_CARRIER_1_FEEDBACK;
		soundParamToCC[params::LOCAL_CARRIER_1_FEEDBACK] = 29;
		// Note: CC30 overwrites OSC_A_WAVE_INDEX (same param as CC25 — last-write-wins)
		ccToSoundParam[30] = params::LOCAL_OSC_A_WAVE_INDEX;
		soundParamToCC[params::LOCAL_OSC_A_WAVE_INDEX] = 30;
		ccToSoundParam[36] = params::UNPATCHED_START + params::UNPATCHED_REVERSE_PROBABILITY;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_REVERSE_PROBABILITY] = 36;
		ccToSoundParam[37] = params::UNPATCHED_START + params::UNPATCHED_SPREAD_VELOCITY;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_SPREAD_VELOCITY] = 37;
		ccToSoundParam[39] = params::UNPATCHED_START + params::UNPATCHED_ARP_SPREAD_OCTAVE;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_ARP_SPREAD_OCTAVE] = 39;
		ccToSoundParam[40] = params::UNPATCHED_START + params::UNPATCHED_ARP_SPREAD_GATE;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_ARP_SPREAD_GATE] = 40;
		ccToSoundParam[41] = params::LOCAL_NOISE_VOLUME;
		soundParamToCC[params::LOCAL_NOISE_VOLUME] = 41;
		ccToSoundParam[42] = params::UNPATCHED_START + params::UNPATCHED_ARP_RHYTHM;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_ARP_RHYTHM] = 42;
		ccToSoundParam[43] = params::UNPATCHED_START + params::UNPATCHED_ARP_SEQUENCE_LENGTH;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_ARP_SEQUENCE_LENGTH] = 43;
		ccToSoundParam[44] = params::UNPATCHED_START + params::UNPATCHED_ARP_CHORD_POLYPHONY;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_ARP_CHORD_POLYPHONY] = 44;
		ccToSoundParam[45] = params::UNPATCHED_START + params::UNPATCHED_ARP_RATCHET_AMOUNT;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_ARP_RATCHET_AMOUNT] = 45;
		ccToSoundParam[46] = params::UNPATCHED_START + params::UNPATCHED_NOTE_PROBABILITY;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_NOTE_PROBABILITY] = 46;
		ccToSoundParam[47] = params::UNPATCHED_START + params::UNPATCHED_ARP_BASS_PROBABILITY;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_ARP_BASS_PROBABILITY] = 47;
		ccToSoundParam[48] = params::UNPATCHED_START + params::UNPATCHED_ARP_CHORD_PROBABILITY;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_ARP_CHORD_PROBABILITY] = 48;
		ccToSoundParam[49] = params::UNPATCHED_START + params::UNPATCHED_ARP_RATCHET_PROBABILITY;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_ARP_RATCHET_PROBABILITY] = 49;
		ccToSoundParam[50] = params::UNPATCHED_START + params::UNPATCHED_ARP_GATE;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_ARP_GATE] = 50;
		ccToSoundParam[51] = params::GLOBAL_ARP_RATE;
		soundParamToCC[params::GLOBAL_ARP_RATE] = 51;
		ccToSoundParam[52] = params::GLOBAL_DELAY_FEEDBACK;
		soundParamToCC[params::GLOBAL_DELAY_FEEDBACK] = 52;
		ccToSoundParam[53] = params::GLOBAL_DELAY_RATE;
		soundParamToCC[params::GLOBAL_DELAY_RATE] = 53;
		ccToSoundParam[54] = params::LOCAL_MODULATOR_0_VOLUME;
		soundParamToCC[params::LOCAL_MODULATOR_0_VOLUME] = 54;
		ccToSoundParam[55] = params::LOCAL_MODULATOR_0_FEEDBACK;
		soundParamToCC[params::LOCAL_MODULATOR_0_FEEDBACK] = 55;
		ccToSoundParam[56] = params::LOCAL_MODULATOR_1_VOLUME;
		soundParamToCC[params::LOCAL_MODULATOR_1_VOLUME] = 56;
		ccToSoundParam[57] = params::LOCAL_MODULATOR_1_FEEDBACK;
		soundParamToCC[params::LOCAL_MODULATOR_1_FEEDBACK] = 57;
		ccToSoundParam[58] = params::GLOBAL_LFO_FREQ_1;
		soundParamToCC[params::GLOBAL_LFO_FREQ_1] = 58;
		ccToSoundParam[59] = params::LOCAL_LFO_LOCAL_FREQ_1;
		soundParamToCC[params::LOCAL_LFO_LOCAL_FREQ_1] = 59;
		ccToSoundParam[60] = params::UNPATCHED_START + params::UNPATCHED_SIDECHAIN_SHAPE;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_SIDECHAIN_SHAPE] = 60;
		ccToSoundParam[62] = params::UNPATCHED_START + params::UNPATCHED_BITCRUSHING;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_BITCRUSHING] = 62;
		ccToSoundParam[63] = params::UNPATCHED_START + params::UNPATCHED_SAMPLE_RATE_REDUCTION;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_SAMPLE_RATE_REDUCTION] = 63;
		ccToSoundParam[70] = params::LOCAL_LPF_MORPH;
		soundParamToCC[params::LOCAL_LPF_MORPH] = 70;
		ccToSoundParam[71] = params::LOCAL_LPF_RESONANCE;
		soundParamToCC[params::LOCAL_LPF_RESONANCE] = 71;
		ccToSoundParam[72] = params::LOCAL_ENV_0_RELEASE;
		soundParamToCC[params::LOCAL_ENV_0_RELEASE] = 72;
		ccToSoundParam[73] = params::LOCAL_ENV_0_ATTACK;
		soundParamToCC[params::LOCAL_ENV_0_ATTACK] = 73;
		ccToSoundParam[74] = params::LOCAL_LPF_FREQ;
		soundParamToCC[params::LOCAL_LPF_FREQ] = 74;
		ccToSoundParam[75] = params::LOCAL_ENV_0_DECAY;
		soundParamToCC[params::LOCAL_ENV_0_DECAY] = 75;
		ccToSoundParam[76] = params::LOCAL_ENV_0_SUSTAIN;
		soundParamToCC[params::LOCAL_ENV_0_SUSTAIN] = 76;
		ccToSoundParam[77] = params::LOCAL_ENV_1_ATTACK;
		soundParamToCC[params::LOCAL_ENV_1_ATTACK] = 77;
		ccToSoundParam[78] = params::LOCAL_ENV_1_DECAY;
		soundParamToCC[params::LOCAL_ENV_1_DECAY] = 78;
		ccToSoundParam[79] = params::LOCAL_ENV_1_SUSTAIN;
		soundParamToCC[params::LOCAL_ENV_1_SUSTAIN] = 79;
		ccToSoundParam[80] = params::LOCAL_ENV_1_RELEASE;
		soundParamToCC[params::LOCAL_ENV_1_RELEASE] = 80;
		ccToSoundParam[81] = params::LOCAL_HPF_FREQ;
		soundParamToCC[params::LOCAL_HPF_FREQ] = 81;
		ccToSoundParam[82] = params::LOCAL_HPF_RESONANCE;
		soundParamToCC[params::LOCAL_HPF_RESONANCE] = 82;
		ccToSoundParam[83] = params::LOCAL_HPF_MORPH;
		soundParamToCC[params::LOCAL_HPF_MORPH] = 83;
		ccToSoundParam[84] = params::UNPATCHED_START + params::UNPATCHED_BASS_FREQ;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_BASS_FREQ] = 84;
		ccToSoundParam[85] = params::UNPATCHED_START + params::UNPATCHED_TREBLE_FREQ;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_TREBLE_FREQ] = 85;
		ccToSoundParam[86] = params::UNPATCHED_START + params::UNPATCHED_BASS;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_BASS] = 86;
		ccToSoundParam[87] = params::UNPATCHED_START + params::UNPATCHED_TREBLE;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_TREBLE] = 87;
		ccToSoundParam[91] = params::GLOBAL_REVERB_AMOUNT;
		soundParamToCC[params::GLOBAL_REVERB_AMOUNT] = 91;
		ccToSoundParam[93] = params::GLOBAL_MOD_FX_DEPTH;
		soundParamToCC[params::GLOBAL_MOD_FX_DEPTH] = 93;
		ccToSoundParam[102] = params::LOCAL_ENV_2_ATTACK;
		soundParamToCC[params::LOCAL_ENV_2_ATTACK] = 102;
		ccToSoundParam[103] = params::LOCAL_ENV_2_DECAY;
		soundParamToCC[params::LOCAL_ENV_2_DECAY] = 103;
		ccToSoundParam[104] = params::LOCAL_ENV_2_SUSTAIN;
		soundParamToCC[params::LOCAL_ENV_2_SUSTAIN] = 104;
		ccToSoundParam[105] = params::LOCAL_ENV_2_RELEASE;
		soundParamToCC[params::LOCAL_ENV_2_RELEASE] = 105;
		ccToSoundParam[106] = params::LOCAL_ENV_3_ATTACK;
		soundParamToCC[params::LOCAL_ENV_3_ATTACK] = 106;
		ccToSoundParam[107] = params::LOCAL_ENV_3_DECAY;
		soundParamToCC[params::LOCAL_ENV_3_DECAY] = 107;
		ccToSoundParam[108] = params::LOCAL_ENV_3_SUSTAIN;
		soundParamToCC[params::LOCAL_ENV_3_SUSTAIN] = 108;
		ccToSoundParam[109] = params::LOCAL_ENV_3_RELEASE;
		soundParamToCC[params::LOCAL_ENV_3_RELEASE] = 109;
		ccToSoundParam[110] = params::GLOBAL_LFO_FREQ_2;
		soundParamToCC[params::GLOBAL_LFO_FREQ_2] = 110;
		ccToSoundParam[111] = params::LOCAL_LFO_LOCAL_FREQ_2;
		soundParamToCC[params::LOCAL_LFO_LOCAL_FREQ_2] = 111;
		ccToSoundParam[112] = params::UNPATCHED_START + params::UNPATCHED_ARP_SWAP_PROBABILITY;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_ARP_SWAP_PROBABILITY] = 112;
		ccToSoundParam[113] = params::UNPATCHED_START + params::UNPATCHED_ARP_GLIDE_PROBABILITY;
		soundParamToCC[params::UNPATCHED_START + params::UNPATCHED_ARP_GLIDE_PROBABILITY] = 113;

		// GLOBAL PARAMS
		ccToGlobalParam[3] = params::UNPATCHED_PITCH_ADJUST;
		globalParamToCC[params::UNPATCHED_PITCH_ADJUST] = 3;
		ccToGlobalParam[7] = params::UNPATCHED_VOLUME;
		globalParamToCC[params::UNPATCHED_VOLUME] = 7;
		ccToGlobalParam[10] = params::UNPATCHED_PAN;
		globalParamToCC[params::UNPATCHED_PAN] = 10;
		ccToGlobalParam[16] = params::UNPATCHED_MOD_FX_RATE;
		globalParamToCC[params::UNPATCHED_MOD_FX_RATE] = 16;
		ccToGlobalParam[17] = params::UNPATCHED_MOD_FX_FEEDBACK;
		globalParamToCC[params::UNPATCHED_MOD_FX_FEEDBACK] = 17;
		ccToGlobalParam[18] = params::UNPATCHED_MOD_FX_OFFSET;
		globalParamToCC[params::UNPATCHED_MOD_FX_OFFSET] = 18;
		ccToGlobalParam[20] = params::UNPATCHED_STUTTER_RATE;
		globalParamToCC[params::UNPATCHED_STUTTER_RATE] = 20;
		ccToGlobalParam[51] = params::UNPATCHED_ARP_RATE;
		globalParamToCC[params::UNPATCHED_ARP_RATE] = 51;
		ccToGlobalParam[52] = params::UNPATCHED_DELAY_AMOUNT;
		globalParamToCC[params::UNPATCHED_DELAY_AMOUNT] = 52;
		ccToGlobalParam[53] = params::UNPATCHED_DELAY_RATE;
		globalParamToCC[params::UNPATCHED_DELAY_RATE] = 53;
		ccToGlobalParam[60] = params::UNPATCHED_SIDECHAIN_SHAPE;
		globalParamToCC[params::UNPATCHED_SIDECHAIN_SHAPE] = 60;
		ccToGlobalParam[61] = params::UNPATCHED_SIDECHAIN_VOLUME;
		globalParamToCC[params::UNPATCHED_SIDECHAIN_VOLUME] = 61;
		ccToGlobalParam[62] = params::UNPATCHED_BITCRUSHING;
		globalParamToCC[params::UNPATCHED_BITCRUSHING] = 62;
		ccToGlobalParam[63] = params::UNPATCHED_SAMPLE_RATE_REDUCTION;
		globalParamToCC[params::UNPATCHED_SAMPLE_RATE_REDUCTION] = 63;
		ccToGlobalParam[70] = params::UNPATCHED_LPF_MORPH;
		globalParamToCC[params::UNPATCHED_LPF_MORPH] = 70;
		ccToGlobalParam[71] = params::UNPATCHED_LPF_RES;
		globalParamToCC[params::UNPATCHED_LPF_RES] = 71;
		ccToGlobalParam[74] = params::UNPATCHED_LPF_FREQ;
		globalParamToCC[params::UNPATCHED_LPF_FREQ] = 74;
		ccToGlobalParam[81] = params::UNPATCHED_HPF_FREQ;
		globalParamToCC[params::UNPATCHED_HPF_FREQ] = 81;
		ccToGlobalParam[82] = params::UNPATCHED_HPF_RES;
		globalParamToCC[params::UNPATCHED_HPF_RES] = 82;
		ccToGlobalParam[83] = params::UNPATCHED_HPF_MORPH;
		globalParamToCC[params::UNPATCHED_HPF_MORPH] = 83;
		ccToGlobalParam[84] = params::UNPATCHED_BASS_FREQ;
		globalParamToCC[params::UNPATCHED_BASS_FREQ] = 74; // Note: shares CC with LPF_FREQ
		ccToGlobalParam[85] = params::UNPATCHED_TREBLE_FREQ;
		globalParamToCC[params::UNPATCHED_TREBLE_FREQ] = 81; // Note: shares CC with HPF_FREQ
		ccToGlobalParam[86] = params::UNPATCHED_BASS;
		globalParamToCC[params::UNPATCHED_BASS] = 71; // Note: shares CC with LPF_RES
		ccToGlobalParam[87] = params::UNPATCHED_TREBLE;
		globalParamToCC[params::UNPATCHED_TREBLE] = 82; // Note: shares CC with HPF_RES
		ccToGlobalParam[91] = params::UNPATCHED_REVERB_SEND_AMOUNT;
		globalParamToCC[params::UNPATCHED_REVERB_SEND_AMOUNT] = 91;
		ccToGlobalParam[93] = params::UNPATCHED_MOD_FX_DEPTH;
		globalParamToCC[params::UNPATCHED_MOD_FX_DEPTH] = 93;
	}

	// Reimplements getCCFromParam for synth/kit-row context (non-global-effectable)
	int32_t getCCFromParamSound(params::Kind paramKind, int32_t paramID) const {
		if (paramKind == params::Kind::PATCHED) {
			return soundParamToCC[paramID];
		}
		else if (paramKind == params::Kind::UNPATCHED_SOUND) {
			return soundParamToCC[params::UNPATCHED_START + paramID];
		}
		return MIDI_CC_NONE;
	}

	// Reimplements getCCFromParam for global-effectable context (audio clip, kit affect-entire)
	int32_t getCCFromParamGlobal(params::Kind paramKind, int32_t paramID) const {
		if (paramKind == params::Kind::UNPATCHED_GLOBAL) {
			return globalParamToCC[paramID];
		}
		return MIDI_CC_NONE;
	}
};

// ── Reimplemented clipForLastNoteReceived management ────────────────────
// These mirror the array operations in midi_follow.cpp without needing
// the full Clip type — we just use opaque pointers.

struct ClipNoteTracker {
	void* clips[128] = {};

	void clear() {
		for (int i = 0; i < 128; i++) {
			clips[i] = nullptr;
		}
	}

	void removeClip(void* clip) {
		for (int i = 0; i < 128; i++) {
			if (clips[i] == clip) {
				clips[i] = nullptr;
			}
		}
	}

	void noteOn(int note, void* clip) {
		if (note >= 0 && note < 128) {
			clips[note] = clip;
		}
	}

	void noteOff(int note) {
		if (note >= 0 && note < 128) {
			clips[note] = nullptr;
		}
	}

	void* getClipForNote(int note) const {
		if (note >= 0 && note < 128) {
			return clips[note];
		}
		return nullptr;
	}
};

// ═══════════════════════════════════════════════════════════════════════
// TEST GROUP: MidiFollowMappings — CC-to-param table invariants
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(MidiFollowMappings) {
	MidiFollowMappings m;

	void setup() override { m.initDefaults(); }
};

// ── Clear state ─────────────────────────────────────────────────────────

TEST(MidiFollowMappings, ClearSetsAllToNone) {
	m.clear();
	for (int cc = 0; cc <= kMaxMIDIValue; cc++) {
		CHECK_EQUAL(PARAM_ID_NONE, m.ccToSoundParam[cc]);
		CHECK_EQUAL(PARAM_ID_NONE, m.ccToGlobalParam[cc]);
	}
	for (size_t i = 0; i < m.soundParamToCC.size(); i++) {
		CHECK_EQUAL(MIDI_CC_NONE, m.soundParamToCC[i]);
	}
	for (size_t i = 0; i < m.globalParamToCC.size(); i++) {
		CHECK_EQUAL(MIDI_CC_NONE, m.globalParamToCC[i]);
	}
}

// ── Spot-check well-known CC assignments ────────────────────────────────

TEST(MidiFollowMappings, CC7IsVolume) {
	CHECK_EQUAL(params::GLOBAL_VOLUME_POST_FX, m.ccToSoundParam[7]);
}

TEST(MidiFollowMappings, CC74IsLPFFreq) {
	CHECK_EQUAL(params::LOCAL_LPF_FREQ, m.ccToSoundParam[74]);
}

TEST(MidiFollowMappings, CC71IsLPFResonance) {
	CHECK_EQUAL(params::LOCAL_LPF_RESONANCE, m.ccToSoundParam[71]);
}

TEST(MidiFollowMappings, CC73IsEnvAttack) {
	CHECK_EQUAL(params::LOCAL_ENV_0_ATTACK, m.ccToSoundParam[73]);
}

TEST(MidiFollowMappings, CC72IsEnvRelease) {
	CHECK_EQUAL(params::LOCAL_ENV_0_RELEASE, m.ccToSoundParam[72]);
}

TEST(MidiFollowMappings, CC10IsPan) {
	CHECK_EQUAL(params::LOCAL_PAN, m.ccToSoundParam[10]);
}

TEST(MidiFollowMappings, CC91IsReverb) {
	CHECK_EQUAL(params::GLOBAL_REVERB_AMOUNT, m.ccToSoundParam[91]);
}

TEST(MidiFollowMappings, CC3IsPitchAdjust) {
	CHECK_EQUAL(params::LOCAL_PITCH_ADJUST, m.ccToSoundParam[3]);
}

// ── Unmapped CCs remain PARAM_ID_NONE ───────────────────────────────────

TEST(MidiFollowMappings, UnmappedCCsAreNone) {
	// CC0, CC1, CC2, CC4 are not mapped in defaults
	CHECK_EQUAL(PARAM_ID_NONE, m.ccToSoundParam[0]);
	CHECK_EQUAL(PARAM_ID_NONE, m.ccToSoundParam[1]);
	CHECK_EQUAL(PARAM_ID_NONE, m.ccToSoundParam[2]);
	CHECK_EQUAL(PARAM_ID_NONE, m.ccToSoundParam[4]);
	// CC64 (sustain) is deliberately not mapped to MIDI Follow
	CHECK_EQUAL(PARAM_ID_NONE, m.ccToSoundParam[64]);
}

// ── Bidirectional consistency: sound param table ────────────────────────

TEST(MidiFollowMappings, SoundParamForwardLookupConsistency) {
	// For every CC that maps to a sound param, the reverse lookup should
	// yield a CC (though not necessarily the same one due to the CC30/CC25
	// overlap on LOCAL_OSC_A_WAVE_INDEX).
	for (int cc = 0; cc <= kMaxMIDIValue; cc++) {
		uint8_t paramId = m.ccToSoundParam[cc];
		if (paramId != PARAM_ID_NONE) {
			uint8_t reverseCC = m.soundParamToCC[paramId];
			// The reverse mapping should NOT be MIDI_CC_NONE
			CHECK_TEXT(reverseCC != MIDI_CC_NONE,
			           "Sound param has no reverse CC mapping");
			// The reverse CC should map back to the same param
			CHECK_EQUAL(paramId, m.ccToSoundParam[reverseCC]);
		}
	}
}

TEST(MidiFollowMappings, SoundParamReverseLookupConsistency) {
	// For every param that maps to a CC, that CC should map back to this param
	for (size_t paramId = 0; paramId < m.soundParamToCC.size(); paramId++) {
		uint8_t cc = m.soundParamToCC[paramId];
		if (cc != MIDI_CC_NONE) {
			CHECK_EQUAL(paramId, m.ccToSoundParam[cc]);
		}
	}
}

// ── Bidirectional consistency: global param table ───────────────────────

TEST(MidiFollowMappings, GlobalParamForwardLookupConsistency) {
	for (int cc = 0; cc <= kMaxMIDIValue; cc++) {
		uint8_t paramId = m.ccToGlobalParam[cc];
		if (paramId != PARAM_ID_NONE) {
			// For global params, the reverse lookup may have been overwritten
			// (e.g. BASS_FREQ and LPF_FREQ share CC74 in globalParamToCC).
			// At minimum, the reverse CC should not be MIDI_CC_NONE.
			uint8_t reverseCC = m.globalParamToCC[paramId];
			CHECK_TEXT(reverseCC != MIDI_CC_NONE,
			           "Global param has no reverse CC mapping");
		}
	}
}

TEST(MidiFollowMappings, GlobalParamReverseLookupConsistency) {
	for (size_t paramId = 0; paramId < m.globalParamToCC.size(); paramId++) {
		uint8_t cc = m.globalParamToCC[paramId];
		if (cc != MIDI_CC_NONE) {
			// The CC should map to SOME global param (may be overwritten)
			CHECK_TEXT(m.ccToGlobalParam[cc] != PARAM_ID_NONE,
			           "Global CC maps to no param in reverse");
		}
	}
}

// ── Sound/global tables share CCs for related params ────────────────────

TEST(MidiFollowMappings, SoundAndGlobalShareCCForVolume) {
	// CC7 controls GLOBAL_VOLUME_POST_FX in sound context, UNPATCHED_VOLUME in global
	CHECK_EQUAL(params::GLOBAL_VOLUME_POST_FX, m.ccToSoundParam[7]);
	CHECK_EQUAL(params::UNPATCHED_VOLUME, m.ccToGlobalParam[7]);
}

TEST(MidiFollowMappings, SoundAndGlobalShareCCForPan) {
	CHECK_EQUAL(params::LOCAL_PAN, m.ccToSoundParam[10]);
	CHECK_EQUAL(params::UNPATCHED_PAN, m.ccToGlobalParam[10]);
}

TEST(MidiFollowMappings, SoundAndGlobalShareCCForLPF) {
	CHECK_EQUAL(params::LOCAL_LPF_FREQ, m.ccToSoundParam[74]);
	CHECK_EQUAL(params::UNPATCHED_LPF_FREQ, m.ccToGlobalParam[74]);
}

// ── Known duplicate: CC25 and CC30 both map to OSC_A_WAVE_INDEX ─────────

TEST(MidiFollowMappings, CC25AndCC30BothMapToOscAWaveIndex) {
	// Both CCs map to the same param
	CHECK_EQUAL(params::LOCAL_OSC_A_WAVE_INDEX, m.ccToSoundParam[25]);
	CHECK_EQUAL(params::LOCAL_OSC_A_WAVE_INDEX, m.ccToSoundParam[30]);
	// But the reverse lookup only remembers the last one written (CC30)
	CHECK_EQUAL(30, m.soundParamToCC[params::LOCAL_OSC_A_WAVE_INDEX]);
}

// ── Global param overlap: BASS_FREQ/TREBLE_FREQ overwrite in reverse ───

TEST(MidiFollowMappings, GlobalParamBassFreqOverwritesLPFFreqReverse) {
	// globalParamToCC[UNPATCHED_BASS_FREQ] = 74, same as LPF_FREQ
	// globalParamToCC[UNPATCHED_LPF_FREQ] was set to 74, but then
	// globalParamToCC[UNPATCHED_BASS_FREQ] is also set to 74.
	// Forward: CC84 -> UNPATCHED_BASS_FREQ, CC74 -> UNPATCHED_LPF_FREQ
	CHECK_EQUAL(params::UNPATCHED_LPF_FREQ, m.ccToGlobalParam[74]);
	CHECK_EQUAL(params::UNPATCHED_BASS_FREQ, m.ccToGlobalParam[84]);
}

// ── CC64 (sustain) is not mapped ────────────────────────────────────────

TEST(MidiFollowMappings, CC64SustainNotMapped) {
	// CC64 is handled as sustain pedal through the learned MIDI system,
	// not through MIDI Follow param mapping
	CHECK_EQUAL(PARAM_ID_NONE, m.ccToSoundParam[64]);
	CHECK_EQUAL(PARAM_ID_NONE, m.ccToGlobalParam[64]);
}

// ── CC mapping count ────────────────────────────────────────────────────

TEST(MidiFollowMappings, MappedSoundCCCount) {
	int count = 0;
	for (int cc = 0; cc <= kMaxMIDIValue; cc++) {
		if (m.ccToSoundParam[cc] != PARAM_ID_NONE) {
			count++;
		}
	}
	// Should be a substantial number of mapped CCs (60+)
	CHECK_TEXT(count >= 55, "Expected at least 55 mapped sound CCs");
	CHECK_TEXT(count <= 90, "Unexpectedly high number of mapped sound CCs");
}

TEST(MidiFollowMappings, MappedGlobalCCCount) {
	int count = 0;
	for (int cc = 0; cc <= kMaxMIDIValue; cc++) {
		if (m.ccToGlobalParam[cc] != PARAM_ID_NONE) {
			count++;
		}
	}
	// Global params are fewer than sound params
	CHECK_TEXT(count >= 15, "Expected at least 15 mapped global CCs");
	CHECK_TEXT(count <= 40, "Unexpectedly high number of mapped global CCs");
}

// ═══════════════════════════════════════════════════════════════════════
// TEST GROUP: MidiFollowCCLookup — getCCFromParam logic
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(MidiFollowCCLookup) {
	MidiFollowMappings m;

	void setup() override { m.initDefaults(); }
};

TEST(MidiFollowCCLookup, PatchedParamReturnsCC_SoundContext) {
	int32_t cc = m.getCCFromParamSound(params::Kind::PATCHED, params::LOCAL_LPF_FREQ);
	CHECK_EQUAL(74, cc);
}

TEST(MidiFollowCCLookup, UnpatchedSoundParamReturnsCC) {
	int32_t cc = m.getCCFromParamSound(params::Kind::UNPATCHED_SOUND, params::UNPATCHED_PORTAMENTO);
	CHECK_EQUAL(5, cc);
}

TEST(MidiFollowCCLookup, GlobalParamReturnsCCInGlobalContext) {
	int32_t cc = m.getCCFromParamGlobal(params::Kind::UNPATCHED_GLOBAL, params::UNPATCHED_VOLUME);
	CHECK_EQUAL(7, cc);
}

TEST(MidiFollowCCLookup, WrongKindReturnsNone_SoundContext) {
	// UNPATCHED_GLOBAL kind in sound context => MIDI_CC_NONE
	int32_t cc = m.getCCFromParamSound(params::Kind::UNPATCHED_GLOBAL, params::UNPATCHED_VOLUME);
	CHECK_EQUAL(MIDI_CC_NONE, cc);
}

TEST(MidiFollowCCLookup, WrongKindReturnsNone_GlobalContext) {
	// PATCHED kind in global context => MIDI_CC_NONE
	int32_t cc = m.getCCFromParamGlobal(params::Kind::PATCHED, params::LOCAL_LPF_FREQ);
	CHECK_EQUAL(MIDI_CC_NONE, cc);
}

TEST(MidiFollowCCLookup, UnmappedParamReturnsNone) {
	// LOCAL_OSC_B_WAVE_INDEX is not mapped in defaults
	int32_t cc = m.getCCFromParamSound(params::Kind::PATCHED, params::LOCAL_OSC_B_WAVE_INDEX);
	CHECK_EQUAL(MIDI_CC_NONE, cc);
}

TEST(MidiFollowCCLookup, NoneKindAlwaysReturnsNone) {
	int32_t cc = m.getCCFromParamSound(params::Kind::NONE, 0);
	CHECK_EQUAL(MIDI_CC_NONE, cc);
	cc = m.getCCFromParamGlobal(params::Kind::NONE, 0);
	CHECK_EQUAL(MIDI_CC_NONE, cc);
}

// ═══════════════════════════════════════════════════════════════════════
// TEST GROUP: ClipNoteTracker — clipForLastNoteReceived array management
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(ClipNoteTracker){};

TEST(ClipNoteTracker, InitialStateIsAllNull) {
	ClipNoteTracker tracker;
	for (int i = 0; i < 128; i++) {
		POINTERS_EQUAL(nullptr, tracker.getClipForNote(i));
	}
}

TEST(ClipNoteTracker, NoteOnStoresClip) {
	ClipNoteTracker tracker;
	int dummy = 42;
	tracker.noteOn(60, &dummy);
	POINTERS_EQUAL(&dummy, tracker.getClipForNote(60));
}

TEST(ClipNoteTracker, NoteOffClearsClip) {
	ClipNoteTracker tracker;
	int dummy = 42;
	tracker.noteOn(60, &dummy);
	tracker.noteOff(60);
	POINTERS_EQUAL(nullptr, tracker.getClipForNote(60));
}

TEST(ClipNoteTracker, ClearRemovesAll) {
	ClipNoteTracker tracker;
	int a = 1, b = 2, c = 3;
	tracker.noteOn(0, &a);
	tracker.noteOn(60, &b);
	tracker.noteOn(127, &c);

	tracker.clear();

	POINTERS_EQUAL(nullptr, tracker.getClipForNote(0));
	POINTERS_EQUAL(nullptr, tracker.getClipForNote(60));
	POINTERS_EQUAL(nullptr, tracker.getClipForNote(127));
}

TEST(ClipNoteTracker, RemoveClipClearsAllMatchingEntries) {
	ClipNoteTracker tracker;
	int clipA = 1, clipB = 2;
	tracker.noteOn(36, &clipA);
	tracker.noteOn(60, &clipA);
	tracker.noteOn(48, &clipB);

	tracker.removeClip(&clipA);

	POINTERS_EQUAL(nullptr, tracker.getClipForNote(36));
	POINTERS_EQUAL(nullptr, tracker.getClipForNote(60));
	// clipB should still be there
	POINTERS_EQUAL(&clipB, tracker.getClipForNote(48));
}

TEST(ClipNoteTracker, RemoveNonexistentClipIsNoOp) {
	ClipNoteTracker tracker;
	int clipA = 1, clipB = 2;
	tracker.noteOn(60, &clipA);

	tracker.removeClip(&clipB); // not in array

	POINTERS_EQUAL(&clipA, tracker.getClipForNote(60));
}

TEST(ClipNoteTracker, NoteOnOverwritesPreviousClip) {
	ClipNoteTracker tracker;
	int clipA = 1, clipB = 2;
	tracker.noteOn(60, &clipA);
	tracker.noteOn(60, &clipB); // overwrite

	POINTERS_EQUAL(&clipB, tracker.getClipForNote(60));
}

TEST(ClipNoteTracker, OutOfRangeNoteIgnored) {
	ClipNoteTracker tracker;
	int dummy = 42;
	// Should not crash or corrupt
	tracker.noteOn(-1, &dummy);
	tracker.noteOn(128, &dummy);
	tracker.noteOff(-1);
	tracker.noteOff(128);
	POINTERS_EQUAL(nullptr, tracker.getClipForNote(-1));
	POINTERS_EQUAL(nullptr, tracker.getClipForNote(128));
}

TEST(ClipNoteTracker, AllNotesOnThenClear) {
	ClipNoteTracker tracker;
	int dummy = 42;
	for (int i = 0; i < 128; i++) {
		tracker.noteOn(i, &dummy);
	}
	for (int i = 0; i < 128; i++) {
		POINTERS_EQUAL(&dummy, tracker.getClipForNote(i));
	}
	tracker.clear();
	for (int i = 0; i < 128; i++) {
		POINTERS_EQUAL(nullptr, tracker.getClipForNote(i));
	}
}

// ═══════════════════════════════════════════════════════════════════════
// TEST GROUP: MidiFollowParamKindRouting — the param kind classification
// logic used in getModelStackWithParamForSynthClip/KitClip
// ═══════════════════════════════════════════════════════════════════════

namespace {

// Reimplements the param kind determination from getModelStackWithParamForSynthClip
struct SynthClipParamResult {
	params::Kind kind = params::Kind::NONE;
	int32_t paramID = PARAM_ID_NONE;
};

SynthClipParamResult classifySynthParam(int32_t soundParamId) {
	SynthClipParamResult r;
	if (soundParamId != PARAM_ID_NONE && soundParamId < params::UNPATCHED_START) {
		r.kind = params::Kind::PATCHED;
		r.paramID = soundParamId;
	}
	else if (soundParamId != PARAM_ID_NONE && soundParamId >= params::UNPATCHED_START) {
		r.kind = params::Kind::UNPATCHED_SOUND;
		r.paramID = soundParamId - params::UNPATCHED_START;
	}
	return r;
}

// For kit with affect entire = false, same logic as synth but portamento is excluded
SynthClipParamResult classifyKitRowParam(int32_t soundParamId) {
	SynthClipParamResult r;
	if (soundParamId != PARAM_ID_NONE && soundParamId < params::UNPATCHED_START) {
		r.kind = params::Kind::PATCHED;
		r.paramID = soundParamId;
	}
	else if (soundParamId != PARAM_ID_NONE && soundParamId >= params::UNPATCHED_START) {
		if (soundParamId - params::UNPATCHED_START != params::UNPATCHED_PORTAMENTO) {
			r.kind = params::Kind::UNPATCHED_SOUND;
			r.paramID = soundParamId - params::UNPATCHED_START;
		}
	}
	return r;
}

} // namespace

TEST_GROUP(MidiFollowParamRouting){};

TEST(MidiFollowParamRouting, SynthClip_PatchedParam) {
	auto r = classifySynthParam(params::LOCAL_LPF_FREQ);
	CHECK_EQUAL((int)params::Kind::PATCHED, (int)r.kind);
	CHECK_EQUAL(params::LOCAL_LPF_FREQ, r.paramID);
}

TEST(MidiFollowParamRouting, SynthClip_UnpatchedParam) {
	int32_t soundParamId = params::UNPATCHED_START + params::UNPATCHED_PORTAMENTO;
	auto r = classifySynthParam(soundParamId);
	CHECK_EQUAL((int)params::Kind::UNPATCHED_SOUND, (int)r.kind);
	CHECK_EQUAL(params::UNPATCHED_PORTAMENTO, r.paramID);
}

TEST(MidiFollowParamRouting, SynthClip_NoneParam) {
	auto r = classifySynthParam(PARAM_ID_NONE);
	CHECK_EQUAL((int)params::Kind::NONE, (int)r.kind);
	CHECK_EQUAL(PARAM_ID_NONE, r.paramID);
}

TEST(MidiFollowParamRouting, KitRow_PortamentoBlocked) {
	int32_t soundParamId = params::UNPATCHED_START + params::UNPATCHED_PORTAMENTO;
	auto r = classifyKitRowParam(soundParamId);
	// Kit rows don't allow portamento
	CHECK_EQUAL((int)params::Kind::NONE, (int)r.kind);
}

TEST(MidiFollowParamRouting, KitRow_OtherUnpatchedAllowed) {
	int32_t soundParamId = params::UNPATCHED_START + params::UNPATCHED_BASS;
	auto r = classifyKitRowParam(soundParamId);
	CHECK_EQUAL((int)params::Kind::UNPATCHED_SOUND, (int)r.kind);
	CHECK_EQUAL(params::UNPATCHED_BASS, r.paramID);
}

TEST(MidiFollowParamRouting, KitRow_PatchedAllowed) {
	auto r = classifyKitRowParam(params::LOCAL_LPF_FREQ);
	CHECK_EQUAL((int)params::Kind::PATCHED, (int)r.kind);
}

// ── handleReceivedCC early-out: unmapped CC ─────────────────────────────

TEST(MidiFollowMappings, HandleReceivedCC_UnmappedCCReturnsEarly) {
	// Simulates the early return in handleReceivedCC when both sound and global
	// param IDs are PARAM_ID_NONE
	MidiFollowMappings m2;
	m2.initDefaults();

	uint8_t soundParamId = m2.ccToSoundParam[64]; // sustain — not mapped
	uint8_t globalParamId = m2.ccToGlobalParam[64];
	CHECK_EQUAL(PARAM_ID_NONE, soundParamId);
	CHECK_EQUAL(PARAM_ID_NONE, globalParamId);
	// In real code: if both are PARAM_ID_NONE, handleReceivedCC returns immediately
}
