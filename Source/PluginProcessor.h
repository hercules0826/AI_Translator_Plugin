#pragma once
class Pipeline;
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/LockFreeRingBuffer.h"
#include "engine/WhisperEngine.h"
#include "engine/Pipeline.h"
#include "engine/MessageBus.h"
#include "tts/BeepTts.h"
#include "translate/PassThroughTranslator.h"
#include "dsp/Resample16k.h"
#include <atomic>
#include <mutex>
#include "translate/GoogleTranslator.h"
#include "tts/AzureTTs.h"

class LiveTranslatorAudioProcessor : public juce::AudioProcessor
{
public:
    LiveTranslatorAudioProcessor();
    ~LiveTranslatorAudioProcessor() override;

    juce::AudioProcessorValueTreeState apvts;

    GoogleTranslator translator;
    AzureTTS tts;

    GoogleTranslator& getTranslator() { return translator; }
    AzureTTS& getAzureTTS() { return tts; }

    void setAzureKey(const juce::String& key) { tts.setKey(key); }
    void setAzureRegion(const juce::String& reg) { tts.setRegion(reg); }

    // JUCE
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    //Programs (unused)
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    const juce::String getName() const override { return "LiveTranslator"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // buses
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // UI <---> Engine hooks
    void setLanguages(const juce::String& in, const juce::String& out);
    void setAutoDetect(bool enabled);
    bool getAutoDetect() const noexcept { return autoDetect.load(); }
    juce::String getLastTranscript() const; //safe copy
    void appendDebug(const juce::String& line);
    juce::String pullDebugSinceLast();      // returns & clears incremental buffer

    void setVoiceGender(const juce::String& g) { voiceGender = g; }
    void setVoiceStyle (const juce::String& s) { voiceStyle = s;  }
    juce::String getVoiceGender() const { return voiceGender; }
    juce::String getVoiceStyle () const { return voiceStyle;  }

    void previewVoice() { previewPending.store(true); }

    // Optional Azure config passthroughs
    juce::String getAzureKey() const    { return azureKey; }
    juce::String getAzureRegion() const { return azureRegion; }

    //mutable std::mutex textMx;
    juce::CriticalSection textMx;
    juce::String lastTranscript;     // single latest line
    juce::String pendingDebug;       // accumulated since last UI pull

private:
    
    // Azure config (set these via your own UI if needed)
    juce::String azureKey, azureRegion;

    std::atomic<bool> autoDetect { true };

    // Input FIFO -> 16k audio pipeline
    LockFreeRingBuffer input16k { 16000 * 20, 1 }; // 20s safety
    Resample16k resampler;

    // Messaging
    MessageBus bus;

    // Modules
    
    std::unique_ptr<WhisperEngine> whisper;

    // Scratch buffers
    juce::AudioBuffer<float> scratch;
    std::vector<float> monoTmp;
    std::vector<float> mono16k;
    std::vector<float> ttsOutMono;

    // UI / state
    juce::String inLang  { "auto" };
    juce::String outLang{ "en" };

    juce::String voiceGender = "Female";
    juce::String voiceStyle = "Conversational";

    // preview buffer
    juce::AudioBuffer<float> previewBuffer;
    std::atomic<bool> previewPending { false };

    std::unique_ptr<Pipeline> pipeline;

    LockFreeRingBuffer inFifo  { 48000 * 10, 2 }; // 10s capacity, stereo
    LockFreeRingBuffer outFifo { 48000 * 10, 2 }; // TTS audio out (stereo)
    double sampleRateHz = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiveTranslatorAudioProcessor)
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
};