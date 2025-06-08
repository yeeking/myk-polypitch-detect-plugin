
// Transcriber.h
#pragma once

#include <JuceHeader.h>
#include "BasicPitch.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "AudioUtils.h"

class Transcriber
{
public:
    Transcriber();
    ~Transcriber();

    void resetBuffers(int bufLenInMs);
    /** store the sent audio. sampleRate should be == BASIC_PITCH_SAMPLE_RATE
     * otherwise an assertion will cause a crash
     */
    void storeAudio(const float* inAudio, int numSamples, double sampleRate);

    bool isReadyToTranscribe() const { return status == readyToTranscribe; }

    void setNoteSensitivity(float s)   { noteSensitivity   = s; }
    void setSplitSensitivity(float s)  { splitSensitivity  = s; }
    void setMinNoteDuration(float ms)  { minNoteDurationMs = ms; }

    /** Called from your AudioProcessor::processBlock */
    void collectMidi(juce::MidiBuffer& outputBuffer);

private:
    enum TranscriberStatus { notEnoughAudio, readyToTranscribe, transcribing };
    void        runModel(float* readBuffer);
    void        threadLoop();

    BasicPitch  mBasicPitch;

    float*      bufferA            = nullptr;
    float*      bufferB            = nullptr;
    float*      currentWriteBuffer = nullptr;
    float*      currentReadBuffer  = nullptr;

    bool        noteHeld[127]      = { false };

    TranscriberStatus status       = notEnoughAudio;
    int      bufferLenSamples      = 0;
    int      samplesWritten        = 0;

    float    noteSensitivity       = 0.7f;
    float    splitSensitivity      = 0.5f;
    float    minNoteDurationMs     = 125.0f;

    std::thread              workerThread;
    std::mutex               statusMutex, midiMutex;
    std::condition_variable  statusCV;
    std::atomic<bool>        keepRunning { true };

    // This is where we queue up messages from runModel()
    juce::MidiBuffer         pendingMidi;
};
