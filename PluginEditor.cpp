#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

using namespace epat;

namespace
{
juce::Font caps (float size, float kerning = 0.15f)
{
    return juce::Font (juce::FontOptions (size).withStyle ("Bold")).withExtraKerningFactor (kerning);
}

// Speed spans -24..+12 semitones, so "no change" sits at 24/36 of the dial.
constexpr float kSpeedZeroNorm = 24.0f / 36.0f;

constexpr float kDialStart = juce::MathConstants<float>::pi * 1.2f;
constexpr float kDialEnd   = juce::MathConstants<float>::pi * 2.8f;
} // namespace

//==============================================================================
EpatolesLookAndFeel::EpatolesLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, kBg);

    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, kAccent.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, kAccent);

    setColour (juce::ComboBox::backgroundColourId, kPanelHi);
    setColour (juce::ComboBox::outlineColourId, kLine);
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::arrowColourId, kMuted);

    setColour (juce::PopupMenu::backgroundColourId, kPanelHi);
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
    setColour (juce::PopupMenu::highlightedTextColourId, kBg);
}

void EpatolesLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float startAngle, float endAngle,
                                            juce::Slider& slider)
{
    const bool big     = (bool) slider.getProperties().getWithDefault ("big", false);
    const bool bipolar = (bool) slider.getProperties().getWithDefault ("bipolar", false);

    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                      .reduced (big ? 10.0f : 7.0f);
    const float dia = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto area = bounds.withSizeKeepingCentre (dia, dia);
    const auto c = area.getCentre();

    const float radius = dia * 0.5f;
    const float thick  = big ? 6.0f : 4.0f;
    const float arcR   = radius - thick * 0.5f;
    const float angle  = startAngle + sliderPos * (endAngle - startAngle);

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (kTrack);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // value arc (from the zero-semitone point for the speed dial)
    const float fromAngle = bipolar ? startAngle + kSpeedZeroNorm * (endAngle - startAngle)
                                    : startAngle;
    if (std::abs (angle - fromAngle) > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f,
                           juce::jmin (fromAngle, angle), juce::jmax (fromAngle, angle), true);
        g.setColour (slider.isEnabled() ? kAccent : kMuted);
        g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    // cap
    const float innerR = radius - thick - (big ? 12.0f : 7.0f);
    g.setColour (kPanelHi);
    g.fillEllipse (c.x - innerR, c.y - innerR, innerR * 2.0f, innerR * 2.0f);
    g.setColour (kLine);
    g.drawEllipse (c.x - innerR, c.y - innerR, innerR * 2.0f, innerR * 2.0f, 1.0f);

    // pointer
    juce::Path pointer;
    const float pLen = innerR * 0.5f;
    pointer.addRoundedRectangle (-1.5f, -innerR * 0.9f, 3.0f, pLen, 1.5f);
    g.setColour (kText);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));

    // live transport speed marker (speed dial only)
    if (slider.getProperties().contains ("liveNorm"))
    {
        const float ln = (float) slider.getProperties()["liveNorm"];
        const float la = startAngle + ln * (endAngle - startAngle);
        const float lr = radius + 4.0f;
        const float lx = c.x + lr * std::sin (la);
        const float ly = c.y - lr * std::cos (la);
        g.setColour (kText);
        g.fillEllipse (lx - 2.5f, ly - 2.5f, 5.0f, 5.0f);
    }
}

void EpatolesLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                            bool highlighted, bool)
{
    const auto b  = button.getLocalBounds().toFloat();
    const bool on = button.getToggleState();

    if (button.getProperties().contains ("pill"))
    {
        const auto r = b.reduced (1.0f);
        const float corner = r.getHeight() * 0.5f;

        g.setColour (on ? kAccent : kPanelHi);
        g.fillRoundedRectangle (r, corner);

        if (highlighted && ! on)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (r, corner);
        }

        g.setColour (on ? kAccent : kLine);
        g.drawRoundedRectangle (r, corner, 1.0f);

        g.setColour (on ? kBg : kText);
        g.setFont (caps (12.0f, 0.15f));
        g.drawText (button.getButtonText(), r.toNearestInt(), juce::Justification::centred, false);
    }
    else
    {
        constexpr float w = 26.0f, h = 14.0f;
        const juce::Rectangle<float> sw (b.getX() + 2.0f, b.getCentreY() - h * 0.5f, w, h);

        g.setColour (on ? kAccent.withAlpha (0.9f) : kTrack);
        g.fillRoundedRectangle (sw, h * 0.5f);

        const float kd = h - 4.0f;
        const float kx = on ? sw.getRight() - kd - 2.0f : sw.getX() + 2.0f;
        g.setColour (on ? kBg : kMuted);
        g.fillEllipse (kx, sw.getY() + 2.0f, kd, kd);

        g.setColour (on ? kText : kMuted);
        g.setFont (caps (11.0f, 0.12f));
        g.drawText (button.getButtonText(), b.withTrimmedLeft (w + 10.0f).toNearestInt(),
                    juce::Justification::centredLeft, false);
    }
}

