// Basalt — custom vector UI (DPF/NanoVG). Original design. License: MIT.
#include "DistrhoUI.hpp"
#include "ParamDefs.hpp"
#include "Presets.hpp"
#include "UserPresets.hpp"
#include "ResizeHandle.hpp"
#include <cctype>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

START_NAMESPACE_DISTRHO

using namespace basalt;
using DGL_NAMESPACE::ResizeHandle;
typedef std::complex<float> cplx;

static constexpr float W = 1000.f, H = 640.f;

#define C_BG        Color(0x0e, 0x0f, 0x11)
#define C_PANEL     Color(0x17, 0x19, 0x1c)
#define C_PANEL_HI  Color(0x22, 0x25, 0x29)
#define C_LINE      Color(0x2d, 0x31, 0x36)
#define C_TEXT      Color(0xe6, 0xe8, 0xea)
#define C_DIM       Color(0x86, 0x8b, 0x92)
#define C_FAINT     Color(0x4a, 0x4f, 0x56)
#define C_F1        Color(0xff, 0x7a, 0x4a)       // lava
#define C_F2        Color(0x7c, 0xc4, 0xe8)       // cooled-stone blue
#define C_DISPLAY   Color(0x0a, 0x0b, 0x0c)

static Color withAlpha(const Color& c, float a) { Color r(c); r.alpha = a; return r; }

// ---------------------------------------------------------------- layout
enum WType { Knob, Toggle, Segment, Button };
struct Ctl { WType type; uint32_t param; float x, y, r; const char* label; };    // Segment/Button: r = width
struct Panel { float x, y, w, h; const char* title; };

static constexpr float DX = 16, DY = 64, DW = 968, DH = 300;          // filter display
static constexpr float PY = 376, PH = 248;                             // panels
static constexpr float F1X = 16, F2X = 608, FW = 376, CX = 400, CPW = 200;
static constexpr float SEG_H = 26;

static const Panel kPanels[] = { { F1X, PY, FW, PH, "" }, { CX, PY, CPW, PH, "" }, { F2X, PY, FW, PH, "" } };

static std::vector<Ctl> buildControls() {
    std::vector<Ctl> c;
    for (int f = 0; f < 2; ++f) {
        const float x = f == 0 ? F1X : F2X;
        c.push_back({ Toggle,  fp(f, 0), x + 32,  PY + 26, 0,   f == 0 ? "FILTER 1" : "FILTER 2" });
        c.push_back({ Segment, fp(f, 1), x + 262, PY + 26, 210, "" });
        c.push_back({ Segment, fp(f, 2), x + 100, PY + 68, 172, "" });
        c.push_back({ Segment, fp(f, 3), x + 280, PY + 68, 172, "" });
        c.push_back({ Knob,    fp(f, 4), x + 110, PY + 156, 50, "FREQUENCY" });
        c.push_back({ Knob,    fp(f, 5), x + 276, PY + 156, 42, "RESONANCE" });
    }
    const float cx = CX + CPW / 2;
    c.push_back({ Segment, kRouting, cx, PY + 40, 172, "" });
    c.push_back({ Button,  kLink,    cx, PY + 84, 172, "LINK FILTERS" });
    c.push_back({ Knob, kInGain,  cx - 52, PY + 146, 20, "INPUT" });
    c.push_back({ Knob, kOutGain, cx + 52, PY + 146, 20, "OUTPUT" });
    c.push_back({ Knob, kMix,     cx,      PY + 200, 22, "MIX" });
    return c;
}
static const std::vector<Ctl>& controls() { static std::vector<Ctl> c = buildControls(); return c; }

static const char* const* segLabels(uint32_t p, int& n) {
    static const char* const style[] = { "SMOOTH", "FAT", "RAW" };
    static const char* const type[]  = { "LP", "BP", "HP" };
    static const char* const slope[] = { "12", "24", "48" };
    static const char* const route[] = { "SERIAL", "PARALLEL" };
    switch (p) {
    case kF1Style: case kF2Style: n = 3; return style;
    case kF1Type: case kF2Type: n = 3; return type;
    case kF1Slope: case kF2Slope: n = 3; return slope;
    case kRouting: n = 2; return route;
    default: n = 0; return nullptr;
    }
}

static constexpr float PS_X = 336.f, PS_Y = 20.f, PS_W = 250.f, PS_H = 28.f, PS_ARROW = 30.f;
static constexpr float UNDO_X = 600.f, REDO_X = 658.f, UR_W = 54.f, LEARN_X = 724.f, LEARN_W = 72.f, MIDI_X = 800.f, MIDI_W = 56.f;
static constexpr float PM_COLW = 146.f, PM_ROW = 23.f, PM_HEAD = 22.f, PM_PAD = 8.f;

// filter magnitude for drawing (linear prototype of the engine's cascade)
static cplx filterH(const float* v, int f, float hz) {
    const int type = (int)(v[fp(f, 2)] + .5f), stages = slopeStages((int)(v[fp(f, 3)] + .5f));
    const float fc = v[fp(f, 4)], r = std::pow(clampf(v[fp(f, 5)], 0.f, 1.f), 0.6f);
    const StyleDef& sd = styleDef((int)(v[fp(f, 1)] + .5f));
    const float* kb = bwK(stages);
    const float w = hz / fc;
    cplx h(1, 0);
    for (int i = 0; i < stages; ++i) {
        const float k = std::max(i == stages - 1 ? kb[i] * (1 - r) + sd.kMin * r : kb[i], 0.035f);
        const cplx den(1 - w * w, k * w);
        h *= type == 0 ? cplx(1, 0) / den : (type == 2 ? cplx(-w * w, 0) / den : cplx(0, w * std::max(k, 0.12f)) / den);
    }
    return h;
}

class BasaltUI : public UI {
public:
    BasaltUI() : UI((uint)W, (uint)H), resizeHandle(this) {
        loadSharedResources();
        Engine def;
        for (uint32_t i = 0; i < kParamCount; ++i) { val[i] = def.p[i]; defv[i] = def.p[i]; }
        for (int& m : paramCC) m = -1;
        const double sf = getScaleFactor();
        if (sf != 1.0) setSize((uint)(W * sf), (uint)(H * sf));
        setGeometryConstraints((uint)(W * 0.6 * sf), (uint)(H * 0.6 * sf), true);
        if (isResizable()) resizeHandle.hide();
        userPresets = listUserPresets();
        std::copy(val, val + kParamCount, before);
    }

protected:
    float uiScale() const { return (float)getWidth() / W; }

