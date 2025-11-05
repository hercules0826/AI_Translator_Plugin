// #pragma once
// #include <atomic>
// #include <JuceHeader.h>
// #include "whisper.h"
// #include "WhisperThread.h"

// class PluginAudioProcessor : public juce::AudioProcessor
// {
// public:
//     PluginAudioProcessor();
//     ~PluginAudioProcessor() override;

//     bool acceptsMidi() const override;
//     bool producesMidi() const override;
//     bool isMidiEffect() const override;

//     void prepareToPlay (double sampleRate, int samplesPerBlock) override;
//     void releaseResources() override;
//     bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
//     void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

//     juce::AudioProcessorEditor* createEditor() override;
//     bool hasEditor() const override { return true; }
//     const juce::String getName() const override { return "LiveTranslator"; }
    
//     double getTailLengthSeconds() const override { return 0.0; }

//     // Programs (unused)
//     int getNumPrograms() override { return 1; }
//     int getCurrentProgram() override { return 0; }
//     void setCurrentProgram(int) override {}
//     const juce::String getProgramName(int) override { return {}; }
//     void changeProgramName(int, const juce::String&) override {}

//     void getStateInformation(juce::MemoryBlock&) override {}
//     void setStateInformation(const void*, int) override {}

// private:
//     // static constexpr int fifoSize = 48000 * 5;
//     static constexpr int whisperSampleRate = 16000;
//     static constexpr int fifoSize = 48000 * 10; // 10s safety
//     juce::AbstractFifo fifo { fifoSize };
//     std::vector<float> audioFIFO;

//     // resample state
//     juce::LagrangeInterpolator resampler;
//     double currentHostSR = 48000.0;
//     double resampleRatio = 1.0; // host->16k
//     std::vector<float> resampleBuf; // temp

//     whisper_context* whisperCtx = nullptr;
//     std::unique_ptr<WhisperThread> whisperThread;

//     JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginAudioProcessor)
//     static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// };
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/LockFreeRingBuffer.h"
#include "engine/Pipeline.h"

class LiveTranslatorAudioProcessor : public juce::AudioProcessor
{
public:
    LiveTranslatorAudioProcessor();
    ~LiveTranslatorAudioProcessor() override;

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

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    const juce::String getName() const override { return "LiveTranslator"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // buses
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // UI hooks
    void setLanguages(const juce::String& in, const juce::String& out);
    juce::String getLastTranscript() const { return pipeline ? pipeline->getLastTranscript() : juce::String(); }

private:
    std::unique_ptr<Pipeline> pipeline;

    LockFreeRingBuffer inFifo  { 48000 * 10, 2 }; // 10s capacity, stereo
    LockFreeRingBuffer outFifo { 48000 * 10, 2 }; // TTS audio out (stereo)
    double sampleRateHz = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiveTranslatorAudioProcessor)
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
};
