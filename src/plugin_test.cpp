// Drives the real PlaitsAudioProcessor (parameters, MIDI -> voice, state save/restore) without a host or a window.
//   - the "engine" choice parameter must map index i to models::kList[i].name (this is what the menu relies on)
//   - every model must sound when played through the processor with a MIDI note
//   - Macro 4 must change the sound of a Palette model that uses it
//   - saved state must come back as it was
//   - Pitch Snap (0.2.0): defaults to Off; Steps / Semitones / Off through the real "pitch_snap", "transpose" and "fine"
//     parameters give the expected pitch; a state saved without it loads with it Off
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

// one held note for 3 s; returns the left channel
static std::vector<float> holdNote (PlaitsAudioProcessor& p, int note)
{
    const int block = 512, blocks = 281;
    std::vector<float> out;
    juce::AudioBuffer<float> buf (2, block);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);
        buf.clear();
        p.processBlock (buf, midi);
        out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + block);
    }
    return out;
}

static double peakHz (const std::vector<float>& x0, double sr, size_t from, size_t count, double lo, double hi)
{
    std::vector<float> x (count);
    for (size_t i = 0; i < count; ++i)
        x[i] = x0[from + i] * (float) (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * (double) i / (double) count));
    double bestF = 0, bestM = -1;
    for (double f = lo; f < hi; f *= 1.0002)
    {
        const double w = 2.0 * 3.14159265358979323846 * f / sr, cr = std::cos (w), ci = std::sin (w);
        double zr = 1.0, zi = 0.0, re = 0.0, im = 0.0;
        for (size_t i = 0; i < count; ++i)
        {
            re += x[i] * zr; im += x[i] * zi;
            const double nr = zr * cr - zi * ci; zi = zr * ci + zi * cr; zr = nr;
        }
        const double m = re * re + im * im;
        if (m > bestM) { bestM = m; bestF = f; }
    }
    return bestF;
}

static std::unique_ptr<PlaitsAudioProcessor> freshProcessor()
{
    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto* q = static_cast<PlaitsAudioProcessor*> (base.release());
    q->setPlayConfigDetails (0, 2, 48000.0, 512);
    q->prepareToPlay (48000.0, 512);
    return std::unique_ptr<PlaitsAudioProcessor> (q);
}

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

    // 5. Pitch Snap through the real parameters
    {
        auto d = freshProcessor();
        check ((int) d->apvts.getRawParameterValue ("pitch_snap")->load() == 0, "Pitch Snap defaults to Off");
        check (d->apvts.getParameter ("pitch_snap")->getCurrentValueAsText() == "Off", "Pitch Snap menu text is Off");

        int waveshaping = -1; for (int i = 0; i < models::kCount; ++i) if (! std::strcmp (models::kList[i].id, "waveshaping")) waveshaping = i;
        check (waveshaping >= 0, "waveshaping present");
        struct C { int snap; float transpose, fine; double offset; };
        const C cs[] = { { 0, 4, 0.0f, 4.0 }, { 0, 4, 0.6f, 4.6 }, { 1, 4, 0.0f, 7 }, { 1, 11, 0.9f, 12 }, { 1, -9, 0.0f, -7 },
                         { 2, 4, 0.6f, 5 }, { 2, 4, -0.6f, 3 }, { 2, -3, 0.2f, -3 } };
        for (const auto& c : cs)
        {
            auto q = freshProcessor();
            setParam (*q, "engine", (float) waveshaping); setParam (*q, "envelope", 2.0f); setParam (*q, "timbre", 0.5f); setParam (*q, "morph", 0.0f);
            setParam (*q, "pitch_snap", (float) c.snap); setParam (*q, "transpose", c.transpose); setParam (*q, "fine", c.fine);
            const auto a = holdNote (*q, 57);
            const double want = 440.0 * std::pow (2.0, (57.0 + c.offset - 69.0) / 12.0);
            const double got = peakHz (a, 48000.0, 96000, 32768, want / 1.5, want * 1.5);
            const double cents = 1200.0 * std::log2 (got / want);
            char msg[160]; std::snprintf (msg, sizeof msg, "processor: snap=%d transpose=%+.0f fine=%+.1f -> %+.1f st (want %.2f Hz, got %.2f Hz, %+.1f cents)",
                                          c.snap, c.transpose, c.fine, c.offset, want, got, cents);
            check (std::fabs (cents) < 5.0, msg);
        }

        auto a1 = freshProcessor();
        setParam (*a1, "pitch_snap", 1.0f); setParam (*a1, "harmonics", 0.9f);
        juce::MemoryBlock st; a1->getStateInformation (st);
        auto a2 = freshProcessor();
        a2->setStateInformation (st.getData(), (int) st.getSize());
        check ((int) a2->apvts.getRawParameterValue ("pitch_snap")->load() == 1, "Pitch Snap restored from state");

        auto xml = juce::AudioProcessor::getXmlFromBinary (st.getData(), (int) st.getSize());
        bool removed = false;
        if (xml != nullptr)
            for (auto* child = xml->getFirstChildElement(); child != nullptr; child = child->getNextElement())
                if (child->getStringAttribute ("id") == "pitch_snap") { xml->removeChildElement (child, true); removed = true; break; }
        check (removed, "test setup: found pitch_snap in the saved state");
        juce::MemoryBlock old; juce::AudioProcessor::copyXmlToBinary (*xml, old);
        auto a3 = freshProcessor();
        a3->setStateInformation (old.getData(), (int) old.getSize());
        check ((int) a3->apvts.getRawParameterValue ("pitch_snap")->load() == 0, "a state saved without Pitch Snap loads with it Off");
        check (std::fabs (a3->apvts.getRawParameterValue ("harmonics")->load() - 0.9f) < 1e-3f, "...and its other settings still load");
    }

    std::printf ("%d models through the processor, %d failures\n", models::kCount, fails);
    return fails != 0;
}
