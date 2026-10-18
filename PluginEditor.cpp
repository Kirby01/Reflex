#include "PluginEditor.h"

namespace
{
    const juce::Colour cream (0xffeee9d9), amber (0xffedb85d), dark (0xff101c27);
    const std::array<const char*, 5> ids { "input", "drive", "speed", "release", "output" };
    const std::array<const char*, 5> names { "INPUT", "DRIVE", "ATTACK SPEED", "RELEASE", "OUTPUT" };
    const std::array<int, 5> centres { 100, 255, 410, 565, 720 };

    juce::String releaseText (double ms)
    {
        if (ms >= 60000.0) return juce::String (ms / 60000.0, 2) + " min";
        if (ms >= 1000.0) return juce::String (ms / 1000.0, 2) + " s";
        return juce::String (ms, 1) + " ms";
    }
}

ReflexLookAndFeel::ReflexLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, cream);
    setColour (juce::Slider::textBoxBackgroundColourId, dark.withAlpha (0.6f));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, amber.withAlpha (0.4f));
    setColour (juce::TooltipWindow::backgroundColourId, dark);
    setColour (juce::TooltipWindow::textColourId, cream);
    setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xff5c7281));
}

void ReflexLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float position, float startAngle, float endAngle, juce::Slider&)
{
    const float diameter = static_cast<float> (juce::jmin (width, height)) - 20.0f;
    const float cx = x + width * 0.5f, cy = y + height * 0.5f;
    const float radius = diameter * 0.5f;
    const float angle = startAngle + position * (endAngle - startAngle);
    juce::Path track;
    track.addCentredArc (cx, cy, radius + 5.0f, radius + 5.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (dark); g.strokePath (track, juce::PathStrokeType (3.0f));
    juce::Path progress;
    progress.addCentredArc (cx, cy, radius + 5.0f, radius + 5.0f, 0.0f, startAngle, angle, true);
    g.setColour (amber); g.strokePath (progress, juce::PathStrokeType (3.0f));
    for (int i = 0; i <= 10; ++i)
    {
        const float tick = startAngle + (endAngle - startAngle) * static_cast<float> (i) / 10.0f;
        const float sn = std::sin (tick), cs = std::cos (tick);
        g.setColour (cream.withAlpha (0.28f));
        g.drawLine (cx + (radius + 10.0f) * sn, cy - (radius + 10.0f) * cs,
                    cx + (radius + 13.0f) * sn, cy - (radius + 13.0f) * cs, 1.0f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (cx - radius + 1.0f, cy - radius + 5.0f, diameter, diameter);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe2e7e7), cx, cy - radius,
                                            juce::Colour (0xff6b7b85), cx, cy + radius, false));
    g.fillEllipse (cx - radius, cy - radius, diameter, diameter);
    g.setColour (juce::Colour (0xffe4ecee).withAlpha (0.75f));
    g.drawEllipse (cx - radius + 1.0f, cy - radius + 1.0f, diameter - 2.0f, diameter - 2.0f, 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffbac5ca), cx - radius, cy,
                                            juce::Colour (0xff8899a3), cx + radius, cy, false));
    g.fillEllipse (cx - radius * 0.82f, cy - radius * 0.82f, diameter * 0.82f, diameter * 0.82f);
    g.setColour (dark);
    const float sn = std::sin (angle), cs = std::cos (angle);
    g.drawLine (cx + radius * 0.48f * sn, cy - radius * 0.48f * cs,
                cx + radius * 0.86f * sn, cy - radius * 0.86f * cs, 3.0f);
}

void ReflexLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                             const juce::Colour&, bool highlighted, bool down)
{
    auto rect = b.getLocalBounds().toFloat().reduced (1.0f);
    auto colour = b.getToggleState() ? amber : dark;
    if (highlighted) colour = colour.brighter (0.12f);
    if (down) colour = colour.darker (0.12f);
    g.setColour (colour); g.fillRoundedRectangle (rect, 4.0f);
    g.setColour (cream.withAlpha (0.3f)); g.drawRoundedRectangle (rect, 4.0f, 1.0f);
}

void ReflexLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? dark : cream);
    g.setFont (juce::Font (13.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

ReflexAudioProcessorEditor::ReflexAudioProcessorEditor (ReflexAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), tooltip (this, 600)
{
    setLookAndFeel (&look);
    const std::array<double, 5> defaults { 0.0, 4.103763, 1.0, ReflexDynamics::defaultReleaseMs(), 0.0 };
    const std::array<const char*, 5> tips {
        "Gain before compression. Double-click to reset.",
        "Detector drive. Higher values request more compression.",
        "Higher values catch peaks faster; this is not an attack time in milliseconds.",
        "Release time constant. Higher values recover more slowly. Combines both JSFX release controls.",
        "Output / makeup gain, also applied when monitoring Delta." };
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& knob = knobs[i];
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 122, 24);
        knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                   juce::MathConstants<float>::pi * 2.75f, true);
        knob.setName (names[i]); knob.setTooltip (tips[i]);
        addAndMakeVisible (knob);
        attachments[i] = std::make_unique<SliderAttachment> (processor.parameters, ids[i], knob);
        knob.setDoubleClickReturnValue (true, defaults[i]);
        if (i == 0 || i == 4)
        {
            knob.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };
            knob.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue(); };
        }
        else if (i == 3)
        {
            knob.textFromValueFunction = [] (double v) { return releaseText (v); };
            knob.valueFromTextFunction = [] (const juce::String& input)
            {
                auto s = input.trim().toLowerCase();
                const double value = s.getDoubleValue();
                if (s.endsWith ("min")) return value * 60000.0;
                if (s.endsWith ("s") && ! s.endsWith ("ms")) return value * 1000.0;
                return value;
            };
        }
        else
        {
            knob.textFromValueFunction = [] (double v) { return juce::String (v, 3); };
            knob.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue(); };
        }
        knob.updateText();
    }
    deltaButton.setClickingTogglesState (true);
    deltaButton.setTooltip ("Listen to the signal removed by compression. Amber means Delta is active.");
    addAndMakeVisible (deltaButton);
    deltaAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                        (processor.parameters, "delta", deltaButton);
    setResizable (false, false);
    setSize (820, 430);
    startTimerHz (30);
}

