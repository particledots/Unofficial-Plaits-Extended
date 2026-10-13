// Measures how much of the shared engine RAM each model asks for, and fails if any needs more than the plug-in provides.
// stmlib's BufferAllocator hands back NULL (no error) when a block is too small, so an undersized buffer would otherwise
// show up only as a crash or silence in one particular model. Run: build/ram_probe
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>
#include "PlaitsEngine.h"        // models::kList, kSharedRamBytes

#include "plaits/dsp/voice.h"      // pulls in the generated engine_config.h: PLAITS_ENGINE_MEMBERS / PLAITS_REGISTER_ENGINES

namespace plaits
{
    // Same members and registration as Voice, so the registry macro resolves, but with Init() split open per engine.
    struct Probe
    {
        PLAITS_ENGINE_MEMBERS
        EngineRegistry<kMaxEngines> engines_;
        void Register() { engines_.Init(); PLAITS_REGISTER_ENGINES (engines_); }
    };
}

int main()
{
    void* mem = std::calloc (1, sizeof (plaits::Probe));
    auto* voice = new (mem) plaits::Probe();

    const size_t probeBytes = 1u << 22;                        // 4 MB: big enough that nothing can overflow
    std::vector<char> ram (probeBytes, 0);
    stmlib::BufferAllocator alloc (ram.data(), ram.size());

    voice->Register();

    const int n = voice->engines_.size();
    if (n != models::kCount) { std::printf ("registry has %d engines, model list has %d\n", n, models::kCount); return 2; }

    size_t worst = 0; int worstIdx = 0; int over = 0;
    for (int i = 0; i < n; ++i)
    {
        alloc.Free();
        voice->engines_.get (i)->Init (&alloc);
        const size_t used = probeBytes - alloc.free();
        if (used > worst) { worst = used; worstIdx = i; }
        if (used > PlaitsEngine::kSharedRamBytes)
        {
            ++over;
            std::printf ("TOO BIG: %-28s needs %zu bytes (plug-in provides %zu)\n", models::kList[i].name, used, PlaitsEngine::kSharedRamBytes);
        }
    }
    std::printf ("%d models; largest shared-RAM need: %zu bytes (%s); plug-in provides %zu; sizeof(engines) = %zu\n",
                 n, worst, models::kList[worstIdx].name, PlaitsEngine::kSharedRamBytes, sizeof (plaits::Probe));
    std::printf ("%s\n", over ? "FAIL" : "ok");
    return over ? 1 : 0;
}
