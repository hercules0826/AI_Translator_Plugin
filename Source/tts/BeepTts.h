#pragma once
#include "ITts.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class BeepTts : public ITts {
public:
    void synthesize(const TtsRequest& req,
                    std::function<void(const std::vector<float>&, bool)> onChunk) override {
        // placeholder: create a short “beep + envelope” indicating text length
        const int sr = 16000;
        const float durSec = std::min(1.5f, std::max(0.2f, (float)req.text.size() * 0.02f));
        const int N = (int)(durSec * sr);
        std::vector<float> pcm(N);
        const float f = 880.0f;
        for (int i = 0; i < N; ++i) {
            float env = std::min(1.0f, i / (0.01f * sr)) * std::min(1.0f, (N - i) / (0.05f * sr));
            pcm[i] = 0.2f * env * std::sin(2.0f * float(M_PI) * f * (float)i / (float)sr);
        }
        onChunk(pcm, true);
    }
};
