#pragma once

#include <JuceHeader.h>

class FooterComponent : public juce::Component
{
public:
    FooterComponent();

    bool isExpanded() const;
    int getPreferredHeight() const;
    void setOnToggle(std::function<void(bool)> callback);

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void toggleExpanded();

    bool expanded = false;
    std::function<void(bool)> onToggle;
    juce::TextButton toggleButton;
    juce::Label titleLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FooterComponent)
};
