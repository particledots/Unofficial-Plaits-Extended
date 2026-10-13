#!/usr/bin/env python3
"""Generates the plugin's engine list from the Plaits Palette model catalog.

  tools/gen_models.py --catalog <eurorack>/alt_firmwares/plaits_lab_catalog   (re-extract + generate)
  tools/gen_models.py                                                         (regenerate from tools/models.json)

Outputs (all committed, so the GitHub build never needs the catalog or Python):
  tools/models.json                               trimmed copy of the catalog (the part we use)
  third_party/plaits/dsp/engine_config.h          engine members, registry and behaviour masks for voice.cc
  src/generated/ModelList.h                       model names / authors / families for the plugin UI and credits
  cmake/PlaitsSources.cmake                       the .cc files the DSP core needs

Models left out on purpose are listed in EXCLUDED below, with the reason.
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# id -> reason
EXCLUDED = {
    # Retired in the catalog itself: Virtual Analog Variant replaces both.
    "virtual-analog-dual": "retired by the catalog (use virtual-analog-variant)",
    "virtual-analog-crossfade": "retired by the catalog (use virtual-analog-variant)",
    # The built-in demo bank is encoded from audio produced by a TTS model; the file claims MIT,
    # but no licence for that source audio / voice is stated anywhere. Left out until it is confirmed.
    "natural-speech": "bundled demo data comes from TTS-generated audio with no stated data licence",
}

ORIGIN_ORDER = ["Mutable Instruments", "Rubato Lab", "Community"]

# Code credits that sit underneath a model but are not its catalog author. Taken from the copyright / SPDX
# headers of the vendored files (checked Oct 2026); every one of them is MIT.
EXTRA_CREDITS = [
    ("Perry R. Cook and Gary P. Scavone", "STK, the Synthesis ToolKit (1995-2023): the basis of Shakers and Brass"),
    ("Robin Schmidt (rs-met.com)", "Open303 (2009): the resonance taper and feedback-path filter ideas in Acid"),
    ("Olivier Gillet", "original 2012 Bytebeat code that Tim Churches' model builds on"),
]

# Always-needed core translation units (relative to third_party/).
CORE_SOURCES = [
    "plaits/dsp/voice.cc",
    "plaits/dsp/envelope.cc",
    "plaits/dsp/parameter_randomizer.cc",
    "plaits/resources.cc",
    "stmlib/utils/random.cc",
    "stmlib/dsp/units.cc",
    "stmlib/dsp/atan.cc",
]


def extract(catalog_dir: Path) -> dict:
    cat = json.loads((catalog_dir / "catalog.json").read_text(encoding="utf-8"))
    shared = json.loads((catalog_dir / "shared_modules.json").read_text(encoding="utf-8"))["modules"]
    engines = []
    for e in cat["engines"]:
        engines.append({
            "id": e["id"],
            "name": e["name"],
            "author": e["author"],
            "origin": e["origin"],
            "family": e["family"],
            "description": e["description"],
            "controls": e["controls"],
            "source": e["source"],
            "sharedModules": e.get("sharedModules", []),
            "postProcessing": e["postProcessing"],
        })
    return {
        "note": "Trimmed from Rubato Audio's Plaits Palette catalog (lylepmills/eurorack, alt_firmwares/plaits_lab_catalog).",
        "retired": cat["retiredEngines"],
        "sharedModules": {k: v["sources"] for k, v in shared.items()},
        "engines": engines,
    }


def cpp_str(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def mask_words(indices, n):
    words = [0] * ((n + 31) // 32)
    for i in indices:
        words[i // 32] |= 1 << (i % 32)
    return ", ".join(f"0x{w:08x}u" for w in words)


def generate(models: dict) -> None:
    selected = [e for e in models["engines"] if e["id"] not in EXCLUDED]
    # Group by source for the plugin's menu (stable: catalog order is kept inside each group).
    selected.sort(key=lambda e: ORIGIN_ORDER.index(e["origin"]))
    n = len(selected)

    # --- engine_config.h -------------------------------------------------
    includes, members, seen_members = [], [], set()
    regs = []
    for e in selected:
        src = e["source"]
        inc = f'#include "{src["header"]}"'
        if inc not in includes:
            includes.append(inc)
        if src["member"] not in seen_members:
            seen_members.add(src["member"])
            members.append(f'{src["className"]} {src["member"]};')
        pp = e["postProcessing"]
        regs.append(
            f'(registry).RegisterInstance(&{src["member"]}, '
            f'{"true" if pp["alreadyEnveloped"] else "false"}, {pp["outGain"]!r}f, {pp["auxGain"]!r}f);')

    def behavior_indices(b):
        return [i for i, e in enumerate(selected) if e["source"].get("behavior") == b]

    speech, lpc, chip = (behavior_indices(b) for b in ("speech", "lpc-words", "chiptune"))
    user_banks = [e["source"].get("userDataBank", -1) for e in selected]
    if any(e["source"].get("behavior") == "natural-speech" for e in selected):
        raise SystemExit("natural-speech needs PLAITS_HAS_NATURAL_SPEECH_ENGINE wiring; it is excluded for now")

    hdr = f"""// GENERATED by tools/gen_models.py from the Plaits Palette catalog. Do not edit by hand.
