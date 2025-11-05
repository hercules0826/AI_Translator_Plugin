#pragma once
class LiveTranslatorAudioProcessor;
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
//#include "ui/Languages.h"

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
    juce::ToggleButton autoDetect{ "Auto-detect input language" };
    juce::Label status{ {}, "Listening…" };
    juce::TextEditor transcript;
    //juce::ToggleButton silenceIfSame { "Silence if same language" };
    juce::ToggleButton showDebug{ "Show debug panel" };
    juce::TextEditor debug;

    void timerCallback() override;
    void updateLanguagesFromUI();
    void buildLangBoxes();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveTranslatorAudioProcessorEditor)
};
