#include "PluginEditor.h"
#include "generated/Credits.h"

namespace
{
    const juce::Colour kBg      (0xff1a1c20);
    const juce::Colour kPanel   (0xff23262c);
    const juce::Colour kTrack   (0xff3a3e47);
    const juce::Colour kAccent  (0xffe0a458);
    const juce::Colour kText    (0xffe8e6e1);
    const juce::Colour kDim     (0xff8d909a);
}

PlaitsLookAndFeel::PlaitsLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, kDim);
    setColour (juce::ComboBox::backgroundColourId, kPanel);
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::outlineColourId, kTrack);
    setColour (juce::ComboBox::arrowColourId, kAccent);
    setColour (juce::PopupMenu::backgroundColourId, kPanel);
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
}

void PlaitsLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float a0, float a1, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    r = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
    const float thick = juce::jmax (3.0f, d * 0.09f);
    const auto arcR = r.reduced (thick * 0.5f);

    juce::Path track;
    track.addCentredArc (arcR.getCentreX(), arcR.getCentreY(), arcR.getWidth() * 0.5f, arcR.getHeight() * 0.5f, 0, a0, a1, true);
    g.setColour (kTrack);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float angle = a0 + pos * (a1 - a0);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;   // bipolar controls fill from the centre
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    juce::Path val;
    val.addCentredArc (arcR.getCentreX(), arcR.getCentreY(), arcR.getWidth() * 0.5f, arcR.getHeight() * 0.5f, 0,
                       juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (s.isEnabled() ? kAccent : kDim);
    g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto c = r.getCentre();
    const float len = d * 0.5f - thick * 2.2f;
    juce::Line<float> line (c.getPointOnCircumference (len * 0.35f, angle),
                            c.getPointOnCircumference (len, angle));
    g.setColour (kText);
    g.drawLine (line, juce::jmax (2.0f, d * 0.045f));
}

void PlaitsLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? kAccent : kPanel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? kAccent : kTrack);
    g.drawRoundedRectangle (r, 6.0f, 1.2f);
    g.setColour (on ? juce::Colours::black : kText);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

PlaitsEditor::PlaitsEditor (PlaitsAudioProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    addKnob (harmonics,    "harmonics",     "HARMONICS");
    addKnob (timbre,       "timbre",        "TIMBRE");
    addKnob (morph,        "morph",         "MORPH");
    addKnob (macro,        "macro",         "MACRO 4");
    addKnob (fmAmount,     "fm_amount",     "FM AMOUNT");
    addKnob (timbreAmount, "timbre_amount", "TIMBRE AMOUNT");
    addKnob (morphAmount,  "morph_amount",  "MORPH AMOUNT");
    addKnob (transpose,    "transpose",     "TRANSPOSE");
    addKnob (fine,         "fine",          "FINE TUNE");
    addKnob (decay,        "decay",         "DECAY");
    addKnob (lpgColour,    "lpg_colour",    "LPG COLOUR");
    transpose.slider.setNumDecimalPlacesToDisplay (0);
    fine.slider.setNumDecimalPlacesToDisplay (2);

    // the four main knobs show what the current model calls each of them
    for (Knob* k : { &harmonics, &timbre, &morph, &macro })
    {
        k->sub.setJustificationType (juce::Justification::centred);
        k->sub.setFont (juce::Font (juce::FontOptions (11.0f)));
        k->sub.setColour (juce::Label::textColourId, kAccent);
        k->sub.setMinimumHorizontalScale (0.7f);
        addAndMakeVisible (k->sub);
    }

    addChoice (engine,   "engine",   "MODEL",    PlaitsAudioProcessor::engineNames());
    populateModelMenu();
    // re-attach so the saved choice is shown against the menu we just rebuilt
    engine.att = std::make_unique<ComboAtt> (proc.apvts, "engine", engine.box);
    addChoice (envelope, "envelope", "ENVELOPE", PlaitsAudioProcessor::envelopeNames());
    addChoice (pitchSnap, "pitch_snap", "PITCH SNAP", { "Off", "Steps", "Semitones" });
    pitchSnap.box.setTooltip ("What Transpose + Fine Tune settle on. Steps = octaves and fifths (0, 7, 12, 19, 24 ... up to 48, both directions); "
                              "Semitones = whole semitones. With Pitch Snap on, Fine Tune only matters when it tips the total over a step. "
                              "The Transpose readout shows the offset that plays.");
    // With Pitch Snap on, the Transpose readout shows the offset that actually plays (Transpose + Fine, snapped).
    transpose.slider.textFromValueFunction = [this] (double v)
    {
        const int mode = (int) proc.apvts.getRawParameterValue ("pitch_snap")->load();
        if (mode == PlaitsEngine::SnapOff) return juce::String ((int) std::lround (v));
        const float total = (float) std::lround (v) + proc.apvts.getRawParameterValue ("fine")->load();
        const int st = (int) std::lround (PlaitsEngine::snapPitch (total, mode));
        return st > 0 ? "+" + juce::String (st) : juce::String (st);
    };
    snapReadoutAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter ("pitch_snap"),
                                                                  [this] (float) { transpose.slider.updateText(); });
    fineReadoutAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter ("fine"),
                                                                  [this] (float) { transpose.slider.updateText(); });
    snapReadoutAtt->sendInitialUpdate();
    fineReadoutAtt->sendInitialUpdate();

    modelInfo.setJustificationType (juce::Justification::centredLeft);
    modelInfo.setFont (juce::Font (juce::FontOptions (11.0f)));
    modelInfo.setColour (juce::Label::textColourId, kDim);
    modelInfo.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (modelInfo);

    aboutButton.onClick = [] { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Unofficial Plaits Extended", kAboutText); };
    addAndMakeVisible (aboutButton);

    addAndMakeVisible (monoOut);
    monoOutAtt = std::make_unique<ButtonAtt> (proc.apvts, "mono_out", monoOut);

    setSize (640, 648);
    updateModelInfo();
    startTimerHz (10);        // the model can also change from host automation
}

