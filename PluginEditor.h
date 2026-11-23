#pragma once

#include "EpatolesProcessor.h"
#include <memory>
#include <vector>

namespace epat
{
// Same graphite as Selotape, with the accent turned around: amber becomes ice blue.
inline const juce::Colour kBg      { 0xff0e1116 };
inline const juce::Colour kPanel   { 0xff14181e };
inline const juce::Colour kPanelHi { 0xff1c222a };
inline const juce::Colour kLine    { 0xff2a313b };
inline const juce::Colour kTrack   { 0xff2a303a };
inline const juce::Colour kText    { 0xffe9ecee };
inline const juce::Colour kMuted   { 0xff7c8591 };
inline const juce::Colour kAccent  { 0xff6ec6ff };
} // namespace epat

// Flat, dark look: thin arc dials, pill toggles for Tape Stop and Reverse, a small
// switch for Auto Gain. All drawing is vector, no image assets.
class EpatolesLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EpatolesLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
};

class EpatolesEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit EpatolesEditor (EpatolesProcessor&);
    ~EpatolesEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    Knob* addKnob (const juce::String& paramId, const juce::String& text, bool big, bool bipolar);

    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment    = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    EpatolesProcessor& proc;
    EpatolesLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> knobs;
    Knob* speedKnob = nullptr;

    juce::ToggleButton stopButton, reverseButton, autoGainButton;
    juce::ComboBox osBox, syncBox;

    std::unique_ptr<ButtonAttachment> stopAtt, reverseAtt, autoGainAtt;
    std::unique_ptr<ComboAttachment> osAtt, syncAtt;

    int liveDir = 0;   // -1 reverse, 0 stopped, +1 forward (from the live transport speed)

    juce::Rectangle<int> leftPanel, rightPanel, osText, syncText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EpatolesEditor)
};
