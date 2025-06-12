/**
 * Written mostly by a human with hands  on a keyboard - now that's refreshing
 */
#include <JuceHeader.h>


#include "../plugin/Transcriber.h"
#include "../lib/DSP/Resampler.h"
#include "tinywav/myk_tiny.h"
#include <filesystem>

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
            transcriber.storeAudio(audio, 10, 22050);
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
            // std::string filename("../../../test_data/output_midis/frame_1s_mono_cross_frame_piano.wav");
            std::string filename("../../../white_noise.wav");
            
            assert(std::filesystem::exists(filename) == true);
            std::cout << "Reading the wav file " << filename << std::endl;
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
                std::cout << i << "sent " << blockSize << " got " << written << std::endl;
                outPos += written; 
                
            }
            // // void myk_tiny::saveWav( std::vector<float>& buffer, const int channels, const int sampleRate, const std::string& filename){
            std::string outfile("test_22050.wav");
            myk_tiny::saveWav(outputAudio, 1, 22050, outfile);
            size_t startSamples = inputAudio.size();
            // expected no. resampled samples is original no. scaled by sample rate ration
            double outSamples = static_cast<double>(startSamples) * (22050.0 / 44100.0);

            std::vector<float> resampAudio = myk_tiny::loadWav(outfile);
            std::cout << "orig len "<< inputAudio.size() << "resampled len " << resampAudio.size() << std::endl;         

            expect(static_cast<double>(resampAudio.size()) == outSamples);
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