    // ---- values -------------------------------------------------------------
    void setValue(uint32_t p, float v) {
        const ParamDef& d = paramDef(p);
        v = clampf(v, d.min, d.max);
        if (v == val[p]) return;
        val[p] = v; setParameterValue(p, v); repaint();
    }
    // set a value; with Link on, frequency and resonance moves are mirrored on the other filter
    void setLinked(uint32_t p, float v) {
        const uint32_t q = linkPartner(p);
        const float old = val[p];
        setValue(p, v);
        if (val[kLink] > 0.5f && q != (uint32_t)-1) setValue(q, linkedValue(p, old, val[p], val[q]));
    }
    void commit(uint32_t p, float v) { editParameter(p, true); setValue(p, v); editParameter(p, false); }
    void commitLinked(uint32_t p, float v) {
        const uint32_t q = linkPartner(p); const bool lk = val[kLink] > 0.5f && q != (uint32_t)-1;
        editParameter(p, true); if (lk) editParameter(q, true);
        setLinked(p, v);
        editParameter(p, false); if (lk) editParameter(q, false);
    }

    void parameterChanged(uint32_t i, float v) override {
        if (i >= kParamCount) return;
        if (i == kOutLearn) {
            if (v != lastLearnEcho && v > 0) {
                const int code = (int)v % 10000 - 1, prm = code / 128, cc = code % 128;
                for (int& m : paramCC) if (m == cc) m = -1;
                if (prm >= 0 && prm < (int)kParamCount) paramCC[prm] = cc;
                learnWaiting = -1;
            }
            lastLearnEcho = v; repaint(); return;
        }
        if (val[i] != v) { val[i] = v; if (!gesture) before[i] = v; repaint(); }   // host/MIDI changes are not undo steps
    }
    void stateChanged(const char* key, const char* value) override {
        if (std::strcmp(key, "midimap") != 0) return;
        for (int& m : paramCC) m = -1;
        const char* q = value;
        while (q && *q) {
            int cc = -1, prm = -1;
            if (std::sscanf(q, "%d:%d", &cc, &prm) == 2 && prm >= 0 && prm < (int)kParamCount) paramCC[prm] = cc;
            const char* s = std::strchr(q, ';'); if (!s) break; q = s + 1;
        }
        repaint();
    }

    // ---- undo / redo: whole-state snapshots, one per completed gesture ---------
    typedef std::vector<float> Snap;
    Snap snapshot() const { return Snap(val, val + kParamCount); }
    bool differs(const float* a, const float* b) const {
        for (uint32_t i = 0; i < kParamCount; ++i) if (!(paramDef(i).hints & kHintOutput) && a[i] != b[i]) return true;
        return false;
    }
    void beginGesture() { gesture = true; std::copy(val, val + kParamCount, before); }
    void endGesture() {
        gesture = false;
        if (differs(before, val)) {
            undoStack.push_back(Snap(before, before + kParamCount)); if (undoStack.size() > 200) undoStack.erase(undoStack.begin());
            redoStack.clear();
        }
        std::copy(val, val + kParamCount, before); repaint();
    }
    void restoreSnap(const Snap& s) {
        for (uint32_t i = 0; i < kParamCount; ++i) if (!(paramDef(i).hints & kHintOutput) && val[i] != s[i]) commit(i, s[i]);
        std::copy(val, val + kParamCount, before);
    }
    void undo() { if (undoStack.empty()) return; redoStack.push_back(snapshot()); restoreSnap(undoStack.back()); undoStack.pop_back(); flash("Undo"); }
    void redo() { if (redoStack.empty()) return; undoStack.push_back(snapshot()); restoreSnap(redoStack.back()); redoStack.pop_back(); flash("Redo"); }

    // ---- formatting ---------------------------------------------------------
    void fmt(uint32_t p, char* b, size_t n) const {
        const float v = val[p];
        int ne = 0;
        if (const char* const* l = enumLabels(p, ne)) { snprintf(b, n, "%s", l[std::max(0, std::min(ne - 1, (int)std::round(v)))]); return; }
        const ParamDef& d = paramDef(p);
        if (d.hints & kHintBoolean) { snprintf(b, n, "%s", v > 0.5f ? "On" : "Off"); return; }
        if (p == kF1Freq || p == kF2Freq) { if (v >= 1000) snprintf(b, n, "%.2f kHz", v / 1000); else snprintf(b, n, "%.0f Hz", v); return; }
        if (p == kInGain || p == kOutGain) { snprintf(b, n, "%+.1f dB", v); return; }
        snprintf(b, n, "%.0f%%", v * 100);
    }
    bool parse(uint32_t p, const std::string& in, float& out) const {
        std::string s; for (char ch : in) if (!std::isspace((unsigned char)ch)) s += (char)std::tolower((unsigned char)ch);
        if (s.empty()) return false;
        char* end = nullptr; double x = std::strtod(s.c_str(), &end);
        if (end == s.c_str()) return false;
        std::string suf(end);
        if (suf == "k" || suf == "khz") x *= 1000;
        if (p == kF1Freq || p == kF2Freq || p == kInGain || p == kOutGain) { out = (float)x; return true; }
        out = (float)(x / 100.0); return true;
    }

    // ---- drawing ------------------------------------------------------------
    void onNanoDisplay() override {
        const float s = uiScale();
        save(); scale(s, s);
        beginPath(); rect(0, 0, W, H); fillColor(C_BG); fill();
        fontFace(NANOVG_DEJAVU_SANS_TTF);

        // header: three basalt columns
        const float colH[3] = { 22, 28, 18 };
        for (int i = 0; i < 3; ++i) {
            const float cx = 21 + i * 8.5f, top = 48 - colH[i];
            beginPath(); moveTo(cx - 3.6f, top + 2); lineTo(cx, top); lineTo(cx + 3.6f, top + 2); lineTo(cx + 3.6f, 48); lineTo(cx - 3.6f, 48); closePath();
            fillColor(i == 1 ? C_F1 : Color(0x5a, 0x60, 0x68)); fill();
        }
        fontSize(22); textLetterSpacing(9); fillColor(C_TEXT); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); text(52, 34, "BASALT", nullptr);
        fontSize(10); textLetterSpacing(2.5f); fillColor(C_DIM); text(206, 35, "DUAL FILTER", nullptr);
        fontSize(8); textLetterSpacing(2.f); textAlign(ALIGN_RIGHT | ALIGN_MIDDLE); fillColor(C_FAINT);
        text(W - 20, 29, "EPSILON AUDIO", nullptr); text(W - 20, 41, "1.0", nullptr);
        drawPresetBar();
        drawHeaderButton(UNDO_X, UR_W, "UNDO", false, hoverHeader == 3, undoStack.empty());
        drawHeaderButton(REDO_X, UR_W, "REDO", false, hoverHeader == 4, redoStack.empty());
        drawHeaderButton(LEARN_X, LEARN_W, learnMode ? "LEARNING" : "LEARN", learnMode, hoverHeader == 1, false);
        drawHeaderButton(MIDI_X, MIDI_W, "MIDI", val[kMidiOn] > 0.5f, hoverHeader == 2, false);