PlaitsEditor::~PlaitsEditor() { stopTimer(); setLookAndFeel (nullptr); }

// Item ids are model index + 1, so the ComboBox attachment keeps working; section headings do not take an id.
void PlaitsEditor::populateModelMenu()
{
    engine.att.reset();
    engine.box.clear (juce::dontSendNotification);
    juce::String lastOrigin;
    for (int i = 0; i < models::kCount; ++i)
    {
        const juce::String origin (models::kList[i].origin);
        if (origin != lastOrigin) { engine.box.addSectionHeading (origin); lastOrigin = origin; }
        engine.box.addItem (models::kList[i].name, i + 1);
    }
}

void PlaitsEditor::updateModelInfo()
{
    const int i = juce::jlimit (0, models::kCount - 1, (int) proc.apvts.getRawParameterValue ("engine")->load());
    if (i == shownModel) return;
    shownModel = i;
    const auto& m = models::kList[i];
    modelInfo.setText (juce::String (m.family) + "  -  by " + m.author + "  -  " + m.origin, juce::dontSendNotification);
    harmonics.sub.setText (m.control[0], juce::dontSendNotification);
    timbre.sub.setText    (m.control[1], juce::dontSendNotification);
    morph.sub.setText     (m.control[2], juce::dontSendNotification);
    macro.sub.setText     (m.control[3], juce::dontSendNotification);
}

void PlaitsEditor::timerCallback() { updateModelInfo(); }

void PlaitsEditor::addKnob (Knob& k, const juce::String& id, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    k.slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (proc.apvts.getParameter (id)->getDefaultValue()));
    k.slider.setPopupDisplayEnabled (false, false, nullptr);
    k.slider.setNumDecimalPlacesToDisplay (2);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxTextColourId, kText);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
}

void PlaitsEditor::addChoice (Choice& c, const juce::String& id, const juce::String& name, const juce::StringArray& items)
{
    c.box.addItemList (items, 1);
    c.label.setText (name, juce::dontSendNotification);
    c.label.setJustificationType (juce::Justification::centredLeft);
    c.label.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    addAndMakeVisible (c.box);
    addAndMakeVisible (c.label);
    c.att = std::make_unique<ComboAtt> (proc.apvts, id, c.box);
}

void PlaitsEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kText);
    g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    g.drawText ("UNOFFICIAL PLAITS EXTENDED", 20, 10, 420, 26, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("Plaits + Plaits Palette models - community port, not a Mutable Instruments or Rubato Audio product", 20, 34, 560, 16, juce::Justification::centredLeft);

    g.setColour (kPanel);
    const float w = (float) getWidth() - 24.0f;
    g.fillRoundedRectangle (12.0f, 58.0f,  w, 84.0f,  10.0f);   // model + envelope + mono + info
    g.fillRoundedRectangle (12.0f, 150.0f, w, 172.0f, 10.0f);   // harmonics / timbre / morph / macro 4
    g.fillRoundedRectangle (12.0f, 330.0f, w, 120.0f, 10.0f);   // attenuverters
    g.fillRoundedRectangle (12.0f, 458.0f, w, 126.0f, 10.0f);   // transpose / fine / decay / lpg colour
    g.fillRoundedRectangle (12.0f, 592.0f, w, 48.0f,  10.0f);   // pitch snap
}

void PlaitsEditor::resized()
{
    const int W = getWidth();

    auto place = [&] (Knob& k, juce::Rectangle<int> r)
    {
        k.label.setBounds (r.removeFromTop (16));
        if (k.sub.getParentComponent() != nullptr) k.sub.setBounds (r.removeFromTop (14));
        k.slider.setBounds (r);
    };
    auto placeChoice = [&] (Choice& c, juce::Rectangle<int> r)
    {
        c.label.setBounds (r.removeFromTop (16));
        c.box.setBounds (r.removeFromTop (28));
    };

    aboutButton.setBounds (W - 12 - 70, 12, 70, 24);

    placeChoice (engine,   juce::Rectangle<int> (24, 64, 300, 48));
    placeChoice (envelope, juce::Rectangle<int> (336, 64, 170, 48));
    monoOut.setBounds (W - 12 - 100 - 8, 80, 100, 36);
    modelInfo.setBounds (24, 116, W - 48, 20);
    pitchSnap.label.setBounds (24, 602, 90, 28);
    pitchSnap.box.setBounds (118, 602, 170, 28);

    const int col4 = (W - 24) / 4;
    Knob* mainRow[4] = { &harmonics, &timbre, &morph, &macro };
    for (int i = 0; i < 4; ++i)
        place (*mainRow[i], juce::Rectangle<int> (12 + i * col4, 154, col4, 164).reduced (14, 0));

    const int col3 = (W - 24) / 3;
    Knob* amtRow[3] = { &fmAmount, &timbreAmount, &morphAmount };
    for (int i = 0; i < 3; ++i)
        place (*amtRow[i], juce::Rectangle<int> (12 + i * col3, 334, col3, 112).reduced (30, 0));

    Knob* lowRow[4] = { &transpose, &fine, &decay, &lpgColour };
    for (int i = 0; i < 4; ++i)
        place (*lowRow[i], juce::Rectangle<int> (12 + i * col4, 462, col4, 118).reduced (14, 0));
}
