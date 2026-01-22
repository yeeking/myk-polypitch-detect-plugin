#include "PianoRollComponent.h"
#include "BasicPitchConstants.h"
#include <algorithm>
#include <cmath>

PianoRollComponent::PianoRollComponent()
{
    setOpaque(true);
    setWantsKeyboardFocus(true);
    startTimerHz(60);
}

void PianoRollComponent::noteOn(int note, float velocity, double timeSeconds)
{
    if (note < 0 || note >= static_cast<int>(activeNotes.size()))
        return;

    auto& slot = activeNotes[static_cast<size_t>(note)];
    if (!slot.active)
    {
        slot.active = true;
        slot.startTime = timeSeconds;
        slot.velocity = velocity;
    }
    else
    {
        slot.velocity = juce::jmax(slot.velocity, velocity);
    }

    lastNoteEventTime = timeSeconds;
}

void PianoRollComponent::noteOff(int note, double timeSeconds)
{
    if (note < 0 || note >= static_cast<int>(activeNotes.size()))
        return;

    auto& slot = activeNotes[static_cast<size_t>(note)];
    if (!slot.active)
        return;

    NoteBar bar;
    bar.note = note;
    bar.velocity = slot.velocity;
    bar.startTime = slot.startTime;
    bar.endTime = timeSeconds;
    bar.voiceId = 0;
    noteHistory.push_back(bar);

    slot.active = false;
    slot.velocity = 0.0f;
    lastNoteEventTime = timeSeconds;
}

void PianoRollComponent::clear()
{
    noteHistory.clear();
    for (auto& note : activeNotes)
        note = ActiveNote{};
}

void PianoRollComponent::setTimeWindowSeconds(double seconds)
{
    timeWindowSeconds = juce::jlimit(2.0, 20.0, seconds);
}

double PianoRollComponent::getLastNoteEventTimeSeconds() const
{
    return lastNoteEventTime;
}

void PianoRollComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xFF10151A));

    const int minNote = MIN_MIDI_NOTE;
    const int maxNote = MAX_MIDI_NOTE;
    const int noteCount = maxNote - minNote + 1;
    const float noteHeight = area.getHeight() / static_cast<float>(noteCount);

    for (int note = minNote; note <= maxNote; ++note)
    {
        const float y = noteToY(note, area);
        const bool blackKey = isBlackKey(note);
        if (blackKey)
        {
            g.setColour(juce::Colour(0xFF121A21));
            g.fillRect(area.withY(y).withHeight(noteHeight));
        }

        const bool isOctave = (note % 12) == 0;
        g.setColour(isOctave ? juce::Colour(0xFF28323A) : juce::Colour(0xFF1C232A));
        g.drawLine(area.getX(), y, area.getRight(), y, isOctave ? 1.6f : 0.8f);

        if (isOctave)
        {
            g.setColour(juce::Colour(0xFF4A5862));
            g.setFont(11.0f);
            g.drawText(noteName(note), area.getX() + 6.0f, y - noteHeight * 0.5f, 40.0f, noteHeight,
                       juce::Justification::centredLeft);
        }
    }

    const float nowX = area.getX() + area.getWidth() * 0.88f;
    g.setColour(juce::Colour(0xFF41515C));
    g.drawLine(nowX, area.getY(), nowX, area.getBottom(), 2.0f);

    const double currentTime = isFrozen ? freezeTimeSeconds : currentTimeSeconds;

    auto drawEvent = [&](const NoteBar& event, bool isActive)
    {
        const double endTime = isActive ? currentTime : event.endTime;
        const double duration = juce::jmax(0.0, endTime - event.startTime);
        if (duration <= 0.0)
            return;

        auto rect = noteRectForEvent({ event.note, event.velocity, event.startTime, endTime, event.voiceId }, currentTime);
        if (!rect.intersects(area))
            return;

        const float hue = std::fmod(static_cast<float>(event.note * 0.07f), 1.0f);
        const float alpha = juce::jlimit(0.25f, 0.95f, event.velocity);
        const auto colour = juce::Colour::fromHSV(hue, 0.65f, 0.92f, alpha);
        g.setColour(colour);
        g.fillRoundedRectangle(rect, 3.0f);
    };

    for (const auto& event : noteHistory)
        drawEvent(event, false);

    for (size_t i = 0; i < activeNotes.size(); ++i)
    {
        const auto& note = activeNotes[i];
        if (!note.active)
            continue;

        NoteBar bar;
        bar.note = static_cast<int>(i);
        bar.velocity = note.velocity;
        bar.startTime = note.startTime;
        bar.endTime = currentTime;
        drawEvent(bar, true);
    }

    if (isFrozen)
    {
        g.setColour(juce::Colour(0xFFDAA632));
        g.setFont(12.0f);
        g.drawText("Frozen", area.removeFromBottom(18.0f).removeFromRight(70.0f),
                   juce::Justification::centredRight);
    }
}

