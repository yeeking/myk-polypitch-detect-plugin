#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
    , parameters (*this, nullptr, juce::Identifier ("APVTSTutorial"),
          {
    // void setNoteSensitivity(float s)   { noteSensitivity   = s; }
    // void setSplitSensitivity(float s)  { splitSensitivity  = s; }
    // void setMinNoteDuration(float ms)  { minNoteDurationMs = ms; }
    // float    noteSensitivity       = 0.7f;
    // float    splitSensitivity      = 0.5f;
    // float    minNoteDurationMs     = 125.0f;
            std::make_unique<juce::AudioParameterFloat> ("noteSensitivity", // parameterID
                "noteSensitivity", // parameter name
                0.0f, // minimum value
                1.0f, // maximum value
                0.7f), // default value
            std::make_unique<juce::AudioParameterFloat> ("splitSensitivity", // parameterID
                "splitSensitivity", // parameter name
                0.0f, // minimum value
                1.0f, // maximum value
                0.5f), // default value
            std::make_unique<juce::AudioParameterFloat> ("minNoteDurationMs", // parameterID
                "minNoteDurationMs", // parameter name
                0.0f, // minimum value
                250.0f, // maximum value
                125.0f), // default value
            std::make_unique<juce::AudioParameterFloat> ("noteHoldSensitivity", // parameterID
                    "noteHoldSensitivity", // parameter name
                    0.5f, // minimum value
                    1.0f, // maximum value
                    0.95f), // default value
              std::make_unique<juce::AudioParameterBool> ("TrackingToggle", // parameterID
                  "Enable Tracking", // parameter name
                  false) // default value
          }), sampleOffset{0}
{
    transcriber = std::make_unique<Transcriber>();
    transcriber->resetBuffersSamples(22050);

    trackingParameter = parameters.getRawParameterValue ("TrackingToggle");
    minNoteDurationParameter = parameters.getRawParameterValue ("minNoteDurationMs");
    noteSensitivityParameter = parameters.getRawParameterValue ("noteSensitivity");
    splitSensitivityParameter = parameters.getRawParameterValue ("splitSensitivity");
    noteHoldSensitivityParameter = parameters.getRawParameterValue ("noteHoldSensitivity");
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AudioPluginAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}


void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // tell your resampler the incoming and target rates
    resampleProcessor.prepareToPlay(sampleRate,
                                    samplesPerBlock,
                                    BASIC_PITCH_SAMPLE_RATE);

    // 1) mono buffer: one channel, up to samplesPerBlock
    internalMonoBuffer.setSize(1, samplesPerBlock, /*keepExisting*/ false,
                                           /*clearExtra*/ true,
                                           /*avoidRealloc*/ false);

    // 2) downsampled buffer: one channel, enough to hold the max output
    int maxDown = resampleProcessor.getNumOutSamplesOnNextProcessBlock(samplesPerBlock);
    internalDownsampledBuffer.setSize(1, maxDown, false, true, false);

    std::cout << "prepare to play sr: "<< getSampleRate() << " mono buff len " << internalMonoBuffer.getNumSamples() << " downsamp buff len: " << internalDownsampledBuffer.getNumSamples() << std::endl;
}

void AudioPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}


void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numInputChannels = buffer.getNumChannels();
    const int numInputSamples  = buffer.getNumSamples();

    float noteSensitivity = *parameters.getRawParameterValue("noteSensitivity");
    float splitSensitivity = *parameters.getRawParameterValue("splitSensitivity");
    float minNoteDuration = *parameters.getRawParameterValue("minNoteDurationMs");
    float noteHoldSensitivity = *parameters.getRawParameterValue("noteHoldSensitivity");
    bool tracking = *parameters.getRawParameterValue("TrackingToggle");

    transcriber->setNoteSensitivity(noteSensitivity);
    transcriber->setSplitSensitivity(splitSensitivity);
    transcriber->setMinNoteDuration(minNoteDuration);
    transcriber->setNoteHoldSensitivity(noteHoldSensitivity);

    internalMonoBuffer.copyFrom(0, 0, buffer, 0, 0, numInputSamples);
    // add other channels
    for (int ch = 1; ch < numInputChannels; ++ch)
        internalMonoBuffer.addFrom(0, 0, buffer, ch, 0, numInputSamples);
    // average
    internalMonoBuffer.applyGain(1.0f / static_cast<float>(numInputChannels));
    // internalMonoBuffer.applyGate(0.0f, 1.0f); // remove DC offset

    const float* src = internalMonoBuffer.getReadPointer(0);
    float*       dst = internalDownsampledBuffer.getWritePointer(0);
    // the resample step 
    int numDown = resampleProcessor.processBlock(src, dst, numInputSamples);
    jassert(numDown <= internalDownsampledBuffer.getNumSamples());

    // --- 3) Send into transcriber at the model's sample rate ---
    // we know this buffer is at BASIC_PITCH_SAMPLE_RATE now:
    // transcriber->storeAudio(internalDownsampledBuffer.getReadPointer(0), numDown, BASIC_PITCH_SAMPLE_RATE);
    // queueAudioForTranscription(const float* inAudio, int numSamples, double sampleRate);
    transcriber->queueAudioForTranscription(internalDownsampledBuffer.getReadPointer(0), numDown, BASIC_PITCH_SAMPLE_RATE);

    // --- 4) Pull out any MIDI the transcriber generated ---
    bool gotNewMIDI = collectMIDIFromTranscriber();
    // now send any MIDI from pending MIDI that has the right timestamp
    if (gotNewMIDI) {// reset the sample offset if a new transcription came in 
        sampleOffset = 0;
    }
    // add midi from sampleOffset to sampleOffset + buffer length to
    // midiMessages buffer
    int endSample = sampleOffset + numInputSamples;
    for (auto metadata : pendingMidi) // JUCE11‑style iteration
    {
        int samplePos = metadata.samplePosition;
        if (samplePos >= sampleOffset && samplePos < endSample)
        {
            midiMessages.addEvent (metadata.getMessage(), samplePos % numInputSamples);
        }
    }
    // clear the messages we've used 
    pendingMidi.clear (sampleOffset, numInputSamples);
    sampleOffset = endSample;

}


bool AudioPluginAudioProcessor::collectMIDIFromTranscriber()
{
    int before = pendingMidi.getNumEvents();
    // if we call this before we've sent everything 
    // in pending MIDI ... we have a problem cos remaining stuff in pending gets wiped
    if (before > 0) {
        // still need to send out this pending midi before collecting the new stuff
        // so just return false 
        return false; 
    }
    // ok we have no pending midi so we are ready to collect! 
    MidiBuffer tempBuffer{};
    transcriber->collectMidi(tempBuffer);
    // the collected MIDI sample positions will be based on a sample rate of 
    // BASIC_PITCH_SAMPLE_RATE
    double ratio = getSampleRate() / BASIC_PITCH_SAMPLE_RATE;
    // https://docs.juce.com/master/structMidiMessageMetadata.html
    for (auto metadata : tempBuffer) // JUCE11‑style iteration
    {
        double samplePos = metadata.samplePosition;
        samplePos *= ratio; // shift to host's sample rate
        MidiMessage msg = metadata.getMessage();
        msg.setTimeStamp(samplePos);
        pendingMidi.addEvent(msg, static_cast<int>(samplePos));
    }

    int after = pendingMidi.getNumEvents();

    if (after > before) return true; // got some midi 
    else return false; // got no midi 

}



//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this, parameters);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    // juce::ignoreUnused (destData);
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    // juce::ignoreUnused (data, sizeInBytes);

    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xmlState));


}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
