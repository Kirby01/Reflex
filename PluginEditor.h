#pragma once
#include "PluginProcessor.h"
#include <array>

class ReflexLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ReflexLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float position, float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

class ReflexAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ReflexAudioProcessorEditor (ReflexAudioProcessor&);
    ~ReflexAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    ReflexAudioProcessor& processor;
    ReflexLookAndFeel look;
    std::array<juce::Slider, 5> knobs;
    juce::TextButton deltaButton { "DELTA" };
    juce::TooltipWindow tooltip;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::array<std::unique_ptr<SliderAttachment>, 5> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> deltaAttachment;
    float reductionDb = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReflexAudioProcessorEditor)
};
