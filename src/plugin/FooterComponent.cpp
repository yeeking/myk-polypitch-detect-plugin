#include "FooterComponent.h"

FooterComponent::FooterComponent()
{
    toggleButton.setButtonText(">");
    toggleButton.onClick = [this] { toggleExpanded(); };
    toggleButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1A2026));
    toggleButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF98A6AD));
    addAndMakeVisible(toggleButton);

    titleLabel.setText("Advanced", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Bold")));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF98A6AD));
    addAndMakeVisible(titleLabel);
}

bool FooterComponent::isExpanded() const
{
    return expanded;
}

int FooterComponent::getPreferredHeight() const
{
    return expanded ? 90 : 26;
}

void FooterComponent::setOnToggle(std::function<void(bool)> callback)
{
    onToggle = std::move(callback);
}

void FooterComponent::resized()
{
    auto area = getLocalBounds().reduced(8, 4);
    toggleButton.setBounds(area.removeFromLeft(20));
    titleLabel.setBounds(area.removeFromLeft(100));
}

void FooterComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF0F1418));
    if (expanded)
    {
        g.setColour(juce::Colour(0xFF202833));
        g.drawRect(getLocalBounds().reduced(2));
    }
}

void FooterComponent::toggleExpanded()
{
    expanded = !expanded;
    toggleButton.setButtonText(expanded ? "v" : ">");
    if (onToggle)
        onToggle(expanded);
    repaint();
}
