// Transcriber.h
#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include "BasicPitch.h"

class Transcriber
{
public:
    Transcriber();
    ~Transcriber();

    /** set transcription buffer length in ms */
    void resetBuffers(int bufLenInMs);

    /** send the transcriber some audio */
    void storeAudio(float* inAudio, int numSamples);

    /** query whether we’ve filled our first buffer yet */
    bool isReadyToTranscribe() const
    {
        return status == readyToTranscribe;
    }

    /** adjust pitch-tracker parameters on the fly */
    void setNoteSensitivity(float s)    { noteSensitivity = s; }
    void setSplitSensitivity(float s)   { splitSensitivity = s; }
    void setMinNoteDuration(float ms)   { minNoteDurationMs = ms; }

private:
    enum TranscriberStatus { notEnoughAudio, readyToTranscribe, transcribing };

    void runModel(float* readBuffer);

    BasicPitch   mBasicPitch;

    float*       bufferA      = nullptr;
    float*       bufferB      = nullptr;
    float*       currentWriteBuffer = nullptr;
    float*       currentReadBuffer  = nullptr;

    TranscriberStatus status = notEnoughAudio;

    int          bufferLenSamples = 0;  
    int          samplesWritten   = 0;  

    // parameters for the pitch tracker
    float        noteSensitivity    = 0.5f;
    float        splitSensitivity   = 0.5f;
    float        minNoteDurationMs  = 50.0f;

    // threading
    std::thread             workerThread;
    std::mutex              statusMutex;
    std::condition_variable statusCV;
    std::atomic<bool>       keepRunning { true };

    void threadLoop();
};
