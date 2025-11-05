#pragma once
#include <atomic>
#include <functional>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <cstring>

// forward decl from whisper.cpp headers
struct whisper_context;
struct whisper_full_params;

class WhisperEngine
{
public:
    using OnTranscriptFn = std::function<void (const juce::String& text, const juce::String& lang)>;

    WhisperEngine();
    ~WhisperEngine();

    bool loadModel(const juce::File& modelPath);
    void setLanguage(const juce::String& langCode); // "auto" allowed
    void setCallback(OnTranscriptFn cb) { onTranscript = std::move(cb); }

    // Feed interleaved stereo; we downmix to mono 16k
    void pushAudio(const float* interleaved, int numFrames, int numChannels, double sampleRate);

    // Called on worker thread periodically
    void process(int maxMsPerTick = 50);

    void reset();

private:
    whisper_context* ctx = nullptr;
    juce::AudioBuffer<float> downmixMono;   // 16k mono staging
    juce::AudioBuffer<float> ring;          // 16k mono ring for chunking
    int ringWrite = 0;
    juce::CriticalSection ringLock;
    juce::LagrangeInterpolator resampler;

    juce::String currentLang = "auto";
    std::atomic<bool> ready { false };

    OnTranscriptFn onTranscript;

    void ensureCapacity(int samples);
    void transcribeChunk(const float* mono, int numSamples);
};
