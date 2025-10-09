#include "LevelMeterComp.h"
#include "juce_graphics/juce_graphics.h"
#include <atomic>

LevelMeterComp::LevelMeterComp()
{
    updateFrameTimer();
}

LevelMeterComp::~LevelMeterComp()
{
    stopTimer();
}

void LevelMeterComp::setRMS(float rms)
{
    const auto doSet = [this, rms]
    {
        // std::cout << "level meter updating rms to. " << rms << std::endl;
        setRMSMessageThread(rms);
    };

    if (juce::MessageManager::getInstance()->isThisTheMessageThread()) doSet();
    else juce::MessageManager::callAsync(doSet);
}

void LevelMeterComp::setRMSMessageThread(float rms)
{
    float db = Decibels::gainToDecibels (rms);
    lastRMS.store(db, std::memory_order_relaxed);
    brightness.store(((db + 120.0f) / 100.0f), std::memory_order_relaxed);
    // std::cout << "DB " << Decibels::gainToDecibels (rms) << std::endl;
    if (brightness.load(std::memory_order_relaxed) > redrawThresh)
        repaint();
}

void LevelMeterComp::setFrameRateHz(int hz)
{
    frameRateHz = juce::jlimit(1, 240, hz);
    updateFrameTimer();
}

void LevelMeterComp::setDecaySeconds(float seconds)
{
    decaySeconds = juce::jlimit(0.05f, 10.0f, seconds);
}

void LevelMeterComp::setRedrawThreshold(float threshold01)
{
    redrawThresh = juce::jlimit(0.0f, 1.0f, threshold01);
}
// void LevelMeterComp::paint(juce::Graphics& g)
// {
//     auto area = getLocalBounds();
//     const float brightness_0_1 = brightness.load(std::memory_order_relaxed);

//     // Background
//     g.setColour(juce::Colours::darkgrey);
//     g.fillRect(area);

//     // Normalised 0–1 value
//     const float norm = juce::jlimit(0.0f, 1.0f, brightness_0_1);

//     // Left→Right gradient
//     juce::ColourGradient grad(
//         juce::Colours::lime, (float)area.getX(),      (float)area.getCentreY(),
//         juce::Colours::red,  (float)area.getRight(),  (float)area.getCentreY(),
//         false);
//     g.setGradientFill(grad);

//     const int barWidth = (int)std::round(norm * area.getWidth());
//     g.fillRect(area.removeFromLeft(barWidth));
// }

void LevelMeterComp::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();
    const float db = lastRMS.load(std::memory_order_relaxed);

    // Background
    g.setColour(juce::Colours::darkgrey);
    g.fillRect(area);

    // Sensible meter window: from -60 dB (silent) to 0 dB (full scale)
    constexpr float minDb = -60.0f;
    constexpr float maxDb =   0.0f;

    // Clamp and map dB → 0..1
    const float dbClamped = juce::jlimit(minDb, maxDb, db);
    const float norm = juce::jmap(dbClamped, minDb, maxDb, 0.0f, 1.0f);
    // std::cout << "orig db "<< db << " clamps " << dbClamped << " 0 to 1 " << norm  << std::endl; 
    // Left→Right gradient
    juce::ColourGradient grad(
        juce::Colours::lime, (float)area.getX(),     (float)area.getCentreY(),
        juce::Colours::red,  (float)area.getRight(), (float)area.getCentreY(),
        false);
    g.setGradientFill(grad);

    const int barWidth = (int)std::round(norm * area.getWidth());
    g.fillRect(area.removeFromLeft(barWidth));
}

void LevelMeterComp::resized()
{
    // No children to layout
}

void LevelMeterComp::timerCallback()
{

    float prev = brightness.load(std::memory_order_relaxed);
    const float decayPerTick = 1.0f / (decaySeconds * (float) frameRateHz);
    float next = juce::jmax(0.0f, prev - decayPerTick);
    // float next = prev * 0.99f;
    // std::cout << "Brightness " << next << std::endl;
    brightness.store(next, std::memory_order_relaxed);

    const bool needRepaint = (prev > 0.1);// || (next > brightnessRedrawThreshold);
    if (needRepaint){
        // DBG("Note brightness " << prev);

        repaint();
    }


}

void LevelMeterComp::updateFrameTimer()
{
    stopTimer();
    if (frameRateHz > 0)
        startTimerHz(frameRateHz);
}
