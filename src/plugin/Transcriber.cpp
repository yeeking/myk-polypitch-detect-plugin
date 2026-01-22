

// Transcriber.cpp
#include "Transcriber.h"
#include <chrono>
#include <algorithm>
#include <cmath>
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
    resetBuffersSamples(bufferLenSamples);
}


void Transcriber::resetBuffersSamples(int _bufLenInSamples)
{
    bufferLenSamples = std::max(_bufLenInSamples, 1);
    bufferLenSecs = (bufferLenSamples / BASIC_PITCH_SAMPLE_RATE);
    if (captureLenSamples <= 0 || captureLenSamples > bufferLenSamples) {
        captureLenSamples = bufferLenSamples;
    }
    captureLenSecs = (captureLenSamples / BASIC_PITCH_SAMPLE_RATE);
    silenceLenSamples = bufferLenSamples - captureLenSamples;
    silenceLenSecs = (silenceLenSamples / BASIC_PITCH_SAMPLE_RATE);
    // std::cout << "Set buf len secs to " << bufferLenSecs << std::endl;

    delete[] bufferA; delete[] bufferB;
    bufferA = new float[bufferLenSamples];
    bufferB = new float[bufferLenSamples];

    currentWriteBuffer = bufferA;
    currentReadBuffer  = nullptr;
    samplesWritten     = 0;

    std::fill(std::begin(noteHeld), std::end(noteHeld), false);
    std::fill(std::begin(noteSeen), std::end(noteSeen), false);
    std::fill(std::begin(noteLastSeenTime), std::end(noteLastSeenTime), 0.0);
    if (bufferA) std::fill_n(bufferA, bufferLenSamples, 0.0f);
    if (bufferB) std::fill_n(bufferB, bufferLenSamples, 0.0f);
    processedAudioSecs = 0.0;

    {
        std::lock_guard<std::mutex> sl(statusMutex);
        status = collectingAudio;
    }
}

void Transcriber::setLatencySeconds(double latencySeconds)
{
    const int newCaptureLenSamples =
        std::max(1, static_cast<int>(std::round(latencySeconds * BASIC_PITCH_SAMPLE_RATE)));
    captureLenSamples = std::min(newCaptureLenSamples, bufferLenSamples);
    captureLenSecs = (captureLenSamples / BASIC_PITCH_SAMPLE_RATE);
    silenceLenSamples = bufferLenSamples - captureLenSamples;
    silenceLenSecs = (silenceLenSamples / BASIC_PITCH_SAMPLE_RATE);

    samplesWritten = 0;
    currentWriteBuffer = bufferA;
    currentReadBuffer = nullptr;
    if (bufferA) std::fill_n(bufferA, bufferLenSamples, 0.0f);
    if (bufferB) std::fill_n(bufferB, bufferLenSamples, 0.0f);

    std::fill(std::begin(noteHeld), std::end(noteHeld), false);
    std::fill(std::begin(noteSeen), std::end(noteSeen), false);
    std::fill(std::begin(noteLastSeenTime), std::end(noteLastSeenTime), 0.0);
    processedAudioSecs = 0.0;

    {
        
        std::lock_guard<std::mutex> sl(statusMutex);
        status = collectingAudio;
    }
}

