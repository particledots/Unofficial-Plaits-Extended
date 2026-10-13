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

Where it says "Linux" the check ran on a Linux x86_64 build (g++) while developing; the macOS GitHub build and everything on a Mac still
has to be confirmed. This table is updated as each one is.

| Check | Status |
|---|---|
| All 118 DSP files compile, 95 models register | passes on Linux; macOS CI not yet run |
| Every model x 3 envelope modes x 3 sample rates: finite, bounded, not silent; pitch within 5 cents (`engine_test`) | passes on Linux (1440 runs); macOS CI not yet run |
| Switching models while notes play, junk-filled memory (`sweep_test`) | passes on Linux; macOS CI not yet run |
| Every engine fits the shared RAM block (`ram_probe`; largest need 16 KB of 32 KB) | passes on Linux; macOS CI not yet run |
| Builds universal (arm64 + x86_64), macOS 11+ | not yet built |
| Loads in Logic on the M4 Mac mini | **not yet checked** |
| Loads on the Intel Mac (macOS 12.7) | **not yet checked** |
| MIDI gate / velocity behave | **not yet checked** |
| Sounds like the Palette hardware | **not checked: needs listening** |

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
