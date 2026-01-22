#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
//     : AudioProcessorEditor (&p), processorRef (p)
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (p),
    processorRef(p),
      valueTreeState (vts),
      controlPanel(vts)
{
    addAndMakeVisible(header);
    addAndMakeVisible(controlPanel);
    addAndMakeVisible(pianoRoll);
    addAndMakeVisible(footer);

    trackingAttachment.reset(new ButtonAttachment(valueTreeState, "TrackingToggle",
                                                  header.getIndicatorButton()));
    footer.setOnToggle([this](bool) { resized(); });

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (920, 520);

    // start polling the processor for 'last midi message'
    startTimerHz(30);

}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(juce::Colour(0xFF0F1418));
}


void AudioPluginAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto area = getLocalBounds();
    const int headerHeight = 38;
    const int footerHeight = footer.getPreferredHeight();

    header.setBounds(area.removeFromTop(headerHeight));
    footer.setBounds(area.removeFromBottom(footerHeight));

    auto contentArea = area.reduced(12, 8);
    const int leftWidth = juce::jmin(260, contentArea.getWidth() / 3);
    controlPanel.setBounds(contentArea.removeFromLeft(leftWidth));
    contentArea.removeFromLeft(12);
    pianoRoll.setBounds(contentArea);
}


void AudioPluginAudioProcessorEditor::timerCallback()
{
    float rms;
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;

    AudioPluginAudioProcessor::NoteEvent event;
    while (processorRef.popNextNoteEvent(event))
    {
        if (event.isNoteOn)
            pianoRoll.noteOn(event.note, event.velocity, now);
        else
            pianoRoll.noteOff(event.note, now);
    }

    if (processorRef.pullRMSForGUI(rms)){
        lastRms = rms;
        header.setRMS(rms);
    }

    const auto* trackingParam = valueTreeState.getRawParameterValue("TrackingToggle");
    const bool trackingEnabled = trackingParam != nullptr && trackingParam->load() > 0.5f;
    const double lastNoteTime = pianoRoll.getLastNoteEventTimeSeconds();
    const double timeSinceNote = now - lastNoteTime;
    const float rmsDb = juce::Decibels::gainToDecibels(lastRms);
    HeaderComponent::StatusState state = HeaderComponent::StatusState::Silence;
    if (trackingEnabled)
    {
        if (timeSinceNote < 0.5)
            state = HeaderComponent::StatusState::Active;
        else if (rmsDb > -50.0f)
            state = HeaderComponent::StatusState::Low;
    }
    header.setStatus(state);
}
