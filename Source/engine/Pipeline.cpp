#include "Pipeline.h"
#include <cmath>
#include "WhisperEngine.h"
#include "../PluginProcessor.h" // for appendDebug()

Pipeline::Pipeline(LockFreeRingBuffer& in, LockFreeRingBuffer& out, WhisperEngine& we, LiveTranslatorAudioProcessor& o)
: juce::Thread("Pipeline"), input(in), ttsOut(out), whisper(we), owner(o)
{
    whisper.setCallback([this](const juce::String& text, const juce::String& lang){
        // lastTranscript = text;
        owner.appendDebug("ASR: " + text); // owner = processor reference you pass in
        {
            const juce::ScopedLock sl(owner.textMx);
            owner.lastTranscript = text;
        }
        logger.logMessage("ASR: " + text);

        const auto routed = translate(text, lang, outLang);
        if (routed.isEmpty())
            return;

        juce::AudioBuffer<float> ttsBuf;
        synthTTS(routed, ttsBuf);
        if (ttsBuf.getNumSamples() > 0)
            ttsOut.push(ttsBuf.getReadPointer(0), (size_t) ttsBuf.getNumSamples());

        // queue to output fifo
        // ttsOut.push(ttsBuf.getReadPointer(0), ttsBuf.getNumSamples());
    });
}

Pipeline::~Pipeline() { stop(); }

void Pipeline::setLanguages(const juce::String& in, const juce::String& out)
{
    inLang = in; outLang = out;
    whisper.setLanguage(inLang);
}

void Pipeline::start(const juce::File& model)
{
    if (! whisper.loadModel(model)) 
    {
        owner.appendDebug("Whisper: failed to load model\n");
        return;
    }
    running.store(true);
    startThread();
    owner.appendDebug("Pipeline: started\n");
}

void Pipeline::stop()
{
    running.store(false);
    stopThread(1000);
    owner.appendDebug("Pipeline: stopped\n");
}

void Pipeline::pushAudioFromDSP(const float* interleaved, int frames, int channels, double sr)
{
    numCh.store(channels);
    sampleRate.store(sr);
    input.push(interleaved, (size_t) frames);
}

void Pipeline::run()
{
    const int block = 480; // ~10ms at 48k
    std::vector<float> tmp(block * (size_t)numCh.load());

    while (running.load())
    {
        // 1) pull small chunks from FIFO and feed whisper (it accumulates to 1s)
        const size_t got = input.pop(tmp.data(), block);
        if (got > 0)
            whisper.pushAudio(tmp.data(), (int)got, numCh.load(), sampleRate.load());

        // 2) let whisper try a decode tick
        // whisper.process(20);
        whisper.process(decodeBudgetMs.load());

        // 3) sleep a little to avoid burning CPU
        juce::Thread::sleep(5);
    }
}

juce::String Pipeline::translate(const juce::String& txt, const juce::String& in, const juce::String& out)
{
    // TODO: replace with DeepL/Google or offline MarianMT/NLLB
    if (in == out) return {}; // obey “silence if same language”
    // TODO: integrate actual translator (DeepL / Azure Translator / Marian/NLLB)
    TranslateRequest req;
    req.text = txt.toStdString();
    req.srcLang = in.toStdString();
    req.dstLang = out.toStdString();

    auto result = translator.translate(req);
    owner.appendDebug("Translate: " + txt + " -> " + result);
    return juce::String(result);

}

void Pipeline::synthTTS(const juce::String& text, juce::AudioBuffer<float>& out)
{
    auto outCode = outLang.toLowerCase();
    if (outCode == "gsw") outCode = "de";

    auto voice = tts.pickVoice(outCode,
                            owner.getVoiceGender(),
                            owner.getVoiceStyle());

    std::vector<float> pcm;
    TtsRequest req;
    req.text = text.toStdString();

    tts.synthesize(req,
        [this](const std::vector<float>& pcm, bool eof)
        {
            whisper.getBus().pushTts(TtsPcmMsg{ pcm, eof });
        });
}

void Pipeline::setAutoDetect(bool enabled)
{
    // For future: if (enabled) whisper.enableAutoLanguage();
    // else whisper.setLanguage(inLang);
    (void)enabled;
}

