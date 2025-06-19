#include <JuceHeader.h>

#include "../plugin/Transcriber.h"
#include "../lib/DSP/Resampler.h"
#include <vector>
#include <functional>
#include <cmath>
#include <chrono>
#include <thread>
#include <unordered_map>

using namespace juce;


#include <juce_audio_formats/juce_audio_formats.h>

/** 
    Saves raw interleaved or channel-major audio data to a WAV file.

    @param audioData      A flat vector of floats. If interleaved (ch0_s0, ch1_s0, ch0_s1, …),
                          you’ll need to deinterleave into a juce::AudioBuffer first.
    @param numChannels    Number of channels in audioData.
    @param sampleRate     Sample rate (e.g. 22050.0).
    @param outFile        Destination WAV file.
*/
void saveWavFromVector (const std::vector<float>& audioData,
                        int numChannels,
                        double sampleRate,
                        const juce::File& outFile)
{
    // Sanity checks
    jassert (numChannels > 0);
    jassert (sampleRate > 0.0);
    jassert (! audioData.empty());
    const int64 totalSamples = (int64) audioData.size() / numChannels;
    jassert (audioData.size() == totalSamples * numChannels);

    // Create a buffer and fill it channel-major
    juce::AudioBuffer<float> buffer (numChannels, (int) totalSamples);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* writePtr = buffer.getWritePointer (ch);
        for (int64 i = 0; i < totalSamples; ++i)
            writePtr[i] = audioData[(size_t) (ch + i * numChannels)];
    }

    // Ensure parent folder exists
    outFile.getParentDirectory().createDirectory();

    // Create an output stream
    std::unique_ptr<juce::FileOutputStream> fileStream (outFile.createOutputStream());
    if (! fileStream)
    {
        DBG ("Failed to create output stream for " << outFile.getFullPathName());
        return;
    }

    // Set up WAV writer (16-bit PCM)
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer
        (wavFormat.createWriterFor (fileStream.get(),
                                    sampleRate,
                                    (unsigned int) numChannels,
                                    16, // bits per sample
                                    {}, // metadata
                                    0)); // no extra flags

    if (! writer)
    {
        DBG ("Failed to create WAV writer");
        return;
    }

    // The writer now owns the stream
    fileStream.release();

    // Write the buffer in one go
    writer->writeFromAudioSampleBuffer (buffer, 0, (int) totalSamples);
}


