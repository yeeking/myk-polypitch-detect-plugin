#pragma once

#include <JuceHeader.h>

class ParamSliderComponent : public juce::Component
{
public:
    ParamSliderComponent(const juce::String& labelText,
                         const juce::String& paramId,
                         const juce::String& tooltipText,
                         juce::AudioProcessorValueTreeState& vts);

    ~ParamSliderComponent() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    class SliderLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, float minSliderPos, float maxSliderPos,
                              const juce::Slider::SliderStyle style, juce::Slider& slider) override;
    };

    void configureDoubleClickDefault(juce::AudioProcessorValueTreeState& vts,
                                     const juce::String& paramId);

    juce::Slider slider;
    juce::String label;
    juce::String paramId;
    SliderLookAndFeel sliderLaf;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParamSliderComponent)
};
