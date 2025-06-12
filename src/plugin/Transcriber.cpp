

// Transcriber.cpp
#include "Transcriber.h"
#include <chrono>
#include <algorithm>
#include <cstring>


Transcriber::Transcriber()
{
    resetBuffers(2);
    workerThread = std::thread(&Transcriber::threadLoop, this);
}

Transcriber::~Transcriber()
{
    keepRunning = false;
    statusCV.notify_one();
    if (workerThread.joinable()) workerThread.join();
    delete[] bufferA;
    delete[] bufferB;
}

void Transcriber::resetBuffers(double bufLenInSecs)
{
    int newLen = (BASIC_PITCH_SAMPLE_RATE * bufLenInSecs);
    bufferLenSamples = std::max(newLen, 1);
    bufferLenSecs = bufLenInSecs;

    delete[] bufferA; delete[] bufferB;
    bufferA = new float[bufferLenSamples];
    bufferB = new float[bufferLenSamples];

    currentWriteBuffer = bufferA;
    currentReadBuffer  = nullptr;
    samplesWritten     = 0;


    {
        std::lock_guard<std::mutex> sl(statusMutex);
        status = notEnoughAudio;
    }
}


void Transcriber::resetBuffersSamples(int _bufLenInSamples)
{
    bufferLenSamples = _bufLenInSamples;
    bufferLenSecs = (bufferLenSamples / BASIC_PITCH_SAMPLE_RATE);
    std::cout << "Set buf len secs to " << bufferLenSecs << std::endl;

    delete[] bufferA; delete[] bufferB;
    bufferA = new float[bufferLenSamples];
    bufferB = new float[bufferLenSamples];

    currentWriteBuffer = bufferA;
    currentReadBuffer  = nullptr;
    samplesWritten     = 0;

    {
        std::lock_guard<std::mutex> sl(statusMutex);
        status = notEnoughAudio;
    }
}

void Transcriber::queueAudioForTranscription(const float* inAudio, int numSamples, double sampleRate)
{
    assert(sampleRate == BASIC_PITCH_SAMPLE_RATE);

    if (numSamples > this->bufferLenSamples){
        this->resetBuffersSamples(numSamples);    
        std::cout << "You sent me " << numSamples << " but my buffer is " << this->bufferLenSamples <<" samples. You are supposed to send smaller blocks from a realtime audio thread. " << std::endl;
    }
    assert(numSamples <= this->bufferLenSamples);// refuse to accept very long audio chunks
    bool oneJobRequested = false;

    int remaining = numSamples, offset = 0;
    while (remaining > 0)
    {
        int spaceLeft = bufferLenSamples - samplesWritten;
        int chunk     = std::min(spaceLeft, remaining);

        std::memcpy(currentWriteBuffer + samplesWritten,
                    inAudio + offset,
                    chunk * sizeof(float));

        samplesWritten  += chunk;
        offset          += chunk;
        remaining       -= chunk;
        std::cout << "Trnascriber queued " << samplesWritten << " of buff " << bufferLenSamples << std::endl;

        if (samplesWritten >= bufferLenSamples) // time to send the buffer to the model then switch to the other buffer 
        {
            {
                std::lock_guard<std::mutex> sl(statusMutex);
                if (status == transcribing){
                    std::cout << "Transcriber: warning: I am about to switch buffers and request model process but transcription is not done yet. SO I'm ignoring the rest of the audio you sent " << std::endl;
                    // break; // no point doing any more 
                }        
                if (status == notEnoughAudio){
                    std::cout << "transcriber triggering a transcribe " << std::endl;
                    status = readyToTranscribe;
                    
                    oneJobRequested = true;
                }

                currentReadBuffer  = currentWriteBuffer;
                currentWriteBuffer = (currentWriteBuffer == bufferA) ? bufferB : bufferA;
                samplesWritten     = 0;
            }
            statusCV.notify_one();
        }
    }
}

