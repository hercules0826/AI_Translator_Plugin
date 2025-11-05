#include "WhisperThread.h"

void WhisperThread::run() {
    std::vector<float> frame;
    frame.resize(kFrame);

    while (!threadShouldExit()) {
        if (!newAudioAvailable.load(std::memory_order_acquire)) {
            wait(10);
            continue;
        }

        int avail = 0;
        {
            int s1, s2, e1, e2;
            fifo.prepareToRead(kFrame, s1, e1, s2, e2);
            avail = e1 + e2;
            if (avail < kFrame) {
                fifo.finishedRead(0);
                wait(5);
                continue;
            }
            if (e1 > 0) memcpy(frame.data(), buffer.data() + s1, e1 * sizeof(float));
            if (e2 > 0) memcpy(frame.data() + e1, buffer.data() + s2, e2 * sizeof(float));
            fifo.finishedRead(kFrame);
        }

        newAudioAvailable.store(false, std::memory_order_release);

        // Whisper inference
        whisper_full_params p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
        p.n_threads = juce::jlimit(1, (int)std::thread::hardware_concurrency(), 6);
        p.print_progress = false;
        p.translate = false;      // set true to force English
        p.no_context = true;      // independent chunks
        p.single_segment = true;  // expect short chunk

        if (whisper_full(ctx, p, frame.data(), frame.size()) == 0) {
            const char* txt = whisper_full_get_segment_text(ctx, 0);
            if (txt && *txt) {
                DBG("[Whisper] " << txt);
            }
        } else {
            DBG("[Whisper] inference failed");
        }
    }
}
