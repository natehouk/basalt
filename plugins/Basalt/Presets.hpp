// Factory presets (each lists only what it changes from the engine defaults). License: MIT.
// Engine defaults: filter 1 smooth low pass 24 dB at 2 kHz; filter 2 off (high pass 12 dB at 100 Hz); serial.
#pragma once
#include "BasaltEngine.hpp"
#include <algorithm>

namespace basalt {

struct PV { uint32_t id; float v; };
struct PresetDef { const char* name; const char* category; const PV* pv; };
struct Preset { const char* name; const char* category; float v[kParamCount]; };

static const char* const kCategories[] = { "Low Pass", "High & Band", "Dual", "Resonant", "Live" };
static constexpr int kNumCategories = 5;

#define F1(st, ty, sl, hz, rs) {kF1On,1},{kF1Style,st},{kF1Type,ty},{kF1Slope,sl},{kF1Freq,hz},{kF1Res,rs}
#define F2(st, ty, sl, hz, rs) {kF2On,1},{kF2Style,st},{kF2Type,ty},{kF2Slope,sl},{kF2Freq,hz},{kF2Res,rs}
#define END { kParamCount, 0 }
namespace pv {
static const PV init[] = { END };
// ---------------------------------------------------------------- Low Pass
static const PV warmLP[]   = { F1(0,0,1,1200,.2f), END };
static const PV darkLP[]   = { F1(0,0,2,400,.15f), {kOutGain,2}, END };
static const PV fatLP[]    = { F1(1,0,1,700,.55f), END };
static const PV gentle[]   = { F1(0,0,0,5000,.1f), END };
static const PV muffled[]  = { F1(0,0,2,250,0), {kOutGain,4}, END };
static const PV rawLP[]    = { F1(2,0,1,900,.45f), {kOutGain,-2}, END };
// ---------------------------------------------------------------- High & Band
static const PV thinHP[]   = { F1(0,2,1,600,.15f), {kOutGain,4}, END };
static const PV rumble[]   = { F1(0,2,2,40,0), END };
static const PV telephone[]= { F1(0,1,1,1500,.3f), {kOutGain,12}, END };
static const PV radio[]    = { F1(0,2,1,500,.2f), F2(0,0,1,3500,.2f), {kOutGain,5}, END };
static const PV nasal[]    = { F1(1,1,1,900,.6f), {kOutGain,14}, END };
static const PV airHP[]    = { F1(0,2,0,3000,.1f), {kMix,.5f}, END };
// ---------------------------------------------------------------- Dual
static const PV bandpass[] = { F1(0,0,1,2500,.3f), F2(0,2,1,400,.3f), END };
static const PV notch[]    = { F1(0,0,2,800,0), F2(0,2,2,1200,0), {kRouting,1}, END };
static const PV twinPeaks[]= { F1(1,1,1,500,.6f), F2(1,1,1,2500,.6f), {kRouting,1}, {kOutGain,12}, END };
static const PV vowel[]    = { F1(1,1,1,700,.65f), F2(1,1,1,1200,.65f), {kRouting,1}, {kLink,1}, {kOutGain,15}, END };
static const PV linkedBand[]={ F1(0,0,1,3000,.25f), F2(0,2,1,600,.25f), {kLink,1}, {kOutGain,6}, END };
static const PV split[]    = { F1(0,0,2,300,.2f), F2(0,2,2,3000,.2f), {kRouting,1}, END };
// ---------------------------------------------------------------- Resonant
static const PV squelch[]  = { F1(1,0,1,500,.8f), END };
static const PV scream[]   = { F1(2,0,1,1100,.85f), {kOutGain,-4}, END };
static const PV whistle[]  = { F1(1,1,2,1800,.9f), {kOutGain,12}, END };
static const PV selfOsc[]  = { F1(1,0,1,880,1), {kMix,.6f}, END };
static const PV buzz[]     = { F1(2,1,1,700,.9f), {kOutGain,6}, END };
static const PV resoHP[]   = { F1(1,2,1,900,.75f), {kOutGain,4}, END };
// ---------------------------------------------------------------- Live (performance starting points)
static const PV djLP[]     = { F1(0,0,1,20000,.3f), F2(0,2,1,20,.3f), END };
static const PV djHP[]     = { F1(0,2,1,20,.3f), END };
static const PV buildUp[]  = { F1(1,2,2,20,.55f), F2(0,0,1,20000,.2f), {kLink,1}, END };
static const PV breakdown[]= { F1(1,0,2,20000,.5f), END };
static const PV sweepBP[]  = { F1(1,1,1,1000,.5f), {kOutGain,14}, END };
static const PV halfWet[]  = { F1(0,0,1,600,.4f), {kMix,.5f}, END };
} // namespace pv
#undef F1
#undef F2
#undef END

static const PresetDef kPresetDefs[] = {
    {"Init", "Init", pv::init},
    {"Warm Low Pass", "Low Pass", pv::warmLP}, {"Dark", "Low Pass", pv::darkLP}, {"Fat Low Pass", "Low Pass", pv::fatLP},
    {"Gentle Top", "Low Pass", pv::gentle}, {"Muffled", "Low Pass", pv::muffled}, {"Raw Low Pass", "Low Pass", pv::rawLP},
    {"Thin", "High & Band", pv::thinHP}, {"Rumble Cut", "High & Band", pv::rumble}, {"Telephone", "High & Band", pv::telephone},
    {"Radio", "High & Band", pv::radio}, {"Nasal", "High & Band", pv::nasal}, {"Air", "High & Band", pv::airHP},
    {"Band Pass Pair", "Dual", pv::bandpass}, {"Notch", "Dual", pv::notch}, {"Twin Peaks", "Dual", pv::twinPeaks},
    {"Vowel", "Dual", pv::vowel}, {"Linked Band", "Dual", pv::linkedBand}, {"Split Ends", "Dual", pv::split},
    {"Squelch", "Resonant", pv::squelch}, {"Scream", "Resonant", pv::scream}, {"Whistle", "Resonant", pv::whistle},
    {"Self-Oscillate", "Resonant", pv::selfOsc}, {"Buzz", "Resonant", pv::buzz}, {"Resonant High", "Resonant", pv::resoHP},
    {"DJ Low Pass", "Live", pv::djLP}, {"DJ High Pass", "Live", pv::djHP}, {"Build-Up", "Live", pv::buildUp},
    {"Breakdown", "Live", pv::breakdown}, {"Sweep Band", "Live", pv::sweepBP}, {"Half Wet", "Live", pv::halfWet},
};
static constexpr uint32_t kNumPresets = sizeof(kPresetDefs) / sizeof(kPresetDefs[0]);

inline const Preset* presets() {
    static Preset table[kNumPresets];
    static bool done = false;
    if (done) return table;
    done = true;
    Engine e;
    for (uint32_t i = 0; i < kNumPresets; ++i) {
        table[i].name = kPresetDefs[i].name; table[i].category = kPresetDefs[i].category;
        std::copy(e.p, e.p + kParamCount, table[i].v);
        for (const PV* x = kPresetDefs[i].pv; x->id != kParamCount; ++x) table[i].v[x->id] = x->v;
    }
    return table;
}

} // namespace basalt