void Transcriber::queueAudioForTranscription(const float* inAudio, int numSamples, double sampleRate)
{
    assert(sampleRate == BASIC_PITCH_SAMPLE_RATE);

    if (numSamples > this->bufferLenSamples){
        this->resetBuffersSamples(numSamples);    
        // std::cout << "You sent me " << numSamples << " but my buffer is " << this->bufferLenSamples <<" samples. You are supposed to send smaller blocks from a realtime audio thread. " << std::endl;
    }
    assert(numSamples <= this->bufferLenSamples);// refuse to accept very long audio chunks
    
    // i think here we should check for buffersfull status
    {
        std::lock_guard<std::mutex> sl(statusMutex);
        if (status == bothBuffersFullPleaseWait){
            // std::cout << "Transcriber.cpp: warning - waiting for transcription " << std::endl;
            return; // trashy eh?
        }
    }
    

    int remaining = numSamples, offset = 0;
    while (remaining > 0)
    {
        if (samplesWritten == 0) {
            std::fill_n(currentWriteBuffer, bufferLenSamples, 0.0f);
        }

        int spaceLeft = captureLenSamples - samplesWritten;
        int chunk     = std::min(spaceLeft, remaining);

        std::memcpy(currentWriteBuffer + silenceLenSamples + samplesWritten,
                    inAudio + offset,
                    chunk * sizeof(float));

        samplesWritten  += chunk;
        offset          += chunk;
        remaining       -= chunk;
        // std::cout << "Trnascriber queued " << samplesWritten << " of buff " << bufferLenSamples << std::endl;

        if (samplesWritten >= captureLenSamples) // time to send the buffer to the model then switch to the other buffer 
        {
            {
                std::lock_guard<std::mutex> sl(statusMutex);
                if (status == collectingAudioAndTranscribing || 
                    status == bothBuffersFullPleaseWait){
                        // this means transcription is going on 
                        // and we have two full buffers so no point in doing anything 
                        // until transcription job clears
                    status = bothBuffersFullPleaseWait;

                    // std::cout << "Transcriber: warning: I am about to switch buffers and request model process but transcription is not done yet. SO I'm ignoring the rest of the audio you sent " << std::endl;
                    // break; // no point doing any more 
                }        
                if (status == collectingAudio){
                    // std::cout << "transcriber triggering a transcribe " << std::endl;
                    status = collectingAudioAndTranscribing;                    
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

        if ((status == collectingAudioAndTranscribing  || status == bothBuffersFullPleaseWait) && currentReadBuffer != nullptr)
        {
            // status = transcribing;
            float* toProcess = currentReadBuffer;
            ul.unlock();
            // std::cout << "transcriber running model " << std::endl;
            runModel(toProcess);

            ul.lock();
            status = collectingAudio;// transcription done, waiting for more audio
        }
    
    }
}

void Transcriber::runModel(float* readBuffer)
{
    // std::cout << "RunModel called" << std::endl;
    using clock = std::chrono::high_resolution_clock;
    auto startTime = clock::now();

    mBasicPitch.reset();
    mBasicPitch.setParameters(noteSensitivity,
                              splitSensitivity,
                              minNoteDurationMs);


    // 
    // AudioUtils::resampleBuffer
    // std::cout << "transcriber sending " << bufferLenSamples << " to the NN " << std::endl; 
    mBasicPitch.transcribeToMIDI(readBuffer, bufferLenSamples);

    // gather the events
    const auto& events = mBasicPitch.getNoteEvents();

    const double silenceSecs = silenceLenSecs;
    const double captureSecs = captureLenSecs;
    const double bufferStartTime = processedAudioSecs;
    const double bufferEndTime = bufferStartTime + captureSecs;
    const double minHoldSecs = std::max(0.0, minNoteDurationMs / 1000.0);

    // reset the noteSeen array and per-note buffers
    double noteStartInBuffer[128];
    double noteEndInBuffer[128];
    float noteAmp[128];
    for (int i=0;i<128;++i){
        noteSeen[i] = false;
        noteStartInBuffer[i] = captureSecs;
        noteEndInBuffer[i] = 0.0;
        noteAmp[i] = 0.0f;
    }
    juce::MidiBuffer localMidi;
    for (auto& ev : events)
    {
        int note    = static_cast<int>(ev.pitch);
        float amp   = ev.amplitude; 

        if (ev.startTime < silenceSecs) {
            continue;
        }

        double adjustedStart = ev.startTime - silenceSecs;
        double adjustedEnd = ev.endTime - silenceSecs;
        if (adjustedEnd <= 0.0) {
            continue;
        }
        if (adjustedStart < 0.0) adjustedStart = 0.0;
        if (adjustedEnd > captureSecs) adjustedEnd = captureSecs;

        noteSeen[note] = true;
        noteStartInBuffer[note] = std::min(noteStartInBuffer[note], adjustedStart);
        noteEndInBuffer[note] = std::max(noteEndInBuffer[note], adjustedEnd);
        noteAmp[note] = std::max(noteAmp[note], amp);
    }

    for (int i=0;i<128;++i){
        if (noteSeen[i]) {
            const double adjustedStart = noteStartInBuffer[i];
            const double adjustedEnd = noteEndInBuffer[i];
            const float clampedAmp = std::max(minNoteVelocity, std::clamp(noteAmp[i], 0.0f, 1.0f));
            const uint8_t velocity =
                static_cast<uint8_t>(clampedAmp * 127.0f);
            const int startSample =
                static_cast<int>(std::round(adjustedStart * BASIC_PITCH_SAMPLE_RATE));
            const int endSample =
                static_cast<int>(std::round(adjustedEnd * BASIC_PITCH_SAMPLE_RATE));

            if (!noteHeld[i]) {
                std::cout << "Note on " << i << " start " << adjustedStart
                          << " end " << adjustedEnd << " vel " << static_cast<int>(velocity)
                          << " startSample " << startSample << " endSample " << endSample << std::endl;
                localMidi.addEvent(
                    juce::MidiMessage::noteOn(1, i, velocity),
                    std::max(0, startSample));
                noteHeld[i] = true;
            }

            noteLastSeenTime[i] = bufferStartTime + adjustedEnd;
            continue;
        }

        if (noteHeld[i]) {
            const double timeSinceSeen = bufferEndTime - noteLastSeenTime[i];
            if (timeSinceSeen >= minHoldSecs) {
                const double releaseTime = noteLastSeenTime[i] + minHoldSecs;
                if (releaseTime <= bufferEndTime) {
                    int releaseSample = static_cast<int>(
                        std::round((releaseTime - bufferStartTime) * BASIC_PITCH_SAMPLE_RATE));
                    releaseSample = std::clamp(releaseSample, 0, captureLenSamples);
                    std::cout << "Note off " << i << " end " << (releaseTime - bufferStartTime)
                              << " heldFor " << timeSinceSeen << std::endl;
                    localMidi.addEvent(
                        juce::MidiMessage::noteOff(1, i),
                        releaseSample);
                    noteHeld[i] = false;
                }
            }
        }
    }

    // stash them under lock
    {
        std::lock_guard<std::mutex> ml(midiMutex);
        
        
        // pendingMidi.addEvents(localMidi, localMidi.getFirstEventTime(), localMidi.getLastEventTime(), 0);
        pendingMidi.addEvents(localMidi, 0, -1,  0);
        
        // std::cout << "receive MIDI from model: Pending midi has " << pendingMidi.getNumEvents() << " local has " << localMidi.getNumEvents() << std::endl;

    }
    {
        // std::cout << "Transcription done" << std::endl;
        // update status 
        std::unique_lock<std::mutex> ul(statusMutex);
        status = collectingAudio;

    }

    processedAudioSecs += captureSecs;
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
    if (pendingMidi.getNumEvents() > 0){
        outputBuffer.swapWith(pendingMidi);
        pendingMidi.clear();  // ready for next round
    }
}

TranscriberStatus Transcriber::getStatus()
{
    std::lock_guard<std::mutex> ml(statusMutex);
    return this->status;
}
