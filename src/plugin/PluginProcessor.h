#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Transcriber.h"
#include "AudioUtils.h"
#include "Resampler.h"
#include "BasicPitch.h"

//==============================================================================
class AudioPluginAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** call this from anywhere to tell the processor about some midi that was received so it can save it for the GUI to access later */
    void pushMIDIForGUI(const juce::MidiMessage& msg);
    /** call this from the UI message thread if you want to know what the last received midi message was */
    bool pullMIDIForGUI(int& note, float& vel, uint32_t& lastSeenStamp);

private:
    std::unique_ptr<Transcriber> transcriber;
    /** collects midi from transcriber and stores it internally with fixed times
     * if no MIDI collected, returns false, if MIDI collected, return true 
     */
    bool collectMIDIFromTranscriber();
    juce::MidiBuffer pendingMidi; 
    long sampleOffset;
    // juce::AudioBuffer<float> resampledBuffer;
    Resampler resampleProcessor; 

    // in your AudioPluginAudioProcessor.h
    juce::AudioBuffer<float> internalMonoBuffer;
    juce::AudioBuffer<float> internalDownsampledBuffer;

    
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* trackingParameter = nullptr;
    std::atomic<float>* noteSensitivityParameter = nullptr;
    std::atomic<float>* splitSensitivityParameter = nullptr;
    std::atomic<float>* minNoteDurationParameter = nullptr;
    std::atomic<float>* noteHoldSensitivityParameter = nullptr;

    // std::atomic<float>* gainParameter = nullptr;
    // used to expose last note detected to the GUI
    // which will poll us
    std::atomic<int>   lastNote {-1};
    std::atomic<float> lastVelocity  {0.0f};            // 0..1
    std::atomic<uint32_t> lastNoteStamp {0};           // increments on every new note event
    // this one is used for the input level meter
    std::atomic<float> lastRMS  {0.0f};            

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
