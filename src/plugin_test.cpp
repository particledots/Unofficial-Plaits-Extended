// Drives the real PlaitsAudioProcessor (parameters, MIDI -> voice, state save/restore) without a host or a window.
//   - the "engine" choice parameter must map index i to models::kList[i].name (this is what the menu relies on)
//   - every model must sound when played through the processor with a MIDI note
//   - Macro 4 must change the sound of a Palette model that uses it
//   - saved state must come back as it was
// Run: build/plugin_test
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <cstdio>
#include <cmath>
#include <cstring>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static void setParam (PlaitsAudioProcessor& p, const char* id, float plainValue)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
}

// plays one note (0.4 s held) and returns the rendered left channel (1 s)
static std::vector<float> playNote (PlaitsAudioProcessor& p, int note = 48)
{
    const int block = 512, blocks = 94;       // ~1 s at 48 kHz
    std::vector<float> out;
    juce::AudioBuffer<float> buf (2, block);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);
        if (b == 38) midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
        buf.clear();
        p.processBlock (buf, midi);
        out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + block);
    }
    return out;
}

static double energy (const std::vector<float>& a) { double e = 0; for (float v : a) e += (double) v * v; return e; }
static bool finite (const std::vector<float>& a) { for (float v : a) if (! std::isfinite (v)) return false; return true; }

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto& p = *static_cast<PlaitsAudioProcessor*> (base.get());
    p.setPlayConfigDetails (0, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);

    int fails = 0;
    auto check = [&] (bool ok, const char* what) { if (! ok) { ++fails; std::printf ("FAIL: %s\n", what); } };

    // 1. index <-> name mapping, and every model sounds
    auto* engineParam = p.apvts.getParameter ("engine");
    check (engineParam->getNumSteps() == models::kCount, "engine parameter has one step per model");
    int quiet = 0;
    for (int i = 0; i < models::kCount; ++i)
    {
        setParam (p, "engine", (float) i);
        const auto text = engineParam->getCurrentValueAsText();
        if (text != juce::String (models::kList[i].name)) { ++fails; std::printf ("FAIL: index %d shows \"%s\", expected \"%s\"\n", i, text.toRawUTF8(), models::kList[i].name); }

        setParam (p, "envelope", 1.0f);                       // Gate: velocity + held note, the common way to play it
        const auto a = playNote (p);
        if (! finite (a) || energy (a) < 0.05) { ++quiet; std::printf ("FAIL: %-24s via processor: %s (energy %.4f)\n", models::kList[i].name, finite (a) ? "silent" : "non-finite", energy (a)); }
    }
    fails += quiet;

    // 2. Macro 4 changes a model that uses it (Glisson is a Palette model)
    int glisson = -1; for (int i = 0; i < models::kCount; ++i) if (! std::strcmp (models::kList[i].id, "glisson")) glisson = i;
    check (glisson >= 0, "glisson present");
    setParam (p, "engine", (float) glisson); setParam (p, "envelope", 2.0f);   // Drone: continuous, so runs are comparable
    setParam (p, "macro", 0.5f); p.prepareToPlay (48000.0, 512); const auto neutral = playNote (p);
    setParam (p, "macro", 1.0f); p.prepareToPlay (48000.0, 512); const auto high = playNote (p);
    double diff = 0; for (size_t i = 0; i < neutral.size(); ++i) diff += std::fabs (neutral[i] - high[i]);
    std::printf ("macro 4: neutral energy %.1f, macro=1 energy %.1f, summed difference %.1f\n", energy (neutral), energy (high), diff);
    check (diff > 1.0, "Macro 4 changes the sound of Glisson");

    // 3. state round trip
    setParam (p, "engine", 70.0f); setParam (p, "harmonics", 0.9f); setParam (p, "macro", 0.2f);
    juce::MemoryBlock state; p.getStateInformation (state);
    setParam (p, "engine", 3.0f); setParam (p, "harmonics", 0.1f); setParam (p, "macro", 0.8f);
    p.setStateInformation (state.getData(), (int) state.getSize());
    check ((int) std::lround (p.apvts.getRawParameterValue ("engine")->load()) == 70, "engine restored from state");
    check (std::fabs (p.apvts.getRawParameterValue ("harmonics")->load() - 0.9f) < 1e-3f, "harmonics restored from state");
    check (std::fabs (p.apvts.getRawParameterValue ("macro")->load() - 0.2f) < 1e-3f, "macro restored from state");

    // 4. mono bus layout also works
    p.setPlayConfigDetails (0, 1, 48000.0, 512); p.prepareToPlay (48000.0, 512);
    {
        juce::AudioBuffer<float> buf (1, 512); juce::MidiBuffer midi; midi.addEvent (juce::MidiMessage::noteOn (1, 48, 0.8f), 0);
        buf.clear(); p.processBlock (buf, midi);
        check (std::isfinite (buf.getSample (0, 100)), "mono processBlock is finite");
    }

    std::printf ("%d models through the processor, %d failures\n", models::kCount, fails);
    return fails != 0;
}
