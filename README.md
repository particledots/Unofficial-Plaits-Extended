# Unofficial Plaits Extended

A community AU / VST3 instrument for Logic Pro (and other hosts) that plays the Plaits macro-oscillator engines **plus the
extra models from Rubato Audio's Plaits Palette** and the community models in it. 95 models in one plug-in.

This is not a Mutable Instruments, Emilie Gillet or Rubato Audio product, and none of them support it.

Same approach as the other plugins in this series (Unofficial Beads / Rings / Plaits / Elements): the DSP is the original open-source
code, wrapped with JUCE, built on GitHub Actions so nothing has to be compiled locally.

## What is in it

- 61 Mutable Instruments models: the stock Plaits models (including the three 6-Op FM banks) and the other Mutable-authored models in the Palette (Braids-style oscillators, extra percussion and physical models).
- 21 Rubato Lab models and 13 community models (Tom Burns, Dylan Bolink, Combust, Tim Churches, Dirk Hoppmann).
- **Macro 4**: the Palette's fourth synthesis control. 0.5 is neutral; stock Plaits models ignore it.
- Each knob shows what the selected model calls it.

Left out on purpose (`EXCLUDED` in `tools/gen_models.py`): the two retired Virtual Analog variants, and **Natural Speech**, whose
bundled demo data is encoded from TTS-generated audio with no stated data licence.

## Getting the plug-in

1. Open the **Actions** tab, pick the latest passing run, download the **Plaits-Extended-macOS** artifact.
2. Follow `INSTALL.txt` inside it.

## What has and has not been verified

Confirmed by the GitHub macOS build (Actions run for commit `46d3d1e`) and, for the tests, also on Linux with both g++ and clang.

| Check | Status |
|---|---|
| All 118 DSP files compile, 95 models register, AU + VST3 build, ad-hoc sign, package | passes in macOS CI |
| Every model x 3 envelope modes x 3 sample rates: finite, bounded, not silent; pitch within 5 cents (`engine_test`, 1440 runs) | passes in macOS CI |
| Switching models while notes play, with junk-filled memory (`sweep_test`) | passes in macOS CI |
| Every engine fits the shared RAM block (`ram_probe`; largest need 16 KB of 32 KB) | passes in macOS CI |
| All models through the real plug-in code: menu index <-> model name, MIDI -> sound, Macro 4, state save/restore (`plugin_test`) | passes in macOS CI |
| Pitch Snap (Off / Steps / Semitones on Transpose + Fine Tune, 0.2.0): the snapping function is exact, the pitch lands on the snapped note (measured), every model stays finite in all three modes (`pitch_snap_test`); the real parameters give the same pitch and a project saved without it loads with it Off (`plugin_test`) | passes in macOS CI |
| Pitch Snap Off sounds exactly like 0.1.0: 130 million samples (every model x 3 envelope modes x 5 Transpose values x 2 sample rates) from the 0.1.0 engine and the new engine compared byte for byte | identical, checked once on Linux (g++), not part of CI |
| Pitch Snap in Logic: menu, Transpose readout, sound | **not yet checked** |
| Download is universal (arm64 + x86_64), minimum macOS 11 | built with those settings; **not independently confirmed** (the `lipo` lines are in the CI log of the "Package" step) |
| Loads in Logic on the M4 Mac mini | **not yet checked** |
| Loads on the Intel Mac (macOS 12.7) | **not yet checked** |
| MIDI gate / velocity feel right in a real session | **not yet checked** |
| Sounds like the Palette hardware | **not checked: needs listening** |

Tolerances worth knowing: the tests' peak limit is 2.0 (the module clips to 1.0, but resampling a hot, clipped waveform overshoots, and
the overshoot differs slightly between compilers); in the knob-extreme stress run a model may be quiet (e.g. Question Mark with all
three main knobs at 0) as long as it stays finite.

Known behaviour: Kick, Skins, Circuit Zaps and Metalwork only sound when triggered, so they are silent in the Drone envelope mode
(the engines have no free-running mode).

## Layout

- `src/` plug-in code (JUCE), the JUCE-free engine wrapper, headless tests
- `third_party/` vendored DSP; `third_party/PALETTE_VERSION` records the exact source commits
- `tools/vendor_palette.sh` re-copies the DSP (and applies the two small patches that lift the firmware's 32-engine limit)
- `tools/gen_models.py` regenerates the model list, engine config, source list and credits from the Palette catalog
- `licenses/` credits and licence text (also shipped in the download)

## Licence

MIT throughout, see `licenses/`. Every author keeps their copyright; the credits are in `licenses/CREDITS.md` and the plug-in's ABOUT button.
