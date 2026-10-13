#!/usr/bin/env bash
# Copies the Plaits Palette DSP into third_party/ from a checkout of lylepmills/eurorack.
# Usage: tools/vendor_palette.sh <path to eurorack checkout> <path to pichenettes/stmlib checkout>
# The pinned versions used for the committed copy are in third_party/PALETTE_VERSION.
set -euo pipefail
FORK="${1:?path to the lylepmills/eurorack checkout}"
STM="${2:?path to the pichenettes/stmlib checkout}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
T="$HERE/third_party"

rm -rf "$T/plaits" "$T/stmlib"
mkdir -p "$T/plaits"
cp -R "$FORK/plaits/dsp" "$T/plaits/dsp"
for f in resources.cc resources.h build_config.h user_data.h user_data_region.h section_marks.h; do
  cp "$FORK/plaits/$f" "$T/plaits/$f"
done
mkdir -p "$T/stmlib"
cp -R "$STM/dsp" "$T/stmlib/dsp"; cp -R "$STM/utils" "$T/stmlib/utils"
cp "$STM/stmlib.h" "$STM/LICENSE" "$T/stmlib/"
rm -f "$T/stmlib/dsp/atan_approximations.py"

# Not vendored: firmware-only configs (we generate our own), and the Natural Speech engine,
# whose bundled demo data is encoded from TTS-generated audio with no stated data licence.
rm -f "$T/plaits/dsp/engine_config.h" "$T/plaits/dsp/stock_engine_config.h"
rm -f "$T/plaits/dsp/engine2/natural_speech_engine.cc" "$T/plaits/dsp/engine2/natural_speech_engine.h" \
      "$T/plaits/dsp/engine2/natural_speech_data.h"

# --- The only edits made to the vendored code (both lift the firmware's 32-engine ceiling) ---
# 1. build_config.h: the firmware caps PLAITS_ENGINE_COUNT at 32 because its per-engine behaviour masks are
#    single 32-bit words. Raise the cap to 128.
sed -i -E 's/PLAITS_ENGINE_COUNT > 32/PLAITS_ENGINE_COUNT > 128/; s/between 1 and 32"/between 1 and 128"/' "$T/plaits/build_config.h"
# 2. voice.cc: test those masks through EngineMaskHas() (defined in the generated engine_config.h, arrays of words).
sed -i -E 's/\b(k[A-Za-z]+EngineMask) & \(1u << engine_index\)/EngineMaskHas(\1, engine_index)/g' "$T/plaits/dsp/voice.cc"
grep -q "EngineMaskHas" "$T/plaits/dsp/voice.cc" || { echo "voice.cc patch did not apply" >&2; exit 1; }
grep -q "> 128" "$T/plaits/build_config.h" || { echo "build_config.h patch did not apply" >&2; exit 1; }

{
  echo "fork:   https://github.com/lylepmills/eurorack @ $(git -C "$FORK" rev-parse HEAD)"
  echo "stmlib: https://github.com/pichenettes/stmlib @ $(git -C "$STM" rev-parse HEAD)"
} > "$T/PALETTE_VERSION"
echo "vendored:"; cat "$T/PALETTE_VERSION"
