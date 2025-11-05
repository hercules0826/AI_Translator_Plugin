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
    return txt; // pass-through for now (so we can test TTS chain)
}

void Pipeline::synthTTS(const juce::String& text, juce::AudioBuffer<float>& out)
{
    // // Placeholder: short beep “speaks” per character count
    // const double sr = sampleRate.load();
    // const int len   = juce::jlimit(4800, 48000, text.length() * 1200);
    // out.setSize(2, len);
    // for (int n=0; n<len; ++n)
    // {
    //     float v = 0.1f * std::sin(2.0 * juce::MathConstants<double>::pi * (220.0 + (text.length()%5)*30) * n / sr);
    //     out.setSample(0, n, v);
    //     out.setSample(1, n, v);
    // }
    //------------------------------------------------------------------------------------------------------------ 
    // if (!azureTTS)
    //     azureTTS = std::make_unique<AzureTTS>("YOUR_KEY", "YOUR_REGION");

    // if (!azureTTS->synth(text, outLang, out))
    // {
    //     // fallback beep if Azure fails
    //     out.setSize(1, 16000);
    //     for (int i = 0; i < 16000; i++)
    //         out.setSample(0, i, 0.1f * std::sin(2.0 * juce::MathConstants<double>::pi * 440 * i / 16000.0));
    // }
    //-------------------------------------------------------------------------------------------------------------
    auto outCode = outLang.toLowerCase();
    if (outCode == "gsw") outCode = "de"; // route GSW → DE voice bank

    const auto voice = tts.pickVoice(outCode, owner.getVoiceGender(), owner.getVoiceStyle());
    out = tts.synthesizeStreaming(text, voice, sampleRate.load());
}

void Pipeline::setAutoDetect(bool enabled)
{
    // For future: if (enabled) whisper.enableAutoLanguage();
    // else whisper.setLanguage(inLang);
    (void)enabled;
}

