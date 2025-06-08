// Transcriber.cpp
#include "Transcriber.h"
#include <cstring>    // for std::memcpy
#include <algorithm>
#include <chrono>
#include <iostream>

Transcriber::Transcriber()
{
    
    resetBuffers(1000);  // default 1000ms as need at least one second for the model I think
    // start background thread
    workerThread = std::thread(&Transcriber::threadLoop, this);
}

Transcriber::~Transcriber()
{
    // signal thread to exit
    keepRunning = false;
    statusCV.notify_one();
    if (workerThread.joinable())
        workerThread.join();

    delete[] bufferA;
    delete[] bufferB;
}

void Transcriber::resetBuffers(int bufLenInMs)
{
    int newLen = (BASIC_PITCH_SAMPLE_RATE * bufLenInMs) / 1000;
    assert(newLen == BASIC_PITCH_SAMPLE_RATE);

    bufferLenSamples = std::max(newLen, 1);

    delete[] bufferA;
    delete[] bufferB;
    bufferA = new float[bufferLenSamples];
    bufferB = new float[bufferLenSamples];

    currentWriteBuffer = bufferA;
    currentReadBuffer  = nullptr;
    samplesWritten     = 0;

    {
        std::lock_guard<std::mutex> lock(statusMutex);
        status = notEnoughAudio;
    }
}

void Transcriber::storeAudio(float* inAudio, int numSamples)
{
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
                std::lock_guard<std::mutex> lock(statusMutex);
                // first time flip → ready
                if (status == notEnoughAudio)
                    status = readyToTranscribe;

                // latch the buffer to read, flip write buffer
                currentReadBuffer      = currentWriteBuffer;
                currentWriteBuffer     = (currentWriteBuffer == bufferA) ? bufferB : bufferA;
                samplesWritten         = 0;
            }
            statusCV.notify_one();
        }
    }
}

void Transcriber::threadLoop()
{
    std::unique_lock<std::mutex> lock(statusMutex);
    while (keepRunning)
    {
        // wake up every 10ms, or early if notified
        statusCV.wait_for(lock, std::chrono::milliseconds(10));

        if (!keepRunning) break;

        if (status == readyToTranscribe && currentReadBuffer != nullptr)
        {
            status = transcribing;
            // capture read pointer & release lock while we process
            float* toProcess = currentReadBuffer;
            lock.unlock();

            runModel(toProcess);

            lock.lock();
            // after processing, go back to notEnoughAudio so we wait for next fill
            status = notEnoughAudio;
        }
    }
}

// void Transcriber::runModel(float* readBuffer)
// {
//     // reset state
//     mBasicPitch.reset();
//     mBasicPitch.setParameters(noteSensitivity,
//                               splitSensitivity,
//                               minNoteDurationMs);

//     // perform transcription
//     mBasicPitch.transcribeToMIDI(readBuffer, bufferLenSamples);

//     // retrieve and print events
//     const auto& events = mBasicPitch.getNoteEvents();
//     std::cout << "Completed transcription " << std::endl;
//     for (const auto& ev : events)
//     {
//         std::cout << "Note: pitch=" << ev.pitch
//                   << " start=" << ev.startTime
//                   << " end="   << ev.endTime
//                   << "\n";
//     }
// }


#include <chrono>
#include <iostream>

void Transcriber::runModel(float* readBuffer)
{
    using clock = std::chrono::high_resolution_clock;

    // mark start
    auto t0 = clock::now();

    // reset state
    mBasicPitch.reset();
    mBasicPitch.setParameters(noteSensitivity,
                              splitSensitivity,
                              minNoteDurationMs);

    // perform transcription
    mBasicPitch.transcribeToMIDI(readBuffer, bufferLenSamples);

    // mark end
    auto t1 = clock::now();

    // compute elapsed in milliseconds
    double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // compute buffer duration in ms
    double bufferDurationMs = (static_cast<double>(bufferLenSamples) 
                                / BASIC_PITCH_SAMPLE_RATE) * 1000.0;

    // report
    std::cout << "Completed transcription in " 
              << elapsedMs << " ms (buffer length: " 
              << bufferDurationMs << " ms) – ";

    if (elapsedMs <= bufferDurationMs)
        std::cout << "real-time or faster.\n";
    else
        std::cout << "slower than real-time.\n";

    // retrieve and print events
    const auto& events = mBasicPitch.getNoteEvents();
    for (const auto& ev : events)
    {
        std::cout << "Note: pitch=" << ev.pitch
                  << " start=" << ev.startTime
                  << " end="   << ev.endTime
                  << "\n";
    }
}
