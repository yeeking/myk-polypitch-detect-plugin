#pragma once

#include "PluginProcessor.h"
#include "HeaderComponent.h"
#include "ControlPanelComponent.h"
#include "PianoRollComponent.h"
#include "FooterComponent.h"

// shorthands for the gui components for controlling params
typedef juce::AudioProcessorValueTreeState::SliderAttachment SliderAttachment;
typedef juce::AudioProcessorValueTreeState::ButtonAttachment ButtonAttachment;

//==============================================================================
class AudioPluginAudioProcessorEditor final : 
        public juce::AudioProcessorEditor,
        private juce::Timer
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& vts);

    // explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    // from Timer
    void timerCallback() override; // polls processor mailbox

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    AudioPluginAudioProcessor& processorRef;

  juce::AudioProcessorValueTreeState& valueTreeState;

    HeaderComponent header;
    ControlPanelComponent controlPanel;
    PianoRollComponent pianoRoll;
    FooterComponent footer;
    std::unique_ptr<ButtonAttachment> trackingAttachment;
    juce::TooltipWindow tooltipWindow { this, 700 };

    float lastRms = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
