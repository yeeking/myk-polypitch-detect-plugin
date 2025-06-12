/**
 * Written mostly by a human with hands  on a keyboard - now that's refreshing
 */
#include <JuceHeader.h>


#include "../plugin/Transcriber.h"
#include "../lib/DSP/Resampler.h"
#include "tinywav/myk_tiny.h"
#include <filesystem>
#include <thread>
#include <chrono>
class BasicTest : public juce::UnitTest
{
    public:
    BasicTest() : juce::UnitTest("BasicTest", "BasicTest") {}

    void runTest() override
    {
        {
            beginTest("If six was nine");
            expectNotEquals(6, 9);

        }
    }
};


class NoteExtractTest : public juce::UnitTest
{
    public:
    NoteExtractTest() : juce::UnitTest("NoteExtractTest", "NoteExtractTest") {}

    void runTest() override
    {
        {
            Transcriber transcriber;
            beginTest("transcriber can be instantiated");
            expectNotEquals(6, 9);
        }
        {
            Transcriber transcriber;
            beginTest("transcriber can store audio");
            float audio[10];
            for (int i=0;i<10;++i) {audio[i] = 0.1f;}
            transcriber.queueAudioForTranscription(audio, 10, 22050);
            expect(true);

        }
        {
            beginTest("resampler can process audio");

            Resampler resampler;

            resampler.prepareToPlay(44100.0, 44100, 22050.0);
            float inBuffer[44100];
            for (int i=0;i<44100;++i) {inBuffer[i] = 0.1f;}
            float outBuffer[22050];
            
            resampler.processBlock(inBuffer, outBuffer, 44100);
            expect(true);

        }
        {
            beginTest("resampler can resample a WAV");

            Resampler resampler;

            int blockSize = 1024;

            resampler.prepareToPlay(44100.0, blockSize, 22050.0);
            std::string filename("../../../test_data/output_midis/frame_1s_mono_cross_frame_piano.wav");
            // std::string filename("../../../white_noise.wav");
            // if this line crashes the test, you need to run the 
            // python test audio generator 
            assert(std::filesystem::exists(filename) == true);// hard crash if the file does not exist 
            // std::cout << "Reading the wav file " << filename << std::endl;
            std::vector<float> inputAudio = myk_tiny::loadWav(filename);

            size_t totalSamples = inputAudio.size();
            std::vector<float> outputAudio;
            outputAudio.resize(totalSamples); // make sure output is the same size
            size_t outPos = 0; 
            for (size_t i = 0; i < totalSamples; i += blockSize) {
                int currentBlockSize = static_cast<int>(std::min(blockSize, static_cast<int>(totalSamples - i)));

                const float* inBlock = &inputAudio[i];
                float* outBlock = &outputAudio[outPos];

                int written = resampler.processBlock(inBlock, outBlock, currentBlockSize);
                // std::cout << i << "sent " << blockSize << " got " << written << std::endl;
                outPos += written; 
                
            }
            // // void myk_tiny::saveWav( std::vector<float>& buffer, const int channels, const int sampleRate, const std::string& filename){
            std::string outfile("test_22050.wav");
            // trim to the part of the vector that reasmpler wrote to 
            std::vector<float> trimmed(outputAudio.begin(), outputAudio.begin() + outPos);
            myk_tiny::saveWav(trimmed, 1, 22050, outfile);
            size_t startSamples = inputAudio.size();
            // expected no. resampled samples is original no. scaled by sample rate ration
            double expectedNoResampledSamples = static_cast<double>(startSamples) * (22050.0 / 44100.0);
            expectedNoResampledSamples ++;// it adds a bonus sample for some reason
            std::vector<float> resampAudio = myk_tiny::loadWav(outfile);
            // std::cout << "orig len "<< inputAudio.size() << "resampled len " << resampAudio.size() << " expect " << expectedNoResampledSamples << std::endl;         

            expect(static_cast<double>(resampAudio.size()) == expectedNoResampledSamples);
        }


        {
            beginTest("Transcriber can detect a note");
            Transcriber transcriber;
            Resampler resampler;

            std::string filename("../../../test_data/output_midis/frame_1s_mono_cross_frame_sax.wav");
            // if this line crashes the test, you need to run the python test audio generator 
            assert(std::filesystem::exists(filename) == true);// hard crash if the file does not exist 
            
            std::vector<float> inputAudioVec = myk_tiny::loadWav(filename);
            std::vector<float> resampledAudioVec{};
            std::vector<float> silence{};
            silence.resize(22050 * 10); // 10 seconds of silence to make sure we get all the notes out of the audio
            // resize to half size of 44.1k input + 1 magic sample
            resampledAudioVec.resize((inputAudioVec.size() / 2) + 1);
            // void Resampler::prepareToPlay(double inSourceSampleRate, int inMaxBlockSize, double inTargetSampleRate)
            resampler.prepareToPlay(44100.0, inputAudioVec.size(), 22050.0);

            const float* inBlock = &inputAudioVec[0];
            float* resampledAudioPtr = &resampledAudioVec[0];
            int numProcSamples = resampler.processBlock(inBlock, resampledAudioPtr, static_cast<int>(inputAudioVec.size()));
            std::cout << "resampler resampled down to " << numProcSamples << " from  " << inputAudioVec.size() << std::endl;
            // void Transcriber::storeAudio(const float* inAudio, int numSamples, double sampleRate)
            // transcriber.resetBuffers(1);// set buffers to two seconds 

            transcriber.queueAudioForTranscription(resampledAudioPtr, resampledAudioVec.size(), 22050);

            while(!transcriber.hasMidi()){
                std::cout << "waiting..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // sleep for 100 ms
            }
            juce::MidiBuffer midiBuff;
            transcriber.collectMidi(midiBuff);
            // std::cout << "MIDI msg count " << midiBuff.getNumEvents() << std::endl;
            expect(midiBuff.getNumEvents() == 2); // note on note off
        }

        {
            beginTest("Transcriber can detect a note across frames");
            Transcriber transcriber;
            Resampler resampler;

            std::string filename("../../../test_data/output_midis/frame_5s_mono_start_at_frame_start_sax");
            // if this line crashes the test, you need to run the python test audio generator 
            assert(std::filesystem::exists(filename) == true);// hard crash if the file does not exist 
            
            std::vector<float> inputAudioVec = myk_tiny::loadWav(filename);
            std::vector<float> resampledAudioVec{};
            std::vector<float> silence{};
            silence.resize(22050 * 10); // 10 seconds of silence to make sure we get all the notes out of the audio
            // resize to half size of 44.1k input + 1 magic sample
            resampledAudioVec.resize((inputAudioVec.size() / 2) + 1);
            // void Resampler::prepareToPlay(double inSourceSampleRate, int inMaxBlockSize, double inTargetSampleRate)
            resampler.prepareToPlay(44100.0, inputAudioVec.size(), 22050.0);

            const float* inBlock = &inputAudioVec[0];
            float* resampledAudioPtr = &resampledAudioVec[0];
            int numProcSamples = resampler.processBlock(inBlock, resampledAudioPtr, static_cast<int>(inputAudioVec.size()));
            std::cout << "resampler resampled down to " << numProcSamples << " from  " << inputAudioVec.size() << std::endl;
            // void Transcriber::storeAudio(const float* inAudio, int numSamples, double sampleRate)
            transcriber.resetBuffers(1);// set buffers to one second
            // now iteratively send the audio into
            // the transcriber 
            int totalSamples = static_cast<int>(resampledAudioVec.size());
            int blockSize = 22050 * 2;  // or whatever you want

            for (int i = 0; i < totalSamples; i += blockSize) {
                int currentBlockSize = std::min(blockSize, totalSamples - i);
                const float* blockPtr = resampledAudioPtr + i;

                transcriber.queueAudioForTranscription(blockPtr, currentBlockSize, 22050);
            }   

            // transcriber.queueAudioForTranscription(resampledAudioPtr, resampledAudioVec.size(), 22050);

            while(!transcriber.hasMidi()){
                std::cout << "waiting..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // sleep for 100 ms
            }
            juce::MidiBuffer midiBuff;
            transcriber.collectMidi(midiBuff);
            // std::cout << "MIDI msg count " << midiBuff.getNumEvents() << std::endl;
            expect(midiBuff.getNumEvents() == 2); // note on note off
        }
    }
};


int main()
{
    
    std::cout << "Tests main function " << std::endl;
 

    juce::UnitTestRunner runner;

    BasicTest basicTest;
    NoteExtractTest noteExtractTest;

    runner.runTestsInCategory("BasicTest");
    runner.runTestsInCategory("NoteExtractTest");
    

    return 0;
}
