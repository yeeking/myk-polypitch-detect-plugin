/**
 * Written mostly by a human with hands  on a keyboard - now that's refreshing
 */
#include <JuceHeader.h>


#include "../plugin/Transcriber.h"
// #include "tinywav/myk_tiny.h"


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
