

// Transcriber.cpp
#include "Transcriber.h"
#include <chrono>
#include <algorithm>
#include <cstring>


Transcriber::Transcriber()
{
    resetBuffers(1000);
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

void Transcriber::resetBuffers(int bufLenInMs)
{
    int newLen = (BASIC_PITCH_SAMPLE_RATE * bufLenInMs) / 1000;
    bufferLenSamples = std::max(newLen, 1);

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

void Transcriber::storeAudio(float* inAudio, int numSamples, double sampleRate)
{
    assert(sampleRate == BASIC_PITCH_SAMPLE_RATE);
    
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

        if (samplesWritten >= bufferLenSamples)
        {
            {
                std::lock_guard<std::mutex> sl(statusMutex);

                if (status == notEnoughAudio)
                    status = readyToTranscribe;

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

    mBasicPitch.transcribeToMIDI(readBuffer, bufferLenSamples);

    // gather the events
    const auto& events = mBasicPitch.getNoteEvents();

    // now emit JUCE MIDI messages
    juce::MidiBuffer localMidi;
    for (auto& ev : events)
    {
        int note    = static_cast<int>(ev.pitch);
        float amp   = ev.amplitude;          // in [0,1]
        bool wasHeld = noteHeld[note];
        bool isHeld  = (ev.endTime > 0.98f);

        int startSample = static_cast<int>(ev.startTime * bufferLenSamples);
        int endSample   = static_cast<int>(ev.endTime   * bufferLenSamples);

        uint8_t velocity = static_cast<uint8_t>(std::clamp(amp, 0.0f, 1.0f) * 127.0f);

        // NOTE ON - new note detected
        if (!wasHeld)
        {
            std::cout << "New note on "<< note << " start " << ev.startTime << std::endl;
            localMidi.addEvent(
                juce::MidiMessage::noteOn(1, note, velocity),
                startSample);
        }

        // NOTE OFF - held note was released in this buffer
        // or note started and ended in this buffer 
        if ((wasHeld && !isHeld) || (!wasHeld && !isHeld))
        {
            std::cout << "Note off: " << note << " wasHeld "<< wasHeld << " isHeld " << isHeld << std::endl;

            // we’re now releasing
            localMidi.addEvent(
                juce::MidiMessage::noteOff(1, note),
                endSample);
        }

        // if it’s still held at buffer-end, keep the hold flag
        noteHeld[note] = isHeld;
    }

    // stash them under lock
    {
        std::lock_guard<std::mutex> ml(midiMutex);
        pendingMidi.addEvents(localMidi, 0, bufferLenSamples, 0);
    }
}

void Transcriber::collectMidi(juce::MidiBuffer& outputBuffer)
{
    std::lock_guard<std::mutex> ml(midiMutex);
    outputBuffer.swapWith(pendingMidi);
    pendingMidi.clear();  // ready for next round
}
