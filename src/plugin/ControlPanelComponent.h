#pragma once

#include <JuceHeader.h>
#include "ParamSliderComponent.h"

class ControlPanelComponent : public juce::Component
{
public:
    ControlPanelComponent(juce::AudioProcessorValueTreeState& vts);

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    juce::Label detectionLabel;
    juce::Label timeLabel;
    juce::Label outputLabel;

    ParamSliderComponent sensitivitySlider;
    ParamSliderComponent splitSensitivitySlider;
    ParamSliderComponent minNoteDurationSlider;
    ParamSliderComponent latencySlider;
    ParamSliderComponent minVelocitySlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlPanelComponent)
};
