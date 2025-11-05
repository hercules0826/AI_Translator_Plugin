#include "PluginEditor.h"

LiveTranslatorAudioProcessorEditor::LiveTranslatorAudioProcessorEditor (LiveTranslatorAudioProcessor& p)
: AudioProcessorEditor (&p), proc (p)
{
    setSize (540, 320);

    for (const auto& L : kLanguages) inLang.addItem(L.label, L.id);
    for (const auto& L : kLanguages) outLang.addItem(L.label, L.id);

    inLang.setSelectedId(1); // Auto
    outLang.setSelectedId(2); // English

    addAndMakeVisible(inLang);
    addAndMakeVisible(outLang);
    addAndMakeVisible(silenceIfSame);

    transcript.setMultiLine(true);
    transcript.setReadOnly(true);
    transcript.setScrollbarsShown(true);
    addAndMakeVisible(transcript);

    inLang.onChange = [this]{
        auto id = inLang.getSelectedId();
        proc.setLanguages(kLanguages[id-1].code, kLanguages[outLang.getSelectedId()-1].code);
    };
    outLang.onChange = [this]{
        auto id = outLang.getSelectedId();
        proc.setLanguages(kLanguages[inLang.getSelectedId()-1].code, kLanguages[id-1].code);
    };

    startTimerHz(10); // UI refresh @10Hz
}

LiveTranslatorAudioProcessorEditor::~LiveTranslatorAudioProcessorEditor() = default;

void LiveTranslatorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("Live Translator (Whisper → Translate → TTS)", getLocalBounds().removeFromTop(24), juce::Justification::centred);
}

void LiveTranslatorAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced(8);
    auto top = r.removeFromTop(28);
    inLang.setBounds(top.removeFromLeft(180));
    outLang.setBounds(top.removeFromLeft(180).reduced(6,0));
    silenceIfSame.setBounds(top);

    r.removeFromTop(8);
    transcript.setBounds(r);
}

void LiveTranslatorAudioProcessorEditor::timerCallback()
{
    transcript.setText(proc.getLastTranscript(), false);
}
