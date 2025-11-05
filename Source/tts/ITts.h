#pragma once
#include <string>
#include <vector>

struct TtsRequest {
    std::string text;
    // language, voice, style, etc. can be added
};

class ITts {
public:
    virtual ~ITts() = default;
    // Generate 16kHz mono PCM in chunks; may be called on a worker thread.
    virtual void synthesize(const TtsRequest& req,
                            std::function<void(const std::vector<float>&, bool eof)> onChunk) = 0;
};
