#pragma once

#include "PluginProcessor.h"
// shorthands for the gui components for controlling params
typedef juce::AudioProcessorValueTreeState::SliderAttachment SliderAttachment;
typedef juce::AudioProcessorValueTreeState::ButtonAttachment ButtonAttachment;

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& vts);

    // explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    AudioPluginAudioProcessor& processorRef;

  juce::AudioProcessorValueTreeState& valueTreeState;

     juce::Label trackingLabel;
    juce::ToggleButton trackingToggle;
    std::unique_ptr<ButtonAttachment> trackingAttachment;

    juce::Label noteSensitivityLabel;
    juce::Slider noteSensitivitySlider;
    std::unique_ptr<SliderAttachment> noteSensitivityAttachment;

    juce::Label splitSensitivityLabel;
    juce::Slider splitSensitivitySlider;
    std::unique_ptr<SliderAttachment> splitSensitivityAttachment;
    
    juce::Label minNoteDurationLabel;
    juce::Slider minNoteDurationSlider;
    std::unique_ptr<SliderAttachment> minNoteDurationAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
