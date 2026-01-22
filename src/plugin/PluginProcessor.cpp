#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "juce_audio_basics/juce_audio_basics.h"

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
            std::make_unique<juce::AudioParameterFloat> ("minNoteVelocity", // parameterID
                "minNoteVelocity", // parameter name
                0.0f, // minimum value
                1.0f, // maximum value
                0.0f), // default value
            std::make_unique<juce::AudioParameterFloat> ("latencySeconds", // parameterID
                "latencySeconds", // parameter name
                0.05f, // minimum value
                0.5f, // maximum value
                0.1f), // default value
              std::make_unique<juce::AudioParameterBool> ("TrackingToggle", // parameterID
                  "Enable Tracking", // parameter name
                  false) // default value
          }), sampleOffset{0}
{
    transcriber = std::make_unique<Transcriber>();
    transcriber->resetBuffersSamples(22050);

    trackingParameter = parameters.getRawParameterValue ("TrackingToggle");
    minNoteDurationParameter = parameters.getRawParameterValue ("minNoteDurationMs");
    minNoteVelocityParameter = parameters.getRawParameterValue ("minNoteVelocity");
    noteSensitivityParameter = parameters.getRawParameterValue ("noteSensitivity");
    splitSensitivityParameter = parameters.getRawParameterValue ("splitSensitivity");
    latencySecondsParameter = parameters.getRawParameterValue ("latencySeconds");
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

    // set transcriber buffer size to 
    // the closest multiple of 'maxDown' which is
    // the number of samples we send each time in processBlock 
    // so the timing works :) 
    int transcriberBufSize = maxDown;
    while (transcriberBufSize < 22050){
        transcriberBufSize += maxDown;
    }
    transcriber->resetBuffersSamples(transcriberBufSize);
    const float latencySeconds = latencySecondsParameter ? latencySecondsParameter->load() : 0.1f;
    transcriber->setLatencySeconds(latencySeconds);
    lastLatencySeconds = latencySeconds;

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

    pushRMSForGUI(buffer.getRMSLevel(0,0,buffer.getNumSamples()));
    
    float noteSensitivity = *parameters.getRawParameterValue("noteSensitivity");
    float splitSensitivity = *parameters.getRawParameterValue("splitSensitivity");
    float minNoteDuration = *parameters.getRawParameterValue("minNoteDurationMs");
    float minNoteVelocity = *parameters.getRawParameterValue("minNoteVelocity");
    float latencySeconds = *parameters.getRawParameterValue("latencySeconds");
    bool tracking = *parameters.getRawParameterValue("TrackingToggle");

    transcriber->setNoteSensitivity(noteSensitivity);
    transcriber->setSplitSensitivity(splitSensitivity);
    transcriber->setMinNoteDuration(minNoteDuration);
    transcriber->setMinNoteVelocity(minNoteVelocity);
    if (latencySeconds != lastLatencySeconds) {
        transcriber->setLatencySeconds(latencySeconds);
        lastLatencySeconds = latencySeconds;
    }

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
    
    // maybe apply a gate here? 
    float resampleDB = Decibels::gainToDecibels(internalDownsampledBuffer.getRMSLevel(0, 0, internalDownsampledBuffer.getNumSamples()));
    if (resampleDB < -45){
        internalDownsampledBuffer.clear();
    }
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
            if (metadata.getMessage().isNoteOn()){
                pushMIDIForGUI(metadata.getMessage());
            }
            if (metadata.getMessage().isNoteOnOrOff()){
                pushNoteEventForUI(metadata.getMessage());
            }
            midiMessages.addEvent (metadata.getMessage(), samplePos % numInputSamples);
        }
    }
    // clear the messages we've used 
    pendingMidi.clear (sampleOffset, numInputSamples);
    sampleOffset = endSample;

}

void AudioPluginAudioProcessor::pushNoteEventForUI(const juce::MidiMessage& msg)
{
    if (!msg.isNoteOnOrOff())
        return;

    int startIndex = 0;
    int blockSize = 0;
    int startIndex2 = 0;
    int blockSize2 = 0;
    noteEventFifo.prepareToWrite(1, startIndex, blockSize, startIndex2, blockSize2);
    if (blockSize == 0)
        return;

    auto& ev = noteEventBuffer[static_cast<size_t>(startIndex)];
    ev.note = msg.getNoteNumber();
    ev.velocity = msg.isNoteOn() ? msg.getFloatVelocity() : 0.0f;
    ev.isNoteOn = msg.isNoteOn();
    noteEventFifo.finishedWrite(blockSize);
}

bool AudioPluginAudioProcessor::popNextNoteEvent(NoteEvent& event)
{
    int startIndex = 0;
    int blockSize = 0;
    int startIndex2 = 0;
    int blockSize2 = 0;
    noteEventFifo.prepareToRead(1, startIndex, blockSize, startIndex2, blockSize2);
    if (blockSize == 0)
        return false;

    event = noteEventBuffer[static_cast<size_t>(startIndex)];
    noteEventFifo.finishedRead(blockSize);
    return true;
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


// Publish note/velocity to the UI mailbox (RT-safe, no locks/allocs).
void AudioPluginAudioProcessor::pushMIDIForGUI(const juce::MidiMessage& msg)
{
    if (!msg.isNoteOnOrOff())
        return;

    const int   note = msg.getNoteNumber();
    const float vel  = msg.isNoteOn() ? juce::jlimit(0.0f, 1.0f, msg.getFloatVelocity())
                                      : 0.0f;

    lastNote.store(note, std::memory_order_relaxed);
    lastVelocity.store(vel,   std::memory_order_relaxed);
    lastNoteStamp.fetch_add(1, std::memory_order_release);
}

// Pull latest event if stamp changed since lastSeenStamp (message thread).
bool AudioPluginAudioProcessor::pullMIDIForGUI(int& note, float& vel, uint32_t& lastSeenStamp)
{
    const auto s = lastNoteStamp.load(std::memory_order_acquire);
    if (s == lastSeenStamp) return false; // don't send same note twice

    lastSeenStamp = s;
    note = lastNote.load(std::memory_order_relaxed);
    if (note == -1) return false; // starting condition is that the note is -1

    vel  = lastVelocity.load(std::memory_order_relaxed);
    return true;
}

// Publish note/velocity to the UI mailbox (RT-safe, no locks/allocs).
void AudioPluginAudioProcessor::pushRMSForGUI(float rms)
{
    lastRMS.store(rms, std::memory_order_relaxed);
}

// Pull latest event if stamp changed since lastSeenStamp (message thread).
bool AudioPluginAudioProcessor::pullRMSForGUI(float& rms)
{
    rms = lastRMS.load(std::memory_order_relaxed);
    return true;
}



//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
