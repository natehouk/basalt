// CPU: both filters on at 48 dB (Raw), parallel, with the frequencies moving every block.
#include "../plugins/Basalt/ParamDefs.hpp"
#include <chrono>
#include <cstdio>
using namespace basalt;
int main() {
    const int SR = 48000; Engine e; e.setSampleRate(SR);
    e.p[kF2On] = 1; e.p[kF1Slope] = 2; e.p[kF2Slope] = 2; e.p[kF1Style] = 2; e.p[kF2Style] = 1; e.p[kRouting] = 1; e.p[kF1Res] = 0.7f; e.p[kF2Res] = 0.7f;
    static float in[SR], L[SR], R[SR]; uint32_t r = 1; for (float& x : in) { r = r * 1664525u + 1013904223u; x = 0.3f * ((int32_t)r * (1.f / 2147483648.f)); }
    const int secs = 20; auto t0 = std::chrono::steady_clock::now();
    for (int k = 0; k < secs; ++k) for (int b = 0; b < SR; b += 256) { e.p[kF1Freq] = 200 + (b % 5000); e.p[kF2Freq] = 3000 - (b % 2000); e.process(in + b, in + b, L + b, R + b, 256); }
    const double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    printf("%d s of audio in %.3f s: %.1f%% of one core\n", secs, el, 100 * el / secs); return 0;
}
