// Behaviour checks: responses and slopes, characters (self-oscillation), routing, on/off and routing crossfades,
// parameter smoothing, mix and gains, link helpers.
#include "../plugins/Basalt/ParamDefs.hpp"
#include <cstdio>
#include <vector>
using namespace basalt;
static int fails = 0;
#define CHECK(c, msg) do { bool ok_ = (c); printf("%s  %s\n", ok_ ? "PASS" : "FAIL", msg); if (!ok_) fails++; } while (0)
static const int SR = 48000;
struct Buf { std::vector<float> l, r; };
static Buf run(Engine& e, const std::vector<float>& x) {
    Buf b; b.l.resize(x.size()); b.r.resize(x.size());
    for (size_t i = 0; i < x.size(); i += 256) { const uint32_t n = (uint32_t)std::min<size_t>(256, x.size() - i); e.process(x.data() + i, x.data() + i, b.l.data() + i, b.r.data() + i, n); }
    return b;
}
static float rms(const std::vector<float>& v, size_t a = 0, size_t z = 0) { if (!z) z = v.size(); double s = 0; for (size_t i = a; i < z; ++i) s += v[i] * v[i]; return (float)std::sqrt(s / (z - a)); }
static std::vector<float> sine(int n, float f, float amp) { std::vector<float> v(n); for (int i = 0; i < n; ++i) v[i] = amp * std::sin(2 * kPi * f * i / SR); return v; }
// gain (dB) of the engine at one frequency, from a quiet sine (linear region)
static float gainDb(Engine e, float hz) { const float a = 0.05f; Buf b = run(e, sine(SR / 2, hz, a)); return 20 * std::log10(rms(b.l, SR / 4) / (a * 0.7071f) + 1e-12f); }
static Engine one(int style, int type, int slope, float hz, float res) {
    Engine e; e.setSampleRate(SR); e.p[kF1Style] = style; e.p[kF1Type] = type; e.p[kF1Slope] = slope; e.p[kF1Freq] = hz; e.p[kF1Res] = res; e.reset(); return e;
}
int main() {
    char msg[160];
    // slopes: low pass at 1 kHz, attenuation from 2 to 4 kHz ~ 12/24/48 dB per octave
    for (int sl = 0; sl < 3; ++sl) {
        Engine e = one(0, 0, sl, 1000, 0);
        const float a2 = gainDb(e, 2000), a4 = gainDb(e, 4000), pass = gainDb(e, 100);
        const float want = 12.f * (1 << sl), ideal = -10 * std::log10(1 + std::pow(2.f, 2 * 2 * (1 << sl)));   // Butterworth, one octave up
        snprintf(msg, sizeof msg, "LP %d dB/oct: passband %+.1f dB, one octave up %.1f dB (ideal %.1f), two octaves %.1f dB", (int)want, pass, a2, ideal, a4);
        CHECK(std::fabs(a2 - ideal) < 1.5f && a4 < a2 - want * 0.6f && std::fabs(pass) < 0.5f, msg);
    }
    { Engine e = one(0, 2, 1, 1000, 0); const float lo = gainDb(e, 250), hi = gainDb(e, 8000);
      snprintf(msg, sizeof msg, "HP 24 dB: %.1f dB two octaves below, %+.1f dB in the passband", lo, hi); CHECK(lo < -40 && std::fabs(hi) < 1.5f, msg); }
    { Engine e = one(0, 1, 1, 1000, 0.3f); const float c = gainDb(e, 1000), lo = gainDb(e, 125), hi = gainDb(e, 8000);
      snprintf(msg, sizeof msg, "BP 24 dB: centre %+.1f dB, 3 octaves off %.1f / %.1f dB", c, lo, hi); CHECK(c > -6 && lo < c - 30 && hi < c - 30, msg); }
    // characters: Smooth never self-oscillates, Fat and Raw do
    for (int st = 0; st < kNumStyles; ++st) {
        Engine e = one(st, 0, 1, 800, 1); std::vector<float> x(SR, 0.f); x[10] = 0.5f; Buf b = run(e, x);
        const float tail = rms(b.l, SR / 2, SR); int n; const char* nm = enumLabels(kF1Style, n)[st];
        snprintf(msg, sizeof msg, "%-6s at full resonance %s (tail rms %.3f)", nm, st == 0 ? "does not self-oscillate" : "self-oscillates", tail);
        CHECK(st == 0 ? tail < 0.005f : tail > 0.02f, msg);
    }
    { // routing: LP 500 + HP 2000 -> serial nearly silent at 1 kHz, parallel passes plenty
        auto mk = [](float route) { Engine e; e.setSampleRate(SR); e.p[kF2On] = 1; e.p[kF1Freq] = 500; e.p[kF2Type] = 2; e.p[kF2Freq] = 2000; e.p[kF1Res] = 0; e.p[kF2Res] = 0; e.p[kRouting] = route; e.reset(); return e; };
        const float s = gainDb(mk(0), 1000), p = gainDb(mk(1), 1000);
        snprintf(msg, sizeof msg, "routing at 1 kHz: serial %.1f dB, parallel %.1f dB", s, p); CHECK(p > s + 12, msg);
        Engine one2; one2.setSampleRate(SR); one2.p[kF1On] = 0; one2.p[kF2On] = 1; one2.p[kF2Type] = 0; one2.p[kF2Freq] = 200; one2.p[kF2Res] = 0; one2.p[kRouting] = 1; one2.reset();
        CHECK(gainDb(one2, 4000) < -25, "parallel with filter 1 off: filter 2 alone filters (no dry leak)");
        Engine none; none.setSampleRate(SR); none.p[kF1On] = 0; none.reset();
        CHECK(std::fabs(gainDb(none, 4000)) < 0.6f, "both filters off: signal passes (character drive only)");
    }
    { // click-free switching: toggling a filter, routing and a big frequency jump mid-signal
        Engine e = one(0, 0, 1, 300, 0.2f); auto x = sine(SR, 2000, 0.4f);
        std::vector<float> L(SR), R(SR); float worst = 0;
        for (int i = 0; i < SR; i += 256) {
            if (i == 12288) e.p[kF1On] = 0;
            if (i == 24576) { e.p[kF1On] = 1; e.p[kF2On] = 1; e.p[kRouting] = 1; }
            if (i == 36864) e.p[kF1Freq] = 8000;
            e.process(x.data() + i, x.data() + i, L.data() + i, R.data() + i, std::min(256, SR - i));
        }
        // a click shows up as a sample-to-sample step larger than any the settled signal makes before or after each change
        auto stepMax = [&](int a, int z) { float m = 0; for (int i = a + 1; i < z; ++i) m = std::max(m, std::fabs(L[i] - L[i - 1])); return m; };
        worst = stepMax(0, SR);
        const float ref = std::max(std::max(stepMax(10000, 12288), stepMax(22000, 24576)), std::max(stepMax(34000, 36864), stepMax(SR - 4000, SR)));
        snprintf(msg, sizeof msg, "on/off, routing and frequency jumps are click-free (largest step %.3f, settled %.3f)", worst, ref);
        CHECK(worst < ref * 1.1f, msg);
    }
    { // mix and gains
        Engine e = one(0, 0, 1, 100, 0); e.p[kMix] = 0; e.reset(); auto x = sine(SR, 3000, 0.5f); Buf b = run(e, x);
        float d = 0; for (int i = SR / 4; i < SR; ++i) d = std::max(d, std::fabs(b.l[i] - x[i])); CHECK(d < 1e-3f, "mix 0%: output equals input");
        Engine g = one(0, 0, 1, 20000, 0); g.p[kInGain] = -12; g.p[kOutGain] = 6; g.reset();
        const float db = gainDb(g, 200); snprintf(msg, sizeof msg, "input -12 dB, output +6 dB: %.1f dB overall", db); CHECK(std::fabs(db + 6) < 0.8f, msg);
    }
    { // stereo: both channels identical for identical input
        Engine e = one(2, 0, 2, 600, 0.7f); Buf b = run(e, sine(SR / 4, 300, 0.6f)); float d = 0;
        for (int i = 0; i < SR / 4; ++i) d = std::max(d, std::fabs(b.l[i] - b.r[i])); CHECK(d < 1e-6f, "left and right are processed identically");
    }
    { // link helpers
        CHECK(std::fabs(linkedValue(kF1Freq, 1000, 2000, 300) - 600) < 0.01f, "link: doubling filter 1 doubles filter 2");
        CHECK(std::fabs(linkedValue(kF2Res, 0.2f, 0.5f, 0.4f) - 0.7f) < 1e-5f, "link: resonance moves by the same amount");
    }
    printf("%s (%d failures)\n", fails ? "FAILED" : "ALL PASSED", fails);
    return fails ? 1 : 0;
}
