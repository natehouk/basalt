# Basalt 1.1 — User Manual

Basalt is a stereo dual filter built for playing live: two multimode filters with three characters each, serial or parallel routing, a large display you can grab, big controls, Link, undo/redo and MIDI Learn. It runs as VST3, AU, CLAP and VST2 on macOS, Windows and Linux.

**Contents**

1. [Installing](#1-installing)
2. [A quick tour](#2-a-quick-tour)
3. [The filters](#3-the-filters)
4. [Routing: serial and parallel](#4-routing-serial-and-parallel)
5. [The display](#5-the-display)
6. [Link Filters](#6-link-filters)
7. [Knobs and other controls](#7-knobs-and-other-controls)
8. [Input, output and mix](#8-input-output-and-mix)
9. [Undo and redo](#9-undo-and-redo)
10. [MIDI Learn](#10-midi-learn)
11. [Presets](#11-presets)
12. [Help and hover tips](#12-help-and-hover-tips)
13. [Recipes](#13-recipes)
14. [Keyboard and mouse reference](#14-keyboard-and-mouse-reference)
15. [Parameter reference](#15-parameter-reference)
16. [Troubleshooting](#16-troubleshooting)

---

## 1. Installing

Download the zip for your system from the project's **Releases** page and copy the plugin files:

| System | Copy | To |
|---|---|---|
| macOS | `Basalt.vst3`, `Basalt.component` (AU), `Basalt.clap` | `~/Library/Audio/Plug-Ins/VST3`, `.../Components`, `.../CLAP` |
| Windows | the `Basalt.vst3` folder, `Basalt.clap` | `C:\Program Files\Common Files\VST3\`, `...\CLAP\` |
| Linux | `Basalt.vst3`, `Basalt.clap` | `~/.vst3/`, `~/.clap/` |

**macOS:** the builds are signed ad hoc but not notarized, so macOS may block them the first time. Open Terminal and run:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/*/Basalt*
```

Then rescan plugins in your DAW. Basalt appears under **Epsilon Audio** as an audio effect (filter).

---

## 2. A quick tour

![Basalt](../screenshot.png)

- **Header**: the preset browser, **UNDO** / **REDO**, **LEARN** (MIDI Learn), **MIDI** (MIDI control on/off) and **HELP**.
- **Display**: both filter curves (filter 1 orange, filter 2 blue) and, when both are on, the combined response (white). Drag the numbered handles to play the filters.
- **Filter 1 and Filter 2 panels**: on/off, character, type, slope, and big **Frequency** and **Resonance** knobs.
- **Centre panel**: **Routing**, **Link Filters**, and **Input**, **Output** and **Mix**.

Hover over anything for a moment and a tip explains it.

---

## 3. The filters

Each filter has the same controls.

**On/off.** Switching a filter off crossfades it to a clean bypass over a few milliseconds; switching it back on crossfades it in. Its internal state keeps running while it is off, so there is no thump when it returns.

**Type.**
- **LP — low pass**: keeps what is below the frequency and removes what is above. The classic "close the filter" sweep.
- **BP — band pass**: keeps a band around the frequency. With resonance, a narrow, vocal peak.
- **HP — high pass**: keeps what is above the frequency. Thins a sound out; sweeping it up builds tension.

**Slope.** How steeply the filter cuts beyond its frequency:
- **12 dB/oct** — gentle and open.
- **24 dB/oct** — the classic synthesizer slope.
- **48 dB/oct** — steep, almost a brick wall.

Type and slope together give nine responses per filter.

**Character.** How the filter behaves when pushed:

| Character | What it does | Self-oscillates? |
|---|---|---|
| **Smooth** | Moderate overdrive at the input and a wide, polite resonance. The cleanest choice. | No |
| **Fat** | A thicker sound whose resonance is soft-limited, so at high settings it turns into a smooth, sine-like tone. (This is the filter from Monolith.) | Yes, smoothly |
| **Raw** | Hard-clipped feedback: gritty and buzzy, aggressive at high resonance. | Yes, with a buzz |

**Frequency** (20 Hz – 20 kHz) is the cutoff for LP and HP, or the centre for BP.

**Resonance** (0 – 100%) boosts the region around the frequency. With **Fat** or **Raw** near 100%, the filter rings on its own and produces a pitched tone even with little input — sweep the frequency to play it.

---

## 4. Routing: serial and parallel

**Serial**: the input goes through filter 1, then filter 2. Their responses multiply.
- LP + HP = a band pass whose width you set with the two frequencies.
- LP + LP = an extra-steep low pass.

**Parallel**: both filters hear the input, and their outputs are mixed.
- BP + BP = two resonant peaks (vowel-like, formant sounds).
- LP + HP with a gap between them = a notch.

If only one filter is on, routing makes no difference; the Routing buttons dim to show this. Changing routing crossfades smoothly, so it is safe while playing.

---

## 5. The display

The horizontal axis is frequency (20 Hz – 20 kHz, logarithmic); the vertical axis is level in dB (0 dB is the line where nothing is changed).

| Action | Result |
|---|---|
| Drag a numbered handle left / right | Change that filter's frequency |
| Drag a numbered handle up / down | Change that filter's resonance |
| Drag the dot between the handles | Move both filters together |
| Cmd/Ctrl-drag a handle | Move the other filter in the opposite direction (they spread apart or close in) |
| With Link on, drag either handle | Both filters move together |

A dimmed curve or handle belongs to a filter that is switched off.

---

## 6. Link Filters

With **Link Filters** on, every move of a frequency or resonance — from a knob, a display handle, a typed value, the mouse wheel or a **MIDI controller** — is applied to both filters:

- **Frequency** moves by the same *ratio*, so the musical interval between the filters is kept (a band stays the same width as you sweep it).
- **Resonance** moves by the same *amount*.

This is the easiest way to play a two-filter sound with one hand or one controller: learn a knob to filter 1's frequency, turn Link on, and the whole shape sweeps.

---

## 7. Knobs and other controls

Knobs can be turned two ways:

- **Grab the middle and drag up/down** for relative moves. Hold **Shift** for fine control.
- **Grab the rim and move in a circle.** The knob jumps to where you click and follows the angle of the mouse — fast and precise for big moves.

Also:
- **Double-click** a knob to type a value (`1.5k`, `440`, `-6`, `75`), then press Enter.
- **Cmd/Ctrl-click** resets it to its default.
- The **mouse wheel** nudges it (Shift for smaller steps).

The big Frequency and Resonance knobs always show their value underneath. The small knobs show their value while you hover them.

Buttons (character, type, slope, routing) switch with one click.

---

## 8. Input, output and mix

- **Input** (−24 to +24 dB) sets the level going into the filters. More input drives the characters harder — more saturation from Smooth, more grit from Raw.
- **Output** (−24 to +24 dB) sets the level after the filters. Use it to make up level after narrow band passes or steep cuts.
- **Mix** (0 – 100%) blends the dry input with the filtered signal. 100% is fully filtered.

The output has a soft safety ceiling just below 0 dBFS, so runaway resonance cannot produce digital overs.

---

## 9. Undo and redo

Every edit you make in Basalt's window becomes one undo step: a knob drag, a handle drag, a button click, a typed value, or loading a preset.

- **UNDO** / **REDO** buttons in the header (hover them to see how many steps are available).
- **Cmd/Ctrl-Z** to undo, **Cmd/Ctrl-Shift-Z** or **Cmd/Ctrl-Y** to redo.

Changes coming from host automation or MIDI controllers are not recorded as undo steps. Some hosts capture keyboard shortcuts for themselves; the buttons always work.

---

## 10. MIDI Learn

1. Make sure Basalt receives MIDI: in most DAWs, route a MIDI track's output to Basalt (or select Basalt as the MIDI destination).
2. Click **LEARN**. Basalt shows "MIDI Learn: click a control...".
3. Click the control you want to play. It shows a **?** badge.
4. Move a knob, fader or wheel on your controller. The control now shows its **CC** number.
5. Repeat for other controls, then click **LEARN** again (or press Esc) to finish.

To remove an assignment, enter Learn mode and **right-click** the control. **CLEAR ALL ASSIGNMENTS** (top right of the display in Learn mode) removes them all.

Assignments are saved with your project. The **MIDI** button switches MIDI control off without forgetting them. With Link on, a learned frequency or resonance control moves both filters.

---

## 11. Presets

Click the preset name to open the browser.

| Category | What's inside |
|---|---|
| **Low Pass** | Warm, dark, fat and raw low passes |
| **High & Band** | Thinning, rumble removal, telephone and radio sounds |
| **Dual** | Two-filter shapes: band pairs, notches, twin peaks, vowels |
| **Resonant** | Squelch, scream, whistle, self-oscillation |
| **Live** | DJ-style starting points with the filters parked fully open, ready to sweep |

- The **arrows** beside the name (or the mouse wheel over it) step through factory presets, then your own.
- A **\*** after the name means you have changed the preset.
- **SAVE CURRENT AS...** saves your own preset. Type a name and press Enter.
- To delete a user preset, **right-click** it in the browser, then click it again to confirm.

User presets are small text files that load on any computer:

| System | Folder |
|---|---|
| macOS | `~/Library/Audio/Presets/Epsilon Audio/Basalt/` |
| Windows | `%APPDATA%\Epsilon Audio\Basalt\Presets\` |
| Linux | `~/.config/epsilon-audio/basalt/presets/` |

Loading a preset can be undone.

**Search and favourites.** With the browser open, just type to narrow the list to presets whose name or category matches; Backspace edits, Esc clears the search and then closes the browser. Click the star at the right of a preset to add it to the FAVOURITES column, and again to remove it. Favourites are kept in `favourites.txt` in your presets folder.

---

## 12. Help and hover tips

- **Hover tips**: rest the pointer on any control, handle or header button for a moment and a tip explains it, including its current value and any MIDI CC assigned to it.
- **HELP** opens the help window: short pages on every part of Basalt. The **HOVER TIPS** switch at the bottom turns the tips off or on, and **FULL MANUAL (WEB)** opens this manual in your browser.

---

## 13. Recipes

**DJ sweep.** Load *Live › DJ Low Pass*. Learn a knob to Filter 1 Frequency. Turn it down to close the filter, up to open it again. For a high-pass sweep, use Filter 2 (it sits at 20 Hz) or load *DJ High Pass*.

**Build-up.** Load *Live › Build-Up*. Raise Filter 1 (a resonant high pass) over 8 or 16 bars; Link brings filter 2 along. Snap it back down on the drop — the change is smoothed, so it won't click.

**Talking filter.** Load *Dual › Vowel* (two resonant band passes in parallel, linked). Sweep either handle slowly between 300 Hz and 2 kHz.

**Acid squelch.** Fat character, LP 24 dB, Resonance 75–85%, Frequency around 400 Hz. Automate or play the frequency; raise Input for more grit.

**Self-oscillating tone.** Fat, LP, Resonance 100%. The filter rings at its frequency; play the Frequency knob like a theremin. Lower Mix to blend it with the dry signal.

**Telephone.** HP 24 dB at 500 Hz into LP 24 dB at 3.5 kHz, serial (*High & Band › Radio*). Narrow it further for a tiny speaker.

---

## 14. Keyboard and mouse reference

| Action | Result |
|---|---|
| Drag knob middle up/down | Adjust (Shift = fine) |
| Grab knob rim and turn | Set by angle |
| Double-click knob | Type a value |
| Cmd/Ctrl-click knob | Reset to default |
| Mouse wheel on knob | Small steps (Shift = smaller) |
| Drag display handle | Frequency (x) and resonance (y) |
| Drag centre dot | Move both filters |
| Cmd/Ctrl-drag handle | Move the filters in opposite directions |
| Cmd/Ctrl-Z | Undo |
| Cmd/Ctrl-Shift-Z, Cmd/Ctrl-Y | Redo |
| Mouse wheel on preset name | Previous / next preset |
| Right-click (Learn mode) | Remove MIDI assignment |
| Right-click user preset | Arm delete |
| Esc | Leave Learn mode, close Help, cancel typing |

---

## 15. Parameter reference

All parameters can be automated in your DAW.

| Parameter | Range | Default |
|---|---|---|
| Filter 1 / 2 On | Off, On | 1: On · 2: Off |
| Filter 1 / 2 Character | Smooth, Fat, Raw | Smooth |
| Filter 1 / 2 Type | Low Pass, Band Pass, High Pass | 1: Low Pass · 2: High Pass |
| Filter 1 / 2 Slope | 12, 24, 48 dB/oct | 1: 24 · 2: 12 |
| Filter 1 / 2 Freq | 20 Hz – 20 kHz | 1: 2 kHz · 2: 100 Hz |
| Filter 1 / 2 Resonance | 0 – 100% | 1: 25% · 2: 10% |
| Routing | Serial, Parallel | Serial |
| Link Filters | Off, On | Off |
| Input Gain | −24 – +24 dB | 0 dB |
| Output Gain | −24 – +24 dB | 0 dB |
| Mix | 0 – 100% | 100% |
| MIDI | Off, On | On |

---

## 16. Troubleshooting

**The plugin doesn't appear in my DAW.** Check that the files are in the folders above and rescan. On macOS, run the `xattr` command from section 1. Logic needs the `.component` (AU) file; Bitwig, Ableton Live, Reaper and others can use VST3 or CLAP.

**MIDI Learn doesn't respond.** Basalt has to receive MIDI from a track or your controller; check your DAW's MIDI routing to the plugin, and that the **MIDI** button is lit.

**Cmd/Ctrl-Z undoes something in my DAW instead.** Your host is keeping the shortcut; use the **UNDO** / **REDO** buttons.

**A resonant setting is too loud.** Lower Resonance or Output, or switch to the Smooth character, which never self-oscillates.

---

Basalt is free software under the MIT license. "FabFilter" and "Simplon" are trademarks of FabFilter Software Instruments; Basalt is an independent design and is not affiliated with or endorsed by FabFilter.
