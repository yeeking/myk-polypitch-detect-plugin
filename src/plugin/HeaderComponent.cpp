#include "HeaderComponent.h"

HeaderComponent::HeaderComponent()
{
    titleLabel.setText("MykPolyPitch", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions().withHeight(18.0f).withStyle("Bold")));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFEDE7DD));
    addAndMakeVisible(titleLabel);

    indicatorButton.setClickingTogglesState(true);
    indicatorButton.setTooltip("Toggle tracking");
    addAndMakeVisible(indicatorButton);

    panicButton.setButtonText("PANIC");
    panicButton.setTooltip("Send all-notes-off and reset controllers");
    panicButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A1616));
    panicButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFE1A3A3));
    addAndMakeVisible(panicButton);

    levelMeter.setFrameRateHz(30);
    levelMeter.setDecaySeconds(4.0f);
    levelMeter.setRedrawThreshold(0.05f);
    addAndMakeVisible(levelMeter);
}

void HeaderComponent::setStatus(StatusState state)
{
    indicatorButton.setStatus(state);
}

void HeaderComponent::setRMS(float rms)
{
    levelMeter.setRMS(rms);
}

HeaderComponent::TrackingIndicatorButton& HeaderComponent::getIndicatorButton()
{
    return indicatorButton;
}

juce::TextButton& HeaderComponent::getPanicButton()
{
    return panicButton;
}

void HeaderComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 6);
    auto left = area.removeFromLeft(area.getWidth() / 2);
    titleLabel.setBounds(left.removeFromLeft(200));
    indicatorButton.setBounds(left.removeFromLeft(36));
    panicButton.setBounds(area.removeFromRight(70).reduced(0, 2));
    area.removeFromRight(10);
    levelMeter.setBounds(area.removeFromRight(200).reduced(0, 6));
}

void HeaderComponent::TrackingIndicatorButton::setStatus(StatusState newState)
{
    if (state != newState)
    {
        state = newState;
        repaint();
    }
}

void HeaderComponent::TrackingIndicatorButton::paintButton(juce::Graphics& g,
                                                           bool isMouseOverButton,
                                                           bool isButtonDown)
{
    juce::ignoreUnused(isButtonDown);
    auto area = getLocalBounds().toFloat().reduced(4.0f);

    juce::Colour colour;
    switch (state)
    {
        case StatusState::Active: colour = juce::Colour(0xFF25D366); break;
        case StatusState::Low: colour = juce::Colour(0xFFE1A84B); break;
        case StatusState::Silence: colour = juce::Colour(0xFFDA4F4F); break;
    }

    if (!getToggleState())
        colour = colour.darker(0.7f);
    if (isMouseOverButton)
        colour = colour.brighter(0.1f);

    g.setColour(juce::Colour(0xFF0F1418));
    g.fillRoundedRectangle(area, 6.0f);

    const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.35f;
    auto centre = area.getCentre();
    juce::Rectangle<float> circle(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(colour);
    g.fillEllipse(circle);
}
