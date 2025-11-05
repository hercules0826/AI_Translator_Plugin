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
    // {
        // while (!threadShouldExit())
        // {
        //     if (!newAudioAvailable) { wait(5); continue; }

        //     int s1, s2, e1, e2;
        //     fifo.prepareToRead(frameSize, s1, e1, s2, e2);

        //     std::vector<float> temp(frameSize);
        //     if (e1 > 0) memcpy(temp.data(), buffer.data() + s1, e1 * sizeof(float));
        //     if (e2 > 0) memcpy(temp.data() + e1, buffer.data() + s2, e2 * sizeof(float));

        //     fifo.finishedRead(e1 + e2);
        //     newAudioAvailable = false;

        //     whisper_full_params p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
        //     p.n_threads = 4;
        //     whisper_full(ctx, p, temp.data(), temp.size());

        //     auto text = whisper_full_get_segment_text(ctx, 0);
        //     if (text) DBG("[Whisper] " << text);
        // }
    // }

    std::atomic<bool> newAudioAvailable{ false };

private:
    whisper_context* ctx;
    juce::AbstractFifo& fifo;
    std::vector<float>& buffer;
    static constexpr int kFrame = 16000; // 1s @ 16k
};
