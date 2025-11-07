#pragma once
class LiveTranslatorAudioProcessor;
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include "../dsp/LockFreeRingBuffer.h"
#include "WhisperEngine.h"
#include "../PluginProcessor.h"
#include "../tts/AzureTTS.h"
#include "../translate//GoogleTranslator.h"

// Very simple message passing
// struct TranscriptMsg { juce::String text; juce::String lang; double t = 0.0; };
struct AudioMsg      { juce::AudioBuffer<float> buffer; };

class Pipeline : private juce::Thread
{
public:
    Pipeline(LockFreeRingBuffer& inputFifo, LockFreeRingBuffer& ttsOutFifo, WhisperEngine& whisperEngine, LiveTranslatorAudioProcessor& owner);
    ~Pipeline() override;

    void setLanguages(const juce::String& in, const juce::String& out);
    void start(const juce::File& modelPath);
    void stop();

    // called by processor’s audio thread
    void pushAudioFromDSP(const float* interleaved, int frames, int channels, double sr);

    // called by editor to fetch last transcript safely
    juce::String getLastTranscript() const { return lastTranscript; }

    void setAutoDetect(bool e);
    // Non-blocking tick budget (ms)
    void setTickBudgetMs(int ms) { decodeBudgetMs.store(ms); }

private:
    void run() override; // background loop

    LockFreeRingBuffer& input;
    LockFreeRingBuffer& ttsOut;

    WhisperEngine& whisper;
    //std::unique_ptr<WhisperEngine> whisper;

    std::atomic<bool> running { false };
    std::atomic<double> sampleRate { 48000.0 };
    std::atomic<int> numCh { 2 };
    std::atomic<int>  decodeBudgetMs { 20 }; // per loop

    juce::Time startTime { juce::Time::getCurrentTime() };
    juce::String inLang  = "auto";
    juce::String outLang = "en";

	GoogleTranslator translator;
    AzureTTS tts;

    // state
    LiveTranslatorAudioProcessor& owner;
    juce::String lastTranscript;
    juce::FileLogger logger { juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                              .getChildFile("LiveTranslator.log"), "LiveTranslator" };

    // mini translation placeholder
    juce::String translate(const juce::String& txt, const juce::String& in, const juce::String& out);

    // mini TTS placeholder -> generates a short sine “speech” (replace with Azure/Coqui)
    void synthTTS(const juce::String& text, juce::AudioBuffer<float>& out);
};