// Registry order = model order in the plugin ({n} models). Unlike the firmware (24 or 32 slots), the plugin
// is not limited to 32: the per-engine behaviour masks below are arrays of 32-bit words.
#ifndef PLAITS_DSP_ENGINE_CONFIG_H_
#define PLAITS_DSP_ENGINE_CONFIG_H_

// Must come before any engine header pulls in plaits/build_config.h (which would default it to 24).
#ifndef PLAITS_ENGINE_COUNT
#define PLAITS_ENGINE_COUNT {n}
#endif

#define PLAITS_BUILD_LINEAR_TZFM 0
#define PLAITS_BUILD_FAST_FM 0
#define PLAITS_BUILD_ENABLE_SYNC_INPUT 0
#define PLAITS_HAS_TERRAIN_BANK 0
#define PLAITS_WAVE_TERRAIN_FACTORY_MASK 0xff
#define PLAITS_HAS_WAVETABLE_BANK 0
#define PLAITS_WAVETABLE_FACTORY_MASK 0x07
#define PLAITS_HAS_CUSTOM_WAVE_LINES 0

{chr(10).join(includes)}

#include "plaits/resources.h"
#include "plaits/user_data_region.h"

#define PLAITS_HAS_SPEECH_ENGINE {1 if speech else 0}
#define PLAITS_HAS_LPC_WORDS_ENGINE {1 if lpc else 0}
#define PLAITS_HAS_NATURAL_SPEECH_ENGINE 0
#define PLAITS_HAS_CHIPTUNE_ENGINE {1 if chip else 0}
#define PLAITS_HAS_USER_DATA_BANK 1
#define PLAITS_HAS_USER_DATA_BANK_OVERRIDE 0
#define PLAITS_HAS_RESOLVED_USER_DATA_BANK 0
#define PLAITS_HAS_CUSTOM_MODEL_DATA 0

#define PLAITS_ENGINE_MEMBERS \\
  {" \\\n  ".join(members)}

#define PLAITS_REGISTER_ENGINES(registry) do {{ \\
  {" \\\n  ".join(regs)} \\
}} while (0)

namespace plaits {{

// Engine indices are 0-based positions in the registry above. Bit i of word i/32 is engine i.
inline bool EngineMaskHas(const uint32_t* mask, int engine_index) {{
  return ((mask[engine_index >> 5] >> (engine_index & 31)) & 1u) != 0;
}}

#if PLAITS_HAS_USER_DATA_BANK
// The three 6-Op FM models share one engine object and pick their factory bank by registry index.
static const int8_t kEngineUserDataBank[{n}] = {{ {", ".join(str(b) for b in user_banks)} }};
#endif
#if PLAITS_HAS_SPEECH_ENGINE
static const uint32_t kSpeechEngineMask[] = {{ {mask_words(speech, n)} }};
#endif
#if PLAITS_HAS_LPC_WORDS_ENGINE
static const uint32_t kLPCWordsEngineMask[] = {{ {mask_words(lpc, n)} }};
#endif
#if PLAITS_HAS_CHIPTUNE_ENGINE
static const uint32_t kChiptuneEngineMask[] = {{ {mask_words(chip, n)} }};
#endif

}}  // namespace plaits

#endif  // PLAITS_DSP_ENGINE_CONFIG_H_
"""
    (ROOT / "third_party/plaits/dsp/engine_config.h").write_text(hdr, encoding="utf-8")

    # --- ModelList.h ------------------------------------------------------
    rows = []
    for e in selected:
        ctl = (list(e["controls"]) + ["", "", "", ""])[:4]
        fields = [cpp_str(e[k]) for k in ("id", "name", "family", "origin", "author")]
        rows.append("    { " + ", ".join(fields) + ", { " + ", ".join(cpp_str(c) for c in ctl) + " } },")
    model_h = f"""// GENERATED by tools/gen_models.py. Do not edit by hand.
