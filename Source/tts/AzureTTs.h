#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "ITts.h"

struct AzureVoiceProfile
{
    juce::String name;   // "en-US-JennyNeural"
    juce::String style;  // "Conversational"
    juce::String role;   // Optional character role
};

class AzureTTS  :   public ITts
{
public:
    AzureTTS() = default;

    void setKey(const juce::String& key)      { azureKey = key; }
    void setRegion(const juce::String& region){ azureRegion = region; }

    AzureVoiceProfile pickVoice(const juce::String& lang,
                                const juce::String& gender,
                                const juce::String& style) const;

    // STREAMING callback:
    void synthesize(const TtsRequest& req,
        std::function<void(const std::vector<float>&, bool)> onChunk) override;

private:
    juce::String azureKey, azureRegion;

    juce::String buildSsml(const juce::String& text,
                           const AzureVoiceProfile& voice) const;
};
