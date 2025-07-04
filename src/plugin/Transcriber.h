
// Transcriber.h
#pragma once

#include <JuceHeader.h>
#include "BasicPitch.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "AudioUtils.h"

enum TranscriberStatus { collectingAudio, collectingAudioAndTranscribing, bothBuffersFullPleaseWait};


class Transcriber
{
public:
    Transcriber();
    ~Transcriber();
    /** reset the buffers to the sent number of ms at the BASIC_PITCH_SAMPLE_RATE  */
    void resetBuffers(double bufLenInSecs);
    /** reset the buffers to a specific number of samples  */
    void resetBuffersSamples(int bufLenInSamples);
    
    /** store the sent audio. sampleRate should be == BASIC_PITCH_SAMPLE_RATE
     * otherwise an assertion will cause a crash. Transcription is carried out automatically in a background thread
     */
    void queueAudioForTranscription(const float* inAudio, int numSamples, double sampleRate);
 
    void setNoteSensitivity(float s)   { noteSensitivity   = s; }
    void setSplitSensitivity(float s)  { splitSensitivity  = s; }
    void setMinNoteDuration(float ms)  { minNoteDurationMs = ms; }
    void setNoteHoldSensitivity(float s) { noteHoldSensitivity = s; }
    /** call this to ask if the transcriber has any MIDI to give you, since transcriptions happen in the background */
    bool hasMidi();
    /** if any midi has been detected and stored in the transcriber thread
     * put that midi into the sent buffer (clearing what was there), and clear internal pending midi buffer
     */
    void collectMidi(juce::MidiBuffer& outputBuffer);
    TranscriberStatus getStatus();
private:
    void        runModel(float* readBuffer);
    void        threadLoop();

    BasicPitch  mBasicPitch;

    float*      bufferA            = nullptr;
    float*      bufferB            = nullptr;
    float*      currentWriteBuffer = nullptr;
    float*      currentReadBuffer  = nullptr;

    bool        noteHeld[128]      = { false };
    bool        noteSeen[128]      = { false };
    

    TranscriberStatus status       = collectingAudio;
    int      bufferLenSamples      = 0;
    double   bufferLenSecs          = 0;
    int      samplesWritten        = 0;

    float    noteSensitivity       = 0.7f;
    float    splitSensitivity      = 0.5f;
    float    minNoteDurationMs     = 125.0f;
    float   noteHoldSensitivity   = 0.95f;

    std::thread              workerThread;
    std::mutex               statusMutex, midiMutex;
    std::condition_variable  statusCV;
    std::atomic<bool>        keepRunning { true };

    // This is where we queue up messages from runModel()
    juce::MidiBuffer         pendingMidi;
};
