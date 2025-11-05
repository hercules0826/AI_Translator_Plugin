#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

struct Resample16k {
    juce::LagrangeInterpolator to16k, from16k;

    // hostSR -> 16k mono
    void processTo16k(const float* inMono, int numIn, double hostSR, std::vector<float>& out16k) {
        const double ratio = 16000.0 / hostSR;
        const int expected = (int)std::ceil(numIn * ratio) + 8;
        out16k.resize(expected);
        int produced = (int)to16k.process(ratio, inMono, out16k.data(), expected);
        out16k.resize(produced);
    }

    // 16k mono -> host SR mono
    void processFrom16k(const float* in16k, int numIn, double hostSR, std::vector<float>& outMono) {
        const double ratio = hostSR / 16000.0;
        const int expected = (int)std::ceil(numIn * ratio) + 8;
        outMono.resize(expected);
        int produced = (int)from16k.process(ratio, in16k, outMono.data(), expected);
        outMono.resize(produced);
    }

    void reset() { to16k.reset(); from16k.reset(); }
};
