#include "AzureTTS.h"
#include <juce_audio_formats/juce_audio_formats.h>

// -------- Voice selection helper ----------
static AzureVoiceProfile pickDefaultVoice(const juce::String& lang,
    const juce::String& gender)
{
    const auto L = lang.toLowerCase();

    if (L == "en")
    {
        if (gender == "Male")    return { "en-US-GuyNeural", "Conversational", "" };
        if (gender == "Neutral") return { "en-US-AriaNeural", "Conversational", "" };
        return                   { "en-US-JennyNeural", "Conversational", "" };
    }

    if (L == "de" || L == "gsw")
    {
        if (gender == "Male")    return { "de-DE-ConradNeural", "Broadcast", "" };
        if (gender == "Neutral") return { "de-DE-SeraphinaMultilingualNeural", "Broadcast", "" };
        return                   { "de-DE-KatjaNeural", "Broadcast", "" };
    }

    if (L == "fr")
    {
        if (gender == "Male")    return { "fr-FR-HenriNeural", "Elegant", "" };
        if (gender == "Neutral") return { "fr-FR-AlainNeural", "Elegant", "" };
        return                   { "fr-FR-DeniseNeural", "Elegant", "" };
    }

    if (L == "it")
    {
        if (gender == "Male")    return { "it-IT-DiegoNeural", "Warm", "" };
        if (gender == "Neutral") return { "it-IT-IsabellaNeural", "Warm", "" };
        return                   { "it-IT-ElsaNeural", "Warm", "" };
    }

    // fallback
    return { "en-US-JennyNeural", "Conversational", "" };
}

AzureVoiceProfile AzureTTS::pickVoice(const juce::String& lang,
    const juce::String& gender,
    const juce::String& style) const
{
    auto v = pickDefaultVoice(lang, gender);
    if (style.isNotEmpty())
        v.style = style;
    return v;
}

// --------- SSML builder ----------
juce::String AzureTTS::buildSsml(const juce::String& text,
    const AzureVoiceProfile& v) const
{
    return "<speak version='1.0' xml:lang='en-US'>"
        "<voice name='" + v.name + "'>"
        + juce::String(text).replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        + "</voice></speak>";
}

// --------- Main synthesize() override ----------
void AzureTTS::synthesize(const TtsRequest& req,
    std::function<void(const std::vector<float>&, bool)> onChunk)
{
    if (azureKey.isEmpty() || azureRegion.isEmpty() || req.text.empty())
    {
        onChunk({}, true);
        return;
    }

    AzureVoiceProfile voice = pickDefaultVoice("en", "Female");
    juce::String ssml = buildSsml(req.text, voice);

    juce::URL url("https://" + azureRegion + ".tts.speech.microsoft.com/cognitiveservices/v1");

    // ✅ Build header string manually (old JUCE API requirement)
    juce::String headerString;
    headerString << "Ocp-Apim-Subscription-Key: " << azureKey << "\r\n";
    headerString << "Content-Type: application/ssml+xml\r\n";
    headerString << "X-Microsoft-OutputFormat: riff-16000hz-16bit-mono-pcm\r\n";

    // ✅ Use old JUCE createInputStream() signature
    std::unique_ptr<juce::InputStream> stream(
        url.withPOSTData(ssml)
        .createInputStream(true,           // use POST
            nullptr,         // no progress callback
            nullptr,         // no context
            headerString,    // ✅ MUST be juce::String, not StringPairArray
            0)               // no timeout
    );

    if (!stream)
    {
        onChunk({}, true);
        return;
    }

    juce::MemoryBlock wavData;
    stream->readIntoMemoryBlock(wavData);

    juce::MemoryInputStream mis(wavData, false);
    juce::WavAudioFormat fmt;
    auto reader = std::unique_ptr<juce::AudioFormatReader>(fmt.createReaderFor(&mis, true));

    if (!reader)
    {
        onChunk({}, true);
        return;
    }

    int N = (int)reader->lengthInSamples;
    juce::AudioBuffer<float> buffer(1, N);
    reader->read(&buffer, 0, N, 0, true, false);

    std::vector<float> pcm(N);
    memcpy(pcm.data(), buffer.getReadPointer(0), N * sizeof(float));

    onChunk(pcm, true);
}


