#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include "whisper.h"

class WhisperThread : public juce::Thread
{
public:
    WhisperThread(whisper_context* ctx, juce::AbstractFifo& fifo, std::vector<float>& buf)
        : Thread("WhisperThread"), ctx(ctx), fifo(fifo), buffer(buf) {}

    void run() override;

    std::atomic<bool> newAudioAvailable{ false };

private:
    whisper_context* ctx;
    juce::AbstractFifo& fifo;
    std::vector<float>& buffer;
    static constexpr int kFrame = 16000; // 1s @ 16k
};