juce::Label* EpatolesLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::Font (juce::FontOptions (11.5f)));
    l->setColour (juce::Label::textColourId, kText);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

juce::Font EpatolesLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (12.0f));
}

//==============================================================================
EpatolesEditor::Knob* EpatolesEditor::addKnob (const juce::String& paramId, const juce::String& text,
                                               bool big, bool bipolar)
{
    auto k = std::make_unique<Knob>();

    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, big ? 130 : 72, big ? 22 : 18);
    k->slider.setRotaryParameters (kDialStart, kDialEnd, true);
    k->slider.getProperties().set ("big", big);
    k->slider.getProperties().set ("bipolar", bipolar);

    k->label.setText (text, juce::dontSendNotification);
    k->label.setFont (caps (10.0f, 0.14f));
    k->label.setColour (juce::Label::textColourId, kMuted);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setInterceptsMouseClicks (false, false);

    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, paramId, k->slider);

    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);

    knobs.push_back (std::move (k));
    return knobs.back().get();
}

EpatolesEditor::EpatolesEditor (EpatolesProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    // index 0 = speed dial, 1..11 = the grid, row by row (cell 12 is the chunk sync box)
    speedKnob = addKnob ("speed", "SPEED", true, true);

    addKnob ("drive",   "DRIVE",   false, false);
    addKnob ("warmth",  "WARMTH",  false, false);
    addKnob ("tone",    "TONE",    false, false);
    addKnob ("mix",     "MIX",     false, false);
    addKnob ("output",  "OUTPUT",  false, false);

    addKnob ("wow",     "WOW",     false, false);
    addKnob ("flutter", "FLUTTER", false, false);
    addKnob ("hiss",    "HISS",    false, false);
    addKnob ("inertia", "INERTIA", false, false);
    addKnob ("window",  "WINDOW",  false, false);
    addKnob ("smooth",  "SMOOTH",  false, false);

    stopButton.setButtonText ("TAPE STOP");
    stopButton.getProperties().set ("pill", true);
    stopButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible (stopButton);
    stopAtt = std::make_unique<ButtonAttachment> (proc.apvts, "stop", stopButton);

    reverseButton.setButtonText ("REVERSE");
    reverseButton.getProperties().set ("pill", true);
    reverseButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible (reverseButton);
    reverseAtt = std::make_unique<ButtonAttachment> (proc.apvts, "reverse", reverseButton);

    autoGainButton.setButtonText ("AUTO GAIN");
    autoGainButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible (autoGainButton);
    autoGainAtt = std::make_unique<ButtonAttachment> (proc.apvts, "autogain", autoGainButton);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("oversampling")))
        osBox.addItemList (choice->choices, 1);
    addAndMakeVisible (osBox);
    osAtt = std::make_unique<ComboAttachment> (proc.apvts, "oversampling", osBox);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("chunksync")))
        syncBox.addItemList (choice->choices, 1);
    addAndMakeVisible (syncBox);
    syncAtt = std::make_unique<ComboAttachment> (proc.apvts, "chunksync", syncBox);

    setSize (790, 440);
    sendLookAndFeelChange();
    startTimerHz (30);
}

EpatolesEditor::~EpatolesEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void EpatolesEditor::timerCallback()
{
    // Live transport speed -> position on the speed dial ring.
    const float sp   = proc.liveSpeed.load();
    const float st   = 12.0f * std::log2 (std::max (std::abs (sp), 1.0e-4f));
    const float norm = juce::jlimit (0.0f, 1.0f, (st + 24.0f) / 36.0f);

    const int dir = sp < -0.02f ? -1 : (sp > 0.02f ? 1 : 0);
    if (dir != liveDir)
    {
        liveDir = dir;
        repaint (leftPanel);
    }

    auto& props = speedKnob->slider.getProperties();
    const float old = props.contains ("liveNorm") ? (float) props["liveNorm"] : -1.0f;

    if (std::abs (old - norm) > 0.002f)
    {
        props.set ("liveNorm", norm);
        speedKnob->slider.repaint();
    }
}

void EpatolesEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (kBg.brighter (0.05f), 0.0f, 0.0f,
                                             kBg, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // header
    g.setColour (kMuted);
    g.setFont (caps (10.5f, 0.25f));
    g.drawText ("SONGCRAFT AUDIO", 28, 22, 300, 14, juce::Justification::centredLeft, false);

    g.setColour (kText);
    g.setFont (caps (30.0f, 0.02f));
    g.drawText ("Epatoles", 26, 36, 300, 38, juce::Justification::centredLeft, false);

    g.setColour (kAccent);
    g.setFont (caps (9.5f, 0.22f));
    g.drawText ("TAPE, BOTH WAYS", 164, 52, 200, 14, juce::Justification::centredLeft, false);

    g.setColour (kMuted);
    g.setFont (caps (9.5f, 0.18f));
    g.drawText ("OVERSAMPLING", osText, juce::Justification::centredLeft, false);

    // panels
    for (auto r : { leftPanel, rightPanel })
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (r.toFloat(), 14.0f);
        g.setColour (kLine);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 14.0f, 1.0f);
    }

    // chunk sync label (the box sits in the last cell of the knob grid)
    g.setColour (kMuted);
    g.setFont (caps (10.0f, 0.14f));
    g.drawText ("CHUNK SYNC", syncText, juce::Justification::centred, false);

    // live direction arrows above the pills
    {
        const float cx = (float) leftPanel.getCentreX();
        const float cy = (float) (leftPanel.getBottom() - 14 - 36 - 16);

        auto arrow = [&] (float x, bool pointsLeft, bool lit)
        {
            juce::Path p;
            const float s = 6.0f;
            if (pointsLeft)
                p.addTriangle (x - s, cy, x + s, cy - s, x + s, cy + s);
            else
                p.addTriangle (x + s, cy, x - s, cy - s, x - s, cy + s);
            g.setColour (lit ? kAccent : kTrack);
            g.fillPath (p);
        };

        arrow (cx - 46.0f, true,  liveDir < 0);
        arrow (cx - 30.0f, true,  liveDir < 0);
        arrow (cx + 30.0f, false, liveDir > 0);
        arrow (cx + 46.0f, false, liveDir > 0);

        g.setColour (liveDir == 0 ? kMuted : kText);
        g.setFont (caps (10.0f, 0.18f));
        g.drawText (liveDir < 0 ? "REVERSE" : (liveDir > 0 ? "FORWARD" : "STOPPED"),
                    (int) cx - 26, (int) cy - 7, 52, 14, juce::Justification::centred, false);
    }

    // faint divider between the two knob rows
    g.setColour (kLine.withAlpha (0.6f));
    const int midY = rightPanel.getY() + rightPanel.getHeight() / 2;
    g.drawHorizontalLine (midY, (float) rightPanel.getX() + 16.0f, (float) rightPanel.getRight() - 16.0f);
}

void EpatolesEditor::resized()
{
    constexpr int margin = 24;

    // header controls
    osBox.setBounds (getWidth() - margin - 84, 38, 84, 28);
    osText = { osBox.getX(), 22, osBox.getWidth(), 14 };
    autoGainButton.setBounds (osBox.getX() - 20 - 120, 41, 120, 22);

    leftPanel  = { margin, 92, 270, 324 };
    rightPanel = { leftPanel.getRight() + 16, 92, getWidth() - margin - (leftPanel.getRight() + 16), 324 };

    // speed dial + tape stop
    speedKnob->label.setBounds (leftPanel.getX(), leftPanel.getY() + 12, leftPanel.getWidth(), 16);
    speedKnob->slider.setBounds (leftPanel.getX() + 20, leftPanel.getY() + 30,
                                 leftPanel.getWidth() - 40, 240);
    {
        const int gap = 10, side = 20;
        const int pw = (leftPanel.getWidth() - side * 2 - gap) / 2;
        const int py = leftPanel.getBottom() - 14 - 36;
        stopButton.setBounds    (leftPanel.getX() + side, py, pw, 36);
        reverseButton.setBounds (leftPanel.getX() + side + pw + gap, py, pw, 36);
    }

    // 6 x 2 grid: 11 knobs, then the chunk sync box in the last cell
    constexpr int cols = 6, rows = 2;
    const int cw = rightPanel.getWidth() / cols;
    const int ch = rightPanel.getHeight() / rows;

    for (size_t i = 1; i < knobs.size(); ++i)
    {
        const int idx = (int) i - 1;
        const int col = idx % cols;
        const int row = idx / cols;

        const juce::Rectangle<int> cell (rightPanel.getX() + col * cw,
                                         rightPanel.getY() + row * ch, cw, ch);

        knobs[i]->label.setBounds (cell.getX(), cell.getY() + 10, cell.getWidth(), 14);
        knobs[i]->slider.setBounds (cell.getX(), cell.getY() + 26, cell.getWidth(), cell.getHeight() - 34);
    }

    {
        const juce::Rectangle<int> cell (rightPanel.getX() + 5 * cw, rightPanel.getY() + ch, cw, ch);
        syncText = { cell.getX(), cell.getY() + 10, cell.getWidth(), 14 };
        syncBox.setBounds (cell.getX() + 6, cell.getCentreY() - 14, cell.getWidth() - 12, 28);
    }
}
