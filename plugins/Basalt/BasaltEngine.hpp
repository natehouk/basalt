// Basalt — stereo dual filter engine: two multimode filters in serial or parallel, three characters each.
// Clean-room implementation from public, generic techniques: cascaded topology-preserving-transform
// state-variable filters (Zavalishin) with per-character nonlinearities, Butterworth stage damping,
// per-sample one-pole parameter smoothing. No third-party product code, assets or disassembly were used.
// License: MIT (see LICENSE).

#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace basalt {

static constexpr float kPi = 3.14159265358979f;
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float dbToGain(float db) { return std::pow(10.f, db / 20.f); }

// ------------------------------------------------------------------ params (append-only)
#define FILTER_PARAMS(n) kF##n##On, kF##n##Style, kF##n##Type, kF##n##Slope, kF##n##Freq, kF##n##Res
enum Param : uint32_t {
    FILTER_PARAMS(1),
    FILTER_PARAMS(2),
    kRouting, kLink, kInGain, kOutGain, kMix, kMidiOn,
    kOutLearn,                       // output (read-only, for the UI)
    kParamCount
};
#undef FILTER_PARAMS
static constexpr uint32_t kFilterStride = kF2On - kF1On;
inline uint32_t fp(int f, uint32_t field) { return (f == 0 ? kF1On : kF2On) + field; }   // field: 0 on .. 5 res

// ------------------------------------------------------------------ filter characters
static constexpr int kNumStyles = 3;
struct StyleDef { float drive; int nl; float kMin; };
//  nl: 0 linear, 1 soft-limited state, 3 hard-clipped state
inline const StyleDef& styleDef(int s) {
    static const StyleDef t[kNumStyles] = {
        { 1.3f, 0,  0.25f },   // Smooth : moderate overdrive, wide resonance, never self-oscillates
        { 1.2f, 1, -0.04f },   // Fat    : soft-limited resonance, smooth self-oscillation
        { 3.0f, 3, -0.08f },   // Raw    : hard-clipped feedback, buzzy self-oscillation
    };
    return t[s < 0 ? 0 : (s >= kNumStyles ? kNumStyles - 1 : s)];
}
// Butterworth stage damping (k = 2 cos theta) for 1, 2 and 4 cascaded 2-pole stages.
inline const float* bwK(int stages) {
    static const float k1[1] = { 1.41421356f };
    static const float k2[2] = { 1.84775907f, 0.76536686f };
    static const float k4[4] = { 1.96157056f, 1.66293922f, 1.11114047f, 0.39018064f };
    return stages == 1 ? k1 : (stages == 2 ? k2 : k4);
}
inline int slopeStages(int s) { return s <= 0 ? 1 : (s == 1 ? 2 : 4); }

// One filter, one channel: up to 4 cascaded TPT SVF stages. r = resonance shaped as res^0.6.
struct FilterChan {
    float ic1[4] = {}, ic2[4] = {};
    void reset() { for (int i = 0; i < 4; ++i) ic1[i] = ic2[i] = 0; }
    inline float process(float x, float g, float r, int type, int stages, int style) {
        const StyleDef& sd = styleDef(style);
        if (sd.nl == 3) x = (x * sd.drive) / (1.f + std::fabs(x * sd.drive)) / sd.drive;
        else            x = std::tanh(x * sd.drive) / sd.drive;
        const float* kb = bwK(stages);
        for (int i = 0; i < stages; ++i) {
            const float k = i == stages - 1 ? kb[i] * (1.f - r) + sd.kMin * r : kb[i];
            const float a1 = 1.f / (1.f + g * (g + k));
            const float v3 = x - ic2[i];
            const float v1 = a1 * ic1[i] + g * a1 * v3;
            const float v2 = ic2[i] + g * v1;
            switch (sd.nl) {
            case 1: ic1[i] = 2.f * v1 - ic1[i]; ic1[i] = ic1[i] / std::sqrt(1.f + 0.08f * ic1[i] * ic1[i]); break;
            case 3: ic1[i] = clampf(2.f * v1 - ic1[i], -1.f, 1.f); break;
            default: ic1[i] = 2.f * v1 - ic1[i]; break;
            }
            ic2[i] = 2.f * v2 - ic2[i];
            x = type == 0 ? v2 : (type == 2 ? x - k * v1 - v2 : v1 * std::max(std::fabs(k), 0.12f));
        }
        return x;
    }
};

// ------------------------------------------------------------------ engine
class Engine {
public:
    float p[kParamCount];

