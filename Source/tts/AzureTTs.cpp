#include "AzureTTS.h"
#include <cmath>

static AzureVoiceSet enVoices()
{
    return {
        /* male    */ { "en-US-GuyNeural",      "Conversational", "" },
        /* female  */ { "en-US-JennyNeural",    "Conversational", "" },
        /* neutral */ { "en-US-AriaNeural",     "Conversational", "" }
    };
}

static AzureVoiceSet deVoices()
{
    return {
        { "de-DE-ConradNeural", "Broadcast", "" },
        { "de-DE-KatjaNeural",  "Broadcast", "" },
        { "de-DE-SeraphinaMultilingualNeural", "Broadcast", "" }
    };
}

static AzureVoiceSet frVoices()
{
    return {
        { "fr-FR-HenriNeural",  "Elegant", "" },
        { "fr-FR-DeniseNeural", "Elegant", "" },
        { "fr-FR-AlainNeural",  "Elegant", "" }
    };
}

static AzureVoiceSet itVoices()
{
    return {
        { "it-IT-DiegoNeural", "Warm", "" },
        { "it-IT-ElsaNeural",  "Warm", "" },
        { "it-IT-IsabellaNeural", "Warm", "" }
    };
}

AzureVoiceSet AzureTTS::forLang(const juce::String& lang) const
{
    const auto L = lang.toLowerCase();
    if (L == "en") return enVoices();
    if (L == "de" || L == "gsw") return deVoices();    // route gsw → de voices
    if (L == "fr") return frVoices();
    if (L == "it") return itVoices();
    return enVoices();
}

AzureVoiceProfile AzureTTS::pickVoice(const juce::String& language,
                                      const juce::String& gender,
                                      const juce::String& style) const
{
    auto set = forLang(language);
    AzureVoiceProfile v;

    if (gender.equalsIgnoreCase("Male"))      v = set.male;
    else if (gender.equalsIgnoreCase("Neutral")) v = set.neutral;
    else                                      v = set.female;

    if (style.isNotEmpty())
        v.style = style; // override default style

    return v;
}

// Simple placeholder: synth tone varying with style/voice length.
// Replace with Azure SDK stream for production.
juce::AudioBuffer<float> AzureTTS::synthesizeStreaming(const juce::String& text,
                                                       const AzureVoiceProfile& profile,
                                                       double sampleRate) const
{
    const int len = juce::jlimit(2400, (int)sampleRate, text.length() * 1200);
    juce::AudioBuffer<float> out(2, len);

    double baseHz = 220.0;
    if (profile.style.equalsIgnoreCase("Broadcast")) baseHz = 190.0;
    else if (profile.style.equalsIgnoreCase("Elegant")) baseHz = 260.0;
    else if (profile.style.equalsIgnoreCase("Warm")) baseHz = 200.0;

    if (profile.name.containsIgnoreCase("Guy") || profile.name.containsIgnoreCase("Conrad") || profile.name.containsIgnoreCase("Diego"))
        baseHz -= 40.0; // tilt “male” lower

    for (int n = 0; n < len; ++n)
    {
        const float v = 0.1f * std::sin(2.0 * juce::MathConstants<double>::pi * baseHz * (double)n / sampleRate);
        out.setSample(0, n, v);
        out.setSample(1, n, v);
    }
    return out;
}
