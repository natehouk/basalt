// Parameter metadata shared by the DSP wrapper and the UI. License: MIT.
#pragma once
#include "BasaltEngine.hpp"
#include <cstdio>

namespace basalt {

static constexpr uint32_t kHintInteger = 1, kHintBoolean = 2, kHintLog = 4, kHintOutput = 8;
struct ParamDef { const char* name; const char* sym; const char* unit; float min, max; uint32_t hints; };

inline const ParamDef& paramDef(uint32_t i) {
    static ParamDef t[kParamCount];
    static bool done = false;
    if (!done) {
        done = true;
        auto set = [](uint32_t i, const char* n, const char* s, const char* u, float lo, float hi, uint32_t h) { t[i] = { n, s, u, lo, hi, h }; };
        const uint32_t I = kHintInteger, B = kHintBoolean | kHintInteger;
        static const char* fn[2][6] = { { "Filter 1 On", "Filter 1 Character", "Filter 1 Type", "Filter 1 Slope", "Filter 1 Freq", "Filter 1 Resonance" },
                                        { "Filter 2 On", "Filter 2 Character", "Filter 2 Type", "Filter 2 Slope", "Filter 2 Freq", "Filter 2 Resonance" } };
        static const char* fs[2][6] = { { "f1on", "f1char", "f1type", "f1slope", "f1freq", "f1res" },
                                        { "f2on", "f2char", "f2type", "f2slope", "f2freq", "f2res" } };
        for (int f = 0; f < 2; ++f) {
            set(fp(f, 0), fn[f][0], fs[f][0], "", 0, 1, B);
            set(fp(f, 1), fn[f][1], fs[f][1], "", 0, kNumStyles - 1, I);
            set(fp(f, 2), fn[f][2], fs[f][2], "", 0, 2, I);
            set(fp(f, 3), fn[f][3], fs[f][3], "", 0, 2, I);
            set(fp(f, 4), fn[f][4], fs[f][4], "Hz", 20, 20000, kHintLog);
            set(fp(f, 5), fn[f][5], fs[f][5], "", 0, 1, 0);
        }
        set(kRouting, "Routing", "routing", "", 0, 1, I);
        set(kLink, "Link Filters", "link", "", 0, 1, B);
        set(kInGain, "Input Gain", "ingain", "dB", -24, 24, 0);
        set(kOutGain, "Output Gain", "outgain", "dB", -24, 24, 0);
        set(kMix, "Mix", "mix", "", 0, 1, 0);
        set(kMidiOn, "MIDI", "midion", "", 0, 1, B);
        set(kOutLearn, "MIDI Learn", "o_learn", "", 0, 100000, kHintOutput);
    }
    return t[i];
}

inline const char* const* enumLabels(uint32_t p, int& count) {
    static const char* const style[] = { "Smooth", "Fat", "Raw" };
    static const char* const type[]  = { "Low Pass", "Band Pass", "High Pass" };
    static const char* const slope[] = { "12 dB/oct", "24 dB/oct", "48 dB/oct" };
    static const char* const route[] = { "Serial", "Parallel" };
    switch (p) {
    case kF1Style: case kF2Style: count = kNumStyles; return style;
    case kF1Type:  case kF2Type:  count = 3; return type;
    case kF1Slope: case kF2Slope: count = 3; return slope;
    case kRouting: count = 2; return route;
    default: count = 0; return nullptr;
    }
}

// Normalised 0..1 <-> value (log for frequencies); used by the UI and MIDI learn.
inline float toNorm(uint32_t p, float v) {
    const ParamDef& d = paramDef(p);
    if (d.hints & kHintLog) return std::log(v / d.min) / std::log(d.max / d.min);
    return (v - d.min) / (d.max - d.min);
}
inline float fromNorm(uint32_t p, float n) {
    const ParamDef& d = paramDef(p);
    n = clampf(n, 0.f, 1.f);
    float v = (d.hints & kHintLog) ? d.min * std::pow(d.max / d.min, n) : d.min + n * (d.max - d.min);
    if (d.hints & kHintInteger) v = std::round(v);
    return v;
}

// Link: the same move applied to the other filter (frequency by ratio, resonance by offset)
inline uint32_t linkPartner(uint32_t p) {
    switch (p) {
    case kF1Freq: return kF2Freq; case kF2Freq: return kF1Freq;
    case kF1Res: return kF2Res;   case kF2Res: return kF1Res;
    default: return (uint32_t)-1;
    }
}
inline float linkedValue(uint32_t p, float oldV, float newV, float partnerV) {
    if (p == kF1Freq || p == kF2Freq) return clampf(partnerV * newV / std::max(oldV, 1.f), 20.f, 20000.f);
    return clampf(partnerV + newV - oldV, 0.f, 1.f);
}

} // namespace basalt
