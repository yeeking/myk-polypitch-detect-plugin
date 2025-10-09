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

void LevelMeterComp::paint(juce::Graphics& g)
{
    const float brightness_0_1 = brightness.load(std::memory_order_relaxed);
    const uint8 brighness_0_255 = static_cast<uint8>(brightness_0_1  * 255.0f);
    juce::Colour barColour = juce::Colour::fromRGB(brighness_0_255, brighness_0_255, brighness_0_255);
    
    auto bounds = getLocalBounds().toFloat();

    // Base backplate
    auto outlineColour = findColour(juce::GroupComponent::outlineColourId);
    auto fillColour    = findColour(juce::TextButton::buttonColourId).withMultipliedAlpha(0.12f);
    // auto fillColour    = juce::Colours::red;
    //  juce::TextButton::buttonColourId).withMultipliedAlpha(0.12f);
    
    g.setColour(barColour);
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(outlineColour);
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    // Note number text
    juce::String labelText = "-";
    const int rmsValue = static_cast<int> (lastRMS.load(std::memory_order_relaxed));
    labelText = juce::String{"RMS: "+ std::to_string(rmsValue)};

    const float h = bounds.getHeight();
    const float w = bounds.getWidth();
    float fontSize = juce::jmin(h * 0.70f, w * 0.45f);

    g.setFont(juce::Font(fontSize, juce::Font::bold));

    // Contrast-aware text
    
    g.setColour(barColour);
    g.drawFittedText(labelText, getLocalBounds(), juce::Justification::centred, 1);

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