ReflexAudioProcessorEditor::~ReflexAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ReflexAudioProcessorEditor::resized()
{
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int width = i == 1 ? 146 : 130;
        knobs[i].setBounds (centres[i] - width / 2, i == 1 ? 232 : 240, width, i == 1 ? 150 : 142);
    }
    deltaButton.setBounds (696, 134, 88, 34);
}

void ReflexAudioProcessorEditor::timerCallback()
{
    reductionDb = processor.getReductionDb();
    repaint (30, 105, 650, 95);
}

void ReflexAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4b697d), 0.0f, 0.0f,
                                            juce::Colour (0xff263f52), 0.0f, 430.0f, false));
    g.fillAll();
    g.setColour (juce::Colour (0xff101e2a));
    g.drawRect (getLocalBounds(), 3);
    g.setColour (cream.withAlpha (0.22f));
    g.drawHorizontalLine (101, 34.0f, 786.0f);
    g.drawHorizontalLine (395, 34.0f, 786.0f);
    for (const auto& point : { juce::Point<float> (16, 16), juce::Point<float> (804, 16),
                               juce::Point<float> (16, 414), juce::Point<float> (804, 414) })
    {
        g.setColour (dark.withAlpha (0.7f)); g.fillEllipse (point.x - 6, point.y - 5, 12, 12);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd1ba80), point.x, point.y - 5,
                                                juce::Colour (0xff77623f), point.x, point.y + 5, false));
        g.fillEllipse (point.x - 5, point.y - 5, 10, 10);
        g.setColour (juce::Colour (0xff463b29));
        g.drawLine (point.x - 3, point.y + 2, point.x + 3, point.y - 2, 1.5f);
    }
    g.setColour (cream);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText ("H A R M O N I O U S   R E C O R D S", 38, 19, 500, 22, juce::Justification::centredLeft);
    g.setFont (juce::Font (43.0f, juce::Font::bold));
    g.drawText ("REFLEX", 35, 41, 350, 49, juce::Justification::centredLeft);
    g.setFont (juce::Font (13.0f));
    g.setColour (cream.withAlpha (0.7f));
    g.drawText ("RELATIVE TARGET COMPRESSOR", 450, 54, 332, 25, juce::Justification::centredRight);

    g.setColour (dark); g.fillRoundedRectangle (36.0f, 114.0f, 634.0f, 79.0f, 5.0f);
    g.setColour (cream.withAlpha (0.85f)); g.setFont (juce::Font (11.5f, juce::Font::bold));
    g.drawText ("GAIN REDUCTION", 50, 120, 200, 18, juce::Justification::centredLeft);
    g.setColour (amber); g.setFont (juce::Font (16.0f, juce::Font::bold));
    g.drawText (juce::String (reductionDb, 1) + " dB", 550, 119, 104, 21, juce::Justification::centredRight);
    constexpr float barX = 51.0f, barY = 146.0f, barW = 600.0f;
    const float fraction = juce::jlimit (0.0f, 1.0f, reductionDb / 30.0f);
    for (int i = 0; i < 60; ++i)
    {
        const bool active = fraction * 60.0f > static_cast<float> (i);
        g.setColour (active ? amber : juce::Colour (0xff283c48));
        g.fillRect (barX + static_cast<float> (i) * (barW / 60.0f), barY, 7.0f, 15.0f);
    }
    g.setColour (cream.withAlpha (0.7f)); g.setFont (juce::Font (11.0f));
    for (int db = 0; db <= 30; db += 6)
    {
        const float tx = barX + (barW - 7.0f) * static_cast<float> (db) / 30.0f;
        g.drawText (juce::String (db), juce::Rectangle<float> (tx - 12.0f, 168.0f, 24.0f, 16.0f),
                    juce::Justification::centred);
    }
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.setColour (cream);
    for (size_t i = 0; i < names.size(); ++i)
        g.drawText (names[i], centres[i] - 73, 211, 146, 23, juce::Justification::centred);
    g.setColour (cream.withAlpha (0.65f)); g.setFont (juce::Font (11.5f));
    g.drawText ("LINKED DETECTION   /   ZERO LATENCY", 37, 399, 480, 20, juce::Justification::centredLeft);
    g.drawText ("H R   /   01", 640, 399, 143, 20, juce::Justification::centredRight);
}
