#include "ParamSliderComponent.h"

ParamSliderComponent::ParamSliderComponent(const juce::String& labelText,
                                           const juce::String& paramIdIn,
                                           const juce::String& tooltipText,
                                           juce::AudioProcessorValueTreeState& vts)
    : label(labelText),
      paramId(paramIdIn)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 18);
    slider.setPopupDisplayEnabled(false, false, this);
    slider.setNumDecimalPlacesToDisplay(2);
    slider.setLookAndFeel(&sliderLaf);
    slider.setTooltip(tooltipText);
    configureDoubleClickDefault(vts, paramId);

    addAndMakeVisible(slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(vts, paramId, slider);
}

ParamSliderComponent::~ParamSliderComponent()
{
    slider.setLookAndFeel(nullptr);
}

void ParamSliderComponent::resized()
{
    auto area = getLocalBounds().reduced(6);
    const int labelHeight = 18;
    area.removeFromTop(labelHeight);
    slider.setBounds(area);
}

void ParamSliderComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.setColour(juce::Colour(0xFF1A2026));
    g.fillRoundedRectangle(area.toFloat(), 8.0f);

    g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Bold")));
    g.setColour(juce::Colour(0xFF98A6AD));
    g.drawFittedText(label, area.removeFromTop(18), juce::Justification::centredLeft, 1);
}

void ParamSliderComponent::SliderLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                                               int x, int y, int width, int height,
                                                               float sliderPos, float minSliderPos,
                                                               float maxSliderPos,
                                                               const juce::Slider::SliderStyle style,
                                                               juce::Slider& slider)
{
    juce::ignoreUnused(minSliderPos, maxSliderPos, slider);

    auto track = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                        static_cast<float>(width), static_cast<float>(height))
                     .reduced(8.0f, height * 0.35f);

    g.setColour(juce::Colour(0xFF2C353D));
    g.fillRoundedRectangle(track, 6.0f);

    if (style == juce::Slider::LinearHorizontal)
    {
        const float valueWidth = juce::jlimit(0.0f, track.getWidth(), sliderPos - track.getX());
        auto fill = track.withWidth(valueWidth);
        g.setColour(juce::Colour(0xFF3BD3C7));
        g.fillRoundedRectangle(fill, 6.0f);

        const float thumbRadius = 9.0f;
        const float thumbX = sliderPos - thumbRadius;
        const float thumbY = track.getCentreY() - thumbRadius;
        g.setColour(juce::Colour(0xFFEDE7DD));
        g.fillEllipse(thumbX, thumbY, thumbRadius * 2.0f, thumbRadius * 2.0f);
    }
}

void ParamSliderComponent::configureDoubleClickDefault(juce::AudioProcessorValueTreeState& vts,
                                                       const juce::String& param)
{
    if (auto* parameter = vts.getParameter(param))
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged != nullptr)
        {
            const float defaultNorm = ranged->getDefaultValue();
            const auto defaultValue = ranged->getNormalisableRange().convertFrom0to1(defaultNorm);
            slider.setDoubleClickReturnValue(true, defaultValue);
        }
    }
}
