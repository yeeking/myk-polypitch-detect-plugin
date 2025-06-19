#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
//     : AudioProcessorEditor (&p), processorRef (p)
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (p),
    processorRef(p),
      valueTreeState (vts)
{
    // juce::ignoreUnused (processor);

    noteSensitivityLabel.setText ("Note Sensitivity", juce::dontSendNotification);
    // noteSensitivityLabel.attachToComponent (&noteSensitivitySlider, true);
    noteSensitivitySlider.setSliderStyle (juce::Slider::LinearHorizontal);  
    addAndMakeVisible (noteSensitivityLabel);
    addAndMakeVisible (noteSensitivitySlider);
    noteSensitivityAttachment.reset (new SliderAttachment (valueTreeState, "noteSensitivity", noteSensitivitySlider));

    splitSensitivityLabel.setText ("Split Sensitivity", juce::dontSendNotification);
    // splitSensitivityLabel.attachToComponent (&splitSensitivitySlider, true);        
    splitSensitivitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    addAndMakeVisible (splitSensitivityLabel);
    addAndMakeVisible (splitSensitivitySlider);
    splitSensitivityAttachment.reset (new SliderAttachment (valueTreeState, "splitSensitivity", splitSensitivitySlider));

    minNoteDurationLabel.setText ("Min Note Duration (ms)", juce::dontSendNotification);
    // minNoteDurationLabel.attachToComponent (&minNoteDurationSlider, true);
    minNoteDurationSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    addAndMakeVisible (minNoteDurationLabel);
    addAndMakeVisible (minNoteDurationSlider);
    minNoteDurationAttachment.reset (new SliderAttachment (valueTreeState, "minNoteDurationMs", minNoteDurationSlider));

    trackingToggle.setButtonText ("Enable Tracking");
    addAndMakeVisible (trackingToggle);
    trackingAttachment.reset (new ButtonAttachment (valueTreeState, "TrackingToggle", trackingToggle));
    
    // setSize (paramSliderWidth + paramLabelWidth, juce::jmax (100, paramControlHeight * 2));


    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (800, 500);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void AudioPluginAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto area = getLocalBounds().reduced (10);
    // gainLabel.setBounds (area.removeFromTop (20));
    // gainSlider.setBounds (area.removeFromTop (20));
    int x, y, colWidth, rowHeight;
    x=0;
    y=20;
    colWidth = area.getWidth() / 2;
    rowHeight = area.getHeight() / 8;

    trackingLabel.setBounds (x, y, colWidth, rowHeight);
    y+= rowHeight;
    trackingToggle.setBounds (x, y, colWidth, rowHeight);
    y+= rowHeight;    
    noteSensitivityLabel.setBounds (x, y, colWidth, rowHeight);
    y+= rowHeight;
    noteSensitivitySlider.setBounds (x, y, colWidth, rowHeight);
    y+= rowHeight;
    splitSensitivityLabel.setBounds (x, y, colWidth, rowHeight);
    y+= rowHeight;
    splitSensitivitySlider.setBounds (x, y, colWidth, rowHeight);
    y+= rowHeight;
    minNoteDurationLabel.setBounds (x, y, colWidth, rowHeight);
    y+= rowHeight;
    minNoteDurationSlider.setBounds (x, y, colWidth, rowHeight);
}
