#pragma once
#include <atomic>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

struct TranscriptMsg {
    bool isFinal = false;
    double t0Sec = 0.0, t1Sec = 0.0;
    std::string text;
};

struct TtsPcmMsg {
    // 16 kHz mono PCM chunk produced by TTS
    std::vector<float> pcm16k;
    bool eof = false;
};

class MessageBus {
public:
    // transcripts
    void pushTranscript(const TranscriptMsg& m) {
        std::lock_guard<std::mutex> lg(txMutex);
        transcripts.push(m);
    }
    bool popTranscript(TranscriptMsg& out) {
        std::lock_guard<std::mutex> lg(txMutex);
        if (transcripts.empty()) return false;
        out = std::move(transcripts.front());
        transcripts.pop();
        return true;
    }

    // TTS audio
    void pushTts(const TtsPcmMsg& m) {
        std::lock_guard<std::mutex> lg(ttsMutex);
        ttsPcm.push(m);
    }
    bool popTts(TtsPcmMsg& out) {
        std::lock_guard<std::mutex> lg(ttsMutex);
        if (ttsPcm.empty()) return false;
        out = std::move(ttsPcm.front());
        ttsPcm.pop();
        return true;
    }

private:
    std::mutex txMutex, ttsMutex;
    std::queue<TranscriptMsg> transcripts;
    std::queue<TtsPcmMsg> ttsPcm;
};
