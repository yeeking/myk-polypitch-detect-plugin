#include "ControlPanelComponent.h"

ControlPanelComponent::ControlPanelComponent(juce::AudioProcessorValueTreeState& vts)
    : sensitivitySlider("Sensitivity", "noteSensitivity",
                        "Higher values make detection more eager.", vts),
      splitSensitivitySlider("Split", "splitSensitivity",
                             "Higher values split notes more aggressively.", vts),
      minNoteDurationSlider("Min Dur", "minNoteDurationMs",
                            "Minimum note length in milliseconds.", vts),
      latencySlider("Latency", "latencySeconds",
                    "Capture window duration (seconds).", vts),
      minVelocitySlider("Min Vel", "minNoteVelocity",
                        "Minimum MIDI velocity floor.", vts)
{
    detectionLabel.setText("Detection", juce::dontSendNotification);
    detectionLabel.setFont(juce::Font(juce::FontOptions().withHeight(13.0f).withStyle("Bold")));
    detectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF98A6AD));
    addAndMakeVisible(detectionLabel);

    timeLabel.setText("Time", juce::dontSendNotification);
    timeLabel.setFont(juce::Font(juce::FontOptions().withHeight(13.0f).withStyle("Bold")));
    timeLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF98A6AD));
    addAndMakeVisible(timeLabel);

    outputLabel.setText("Output", juce::dontSendNotification);
    outputLabel.setFont(juce::Font(juce::FontOptions().withHeight(13.0f).withStyle("Bold")));
    outputLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF98A6AD));
    addAndMakeVisible(outputLabel);

    addAndMakeVisible(sensitivitySlider);
    addAndMakeVisible(splitSensitivitySlider);
    addAndMakeVisible(minNoteDurationSlider);
    addAndMakeVisible(latencySlider);
    addAndMakeVisible(minVelocitySlider);
}

void ControlPanelComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF141A20));
}

void ControlPanelComponent::resized()
{
    auto area = getLocalBounds().reduced(8, 10);
    const int labelHeight = 18;
    const int gap = 8;
    const int totalSliders = 5;
    const int totalLabels = 3;
    const int totalGaps = totalSliders + totalLabels + 4;
    const int sliderHeight = (area.getHeight() - (totalLabels * labelHeight) - (totalGaps * gap)) / totalSliders;

    detectionLabel.setBounds(area.removeFromTop(labelHeight));
    area.removeFromTop(gap);
    sensitivitySlider.setBounds(area.removeFromTop(sliderHeight));
    area.removeFromTop(gap);
    splitSensitivitySlider.setBounds(area.removeFromTop(sliderHeight));

    area.removeFromTop(gap);
    timeLabel.setBounds(area.removeFromTop(labelHeight));
    area.removeFromTop(gap);
    minNoteDurationSlider.setBounds(area.removeFromTop(sliderHeight));
    area.removeFromTop(gap);
    latencySlider.setBounds(area.removeFromTop(sliderHeight));

    area.removeFromTop(gap);
    outputLabel.setBounds(area.removeFromTop(labelHeight));
    area.removeFromTop(gap);
    minVelocitySlider.setBounds(area.removeFromTop(sliderHeight));
}
