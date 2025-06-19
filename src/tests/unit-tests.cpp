#include <JuceHeader.h>

#include "../plugin/Transcriber.h"
#include "../lib/DSP/Resampler.h"
#include <vector>
#include <functional>
#include <cmath>
#include <chrono>
#include <thread>

using namespace juce;

//------------------------------------------------------------------------------
// Convert frequency (Hz) to nearest MIDI note number
//------------------------------------------------------------------------------
static int freqToMidiNote(double freq)
{
    return int(std::round(69 + 12.0 * std::log2(freq / 440.0)));
}

//------------------------------------------------------------------------------
// Wait for MIDI from transcriber, with timeout in seconds
//------------------------------------------------------------------------------
bool waitForMidi(Transcriber& trans, double timeoutSecs = 2.0)
{
    auto start = std::chrono::steady_clock::now();
    while (! trans.hasMidi())
    {
        if (std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() > timeoutSecs)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

//------------------------------------------------------------------------------
// Append src vector into dest
//------------------------------------------------------------------------------
static void appendVector(std::vector<float>& dest, const std::vector<float>& src)
{
    dest.insert(dest.end(), src.begin(), src.end());
}

//------------------------------------------------------------------------------
// Generate band-limited sawtooth wave for signalSecs, then silence up to totalSecs
//------------------------------------------------------------------------------
std::vector<float> makeSaw(double freq,
                           double signalSecs,
                           double totalSecs,
                           double sampleRate,
                           float amplitude = 0.5f)
{
    int signalSamples = int(signalSecs * sampleRate);
    int totalSamples  = int(totalSecs  * sampleRate);
    std::cout << "makeSaw sr " << sampleRate << " len " << signalSecs << " total " << totalSamples << " sig " << signalSamples << std::endl;
    std::vector<float> v(totalSamples, 0.0f);

    double phase = 0.0;
    double phaseInc = freq / sampleRate;
    // for (int i = 0; i < signalSamples && i < totalSamples; ++i)
    for (int i=0; i<signalSamples; ++ i)
    {
        v[i] = float((2.0 * (phase - std::floor(phase + 0.5))) * amplitude);
        phase += phaseInc;
        if (phase >= 1.0)
            {phase -= std::floor(phase);}

    }
    return v;
}

//------------------------------------------------------------------------------
// Generate pulse wave for signalSecs, then silence up to totalSecs
//------------------------------------------------------------------------------
std::vector<float> makePulse(double freq,
                             double signalSecs,
                             double totalSecs,
                             double sampleRate,
                             float dutyCycle = 0.5f,
                             float amplitude = 0.5f)
{
    int signalSamples = int(signalSecs * sampleRate);
    int totalSamples  = int(totalSecs  * sampleRate);
    std::vector<float> v(totalSamples, 0.0f);

    double phase = 0.0;
    double phaseInc = freq / sampleRate;
    for (int i = 0; i < signalSamples && i < totalSamples; ++i)
    {
        v[i] = (phase < dutyCycle ? amplitude : -amplitude);
        phase += phaseInc;
        if (phase >= 1.0)
            phase -= std::floor(phase);
    }
    return v;
}

//------------------------------------------------------------------------------
// Unit test suite for Transcriber
//------------------------------------------------------------------------------
class TranscriberTest : public UnitTest
{
public:
    TranscriberTest() : UnitTest("TranscriberTest", "Audio to MIDI") {}

    struct Case
    {
        String name;
        std::function<std::vector<float>()> makeAudio;
        int bufferSize;               // chunk size to send per call
        int bufferLenSamples;         // transcriber buffer length in samples
        std::vector<int> expectedOn;  // expected Note On MIDI numbers
        std::vector<int> expectedOff; // expected Note Off MIDI numbers
    };

    void runTest() override
    {
        std::cout << "Running Transcriber unit tests" << std::endl;
        const double sr = BASIC_PITCH_SAMPLE_RATE;

        // Pre-calculate MIDI note numbers
        const int C4 = freqToMidiNote(261.63);
        const int E4 = freqToMidiNote(329.63);
        const int G4 = freqToMidiNote(392.00);

        std::vector<Case> cases =
        {
            {
                "Single C4 (saw) 0.5s",
                [&]{ double totalSecs = 4096.0 / sr;
                     return makeSaw(261.63, 0.1, totalSecs, sr, 0.4f); },
                512, 4096,
                { C4 }, { C4 }
            },

            {
                "Long E4 across 3 buffers",
                [&]{ double totalSecs = (int(0.4 * sr)) / sr;
                     std::vector<float> out;
                     for (int i = 0; i < 3; ++i)
                         appendVector(out,
                                      makeSaw(329.63, 0.4, totalSecs, sr, 0.4f));
                     return out;
                },
                441, int(0.4 * sr),
                { E4 }, { E4 }
            },

            {
                "Major triad chord (saw mix)",
                [&]{ double totalSecs = 4096.0 / sr;
                     auto c = makeSaw(261.63, 0.8, totalSecs, sr, 0.3f);
                     auto e = makeSaw(329.63, 0.8, totalSecs, sr, 0.3f);
                     auto g = makeSaw(392.00, 0.8, totalSecs, sr, 0.3f);
                     std::vector<float> sum(c.size());
                     for (size_t i = 0; i < sum.size(); ++i)
                         sum[i] = c[i] + e[i] + g[i];
                     return sum;
                },
                1024, 4096,
                { C4, E4, G4 },
                { C4, E4, G4 }
            },

            {
                "Staccato C4 pulse bursts",
                [&]{ double totalSecs = 4096.0 / sr;
                     std::vector<float> out;
                     for (int i = 0; i < 5; ++i)
                     {
                         appendVector(out,
                                      makePulse(261.63, 0.1, totalSecs, sr, 0.5f, 0.2f));
                         out.insert(out.end(), int(0.1 * sr), 0.0f);
                     }
                     return out;
                },
                512, 4096,
                std::vector<int>(5, C4),
                std::vector<int>(5, C4)
            },

            {
                "Silence only",
                [&]{ return std::vector<float>(int((4096.0/sr) * sr), 0.0f); },
                256, 4096,
                {}, {}
            }
        };

        for (auto& tc : cases)
        {
            beginTest(tc.name);
            Transcriber trans;
            trans.resetBuffersSamples(tc.bufferLenSamples);

            // Feed audio in blocks
            size_t pos = 0;
            auto audio = tc.makeAudio();
            while (pos < audio.size())
            {
                int chunk = int(std::min<size_t>(tc.bufferSize, audio.size() - pos));
                trans.queueAudioForTranscription(&audio[pos], chunk, sr);
                while (trans.getStatus() == bothBuffersFullPleaseWait || 
                       trans.getStatus() == collectingAudioAndTranscribing)
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                pos += chunk;
            }

            bool got = waitForMidi(trans);
            expect(got, "timeout waiting for MIDI");

            MidiBuffer midi;
            trans.collectMidi(midi);
            std::cout << "Collected midi events. " <<  midi.getNumEvents() << std::endl;
            expect(midi.getNumEvents() == 2);

            // Gather actual On/Off note numbers
            std::vector<int> actualOn, actualOff;
            for (auto metadata : midi)
            { 
                std::cout << "test got midi " << metadata.getMessage().getDescription() << std::endl;
                const auto& msg = metadata.getMessage();
                if (msg.isNoteOn())  actualOn.push_back(msg.getNoteNumber());
                if (msg.isNoteOff()) actualOff.push_back(msg.getNoteNumber());
            }

            // Compare size
            expectEquals((int)actualOn.size(),  (int)tc.expectedOn.size());
            expectEquals((int)actualOff.size(), (int)tc.expectedOff.size());

            // Verify each expected note
            for (size_t i = 0; i < tc.expectedOn.size(); ++i)
                expectEquals(actualOn[i], tc.expectedOn[i]);
            for (size_t i = 0; i < tc.expectedOff.size(); ++i)
                expectEquals(actualOff[i], tc.expectedOff[i]);
        }
    }
};

//==============================================================================
int main()
{
    std::cout << "Running Transcriber unit tests..." << std::endl;
    UnitTestRunner runner;
    TranscriberTest transcriberTest; // register our tests
    runner.runTestsInCategory("Audio to MIDI");
    return 0;
}