void Transcriber::threadLoop()
{
    std::unique_lock<std::mutex> ul(statusMutex);
    while (keepRunning)
    {
        statusCV.wait_for(ul, std::chrono::milliseconds(10));
        if (!keepRunning) break;

        if (status == readyToTranscribe && currentReadBuffer != nullptr)
        {
            status = transcribing;
            float* toProcess = currentReadBuffer;
            ul.unlock();
            std::cout << "transcriber running model " << std::endl;
            runModel(toProcess);

            ul.lock();
            status = notEnoughAudio;// transcription done, waiting for more audio
        }
    }
}

void Transcriber::runModel(float* readBuffer)
{
    using clock = std::chrono::high_resolution_clock;
    auto startTime = clock::now();

    mBasicPitch.reset();
    mBasicPitch.setParameters(noteSensitivity,
                              splitSensitivity,
                              minNoteDurationMs);


    // 
    // AudioUtils::resampleBuffer
    std::cout << "transcriber sending " << bufferLenSamples << " to the NN " << std::endl; 
    mBasicPitch.transcribeToMIDI(readBuffer, bufferLenSamples);

    // gather the events
    const auto& events = mBasicPitch.getNoteEvents();

    // now emit JUCE MIDI messages
    juce::MidiBuffer localMidi;
    for (auto& ev : events)
    {
        std::cout << "Raw note: " <<ev.pitch << "[" << ev.startTime << " -> " << ev.endTime << "]" << std::endl;

        int note    = static_cast<int>(ev.pitch);
        float amp   = ev.amplitude; 
        bool wasHeld = noteHeld[ev.pitch];
        bool isHeld  = (ev.endTime > 0.98 * bufferLenSecs);

        int startSample = static_cast<int>(ev.startTime * bufferLenSamples);
        int endSample   = static_cast<int>(ev.endTime   * bufferLenSamples);

        uint8_t velocity = static_cast<uint8_t>(std::clamp(amp, 0.0f, 1.0f) * 127.0f);

        // NOTE ON - new note detected
        if (!wasHeld)
        {
            std::cout << "New note on "<< note << " start " << ev.startTime << "f: "<< startSample << "ef: " << endSample << std::endl;
            localMidi.addEvent(
                juce::MidiMessage::noteOn(1, note, velocity),
                startSample);
            // std::cout << "After note on add, local midi has "  << localMidi.getNumEvents() << std::endl;
        }

        // NOTE OFF - held note was released in this buffer
        // or note started and ended in this buffer 
        if ((wasHeld && !isHeld) || (!wasHeld && !isHeld))
        {
            std::cout << "Note off: " << note << " at " << ev.endTime <<  " wasHeld "<< wasHeld << " isHeld " << isHeld << std::endl;

            // we’re now releasing
            localMidi.addEvent(
                juce::MidiMessage::noteOff(1, note),
                endSample);
            // std::cout << "After note off add, local midi has "  << localMidi.getNumEvents() << std::endl;
        }

        // if it’s still held at buffer-end, keep the hold flag
        noteHeld[note] = isHeld;
    }

    // stash them under lock
    {
        std::lock_guard<std::mutex> ml(midiMutex);
        
        pendingMidi.addEvents(localMidi, localMidi.getFirstEventTime(), localMidi.getLastEventTime(), 0);
        // std::cout << "receive MIDI from model: Pending midi has " << pendingMidi.getNumEvents() << " local has " << localMidi.getNumEvents() << std::endl;

    }
}

bool Transcriber::hasMidi()
{
    std::lock_guard<std::mutex> ml(midiMutex);
    if (pendingMidi.getNumEvents() > 0){
        return true; 
    }
    else{
        return false; 
    }
}


void Transcriber::collectMidi(juce::MidiBuffer& outputBuffer)
{
    std::lock_guard<std::mutex> ml(midiMutex);
    // std::cout << "collect MIDI Pending midi has " << pendingMidi.getNumEvents() << std::endl;
    outputBuffer.swapWith(pendingMidi);
    pendingMidi.clear();  // ready for next round
}