//------------------------------------------------------------------------------
// Convert frequency (Hz) to nearest MIDI note number
//------------------------------------------------------------------------------
static int freqToMidiNote(double freq)
{
    return int(std::round(69 + 12.0 * std::log2(freq / 440.0)));
}
static double midiNoteToFreq(int midiNote)
{
    return 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
}
//------------------------------------------------------------------------------
// Wait for MIDI from transcriber, with timeout in seconds
//------------------------------------------------------------------------------
bool waitForMidi(Transcriber &trans, double timeoutSecs = 2.0)
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
static void appendVector(std::vector<float> &dest, const std::vector<float> &src)
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
    int totalSamples = int(totalSecs * sampleRate);
    std::cout << "makeSaw sr " << sampleRate << " len " << signalSecs << " total " << totalSamples << " sig " << signalSamples << std::endl;
    std::vector<float> v(totalSamples, 0.0f);

    double phase = 0.0;
    double phaseInc = freq / sampleRate;
    // for (int i = 0; i < signalSamples && i < totalSamples; ++i)
    for (int i = 0; i < signalSamples; ++i)
    {
        v[i] = float((2.0 * (phase - std::floor(phase + 0.5))) * amplitude);
        phase += phaseInc;
        if (phase >= 1.0)
        {
            phase -= std::floor(phase);
        }
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
    int totalSamples = int(totalSecs * sampleRate);
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
                        [&]{ double totalSecs = 1.0;

                             return makeSaw(329.63, 0.5, totalSecs, sr, 0.4f); },
                         512, static_cast<int>(0.2 * sr) ,
                        { E4 }, { E4 }
                    },

                {
                    "Major triad chord (saw mix)",
                    [&]{ double totalSecs = 0.5;// 4096 is about 0.18
                        double noteLenSecs = 0.1;
                         auto c = makeSaw(midiNoteToFreq(C4), noteLenSecs, totalSecs, sr, 0.3f);
                         auto e = makeSaw(midiNoteToFreq(E4), noteLenSecs, totalSecs, sr, 0.3f);
                         auto g = makeSaw(midiNoteToFreq(G4), noteLenSecs, totalSecs, sr, 0.3f);
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
                    [&]
                    {
                        double totalSecs = 2.0;
                        double pulseLenSecs = 0.1;
                        std::vector<float> out;
                        for (int i = 0; i < 5; ++i)
                        {
                            appendVector(out,
                                         makePulse(261.63, pulseLenSecs, totalSecs, sr, 0.5f, 0.2f));
                            out.insert(out.end(), int(0.1 * sr), 0.0f);
                        }
                        return out;
                    },
                    512, 4096,
                    std::vector<int>(5, C4),
                    std::vector<int>(5, C4)},

                // {
                //     "Silence only",
                //     [&]{ return std::vector<float>(int((4096.0/sr) * sr), 0.0f); },
                //     256, 4096,
                //     {}, {}
                // }
            };

        // for (auto& tc : cases)
        // {
        //     beginTest(tc.name);
        //     Transcriber trans;
        //     trans.resetBuffersSamples(tc.bufferLenSamples);

        //     // Feed audio in blocks
        //     size_t pos = 0;
        //     auto audio = tc.makeAudio();
        //     while (pos < audio.size())
        //     {
        //         int chunk = int(std::min<size_t>(tc.bufferSize, audio.size() - pos));
        //         trans.queueAudioForTranscription(&audio[pos], chunk, sr);
        //         while (trans.getStatus() == bothBuffersFullPleaseWait ||
        //                trans.getStatus() == collectingAudioAndTranscribing)
        //             std::this_thread::sleep_for(std::chrono::milliseconds(10));
        //         pos += chunk;
        //     }

        for (auto &tc : cases)
        {
            beginTest(tc.name);
            Transcriber trans;
            trans.resetBuffersSamples(tc.bufferLenSamples);
            auto audio = tc.makeAudio();

            // --- Dump the raw buffer to WAV for inspection ---
            {
                // make a filesystem‐safe filename from the test name
                String filename = tc.name.replaceCharacters(" /\\?%*:|\"<>", "_");
                filename = filename.replaceCharacter(' ', '_') + ".wav";

                File outFile = File::getCurrentWorkingDirectory().getChildFile(filename);
                
                saveWavFromVector(audio, 
                                  1, // mono tests
                                  (double)sr,
                                  outFile);
                std::cout << "Wrote test buffer to " << outFile.getFullPathName() << std::endl;
            }

            // Feed audio in blocks
            size_t pos = 0;
          
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

            // Collect all MIDI into local midi buffer
            MidiBuffer midi;
            trans.collectMidi(midi);
            std::cout << "Collected midi events: " << midi.getNumEvents() << std::endl;



            std::unordered_map<int, int> onCounts, offCounts;

            // JUCE-6+ allows range-for over MidiBuffer :contentReference[oaicite:0]{index=0}:contentReference[oaicite:1]{index=1}
            for (auto metadata : midi)
            {
                const auto &msg = metadata.getMessage();
                int note = msg.getNoteNumber();
                if (msg.isNoteOn())
                    ++onCounts[note];
                if (msg.isNoteOff())
                    ++offCounts[note];
            }

            // Now verify that *each* expected note appears at least once
            for (int expectedNote : tc.expectedOn)
            {
                // unordered_map::count returns 1 if present, 0 otherwise :contentReference[oaicite:2]{index=2}:contentReference[oaicite:3]{index=3}
                bool found = onCounts.count(expectedNote) > 0;
                expect(found, "Expected Note On for MIDI note " + String(expectedNote));
            }

            for (int expectedNote : tc.expectedOff)
            {
                bool found = offCounts.count(expectedNote) > 0;
                expect(found, "Expected Note Off for MIDI note " + String(expectedNote));
            }
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
