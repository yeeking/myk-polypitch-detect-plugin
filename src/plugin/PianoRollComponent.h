#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

class PianoRollComponent : public juce::Component,
                           private juce::Timer
{
public:
    PianoRollComponent();

    void noteOn(int note, float velocity, double timeSeconds);
    void noteOff(int note, double timeSeconds);
    void clear();

    void setTimeWindowSeconds(double seconds);
    double getLastNoteEventTimeSeconds() const;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    struct NoteBar
    {
        int note = 0;
        float velocity = 0.0f;
        double startTime = 0.0;
        double endTime = 0.0;
        int voiceId = 0;
    };

    struct ActiveNote
    {
        bool active = false;
        double startTime = 0.0;
        float velocity = 0.0f;
    };

    void timerCallback() override;
    void pruneOldNotes(double currentTime);
    void updateTooltipForPoint(juce::Point<float> point);
    juce::Rectangle<float> noteRectForEvent(const NoteBar& noteEvent, double currentTime) const;
    float noteToY(int note, const juce::Rectangle<float>& area) const;
    bool isBlackKey(int midiNote) const;
    juce::String noteName(int midiNote) const;

    std::array<ActiveNote, 128> activeNotes;
    std::vector<NoteBar> noteHistory;

    double timeWindowSeconds = 8.0;
    double lastNoteEventTime = 0.0;
    double currentTimeSeconds = 0.0;
    double freezeTimeSeconds = 0.0;
    bool isFrozen = false;

    juce::String hoverTooltip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollComponent)
};
