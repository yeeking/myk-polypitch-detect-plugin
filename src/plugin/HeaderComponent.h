#pragma once

#include <JuceHeader.h>
#include "LevelMeterComp.h"

class HeaderComponent : public juce::Component
{
public:
    enum class StatusState
    {
        Silence,
        Low,
        Active
    };

    class TrackingIndicatorButton : public juce::ToggleButton
    {
    public:
        void setStatus(StatusState newState);
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;

    private:
        StatusState state = StatusState::Silence;
    };

    HeaderComponent();

    void setStatus(StatusState state);
    void setRMS(float rms);
    TrackingIndicatorButton& getIndicatorButton();
    juce::TextButton& getPanicButton();

    void resized() override;

private:
    juce::Label titleLabel;
    TrackingIndicatorButton indicatorButton;
    juce::TextButton panicButton;
    LevelMeterComp levelMeter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderComponent)
};
