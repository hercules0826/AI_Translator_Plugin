#include "Pipeline.h"
#include <cmath>

Pipeline::Pipeline(LockFreeRingBuffer& in, LockFreeRingBuffer& out, WhisperEngine& we)
: juce::Thread("Pipeline"), input(in), ttsOut(out), whisper(we)
{
    whisper.setCallback([this](const juce::String& text, const juce::String& lang){
        lastTranscript = text;
        logger.logMessage("ASR: " + text);

        const auto routed = translate(text, lang, outLang);
        juce::AudioBuffer<float> ttsBuf;
        synthTTS(routed, ttsBuf);

        // queue to output fifo
        ttsOut.push(ttsBuf.getReadPointer(0), ttsBuf.getNumSamples());
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
    if (! whisper.loadModel(model)) return;

    running.store(true);
    startThread();
}

void Pipeline::stop()
{
    running.store(false);
    stopThread(1000);
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
        whisper.process(20);

        // 3) sleep a little to avoid burning CPU
        juce::Thread::sleep(5);
    }
}

juce::String Pipeline::translate(const juce::String& txt, const juce::String& in, const juce::String& out)
{
    // TODO: replace with DeepL/Google or offline MarianMT/NLLB
    if (in == out) return {}; // obey “silence if same language”
    return txt; // pass-through for now (so we can test TTS chain)
}

void Pipeline::synthTTS(const juce::String& text, juce::AudioBuffer<float>& out)
{
    // Placeholder: short beep “speaks” per character count
    const double sr = sampleRate.load();
    const int len   = juce::jlimit(4800, 48000, text.length() * 1200);
    out.setSize(2, len);
    for (int n=0; n<len; ++n)
    {
        float v = 0.1f * std::sin(2.0 * juce::MathConstants<double>::pi * (220.0 + (text.length()%5)*30) * n / sr);
        out.setSample(0, n, v);
        out.setSample(1, n, v);
    }
}
