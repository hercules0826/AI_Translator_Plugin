#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

struct AzureVoiceProfile
{
    juce::String name;   // e.g., "en-US-JennyNeural"
    juce::String style;  // Conversational / Broadcast / Elegant / Warm / ...
    juce::String role;   // (optional) "Girl" / "Boy" etc. – not used here
};

struct AzureVoiceSet
{
    AzureVoiceProfile male;
    AzureVoiceProfile female;
    AzureVoiceProfile neutral;
};

class AzureTTS
{
public:
    void setKey(const juce::String& key) { azureKey = key; }
    void setRegion(const juce::String& regionId) { region = regionId; }

    // language: "en", "de", "gsw"(->de), "fr", "it"
    AzureVoiceProfile pickVoice(const juce::String& language,
                                const juce::String& gender,
                                const juce::String& style) const;

    // Stub “streaming” synth for now (tone). Replace with real Azure SDK calls.
    juce::AudioBuffer<float> synthesizeStreaming(const juce::String& text,
                                                 const AzureVoiceProfile& profile,
                                                 double sampleRate = 48000.0) const;

private:
    juce::String azureKey, region;

    // Default voice map. Swap with your preferred SKUs.
    // NOTE: gsw (Swiss German) is routed to de internally.
    AzureVoiceSet forLang(const juce::String& lang) const;
};