        drawDisplay();
        for (const Panel& p : kPanels) drawPanel(p);
        fontSize(8); textLetterSpacing(2.f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_FAINT); text(CX + CPW / 2, PY + 18, "ROUTING", nullptr);

        const std::vector<Ctl>& cs = controls();
        for (int i = 0; i < (int)cs.size(); ++i) drawCtl(cs[i], i == active || i == hover);
        if (editing >= 0) drawEditBox();
        drawStatus();
        if (menuOpen) drawPresetMenu();
        if (saving) drawSaveBox();
        restore();
    }

    void drawHeaderButton(float x, float w, const char* label, bool on, bool hot, bool disabled) {
        beginPath(); roundedRect(x, PS_Y, w, PS_H, 5); fillColor(on ? withAlpha(C_F1, 0.22f) : C_PANEL); fill();
        strokeColor(on ? C_F1 : (hot && !disabled ? C_DIM : C_LINE)); strokeWidth(1); stroke();
        fontSize(9.5f); textLetterSpacing(2.f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(disabled ? C_FAINT : (on ? C_TEXT : C_DIM));
        text(x + w / 2, PS_Y + PS_H / 2 + 0.5f, label, nullptr);
    }
    void drawPanel(const Panel& p) {
        beginPath(); roundedRect(p.x, p.y, p.w, p.h, 6); fillColor(C_PANEL); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
    }

    // display mapping
    static float xOfHz(float hz) { return DX + 12 + (DW - 24) * std::log(hz / 20.f) / std::log(1000.f); }
    static float hzOfX(float x)  { return 20.f * std::pow(1000.f, (x - DX - 12) / (DW - 24)); }
    static float yOfDb(float db) { return DY + 10 + (DH - 30) * (1 - (db + 36.f) / 60.f); }
    static float yOfRes(float r) { return DY + DH * 0.84f - r * DH * 0.66f; }
    static float resOfY(float y) { return (DY + DH * 0.84f - y) / (DH * 0.66f); }

    void drawDisplay() {
        beginPath(); roundedRect(DX, DY, DW, DH, 6); fillColor(C_DISPLAY); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        fontSize(8.5f); textLetterSpacing(1); fillColor(C_FAINT); textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
        const float grid[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
        const char* gl[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
        beginPath();
        for (float g : grid) { const float x = xOfHz(g); moveTo(x, DY + 6); lineTo(x, DY + DH - 18); }
        for (int db = -24; db <= 12; db += 12) { const float y = yOfDb((float)db); moveTo(DX + 6, y); lineTo(DX + DW - 6, y); }
        strokeColor(Color(255, 255, 255, 9 / 255.f)); strokeWidth(1); stroke();
        for (int i = 0; i < 8; ++i) text(xOfHz(grid[i]), DY + DH - 4, gl[i], nullptr);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        for (int db = -24; db <= 12; db += 12) { char b[8]; snprintf(b, sizeof b, "%+d", db); text(DX + 8, yOfDb((float)db) - 7, db == 0 ? "0 dB" : b, nullptr); }

        const int N = 320;
        std::vector<float> hz(N + 1);
        for (int i = 0; i <= N; ++i) hz[i] = hzOfX(DX + 12 + (DW - 24) * i / N);
        const bool on[2] = { val[kF1On] > 0.5f, val[kF2On] > 0.5f };
        auto yAt = [&](float mag) { return clampf(yOfDb(20 * std::log10(std::max(mag, 1e-4f))), DY + 2, DY + DH - 18); };
        for (int f = 1; f >= 0; --f) {
            const Color col = f == 0 ? C_F1 : C_F2; const float alpha = on[f] ? 1.f : 0.22f;
            std::vector<float> ys(N + 1);
            for (int i = 0; i <= N; ++i) ys[i] = yAt(std::abs(filterH(val, f, hz[i])));
            beginPath(); moveTo(DX + 12, DY + DH - 18);
            for (int i = 0; i <= N; ++i) lineTo(DX + 12 + (DW - 24) * i / N, ys[i]);
            lineTo(DX + DW - 12, DY + DH - 18); closePath();
            fillPaint(linearGradient(0, DY, 0, DY + DH, withAlpha(col, 0.2f * alpha), withAlpha(col, 0.f))); fill();
            beginPath();
            for (int i = 0; i <= N; ++i) { const float x = DX + 12 + (DW - 24) * i / N; if (!i) moveTo(x, ys[i]); else lineTo(x, ys[i]); }
            strokeColor(withAlpha(col, 0.95f * alpha)); strokeWidth(2.f); stroke();
        }
        if (on[0] && on[1]) {     // combined response
            const bool par = val[kRouting] > 0.5f;
            beginPath();
            for (int i = 0; i <= N; ++i) {
                const cplx h1 = filterH(val, 0, hz[i]), h2 = filterH(val, 1, hz[i]);
                const float y = yAt(std::abs(par ? 0.7071f * (h1 + h2) : h1 * h2)), x = DX + 12 + (DW - 24) * i / N;
                if (!i) moveTo(x, y); else lineTo(x, y);
            }
            strokeColor(withAlpha(C_TEXT, 0.75f)); strokeWidth(1.4f); stroke();
        }
        // link line + handles
        float x1, y1, x2, y2; handlePos(0, x1, y1); handlePos(1, x2, y2);
        if (val[kLink] > 0.5f) { beginPath(); moveTo(x1, y1); lineTo(x2, y2); strokeColor(withAlpha(C_TEXT, 0.35f)); strokeWidth(1.5f); stroke(); }
        for (int h = 0; h < 3; ++h) {
            float hx, hy; handlePos(h, hx, hy);
            const bool hot = dragHandle == h || hoverHandle == h;
            if (h < 2) {
                const Color col = h == 0 ? C_F1 : C_F2;
                if (hot) { beginPath(); circle(hx, hy, 20); fillColor(withAlpha(col, 0.15f)); fill(); }
                beginPath(); circle(hx, hy, hot ? 14 : 12); fillColor(withAlpha(col, on[h] ? 0.95f : 0.3f)); fill();
                fontSize(12); textLetterSpacing(0); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_BG); text(hx, hy + 0.5f, h == 0 ? "1" : "2", nullptr);
            } else if (on[0] && on[1]) {
                beginPath(); circle(hx, hy, hot ? 8 : 6.5f); fillColor(val[kLink] > 0.5f ? C_TEXT : C_PANEL_HI); fill(); strokeColor(C_TEXT); strokeWidth(1.2f); stroke();
            }
        }
    }
    void handlePos(int h, float& x, float& y) const {
        if (h < 2) { x = xOfHz(val[fp(h, 4)]); y = yOfRes(val[fp(h, 5)]); return; }
        float x1, y1, x2, y2; handlePos(0, x1, y1); handlePos(1, x2, y2); x = (x1 + x2) / 2; y = (y1 + y2) / 2;
    }

    void drawKnobBody(float x, float y, float r, float n, bool hot, Color col, bool dim) {
        const float a0 = 0.75f * 3.14159265f, a1 = 2.25f * 3.14159265f, ang = a0 + (a1 - a0) * n;
        const float tw = r > 30 ? 6.f : (r > 18 ? 4.f : 3.f);
        beginPath(); arc(x, y, r, a0, a1, CW); strokeColor(C_LINE); strokeWidth(tw); lineCap(ROUND); stroke();
        if (ang - a0 > 0.001f) { beginPath(); arc(x, y, r, a0, ang, CW); strokeColor(dim ? C_FAINT : col); stroke(); }
        const float cr = r - (r > 30 ? 12 : (r > 18 ? 8 : 6));
        beginPath(); circle(x, y, cr);
        fillPaint(radialGradient(x - cr * 0.3f, y - cr * 0.4f, 0, cr * 1.4f, hot ? Color(0x3a, 0x3e, 0x44) : C_PANEL_HI, Color(0x13, 0x14, 0x16)));
        fill(); strokeColor(Color(255, 255, 255, 18 / 255.f)); strokeWidth(1); stroke();
        beginPath(); moveTo(x + std::cos(ang) * cr * 0.35f, y + std::sin(ang) * cr * 0.35f); lineTo(x + std::cos(ang) * cr * 0.9f, y + std::sin(ang) * cr * 0.9f);
        strokeColor(dim ? C_DIM : C_TEXT); strokeWidth(r > 30 ? 3.f : 2.f); lineCap(ROUND); stroke();
    }
    static int filterOf(uint32_t p) { return p <= kF1Res ? 0 : (p <= kF2Res ? 1 : -1); }
    bool dimmed(uint32_t p) const {
        const int f = filterOf(p);
        if (f >= 0 && p != fp(f, 0) && val[fp(f, 0)] < 0.5f) return true;
        if (p == kRouting && (val[kF1On] < 0.5f || val[kF2On] < 0.5f)) return true;
        return false;
    }

    void drawCtl(const Ctl& c, bool hot) {
        const uint32_t p = c.param;
        const bool dim = dimmed(p);
        const int f = filterOf(p);
        const Color col = f == 1 ? C_F2 : C_F1;
        char buf[40];
        switch (c.type) {
        case Knob: {
            drawKnobBody(c.x, c.y, c.r, toNorm(p, val[p]), hot, col, dim);
            const bool big = c.r > 30;
            fontSize(big ? 9.5f : 8.5f); textLetterSpacing(big ? 2.f : 1.6f); textAlign(ALIGN_CENTER | ALIGN_TOP);
            if (big) {            // the big knobs always show their value under the name
                fillColor(dim ? C_FAINT : C_DIM); text(c.x, c.y + c.r + 6, c.label, nullptr);
                fmt(p, buf, sizeof buf); fontSize(12); textLetterSpacing(0.6f); fillColor(dim ? C_FAINT : (hot ? col : C_TEXT)); text(c.x, c.y + c.r + 20, buf, nullptr);
            } else if (hot && editing < 0 && !learnMode) { fmt(p, buf, sizeof buf); fillColor(col); text(c.x, c.y + c.r + 6, buf, nullptr); }
            else { fillColor(dim ? C_FAINT : C_DIM); text(c.x, c.y + c.r + 6, c.label, nullptr); }
            break; }
        case Toggle: {
            const bool on = val[p] > 0.5f;
            const float w = 38, h = 20, x = c.x - w / 2, y = c.y - h / 2;
            beginPath(); roundedRect(x, y, w, h, h / 2); fillColor(on ? withAlpha(col, 0.25f) : C_PANEL_HI); fill();
            strokeColor(on ? col : C_LINE); strokeWidth(1); stroke();
            beginPath(); circle(on ? x + w - h / 2 : x + h / 2, c.y, 7); fillColor(on ? col : C_DIM); fill();
            fontSize(11); textLetterSpacing(2.4f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(on ? C_TEXT : C_DIM); text(c.x + w / 2 + 10, c.y + 0.5f, c.label, nullptr);
            break; }
        case Segment: {
            int n = 0; const char* const* names = segLabels(p, n);
            const int cur = (int)(val[p] + .5f); const float sw = c.r / n, x0 = c.x - c.r / 2;
            const Color sc = f < 0 ? C_TEXT : col;
            for (int i = 0; i < n; ++i) {
                const float x = x0 + sw * i;
                beginPath(); roundedRect(x + 1.5f, c.y - SEG_H / 2, sw - 3, SEG_H, 5); fillColor(i == cur ? (dim ? C_FAINT : sc) : C_PANEL_HI); fill();
                fontSize(10); textLetterSpacing(1.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                fillColor(i == cur ? C_BG : (dim ? C_FAINT : C_DIM)); text(x + sw / 2, c.y + 0.5f, names[i], nullptr);
            }
            break; }
        case Button: {
            const bool on = val[p] > 0.5f;
            beginPath(); roundedRect(c.x - c.r / 2 + 1.5f, c.y - 14, c.r - 3, 28, 5); fillColor(on ? withAlpha(C_TEXT, 0.9f) : C_PANEL_HI); fill();
            strokeColor(on ? C_TEXT : (hot ? C_DIM : C_LINE)); strokeWidth(1); stroke();
            fontSize(10); textLetterSpacing(2.f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(on ? C_BG : C_DIM); text(c.x, c.y + 0.5f, c.label, nullptr);
            break; }
        }
        if (learnMode) {
            const int cc = paramCC[p]; const bool waiting = learnWaiting == (int)p;
            if (cc >= 0 || waiting) {
                const float by = c.type == Knob ? c.y - c.r - 12 : c.y - SEG_H / 2 - 8;
                snprintf(buf, sizeof buf, waiting ? "?" : "CC%d", cc);
                fontSize(8.5f); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                const float bw = waiting ? 16 : 34;
                beginPath(); roundedRect(c.x - bw / 2, by - 7, bw, 14, 3); fillColor(waiting ? C_F1 : C_PANEL_HI); fill(); strokeColor(C_F1); strokeWidth(1); stroke();
                fillColor(waiting ? C_BG : C_F1); text(c.x, by + 0.5f, buf, nullptr);
            }
        }
    }

    void drawStatus() {
        std::string msg; char vb[40];
        if (statusFlash > 0) msg = statusText;
        else if (learnMode) msg = learnWaiting >= 0 ? std::string("Move a MIDI controller to assign: ") + paramDef(learnWaiting).name
                                                  : "MIDI Learn: click a control, then move a controller.  Right-click a control to unassign.";
        else if (hover >= 0) { const uint32_t p = controls()[hover].param; fmt(p, vb, sizeof vb); msg = std::string(paramDef(p).name) + "   " + vb; }
        else if (hoverHandle >= 0 || dragHandle >= 0) {
            const int h = dragHandle >= 0 ? dragHandle : hoverHandle;
            if (h < 2) { char a[24], b[24]; fmt(fp(h, 4), a, sizeof a); fmt(fp(h, 5), b, sizeof b);
                msg = std::string(h == 0 ? "Filter 1   " : "Filter 2   ") + a + "   resonance " + b + "      drag: left/right = frequency, up/down = resonance"; }
            else msg = "Drag to move both filters together";
        }
        if (!msg.empty()) { fontSize(10); textLetterSpacing(0.8f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(learnMode || statusFlash > 0 ? C_F1 : C_DIM); text(DX + 40, DY + 16, msg.c_str(), nullptr); }
        if (learnMode && learnWaiting < 0) { textAlign(ALIGN_RIGHT | ALIGN_MIDDLE); fillColor(hoverClear ? C_TEXT : C_DIM); text(DX + DW - 14, DY + 16, "CLEAR ALL ASSIGNMENTS", nullptr); }
        if (statusFlash > 0) { --statusFlash; repaint(); }
    }
    void flash(const std::string& s) { statusText = s; statusFlash = 60; repaint(); }

    void drawEditBox() {
        const Ctl& c = controls()[editing];
        const float w = 90, h = 22, x = c.x - w / 2, y = c.y + c.r + 4;
        beginPath(); roundedRect(x, y, w, h, 4); fillColor(C_DISPLAY); fill(); strokeColor(C_F1); strokeWidth(1); stroke();
        fontSize(12); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(c.x, y + h / 2 + 0.5f, (editBuf + "|").c_str(), nullptr);
    }

    // ---- presets ------------------------------------------------------------------
    bool matchesValues(const float* v) const {
        for (uint32_t i = 0; i < kParamCount; ++i) {
            if (paramDef(i).hints & kHintOutput) continue;
            if (std::fabs(val[i] - v[i]) > 1e-4f * std::max(1.f, std::fabs(v[i]))) return false;
        }
        return true;
    }
    void applyValues(const float* v) {
        beginGesture();
        for (uint32_t i = 0; i < kParamCount; ++i) if (!(paramDef(i).hints & kHintOutput) && val[i] != v[i]) commit(i, v[i]);
        endGesture();
    }
    void applyFactory(int idx) { idx = (idx % (int)kNumPresets + (int)kNumPresets) % (int)kNumPresets; applyValues(presets()[idx].v); curFactory = idx; curUser.clear(); repaint(); }
    void applyUser(const std::string& name) { float v[kParamCount]; if (!loadUserPreset(name, v)) return; applyValues(v); std::copy(v, v + kParamCount, userVals); curUser = name; curFactory = -1; repaint(); }
    void programLoaded(uint32_t index) override { curFactory = (int)index; curUser.clear(); repaint(); }
    void step(int d) {
        const int nf = (int)kNumPresets, nu = (int)userPresets.size(), total = nf + nu;
        int cur = curFactory; if (!curUser.empty()) for (int i = 0; i < nu; ++i) if (userPresets[i] == curUser) cur = nf + i;
        if (cur < 0) cur = 0;
        const int next = ((cur + d) % total + total) % total;
        if (next < nf) applyFactory(next); else applyUser(userPresets[next - nf]);
    }
    void drawPresetBar() {
        const float x = PS_X, y = PS_Y, w = PS_W, h = PS_H, a = PS_ARROW;
        beginPath(); roundedRect(x, y, w, h, 5); fillColor(C_PANEL); fill(); strokeColor(menuOpen ? C_F1 : C_LINE); strokeWidth(1); stroke();
        beginPath(); moveTo(x + a, y + 5); lineTo(x + a, y + h - 5); moveTo(x + w - a, y + 5); lineTo(x + w - a, y + h - 5); strokeColor(C_LINE); stroke();
        for (int side = 0; side < 2; ++side) {
            const bool hot = presetHover == (side == 0 ? 0 : 2);
            const float cx = side == 0 ? x + a / 2 : x + w - a / 2, cy = y + h / 2, d = side == 0 ? -1.f : 1.f;
            beginPath(); moveTo(cx - 3 * d, cy - 5); lineTo(cx + 3 * d, cy); lineTo(cx - 3 * d, cy + 5); closePath(); fillColor(hot ? C_F1 : C_DIM); fill();
        }
        std::string name; bool modified = false;
        if (!curUser.empty()) { name = curUser; modified = !matchesValues(userVals); }
        else if (curFactory >= 0) { name = presets()[curFactory].name; modified = !matchesValues(presets()[curFactory].v); }
        else name = "Custom";
        if (modified) name += " *";
        fontSize(12); textLetterSpacing(1.2f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(presetHover == 1 || menuOpen ? C_TEXT : Color(0xd0, 0xd2, 0xd4));
        text(x + w / 2 - 6, y + h / 2 + 0.5f, name.c_str(), nullptr);
        const float cx = x + w - a - 14, cy = y + h / 2;
        beginPath(); moveTo(cx - 4, cy - 2); lineTo(cx + 4, cy - 2); lineTo(cx, cy + 3); closePath(); fillColor(C_DIM); fill();
    }
    struct MRect { float x, y, w, h; int kind; int idx; };
    void menuLayout(float& mx, float& my, float& mw, float& mh, std::vector<MRect>& items) const {
        const int cols = kNumCategories + 1;
        mw = PM_COLW * cols + 2 * PM_PAD; mx = std::min(W - mw - 8, std::max(8.f, PS_X + PS_W / 2 - mw / 2)); my = PS_Y + PS_H + 4;
        items.clear();
        const float top = my + PM_PAD + PM_ROW + 6 + PM_HEAD;
        int rows[kNumCategories + 1] = {}; int maxRows = 1;
        for (uint32_t i = 0; i < kNumPresets; ++i) {
            const char* cat = presets()[i].category;
            if (std::strcmp(cat, "Init") == 0) { items.push_back({ mx + PM_PAD, my + PM_PAD, PM_COLW - 4, PM_ROW, 0, (int)i }); continue; }
            for (int c = 0; c < kNumCategories; ++c)
                if (std::strcmp(cat, kCategories[c]) == 0) { items.push_back({ mx + PM_PAD + c * PM_COLW, top + rows[c] * PM_ROW, PM_COLW - 4, PM_ROW, 0, (int)i }); maxRows = std::max(maxRows, ++rows[c]); }
        }
        for (int u = 0; u < (int)userPresets.size() && u < 16; ++u) items.push_back({ mx + PM_PAD + kNumCategories * PM_COLW, top + u * PM_ROW, PM_COLW - 4, PM_ROW, 1, u });
        maxRows = std::max(maxRows, std::min(16, (int)userPresets.size()));
        items.push_back({ mx + mw - PM_PAD - 150, my + PM_PAD, 150, PM_ROW, 2, 0 });
        mh = PM_PAD + PM_ROW + 6 + PM_HEAD + maxRows * PM_ROW + PM_PAD;
    }
    void drawPresetMenu() {
        float mx, my, mw, mh; std::vector<MRect> items; menuLayout(mx, my, mw, mh, items);
        beginPath(); rect(mx - 12, my - 4, mw + 24, mh + 20); fillPaint(boxGradient(mx, my + 2, mw, mh, 6, 14, Color(0, 0, 0, 0.6f), Color(0, 0, 0, 0.f))); fill();
        beginPath(); roundedRect(mx, my, mw, mh, 6); fillColor(C_PANEL_HI); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        beginPath(); moveTo(mx + PM_PAD, my + PM_PAD + PM_ROW + 3); lineTo(mx + mw - PM_PAD, my + PM_PAD + PM_ROW + 3); strokeColor(C_LINE); stroke();
        fontSize(8.5f); textLetterSpacing(2.f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        for (int c = 0; c <= kNumCategories; ++c) {
            std::string up = c < kNumCategories ? kCategories[c] : "User"; for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
            fillColor(c < kNumCategories ? C_F1 : C_F2); text(mx + PM_PAD + c * PM_COLW + 12, my + PM_PAD + PM_ROW + 6 + PM_HEAD / 2, up.c_str(), nullptr);
        }
        if (userPresets.empty()) { fontSize(10); textLetterSpacing(0.4f); fillColor(C_FAINT); text(mx + PM_PAD + kNumCategories * PM_COLW + 12, my + PM_PAD + PM_ROW + 6 + PM_HEAD + PM_ROW / 2, "(none saved yet)", nullptr); }
        for (int i = 0; i < (int)items.size(); ++i) {
            const MRect& r = items[i]; const bool hot = i == menuHover;
            if (r.kind == 2) {
                beginPath(); roundedRect(r.x, r.y + 1, r.w, r.h - 2, 4); fillColor(hot ? withAlpha(C_F2, 0.25f) : C_PANEL); fill(); strokeColor(C_F2); strokeWidth(1); stroke();
                fontSize(9.5f); textLetterSpacing(1.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(r.x + r.w / 2, r.y + r.h / 2 + 0.5f, "SAVE CURRENT AS...", nullptr);
                continue;
            }
            const bool isCur = r.kind == 0 ? (curUser.empty() && r.idx == curFactory) : (curUser == userPresets[r.idx]);
            const bool del = r.kind == 1 && pendingDelete == r.idx;
            if (hot || del) { beginPath(); roundedRect(r.x, r.y, r.w, r.h, 4); fillColor(del ? Color(0xd0, 0x50, 0x40, 0.35f) : withAlpha(C_F1, 0.18f)); fill(); }
            if (isCur) { beginPath(); circle(r.x + 6, r.y + r.h / 2, 2.5f); fillColor(r.kind == 0 ? C_F1 : C_F2); fill(); }
            fontSize(11); textLetterSpacing(0.5f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(hot || isCur || del ? C_TEXT : C_DIM);
            const std::string label = del ? "Delete? click again" : (r.kind == 0 ? std::string(presets()[r.idx].name) : userPresets[r.idx]);
            text(r.x + 12, r.y + r.h / 2 + 0.5f, label.c_str(), nullptr);
        }
    }
    int menuHit(float x, float y) const { float mx, my, mw, mh; std::vector<MRect> it; menuLayout(mx, my, mw, mh, it);
        for (int i = 0; i < (int)it.size(); ++i) if (x >= it[i].x && x < it[i].x + it[i].w && y >= it[i].y && y < it[i].y + it[i].h) return i;
        return -1; }
    MRect menuItem(int i) const { float mx, my, mw, mh; std::vector<MRect> it; menuLayout(mx, my, mw, mh, it); return it[i]; }
    void drawSaveBox() {
        const float w = 360, h = 96, x = W / 2 - w / 2, y = 130;
        beginPath(); rect(0, 0, W, H); fillColor(Color(0, 0, 0, 0.45f)); fill();
        beginPath(); roundedRect(x, y, w, h, 8); fillColor(C_PANEL_HI); fill(); strokeColor(C_F2); strokeWidth(1); stroke();
        fontSize(9.5f); textLetterSpacing(2.f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_DIM); text(x + 16, y + 14, "SAVE PRESET AS", nullptr);
        beginPath(); roundedRect(x + 16, y + 36, w - 32, 26, 4); fillColor(C_DISPLAY); fill(); strokeColor(C_LINE); stroke();
        fontSize(13); textLetterSpacing(0.4f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_TEXT); text(x + 26, y + 49.5f, (saveName + "|").c_str(), nullptr);
        fontSize(8.5f); textLetterSpacing(1.2f); fillColor(C_FAINT); textAlign(ALIGN_RIGHT | ALIGN_TOP);
        text(x + w - 16, y + 72, saveError ? "COULD NOT SAVE - CHECK THE NAME" : "ENTER TO SAVE   ·   ESC TO CANCEL", nullptr);
    }

    // ---- interaction --------------------------------------------------------------
    int hit(float x, float y, int* seg = nullptr) const {
        const std::vector<Ctl>& cs = controls();
        for (int i = 0; i < (int)cs.size(); ++i) {
            const Ctl& c = cs[i];
            switch (c.type) {
            case Knob: { const float rr = c.r + 6; if ((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) <= rr * rr) return i; break; }
            case Toggle: if (x >= c.x - 22 && x <= c.x + 110 && std::fabs(y - c.y) <= 13) return i; break;
            case Segment: { int n = 0; segLabels(c.param, n); const float x0 = c.x - c.r / 2;
                if (x >= x0 && x < x0 + c.r && std::fabs(y - c.y) <= SEG_H / 2 + 1) { if (seg) *seg = (int)((x - x0) / (c.r / n)); return i; } break; }
            case Button: if (std::fabs(x - c.x) <= c.r / 2 && std::fabs(y - c.y) <= 15) return i; break;
            }
        }
        return -1;
    }
    int handleHit(float x, float y) const {
        if (val[kF1On] > 0.5f && val[kF2On] > 0.5f) { float hx, hy; handlePos(2, hx, hy); if ((x - hx) * (x - hx) + (y - hy) * (y - hy) <= 120) return 2; }
        for (int h = 0; h < 2; ++h) { float hx, hy; handlePos(h, hx, hy); if ((x - hx) * (x - hx) + (y - hy) * (y - hy) <= 18 * 18) return h; }
        return -1;
    }
    int headerHit(float x, float y) const {
        if (y < PS_Y || y > PS_Y + PS_H) return 0;
        if (x >= LEARN_X && x <= LEARN_X + LEARN_W) return 1;
        if (x >= MIDI_X && x <= MIDI_X + MIDI_W) return 2;
        if (x >= UNDO_X && x <= UNDO_X + UR_W) return 3;
        if (x >= REDO_X && x <= REDO_X + UR_W) return 4;
        return 0;
    }
    int presetBarHit(float x, float y) const {
        if (y < PS_Y || y > PS_Y + PS_H || x < PS_X || x > PS_X + PS_W) return -1;
        return x < PS_X + PS_ARROW ? 0 : (x > PS_X + PS_W - PS_ARROW ? 2 : 1);
    }
    bool clearHit(float x, float y) const { return learnMode && learnWaiting < 0 && y > DY + 8 && y < DY + 24 && x > DX + DW - 190 && x < DX + DW - 10; }

    void setLearn(bool on) { learnMode = on; learnWaiting = -1; if (!on) setState("cmd", "cancel"); repaint(); }
    void endEdit(bool apply) {
        if (editing < 0) return;
        const uint32_t p = controls()[editing].param; float v;
        if (apply && parse(p, editBuf, v)) { beginGesture(); commitLinked(p, v); endGesture(); }
        editing = -1; editBuf.clear(); repaint();
    }
    // knob angle (0..1) of a point, for circular dragging
    static float angleNorm(const Ctl& c, float x, float y) {
        float a = std::atan2(y - c.y, x - c.x);                          // -pi..pi, 0 = right, +pi/2 = down
        const float a0 = 0.75f * 3.14159265f;
        float rel = a - a0; while (rel < 0) rel += 2 * 3.14159265f;      // 0 .. 2pi from the start of the arc
        if (rel > 1.5f * 3.14159265f) rel = rel > 1.75f * 3.14159265f ? 0.f : 1.5f * 3.14159265f;   // dead zone at the bottom
        return rel / (1.5f * 3.14159265f);
    }

    bool onKeyboard(const KeyboardEvent& ev) override {
        if (!ev.press) return false;
        std::string* buf = saving ? &saveName : (editing >= 0 ? &editBuf : nullptr);
        if (!buf) {
            const bool cmd = (ev.mod & (kModifierControl | kModifierSuper)) != 0;
            if (cmd && (ev.key == 'z' || ev.key == 'Z')) { if (ev.mod & kModifierShift) redo(); else undo(); return true; }
            if (cmd && (ev.key == 'y' || ev.key == 'Y')) { redo(); return true; }
            if (ev.key == kKeyEscape && learnMode) { setLearn(false); return true; }
            return false;
        }
        if (ev.key == kKeyEnter) {
            if (saving) {
                if (saveUserPreset(saveName, val)) { userPresets = listUserPresets(); curUser = sanitizeName(saveName); curFactory = -1; std::copy(val, val + kParamCount, userVals); saving = false; saveError = false; }
                else saveError = true;
            } else endEdit(true);
            repaint(); return true;
        }
        if (ev.key == kKeyEscape) { if (saving) saving = false; else endEdit(false); repaint(); return true; }
        if (ev.key == kKeyBackspace || ev.key == kKeyDelete) { if (!buf->empty()) buf->pop_back(); repaint(); return true; }
        if (ev.key >= 32 && ev.key < 127 && buf->size() < (saving ? 40u : 12u)) { *buf += (char)ev.key; repaint(); return true; }
        return true;
    }

    bool onMouse(const MouseEvent& ev) override {
        const float s = uiScale(), x = ev.pos.getX() / s, y = ev.pos.getY() / s;
        if (saving) return true;
        if (ev.press) statusFlash = 0;
        if (ev.button == 3 && ev.press) {
            if (menuOpen) { const int m = menuHit(x, y); if (m >= 0 && menuItem(m).kind == 1) { pendingDelete = menuItem(m).idx; repaint(); } return true; }
            if (learnMode) { const int i = hit(x, y); if (i >= 0) { const uint32_t p = controls()[i].param; char b[24]; snprintf(b, sizeof b, "unmap:%u", p); setState("cmd", b); paramCC[p] = -1; repaint(); } return true; }
            return false;
        }
        if (ev.button != 1) return false;
        if (!ev.press) {
            if (active >= 0) {
                const uint32_t p = controls()[active].param, q = linkPartner(p);
                editParameter(p, false); if (linkedDrag && q != (uint32_t)-1) editParameter(q, false);
                active = -1; endGesture(); return true;
            }
            if (dragHandle >= 0) {
                const uint32_t ps[4] = { kF1Freq, kF1Res, kF2Freq, kF2Res };
                for (uint32_t p : ps) editParameter(p, false);
                dragHandle = -1; endGesture(); return true;
            }
            return false;
        }
        if (editing >= 0) endEdit(true);
        if (menuOpen) {
            const int m = menuHit(x, y);
            if (m >= 0) {
                const MRect r = menuItem(m);
                if (r.kind == 2) { menuOpen = false; saving = true; saveError = false; saveName = curUser; }
                else if (r.kind == 1 && pendingDelete == r.idx) { deleteUserPreset(userPresets[r.idx]); if (curUser == userPresets[r.idx]) curUser.clear(); userPresets = listUserPresets(); pendingDelete = -1; repaint(); return true; }
                else { menuOpen = false; if (r.kind == 0) applyFactory(r.idx); else applyUser(userPresets[r.idx]); }
            } else menuOpen = false;
            pendingDelete = -1; menuHover = -1; repaint(); return true;
        }
        const int hh = headerHit(x, y);
        if (hh == 1) { setLearn(!learnMode); return true; }
        if (hh == 2) { beginGesture(); commit(kMidiOn, val[kMidiOn] > 0.5f ? 0.f : 1.f); endGesture(); return true; }
        if (hh == 3) { undo(); return true; }
        if (hh == 4) { redo(); return true; }
        const int pb = presetBarHit(x, y);
        if (pb == 0) { step(-1); return true; }
        if (pb == 2) { step(1); return true; }
        if (pb == 1) { userPresets = listUserPresets(); menuOpen = true; menuHover = -1; pendingDelete = -1; repaint(); return true; }
        if (clearHit(x, y)) { setState("cmd", "clear"); for (int& m : paramCC) m = -1; repaint(); return true; }

        int seg = 0; const int i = hit(x, y, &seg);
        if (i >= 0) {
            const Ctl& c = controls()[i];
            const uint32_t p = c.param;
            if (learnMode) { learnWaiting = (int)p; char b[24]; snprintf(b, sizeof b, "learn:%u", p); setState("cmd", b); repaint(); return true; }
            if (c.type == Toggle || c.type == Button) { beginGesture(); commit(p, val[p] > 0.5f ? 0.f : 1.f); endGesture(); return true; }
            if (c.type == Segment) { beginGesture(); commit(p, (float)seg); endGesture(); return true; }
            if (ev.mod & (kModifierControl | kModifierSuper)) { beginGesture(); commitLinked(p, defv[p]); endGesture(); return true; }
            if (i == lastClick && ev.time - lastClickTime < 350) {
                lastClick = -1; editing = i;
                char b[40]; fmt(p, b, sizeof b); editBuf = b;
                const size_t sp = editBuf.find(' '); if (sp != std::string::npos) editBuf.resize(sp);
                if (!editBuf.empty() && editBuf.back() == '%') editBuf.pop_back();
                if ((p == kF1Freq || p == kF2Freq) && val[p] >= 1000) { char kb[16]; snprintf(kb, sizeof kb, "%.2fk", val[p] / 1000); editBuf = kb; }
                repaint(); return true;
            }
            lastClick = i; lastClickTime = ev.time;
            beginGesture();
            active = i; dragY = y; dragNorm = toNorm(p, val[p]);
            // grab the rim to turn the knob in a circle; grab the middle to drag up and down
            const float dist = std::sqrt((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y));
            circular = dist > c.r * 0.62f;
            const uint32_t q = linkPartner(p); linkedDrag = val[kLink] > 0.5f && q != (uint32_t)-1;
            editParameter(p, true); if (linkedDrag) editParameter(q, true);
            if (circular) setLinked(p, fromNorm(p, angleNorm(c, x, y)));
            repaint(); return true;
        }
        if (!learnMode) {
            const int h = handleHit(x, y);
            if (h >= 0) {
                beginGesture();
                dragHandle = h; dragOpposite = (ev.mod & (kModifierControl | kModifierSuper)) != 0; dragX0 = x; dragY0 = y;
                for (int f = 0; f < 2; ++f) { startOct[f] = std::log2(val[fp(f, 4)]); startRes[f] = val[fp(f, 5)]; }
                const uint32_t ps[4] = { kF1Freq, kF1Res, kF2Freq, kF2Res };
                for (uint32_t p : ps) editParameter(p, true);
                return true;
            }
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override {
        const float s = uiScale(), x = ev.pos.getX() / s, y = ev.pos.getY() / s;
        if (saving) return true;
        if (active >= 0) {
            const Ctl& c = controls()[active]; const uint32_t p = c.param;
            if (circular) { setLinked(p, fromNorm(p, angleNorm(c, x, y))); return true; }
            const float sens = (ev.mod & kModifierShift) ? 1500.f : 240.f;
            dragNorm = clampf(dragNorm + (dragY - y) / sens, 0.f, 1.f); dragY = y;
            setLinked(p, fromNorm(p, dragNorm));
            return true;
        }
        if (dragHandle >= 0) {
            const float dOct = std::log2(hzOfX(x) / hzOfX(dragX0)), dRes = resOfY(y) - resOfY(dragY0);
            const bool both = dragHandle == 2 || val[kLink] > 0.5f || dragOpposite;
            for (int f = 0; f < 2; ++f) {
                if (!both && f != dragHandle) continue;
                const float sign = (dragOpposite && dragHandle < 2 && f != dragHandle) ? -1.f : 1.f;
                setValue(fp(f, 4), clampf(std::pow(2.f, startOct[f] + sign * dOct), 20.f, 20000.f));
                setValue(fp(f, 5), clampf(startRes[f] + dRes, 0.f, 1.f));
            }
            return true;
        }
        if (menuOpen) { const int m = menuHit(x, y); if (m != menuHover) { menuHover = m; repaint(); } return true; }
        const int hh = headerHit(x, y); if (hh != hoverHeader) { hoverHeader = hh; repaint(); }
        const int pb = presetBarHit(x, y); if (pb != presetHover) { presetHover = pb; repaint(); }
        const bool hc = clearHit(x, y); if (hc != hoverClear) { hoverClear = hc; repaint(); }
        const int h = hit(x, y); if (h != hover) { hover = h; repaint(); }
        const int hd = h >= 0 || learnMode ? -1 : handleHit(x, y); if (hd != hoverHandle) { hoverHandle = hd; repaint(); }
        return false;
    }

    bool onScroll(const ScrollEvent& ev) override {
        const float s = uiScale(), sx = ev.pos.getX() / s, sy = ev.pos.getY() / s;
        if (menuOpen || saving) return true;
        if (presetBarHit(sx, sy) >= 0) { step(ev.delta.getY() > 0 ? -1 : 1); return true; }
        const int i = hit(sx, sy);
        if (i < 0 || controls()[i].type != Knob) return false;
        const uint32_t p = controls()[i].param;
        const float st = (ev.mod & kModifierShift) ? 0.004f : 0.02f;
        beginGesture(); commitLinked(p, fromNorm(p, toNorm(p, val[p]) + (ev.delta.getY() > 0 ? st : -st))); endGesture();
        return true;
    }

private:
    float val[kParamCount], defv[kParamCount], userVals[kParamCount] = {}, before[kParamCount];
    int paramCC[kParamCount];
    int active = -1, hover = -1, lastClick = -1, editing = -1;
    uint lastClickTime = 0;
    float dragY = 0, dragNorm = 0; bool circular = false, linkedDrag = false, gesture = false;
    int dragHandle = -1, hoverHandle = -1; bool dragOpposite = false;
    float dragX0 = 0, dragY0 = 0, startOct[2] = {}, startRes[2] = {};
    std::vector<Snap> undoStack, redoStack;
    std::string editBuf, statusText; int statusFlash = 0;
    int curFactory = 0, presetHover = -1, menuHover = -1, pendingDelete = -1, hoverHeader = 0;
    std::string curUser; std::vector<std::string> userPresets;
    bool menuOpen = false, saving = false, saveError = false; std::string saveName;
    bool learnMode = false, hoverClear = false; int learnWaiting = -1; float lastLearnEcho = 0;
    ResizeHandle resizeHandle;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BasaltUI)
};

UI* createUI() { return new BasaltUI(); }

END_NAMESPACE_DISTRHO