    Engine() {
        std::fill(p, p + kParamCount, 0.f);
        p[kF1On] = 1; p[kF1Style] = 0; p[kF1Type] = 0; p[kF1Slope] = 1; p[kF1Freq] = 2000; p[kF1Res] = 0.25f;
        p[kF2On] = 0; p[kF2Style] = 0; p[kF2Type] = 2; p[kF2Slope] = 0; p[kF2Freq] = 100;  p[kF2Res] = 0.1f;
        p[kRouting] = 0; p[kLink] = 0; p[kInGain] = 0; p[kOutGain] = 0; p[kMix] = 1; p[kMidiOn] = 1;
        snap();
    }
    void setSampleRate(float sr) {
        fs = sr;
        smooth = 1.f - std::exp(-1.f / (0.012f * sr));     // ~12 ms parameter interpolation
        fade = 1.f - std::exp(-1.f / (0.004f * sr));       // on/off and routing crossfades
        reset();
    }
    void reset() { for (auto& f : flt) for (auto& c : f) c.reset(); snap(); }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t n) {
        int type[2], stages[2], style[2]; float onT[2];
        for (int f = 0; f < 2; ++f) {
            onT[f] = p[fp(f, 0)] > 0.5f ? 1.f : 0.f;
            style[f] = (int)(p[fp(f, 1)] + 0.5f); type[f] = (int)(p[fp(f, 2)] + 0.5f); stages[f] = slopeStages((int)(p[fp(f, 3)] + 0.5f));
            if (type[f] != lastType[f] || stages[f] != lastStages[f]) { flt[f][0].reset(); flt[f][1].reset(); flt2Par[0][f].reset(); flt2Par[1][f].reset(); lastType[f] = type[f]; lastStages[f] = stages[f]; }
        }
        const float parT = p[kRouting] > 0.5f ? 1.f : 0.f;
        const float tOct[2] = { std::log2(clampf(p[kF1Freq], 10.f, 22000.f)), std::log2(clampf(p[kF2Freq], 10.f, 22000.f)) };
        const float tRes[2] = { clampf(p[kF1Res], 0.f, 1.f), clampf(p[kF2Res], 0.f, 1.f) };
        const float tIn = dbToGain(p[kInGain]), tOut = dbToGain(p[kOutGain]), tMix = clampf(p[kMix], 0.f, 1.f);

        for (uint32_t i = 0; i < n; ++i) {
            const float xl = inL[i], xr = inR[i];
            // parameter interpolation; coefficients refreshed every 4 samples
            for (int f = 0; f < 2; ++f) {
                sOct[f] += (tOct[f] - sOct[f]) * smooth; sRes[f] += (tRes[f] - sRes[f]) * smooth; sOn[f] += (onT[f] - sOn[f]) * fade;
            }
            sPar += (parT - sPar) * fade; sIn += (tIn - sIn) * smooth; sOut += (tOut - sOut) * smooth; sMix += (tMix - sMix) * smooth;
            if (--ctl <= 0) {
                ctl = 4;
                for (int f = 0; f < 2; ++f) { g[f] = std::tan(kPi * clampf(std::exp2f(sOct[f]), 10.f, 0.45f * fs) / fs); r[f] = std::pow(sRes[f], 0.6f); }
            }
            const float wl = xl * sIn, wr = xr * sIn;
            // a filter that is switched off fades to a bypass (its state keeps running, so switching back on is click-free)
            auto run = [&](int f, float in, int ch) {
                const float y = flt[f][ch].process(in, g[f], r[f], type[f], stages[f], style[f]);
                return in + (y - in) * sOn[f];
            };
            float out[2];
            for (int ch = 0; ch < 2; ++ch) {
                const float in = ch == 0 ? wl : wr;
                const float serial = run(1, run(0, in, ch), ch);
                // parallel: both filters read the input; both on = -3 dB each, one on = that filter, none = dry
                const float a = flt2Par[ch][0].process(in, g[0], r[0], type[0], stages[0], style[0]);
                const float b = flt2Par[ch][1].process(in, g[1], r[1], type[1], stages[1], style[1]);
                const float wa = sOn[0], wb = sOn[1];
                const float parallel = in * (1.f - std::max(wa, wb)) + (a * wa + b * wb) / (1.f + 0.41421356f * std::min(wa, wb));
                out[ch] = serial * (1.f - sPar) + parallel * sPar;
            }
            const float ol = (sMix * out[0] + (1.f - sMix) * xl) * sOut;
            const float orr = (sMix * out[1] + (1.f - sMix) * xr) * sOut;
            outL[i] = softCeil(ol); outR[i] = softCeil(orr);
        }
        for (auto* set : { &flt, &flt2Par }) for (auto& f : *set) for (auto& c : f) for (int k = 0; k < 4; ++k) {
            if (!(std::fabs(c.ic1[k]) < 1e4f)) c.ic1[k] = 0;
            if (!(std::fabs(c.ic2[k]) < 1e4f)) c.ic2[k] = 0;
        }
    }

private:
    static float softCeil(float x) {
        const float a = std::fabs(x);
        if (!(a < 1e6f)) return 0.f;
        return a > 0.89f ? std::copysign(0.89f + 0.11f * std::tanh((a - 0.89f) / 0.11f), x) : x;
    }
    void snap() {
        for (int f = 0; f < 2; ++f) {
            sOct[f] = std::log2(clampf(p[fp(f, 4)], 10.f, 22000.f)); sRes[f] = p[fp(f, 5)]; sOn[f] = p[fp(f, 0)] > 0.5f ? 1.f : 0.f;
            g[f] = std::tan(kPi * clampf(std::exp2f(sOct[f]), 10.f, 0.45f * fs) / fs); r[f] = std::pow(clampf(sRes[f], 0.f, 1.f), 0.6f);
        }
        sPar = p[kRouting] > 0.5f ? 1.f : 0.f; sIn = dbToGain(p[kInGain]); sOut = dbToGain(p[kOutGain]); sMix = p[kMix]; ctl = 0;
    }

    FilterChan flt[2][2];          // [filter][channel], serial path
    FilterChan flt2Par[2][2];      // [channel][filter], parallel path
    float fs = 48000.f, smooth = 0.01f, fade = 0.01f;
    float sOct[2] = { 10, 10 }, sRes[2] = {}, sOn[2] = {}, sPar = 0, sIn = 1, sOut = 1, sMix = 1;
    float g[2] = { 0.1f, 0.1f }, r[2] = {};
    int lastType[2] = { -1, -1 }, lastStages[2] = { -1, -1 }, ctl = 0;
};

} // namespace basalt
