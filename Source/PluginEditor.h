#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "ui/Languages.h"

class LiveTranslatorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    LiveTranslatorAudioProcessorEditor (LiveTranslatorAudioProcessor&);
    ~LiveTranslatorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    LiveTranslatorAudioProcessor& proc;

    juce::ComboBox inLang, outLang;
    juce::TextEditor transcript;
    juce::ToggleButton silenceIfSame { "Silence if same language" };

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveTranslatorAudioProcessorEditor)
};
