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
// Wait for MIDI from transcriber, with timeout in seconds
//------------------------------------------------------------------------------
bool waitForMidi(Transcriber& trans, double timeoutSecs = 2.0)
{
    auto start = std::chrono::steady_clock::now();
    while (!trans.hasMidi())
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
// Generate band-limited sawtooth wave
//------------------------------------------------------------------------------
std::vector<float> makeSaw(double freq, double durationSecs, double sampleRate, float amplitude = 0.5f)
{
    int totalSamples = int(durationSecs * sampleRate);
    std::vector<float> v(totalSamples);
    double phase = 0.0;
    double phaseInc = freq / sampleRate;

    for (int i = 0; i < totalSamples; ++i)
    {
        v[i] = float((2.0 * (phase - std::floor(phase + 0.5))) * amplitude);
        phase += phaseInc;
        if (phase >= 1.0) phase -= std::floor(phase);
    }

    return v;
}

//------------------------------------------------------------------------------
// Generate pulse (square) wave with duty cycle
//------------------------------------------------------------------------------
std::vector<float> makePulse(double freq, double durationSecs, double sampleRate, float dutyCycle = 0.5f, float amplitude = 0.5f)
{
    int totalSamples = int(durationSecs * sampleRate);
    std::vector<float> v(totalSamples);
    double phase = 0.0;
    double phaseInc = freq / sampleRate;

    for (int i = 0; i < totalSamples; ++i)
    {
        v[i] = (phase < dutyCycle ? amplitude : -amplitude);
        phase += phaseInc;
        if (phase >= 1.0) phase -= std::floor(phase);
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
        int bufferSize;
        int expectedNotesOn;
    };

    void runTest() override
    {
        std::cout << "Running tests" << std::endl;
        const double sr = BASIC_PITCH_SAMPLE_RATE;

        std::vector<Case> cases =
        {
            { "Single C4 (saw)",
              [&]{ return makeSaw(261.63, 0.5, sr, 0.4f); },
              512,
              1
            },

            { "Long E4 across 3 buffers (1.2s total)",
              [&]{
                  std::vector<float> out;
                  for (int i = 0; i < 3; ++i)
                      appendVector(out, makeSaw(329.63, 0.4, sr, 0.4f));
                  return out;
              },
              441,
              1
            },

            { "Major triad chord (saw mix)",
              [&]{
                  auto c = makeSaw(261.63, 0.8, sr, 0.3f);
                  auto e = makeSaw(329.63, 0.8, sr, 0.3f);
                  auto g = makeSaw(392.00, 0.8, sr, 0.3f);
                  std::vector<float> sum(c.size());
                  for (size_t i = 0; i < sum.size(); ++i)
                      sum[i] = c[i] + e[i] + g[i];
                  return sum;
              },
              1024,
              3
            },

            { "Staccato C4 pulse bursts",
              [&]{
                  std::vector<float> out;
                  for (int i = 0; i < 5; ++i)
                  {
                      appendVector(out, makePulse(261.63, 0.1, sr, 0.5f, 0.2f));
                      out.insert(out.end(), int(0.1 * sr), 0.0f);
                  }
                  return out;
              },
              512,
              5
            },

            { "Silence only",
              [&]{ return std::vector<float>(int(sr * 0.5), 0.0f); },
              256,
              0
            }
        };

        for (auto& tc : cases)
        {
            std::cout << "Running test " << tc.name << std::endl;
            beginTest(tc.name);
            Transcriber trans;
            // trans.resetBuffersSamples(tc.bufferSize * 4);
            trans.resetBuffersSamples(4096);
            

            // Feed audio in blocks
            size_t pos = 0;
            auto audio = tc.makeAudio();
            while (pos < audio.size())
            {
                auto chunk = std::min<size_t>(tc.bufferSize, audio.size() - pos);
                trans.queueAudioForTranscription(&audio[pos], int(chunk), sr);
                pos += chunk;
            }

            bool got = waitForMidi(trans);
            expect(got, "timeout waiting for MIDI");

            MidiBuffer midi;
            trans.collectMidi(midi);

            int notesOn = 0;
            MidiBuffer::Iterator it(midi);
            MidiMessage msg;
            int samplePos;
            while (it.getNextEvent(msg, samplePos))
            {
                if (msg.isNoteOn())
                    ++notesOn;
            }

            expectEquals(notesOn, tc.expectedNotesOn);
        }
    }
};

//==============================================================================
int main()
{
    std::cout << "Running Transcriber unit tests...\n";

    UnitTestRunner runner;
    TranscriberTest transcriberTest;
    // BasicTest basicTest;
    // NoteExtractTest noteExtractTest;

    
    runner.runTestsInCategory("Audio to MIDI");
    return 0;
}
