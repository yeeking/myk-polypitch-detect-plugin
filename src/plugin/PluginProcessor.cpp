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
{
    transcriber = std::make_unique<Transcriber>();

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

//==============================================================================
// void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
// {
//     // Use this method as the place to do any pre-playback
//     // initialisation that you need..
//     juce::ignoreUnused (sampleRate, samplesPerBlock);


//     resampleProcessor.prepareToPlay(sampleRate, samplesPerBlock, BASIC_PITCH_SAMPLE_RATE);

//     int maxOut = resampleProcessor.getNumOutSamplesOnNextProcessBlock(samplesPerBlock);
//     internalDownsampledBuffer.setSize(1, maxOut);
// }

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
    const int numInputChannels = buffer.getNumChannels();
    const int numInputSamples  = buffer.getNumSamples();

    // --- 1) Prepare mono buffer ---
    internalMonoBuffer.setSize(1, numInputSamples, false, false, true);
    // copy channel 0
    internalMonoBuffer.copyFrom(0, 0, buffer, 0, 0, numInputSamples);
    // add other channels
    for (int ch = 1; ch < numInputChannels; ++ch)
        internalMonoBuffer.addFrom(0, 0, buffer, ch, 0, numInputSamples);
    // average
    internalMonoBuffer.applyGain(1.0f / static_cast<float>(numInputChannels));

    // --- 2) Resample to BASIC_PITCH_SAMPLE_RATE ---
    // ensure downsample buffer is large enough
    auto maxDownSamples = internalDownsampledBuffer.getNumSamples();
    if (maxDownSamples < numInputSamples) 
        internalDownsampledBuffer.setSize(1, numInputSamples, false, false, true);

    const float* src = internalMonoBuffer.getReadPointer(0);
    float*       dst = internalDownsampledBuffer.getWritePointer(0);

    int numDown = resampleProcessor.processBlock(src, dst, numInputSamples);
    jassert(numDown <= internalDownsampledBuffer.getNumSamples());

    // --- 3) Send into transcriber at the model's sample rate ---
    // we know this buffer is at BASIC_PITCH_SAMPLE_RATE now:
    transcriber->storeAudio(dst, numDown, BASIC_PITCH_SAMPLE_RATE);

    // --- 4) Pull out any MIDI the transcriber generated ---
    // transcriber->collectMidi(midiMessages);

    // …then do the rest of your audio/MIDI processing…
}


// void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
//                                               juce::MidiBuffer& midiMessages)
// {
    
//     // resample 
//     resampleProcessor.processBlock()
    
//     // get a const float* to channel 0’s data…
//     const float* channelData = buffer.getReadPointer (0);
//     // …but our API wants a float*, so we const_cast it away
//     float*       audioData   = const_cast<float*> (channelData);

//     int numSamples = buffer.getNumSamples();

//     transcriber->storeAudio (audioData, numSamples, getSampleRate());
//     transcriber->collectMidi(midiMessages);

// }

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused (destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
