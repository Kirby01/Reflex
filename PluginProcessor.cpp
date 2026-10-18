#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <limits>

namespace
{
    juce::NormalisableRange<float> logarithmicRange (float lo, float hi)
    {
        return { lo, hi,
            [] (float start, float end, float n) { return start * std::pow (end / start, n); },
            [] (float start, float end, float value) { return std::log (value / start) / std::log (end / start); } };
    }
}

ReflexAudioProcessor::ReflexAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "ReflexState", makeParameters())
{
    inputParam   = parameters.getRawParameterValue ("input");
    driveParam   = parameters.getRawParameterValue ("drive");
    speedParam   = parameters.getRawParameterValue ("speed");
    releaseParam = parameters.getRawParameterValue ("release");
    outputParam  = parameters.getRawParameterValue ("output");
    deltaParam   = parameters.getRawParameterValue ("delta");
    setLatencySamples (0);
}

juce::AudioProcessorValueTreeState::ParameterLayout ReflexAudioProcessor::makeParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "input", 1 }, "Input",
                  juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "drive", 1 }, "Drive",
                  logarithmicRange (1.0f, 40.0f), 4.103763f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "speed", 1 }, "Attack Speed",
                  logarithmicRange (0.01f, 8.0f), 1.0f));
    // Full product range of the JSFX's two release controls: 1e-8 .. 0.01.
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "release", 1 }, "Release",
                  logarithmicRange (static_cast<float> (ReflexDynamics::releaseMsFromCoefficient (0.01)),
                                    static_cast<float> (ReflexDynamics::releaseMsFromCoefficient (1.0e-8))),
                  static_cast<float> (ReflexDynamics::defaultReleaseMs())));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "output", 1 }, "Output",
                  juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "delta", 1 }, "Delta", false));
    return layout;
}

bool ReflexAudioProcessor::isBusesLayoutSupported (const BusesLayout& layout) const
{
    const auto out = layout.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && layout.getMainInputChannelSet() == out;
}

void ReflexAudioProcessor::updateTargets (bool immediate)
{
    auto set = [immediate] (Smoother& smoother, double value)
    {
        if (immediate) smoother.setCurrentAndTargetValue (value);
        else smoother.setTargetValue (value);
    };
    set (inputGain, juce::Decibels::decibelsToGain (static_cast<double> (inputParam->load())));
    set (outputGain, juce::Decibels::decibelsToGain (static_cast<double> (outputParam->load())));
    set (driveValue, driveParam->load());
    set (speedValue, speedParam->load());
    set (releaseValue, ReflexDynamics::releaseCoefficient (releaseParam->load(), rate));
    set (deltaValue, deltaParam->load() >= 0.5f ? 1.0 : 0.0);
}

void ReflexAudioProcessor::prepareToPlay (double sampleRate, int)
{
    rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    rateScale = ReflexDynamics::referenceRate / rate;
    meterDecay = std::exp (-1.0 / (0.18 * rate));
    inputGain.reset (rate, 0.02); outputGain.reset (rate, 0.02);
    driveValue.reset (rate, 0.02); speedValue.reset (rate, 0.02);
    releaseValue.reset (rate, 0.02); deltaValue.reset (rate, 0.005);
    reset();
}

void ReflexAudioProcessor::reset()
{
    dynamics.reset();
    meterEnvelope = 0.0;
    meterDb.store (0.0f, std::memory_order_relaxed);
    updateTargets (true);
}

void ReflexAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());
    const int channels = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());
    if (channels < 1) return;
    updateTargets (false);
    auto* left = buffer.getWritePointer (0);
    auto* right = channels > 1 ? buffer.getWritePointer (1) : nullptr;
    auto finiteSample = [] (float s) { return std::isfinite (s) ? static_cast<double> (s) : 0.0; };
    auto toFloat = [] (double s)
    {
        const auto limit = static_cast<double> (std::numeric_limits<float>::max());
        return static_cast<float> (juce::jlimit (-limit, limit, s));
    };
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const double in = inputGain.getNextValue();
        const double l = finiteSample (left[i]) * in;
        const double r = right != nullptr ? finiteSample (right[i]) * in : l;
        const double gain = dynamics.process (std::max (std::abs (l), std::abs (r)),
                                             driveValue.getNextValue(), speedValue.getNextValue(),
                                             releaseValue.getNextValue(), rateScale);
        const double delta = deltaValue.getNextValue();
        const double out = outputGain.getNextValue();
        const double wetL = l * gain, wetR = r * gain;
        left[i] = toFloat ((wetL + delta * ((l - wetL) - wetL)) * out);
        if (right != nullptr) right[i] = toFloat ((wetR + delta * ((r - wetR) - wetR)) * out);
        const double reduction = -20.0 * std::log10 (gain);
        meterEnvelope = std::max (reduction, meterEnvelope * meterDecay);
    }
    meterDb.store (static_cast<float> (meterEnvelope), std::memory_order_relaxed);
}

void ReflexAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = parameters.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ReflexAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ReflexAudioProcessor::createEditor() { return new ReflexAudioProcessorEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReflexAudioProcessor(); }