void PianoRollComponent::resized()
{
}

void PianoRollComponent::mouseMove(const juce::MouseEvent& event)
{
    updateTooltipForPoint(event.position);
}

void PianoRollComponent::mouseDown(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    grabKeyboardFocus();
    isFrozen = !isFrozen;
    if (isFrozen)
        freezeTimeSeconds = currentTimeSeconds;
    repaint();
}

bool PianoRollComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        isFrozen = !isFrozen;
        if (isFrozen)
            freezeTimeSeconds = currentTimeSeconds;
        repaint();
        return true;
    }
    return false;
}

void PianoRollComponent::timerCallback()
{
    if (isFrozen)
        return;

    currentTimeSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    pruneOldNotes(currentTimeSeconds);
    repaint();
}

void PianoRollComponent::pruneOldNotes(double currentTime)
{
    const double minTime = currentTime - timeWindowSeconds;
    noteHistory.erase(std::remove_if(noteHistory.begin(), noteHistory.end(),
                                     [minTime](const NoteBar& event)
                                     {
                                         return event.endTime < minTime;
                                     }),
                      noteHistory.end());
}

void PianoRollComponent::updateTooltipForPoint(juce::Point<float> point)
{
    const double currentTime = isFrozen ? freezeTimeSeconds : currentTimeSeconds;

    auto hitTest = [&](const NoteBar& event, bool isActive) -> bool
    {
        const double endTime = isActive ? currentTime : event.endTime;
        auto rect = noteRectForEvent({ event.note, event.velocity, event.startTime, endTime, event.voiceId }, currentTime);
        return rect.contains(point);
    };

    for (const auto& event : noteHistory)
    {
        if (hitTest(event, false))
        {
            const double duration = event.endTime - event.startTime;
            hoverTooltip = juce::String(noteName(event.note))
                + " | " + juce::String(duration, 2) + "s"
                + " | vel " + juce::String(event.velocity, 2);
           #if JUCE_GUI_BASICS
            setTooltip(hoverTooltip);
           #endif
            return;
        }
    }

    for (size_t i = 0; i < activeNotes.size(); ++i)
    {
        const auto& note = activeNotes[i];
        if (!note.active)
            continue;

        NoteBar event;
        event.note = static_cast<int>(i);
        event.velocity = note.velocity;
        event.startTime = note.startTime;
        event.endTime = currentTime;
        if (hitTest(event, true))
        {
            const double duration = currentTime - note.startTime;
            hoverTooltip = juce::String(noteName(event.note))
                + " | " + juce::String(duration, 2) + "s"
                + " | vel " + juce::String(event.velocity, 2);
           #if JUCE_GUI_BASICS
            setTooltip(hoverTooltip);
           #endif
            return;
        }
    }

    hoverTooltip.clear();
   #if JUCE_GUI_BASICS
    setTooltip({});
   #endif
}

juce::Rectangle<float> PianoRollComponent::noteRectForEvent(const NoteBar& noteEvent,
                                                            double currentTime) const
{
    auto area = getLocalBounds().toFloat();
    const float nowX = area.getX() + area.getWidth() * 0.88f;
    const float pixelsPerSecond = area.getWidth() / static_cast<float>(timeWindowSeconds);
    const float xStart = nowX - static_cast<float>(currentTime - noteEvent.startTime) * pixelsPerSecond;
    const float xEnd = nowX - static_cast<float>(currentTime - noteEvent.endTime) * pixelsPerSecond;
    const float left = juce::jmin(xStart, xEnd);
    const float right = juce::jmax(xStart, xEnd);
    const float height = area.getHeight() / static_cast<float>(MAX_MIDI_NOTE - MIN_MIDI_NOTE + 1);
    const float y = noteToY(noteEvent.note, area);
    return { left, y, right - left, height * 0.9f };
}

float PianoRollComponent::noteToY(int note, const juce::Rectangle<float>& area) const
{
    const int minNote = MIN_MIDI_NOTE;
    const int maxNote = MAX_MIDI_NOTE;
    const int noteCount = maxNote - minNote + 1;
    const float noteHeight = area.getHeight() / static_cast<float>(noteCount);
    const int offset = note - minNote;
    return area.getBottom() - (offset + 1) * noteHeight;
}

bool PianoRollComponent::isBlackKey(int midiNote) const
{
    const int pitchClass = midiNote % 12;
    return pitchClass == 1 || pitchClass == 3 || pitchClass == 6 || pitchClass == 8 || pitchClass == 10;
}

juce::String PianoRollComponent::noteName(int midiNote) const
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int octave = midiNote / 12 - 1;
    return juce::String(names[midiNote % 12]) + juce::String(octave);
}