#pragma once

namespace models
{{
    // control[]: the catalog's names for what the four macro controls do on this model.
    struct Model {{ const char *id, *name, *family, *origin, *author; const char *control[4]; }};

    constexpr int kCount = {n};
    inline const Model kList[kCount] = {{
{chr(10).join(rows)}
    }};
}}
"""
    (ROOT / "src/generated").mkdir(parents=True, exist_ok=True)
    (ROOT / "src/generated/ModelList.h").write_text(model_h, encoding="utf-8")

    # --- PlaitsSources.cmake ---------------------------------------------
    srcs = list(CORE_SOURCES)
    for e in selected:
        for f in e["source"]["files"]:
            if f.endswith(".cc") and f not in srcs:
                srcs.append(f)
        for m in e["sharedModules"]:
            for f in models["sharedModules"].get(m, []):
                if f.endswith(".cc") and f not in srcs:
                    srcs.append(f)
    srcs = [s if s.startswith(("plaits/", "stmlib/")) else s for s in srcs]
    cmake = ["# GENERATED by tools/gen_models.py. Do not edit by hand.",
             f"set(PLAITS_MODEL_COUNT {n})",
             "set(PLAITS_DSP_SOURCES"]
    cmake += [f"  third_party/{s}" for s in srcs]
    cmake += [")", ""]
    (ROOT / "cmake").mkdir(exist_ok=True)
    (ROOT / "cmake/PlaitsSources.cmake").write_text("\n".join(cmake), encoding="utf-8")
    # --- credits (committed: shown in the plug-in's About box and shipped in the download) ----------------
    by_author = {}
    for e in selected:
        by_author.setdefault(e["author"], []).append(e)
    md = ["# Credits", "",
          "Unofficial Plaits Extended is a community port. It is not made, endorsed or supported by Mutable Instruments,",
          "Emilie Gillet, or Rubato Audio. All the code it is built from is MIT licensed; each author keeps their copyright.",
          "", "## Models by author", ""]
    for author in sorted(by_author, key=lambda a: (a != "Emilie Gillet", a.lower())):
        names = ", ".join(e["name"] for e in by_author[author])
        md.append(f"- **{author}** ({len(by_author[author])}): {names}")
    md += ["", "## Code these models build on", ""]
    md += [f"- **{who}** - {what}" for who, what in EXTRA_CREDITS]
    md += ["", "## Where the code comes from", "",
           "- Plaits and the Mutable Instruments models: https://github.com/pichenettes/eurorack and stmlib (MIT, Emilie Gillet)",
           "- The extra Palette models and community models: Rubato Audio's Plaits Palette fork, https://github.com/lylepmills/eurorack",
           "  (see third_party/PALETTE_VERSION for the exact commit)",
           "", "Left out for now: Natural Speech (its bundled demo data has no stated data licence).", ""]
    (ROOT / "licenses").mkdir(exist_ok=True)
    (ROOT / "licenses/CREDITS.md").write_text("\n".join(md), encoding="utf-8")

    # short plain-text version for the About box
    about = ["Unofficial Plaits Extended - community port, not a product of Mutable Instruments or Rubato Audio.",
             "All code is MIT licensed. Models by:"]
    for author in sorted(by_author, key=lambda a: (a != "Emilie Gillet", a.lower())):
        cnt = len(by_author[author])
        about.append(f"  {author}  ({cnt} model{'s' if cnt != 1 else ''})")
    about.append("Built on: " + "; ".join(who for who, _ in EXTRA_CREDITS))
    about.append("Full credits and licences: the licenses folder in the download.")
    (ROOT / "src/generated/Credits.h").write_text(
        "// GENERATED by tools/gen_models.py. Do not edit by hand.\n#pragma once\n"
        "inline const char* const kAboutText =\n" +
        "\n".join('    ' + cpp_str(l + "\n") for l in about) + ";\n", encoding="utf-8")

    print(f"{n} models, {len(srcs)} source files; excluded: {', '.join(EXCLUDED)}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--catalog", type=Path, help="alt_firmwares/plaits_lab_catalog directory (re-extracts models.json)")
    a = ap.parse_args()
    mj = ROOT / "tools/models.json"
    if a.catalog:
        mj.write_text(json.dumps(extract(a.catalog), indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
    generate(json.loads(mj.read_text(encoding="utf-8")))


if __name__ == "__main__":
    main()
