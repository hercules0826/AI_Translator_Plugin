#pragma once
#include <atomic>
#include <thread>
#include <vector>
#include <memory>
#include <functional>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <cstring>
#include "MessageBus.h"
#include "../tts/ITts.h"
#include "../translate/ITranslator.h"
#include "../dsp/LockFreeRingBuffer.h"
#include "whisper.h"

// forward decl from whisper.cpp headers
struct whisper_context;
struct whisper_full_params;
struct WhisperParams {
    std::string modelPath;
    float windowSec = 2.0f;
    float hopSec    = 0.5f;
    float vadEnergy = 1e-5f; // very light gate
    std::string dstLang = "en";
};

class WhisperEngine
{
public:
    using OnTranscriptFn = std::function<void (const juce::String& text, const juce::String& lang)>;

    WhisperEngine(LockFreeRingBuffer& ring16k,
                  MessageBus& bus,
                  ITranslator& translator,
                  ITts& tts,
                  const WhisperParams& p);
    ~WhisperEngine();

    void start();
    void stop();
    bool isRunning() const { return running.load(); }

    bool loadModel(const juce::File& modelPath);
    void setLanguage(const juce::String& langCode); // "auto" allowed
    void setCallback(OnTranscriptFn cb) { onTranscript = std::move(cb); }

    // Feed interleaved stereo; we downmix to mono 16k
    void pushAudio(const float* interleaved, int numFrames, int numChannels, double sampleRate);

    // Called on worker thread periodically
    void process(int maxMsPerTick = 50);

    void reset();

private:
    void threadFn();

    LockFreeRingBuffer& ring16k;
    MessageBus& bus;
    ITranslator& translator;
    ITts& tts;
    WhisperParams params;

    std::atomic<bool> running{false};
    std::thread worker;

    whisper_context* ctx = nullptr;
    // sliding window buffer (16kHz)
    std::vector<float> window;
    size_t windowSamples = 0;
    size_t hopSamples    = 0;

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
