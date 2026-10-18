#pragma once
#include <algorithm>
#include <cmath>

// Standalone core is also compiled by the numerical verification harness.
// At 48 kHz this is algebraically equivalent to the approved JSFX A/B pair
// while A and B remain >= 1. The downward B blend is folded into Release.
class ReflexDynamics
{
public:
    static constexpr double referenceRate = 48000.0;
    static double releaseMsFromCoefficient (double c) noexcept
    {
        return -1000.0 / (referenceRate * std::log1p (-c));
    }
    static double defaultReleaseMs() noexcept
    {
        return releaseMsFromCoefficient (0.02489984 * 0.1 * 0.1);
    }
    static double releaseCoefficient (double ms, double sampleRate) noexcept
    {
        return -std::expm1 (-1000.0 / (std::max (1.0, ms) * sampleRate));
    }
    void reset() noexcept { b = 1.0; }
    double process (double level, double drive, double speed,
                    double releaseAlpha, double rateScale) noexcept
    {
        const double target = std::min (1000000.0, std::max (1.0, level * drive));
        if (target > b)
        {
            const double amount = -std::expm1 (-speed * rateScale * (target / b - 1.0));
            const double a = b + (target - b) * amount;
            // Original B line reduces exactly to B = A on a rise.
            b = a;
        }
        else
        {
            // Original downward A and B blends multiply their coefficients:
            // effectiveRelease = release * 0.1 * p1.
            b += (target - b) * releaseAlpha;
        }
        b = std::min (1000000.0, std::max (1.0, b));
        return 1.0 / b;
    }
private:
    double b = 1.0;
};

#ifndef REFLEX_DSP_ONLY
#include <JuceHeader.h>
#include <atomic>

class ReflexAudioProcessor : public juce::AudioProcessor
{
public:
    ReflexAudioProcessor();
    ~ReflexAudioProcessor() override = default;
    void prepareToPlay (double sampleRate, int maximumBlockSize) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Reflex"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    float getReductionDb() const noexcept { return meterDb.load (std::memory_order_relaxed); }
    juce::AudioProcessorValueTreeState parameters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    void updateTargets (bool immediate);
    ReflexDynamics dynamics;
    using Smoother = juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear>;
    Smoother inputGain, outputGain, driveValue, speedValue, releaseValue, deltaValue;
    std::atomic<float>* inputParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* speedParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* deltaParam = nullptr;
    std::atomic<float> meterDb { 0.0f };
    double rate = 48000.0, rateScale = 1.0, meterDecay = 0.99986, meterEnvelope = 0.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReflexAudioProcessor)
};
#endif
