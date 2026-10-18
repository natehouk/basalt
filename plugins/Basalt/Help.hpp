// Basalt — hover tips and the in-plugin help pages. License: MIT.
#pragma once
#include "BasaltEngine.hpp"

namespace basalt {

static const char* const kManualURL = "https://github.com/natehouk/basalt/blob/main/docs/MANUAL.md";

// A short explanation for each parameter, shown in the hover tip under its name and value.
inline const char* paramTip(uint32_t p) {
    switch (p) {
    case kF1On: case kF2On:
        return "Switches this filter in or out. Turning it off crossfades to a bypass over a few milliseconds, so it is safe mid-performance.";
    case kF1Style: case kF2Style:
        return "The filter's character. SMOOTH: gentle overdrive, never self-oscillates. FAT: thick, soft-limited resonance that sings as a clean tone near 100%. RAW: hard-clipped and buzzy, self-oscillates aggressively.";
    case kF1Type: case kF2Type:
        return "LP keeps everything below the frequency, HP everything above it, BP a band around it. With Resonance up, BP becomes a narrow, vocal peak.";
    case kF1Slope: case kF2Slope:
        return "How steeply the filter cuts beyond its frequency: 12 dB/oct is gentle and open, 24 is the classic synth slope, 48 is a near brick wall.";
    case kF1Freq: case kF2Freq:
        return "Cutoff (LP/HP) or centre (BP) frequency, 20 Hz to 20 kHz. You can also drag this filter's numbered handle left and right in the display. With Link on, the other filter follows by the same ratio.";
    case kF1Res: case kF2Res:
        return "Boost at the cutoff frequency. Higher settings make sweeps more pronounced; with FAT or RAW near 100% the filter rings on its own as a tone. Drag the handle up and down to set it from the display.";
    case kRouting:
        return "SERIAL: filter 1 feeds filter 2, so their responses multiply (LP + HP = band pass). PARALLEL: both filters hear the input and are mixed (two band passes = two peaks). Switching crossfades smoothly.";
    case kLink:
        return "When on, frequency and resonance moves on either filter (knob, handle or MIDI controller) are applied to both, keeping the spacing between them. Ideal for sweeping a two-filter sound with one hand.";
    case kInGain:
        return "Level going into the filters. Raising it drives the characters harder for more grit and saturation; lowering it cleans them up.";
    case kOutGain:
        return "Level after the filters. Use it to make up level after narrow band-pass or steep low-pass settings, or to tame loud resonance.";
    case kMix:
        return "Blend between the dry input (0%) and the filtered signal (100%). Settings in between keep some of the original underneath.";
    case kMidiOn:
        return "Turns MIDI control on or off. When off, learned controllers are ignored.";
    default: return "";
    }
}

// Tips for the header, the display handles and the preset bar.
enum TipTarget { kTipUndo, kTipRedo, kTipLearn, kTipMidi, kTipHelp, kTipPrev, kTipPresets, kTipNext,
                 kTipHandle1, kTipHandle2, kTipHandleBoth, kNumTipTargets };
inline const char* targetTitle(int t) {
    static const char* const n[kNumTipTargets] = { "Undo", "Redo", "MIDI Learn", "MIDI", "Help", "Previous preset", "Presets", "Next preset",
                                                   "Filter 1 handle", "Filter 2 handle", "Both filters" };
    return n[t];
}
inline const char* targetTip(int t) {
    switch (t) {
    case kTipUndo: return "Steps back through your edits, including preset loads. Cmd/Ctrl-Z.";
    case kTipRedo: return "Re-applies an edit you undid. Cmd/Ctrl-Shift-Z or Cmd/Ctrl-Y.";
    case kTipLearn: return "Click, then click any control and move a knob or fader on your MIDI controller to assign it. Right-click a control to unassign. Assignments are saved with your project.";
    case kTipMidi: return paramTip(kMidiOn);
    case kTipHelp: return "Opens the help pages: what every control does, playing live, MIDI Learn and shortcuts.";
    case kTipPrev: return "Load the previous preset. The mouse wheel over the preset name also steps through presets.";
    case kTipPresets: return "Open the preset browser: factory presets by category, your own presets, and Save Current As. A * after the name means you have changed it.";
    case kTipNext: return "Load the next preset.";
    case kTipHandle1: case kTipHandle2:
        return "Drag left/right to set the frequency and up/down to set the resonance. Cmd/Ctrl-drag moves the other filter the opposite way. With Link on, both filters move together.";
    case kTipHandleBoth: return "Drag to move both filters together: left/right shifts both frequencies, up/down both resonances.";
    default: return "";
    }
}

// In-plugin help pages.
struct HelpPage { const char* title; const char* body; };
static const HelpPage kHelpPages[] = {
    { "Overview",
      "Basalt is a stereo dual filter built for playing live. Two multimode filters, each with three characters, run in serial or "
      "parallel. Everything you need is on one screen: a large display you can grab, big knobs, and large buttons.\n\n"
      "The display shows filter 1 (orange), filter 2 (blue) and, when both are on, their combined response (white). "
      "Every change is smoothed, so nothing clicks, even when you switch filters or routing while audio is playing.\n\n"
      "Hover over any control for a moment to see what it does. Turn the tips off at the bottom of this window." },
    { "Filters",
      "TYPE: LP (low pass) keeps what is below the frequency, HP (high pass) what is above, BP (band pass) a band around it.\n\n"
      "SLOPE: 12, 24 or 48 dB per octave, from gentle to near brick wall.\n\n"
      "CHARACTER: SMOOTH has moderate overdrive and wide resonance and never self-oscillates. FAT has a thick, soft-limited "
      "resonance that turns into a clean sine-like tone near 100%. RAW clips its feedback hard for a buzzy, aggressive sound.\n\n"
      "FREQUENCY and RESONANCE: the cutoff (or centre) and the boost around it.\n\n"
      "ROUTING: in SERIAL, filter 1 feeds filter 2 (an LP and an HP make a band pass). In PARALLEL, both hear the input and "
      "are mixed (two band passes make two peaks, like a vowel)." },
    { "The display",
      "Drag a numbered handle: left and right set its frequency, up and down set its resonance.\n\n"
      "When both filters are on, a dot sits between the handles. Drag it to move both filters together.\n\n"
      "Cmd/Ctrl-drag a handle to move the other filter in the opposite direction: the two spread apart or close in.\n\n"
      "The horizontal scale is frequency (20 Hz to 20 kHz), the vertical scale is level in dB." },
    { "Playing live",
      "LINK FILTERS applies every frequency and resonance move to both filters, from knobs, handles or MIDI, keeping their "
      "spacing. Map one controller to filter 1's frequency and you sweep both.\n\n"
      "KNOBS: grab the rim to turn a knob in a circle (it jumps to where you click); grab the middle and drag up and down "
      "for relative moves. Hold Shift for fine control.\n\n"
      "Switching a filter on or off, or changing routing, crossfades over a few milliseconds, so you can do it on the beat.\n\n"
      "The LIVE preset category has DJ-style starting points: filters parked fully open, ready to sweep." },
    { "MIDI Learn",
      "1. Click LEARN in the header.\n2. Click the control you want to play.\n3. Move a knob, fader or wheel on your MIDI "
      "controller. The control shows its CC number.\n\n"
      "Right-click a control in Learn mode to unassign it, or use CLEAR ALL ASSIGNMENTS at the top of the display. "
      "Click LEARN again (or press Esc) to finish.\n\n"
      "Assignments are saved with your project. The MIDI button turns MIDI control off without forgetting them. "
      "Route a MIDI track to Basalt in your DAW so it receives the controller." },
    { "Presets",
      "Click the preset name to open the browser. Factory presets are grouped into Low Pass, High & Band, Dual, Resonant and Live. "
      "The arrows (or the mouse wheel over the name) step through them.\n\n"
      "SAVE CURRENT AS... stores your own preset. They appear in the User column and load the same on every computer:\n"
      "macOS: ~/Library/Audio/Presets/Epsilon Audio/Basalt\nWindows: %APPDATA%\\Epsilon Audio\\Basalt\\Presets\n"
      "Linux: ~/.config/epsilon-audio/basalt/presets\n\n"
      "To delete a user preset, right-click it in the browser, then click it again to confirm.\n\n"
      "Loading a preset can be undone." },
    { "Shortcuts",
      "Double-click a knob: type a value (for example 1.5k, 440, -6).\n"
      "Cmd/Ctrl-click a knob: reset it to its default.\n"
      "Shift-drag: fine adjustment.\n"
      "Mouse wheel over a knob: small steps (Shift for finer).\n"
      "Cmd/Ctrl-Z: undo.   Cmd/Ctrl-Shift-Z or Cmd/Ctrl-Y: redo.\n"
      "Esc: leave MIDI Learn, close this window or a text field.\n\n"
      "Some hosts keep keyboard shortcuts for themselves; the UNDO and REDO buttons always work.\n\n"
      "The window can be resized from its corner (or the host's window frame)." },
};
static constexpr int kNumHelpPages = sizeof(kHelpPages) / sizeof(kHelpPages[0]);

} // namespace basalt
