#include "PluginEditor.h"
#include "PluginProcessor.h"
//#include "ui/Languages.h"

using namespace juce;

static const juce::StringArray& kLanguages()
{
    static juce::StringArray langs{
        "Auto", "English", "German", "Swiss German", "French", "Italian",
        "Spanish", "Portuguese", "Dutch", "Polish", "Czech", "Japanese"
    };
    return langs;
}

LiveTranslatorAudioProcessorEditor::LiveTranslatorAudioProcessorEditor (LiveTranslatorAudioProcessor& p)
: AudioProcessorEditor (&p), proc (p)
{
    setResizable(true, true);
    setSize(680, 420);

    // Language selectors
    addAndMakeVisible(inLang);
    addAndMakeVisible(outLang);
    buildLangBoxes();

    inLang.onChange = [this] { updateLanguagesFromUI(); };
    outLang.onChange = [this] { updateLanguagesFromUI(); };

    // Auto detect
    addAndMakeVisible(autoDetect);
    autoDetect.setToggleState(proc.getAutoDetect(), dontSendNotification);
    autoDetect.onClick = [this] {
        proc.setAutoDetect(autoDetect.getToggleState());
        updateLanguagesFromUI();
        };

    // Status
    addAndMakeVisible(status);
    status.setJustificationType(Justification::centredLeft);

    // Transcript window
    addAndMakeVisible(transcript);
    transcript.setMultiLine(true);
    transcript.setScrollbarsShown(true);
    transcript.setReadOnly(true);
    transcript.setFont(juce::Font(16.0f));

    // Debug panel
    addAndMakeVisible(showDebug);
    showDebug.onClick = [this] { resized(); };

    addAndMakeVisible(debug);
    debug.setMultiLine(true);
    debug.setScrollbarsShown(true);
    debug.setReadOnly(true);
    debug.setFont(juce::Font(12.0f));
    debug.setText(""); // hidden by layout until toggled

    startTimerHz(20); // 50 ms UI refresh
    updateLanguagesFromUI();
}

LiveTranslatorAudioProcessorEditor::~LiveTranslatorAudioProcessorEditor() = default;

void LiveTranslatorAudioProcessorEditor::updateLanguagesFromUI()
{
    const auto inSel = inLang.getText();
    const auto outSel = outLang.getText();

    // When Auto-detect is enabled, force input="Auto"
    if (autoDetect.getToggleState())
        proc.setLanguages("Auto", outSel);
    else
        proc.setLanguages(inSel, outSel);

    status.setText(autoDetect.getToggleState() ? "Auto-detect enabled" : "Manual language", dontSendNotification);
}

void LiveTranslatorAudioProcessorEditor::buildLangBoxes()
{
    const auto langs = kLanguages();
    for (int i = 0; i < langs.size(); ++i)
    {
        inLang.addItem(langs[i], i + 1);
        outLang.addItem(langs[i], i + 1);
    }
    inLang.setSelectedId(1, dontSendNotification);  // Auto
    outLang.setSelectedId(2, dontSendNotification); // English (default)
}

void LiveTranslatorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(juce::Colours::white);
    g.setColour(juce::Colours::black);
    g.setFont(18.0f);
    //g.drawText("Live Translator (Whisper → Translate → TTS)", getLocalBounds().removeFromTop(24), juce::Justification::centred);
    g.drawText("Live Translator", 16, 10, getWidth() - 32, 24, juce::Justification::left);
}

void LiveTranslatorAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced(12);
    auto header = r.removeFromTop(36);
    status.setBounds(header.removeFromRight(240));
    header.removeFromRight(8);
    outLang.setBounds(header.removeFromRight(160));
    header.removeFromRight(8);
    inLang.setBounds(header.removeFromRight(160));

    r.removeFromTop(6);
    autoDetect.setBounds(r.removeFromTop(24));

    r.removeFromTop(6);
    showDebug.setBounds(r.removeFromTop(24));

    r.removeFromTop(6);
    if (showDebug.getToggleState())
    {
        auto upper = r.removeFromTop(r.getHeight() * 2 / 3);
        transcript.setBounds(upper);
        r.removeFromTop(6);
        debug.setBounds(r);
        debug.setVisible(true);
    }
    else
    {
        transcript.setBounds(r);
        debug.setVisible(false);
    }
}

void LiveTranslatorAudioProcessorEditor::timerCallback()
{
    //transcript.setText(proc.getLastTranscript(), false);
    // Transcript polling (safe, lightweight)
    static String lastShown;
    const auto cur = proc.getLastTranscript();
    if (cur.isNotEmpty() && cur != lastShown)
    {
        transcript.moveCaretToEnd(false);
        transcript.insertTextAtCaret(cur + "\n");
        lastShown = cur;
        status.setText("Transcribing…", dontSendNotification);
    }

    // Debug drain
    const auto dbg = proc.pullDebugSinceLast();
    if (dbg.isNotEmpty())
    {
        debug.moveCaretToEnd(false);
        debug.insertTextAtCaret(dbg);
    }
}
