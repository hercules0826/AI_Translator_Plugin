#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "engine/Pipeline.h"

LiveTranslatorAudioProcessor::LiveTranslatorAudioProcessor()
: AudioProcessor (BusesProperties()
    .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
    .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    WhisperParams p;
    p.modelPath = "Source/external/whisper.cpp/models/ggml-base.en.bin";
    p.dstLang = "en";

    whisper = std::make_unique<WhisperEngine>(input16k, bus, translator, tts, p);
    whisper->start();
}

LiveTranslatorAudioProcessor::~LiveTranslatorAudioProcessor() {
    if (whisper) whisper->stop();
};

void LiveTranslatorAudioProcessor::prepareToPlay (double sr, int)
{
    resampler.reset();
    
    sampleRateHz = sr;

    pipeline = std::make_unique<Pipeline>(inFifo, outFifo, *whisper, *this);
    pipeline->setLanguages("auto", "en");
    pipeline->start(juce::File("Source/external/whisper.cpp/models/ggml-base.en.bin"));
    
}

void LiveTranslatorAudioProcessor::releaseResources()
{
    if (pipeline) pipeline->stop();
    pipeline.reset();
}

bool LiveTranslatorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void LiveTranslatorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // 1) enqueue input for ASR
    pipeline->pushAudioFromDSP(buffer.getReadPointer(0), buffer.getNumSamples(),
                               buffer.getNumChannels(), sampleRateHz);

    // 2) mix any pending TTS audio onto output
    static std::vector<float> tmp(4800*2);
    const int frames = buffer.getNumSamples();
    if ((int)tmp.size() < frames * buffer.getNumChannels())
        tmp.resize(frames * buffer.getNumChannels());

    size_t got = outFifo.pop(tmp.data(), (size_t) frames);
    if (got > 0)
    {
        for (int ch=0; ch<buffer.getNumChannels(); ++ch)
        {
            auto* dst = buffer.getWritePointer(ch);
            for (size_t i=0; i<got; ++i)
                dst[i] += tmp[i*buffer.getNumChannels() + ch]; // mix under
        }
    }
    const int numCh = buffer.getNumChannels();
    const int N = buffer.getNumSamples();
    const double hostSR = getSampleRate();

    // 1) downmix to mono (scratch)
    scratch.setSize(1, N, false, false, true);
    auto* mono = scratch.getWritePointer(0);
    if (numCh == 1) {
        std::memcpy(mono, buffer.getReadPointer(0), sizeof(float)*N);
    } else {
        for (int i = 0; i < N; ++i) {
            double s = 0.0;
            for (int c = 0; c < numCh; ++c) s += buffer.getReadPointer(c)[i];
            mono[i] = (float)(s / (double)numCh);
        }
    }

    // 2) resample to 16k and push to ring
    resampler.processTo16k(mono, N, hostSR, mono16k);
    if (!mono16k.empty())
        input16k.push(mono16k.data(), mono16k.size());

    // 3) Pull any TTS chunks and mix to output
    //    (convert back to host SR & stereo)
    TtsPcmMsg msg;
    std::vector<float> outMono;
    while (bus.popTts(msg)) {
        resampler.processFrom16k(msg.pcm16k.data(), (int)msg.pcm16k.size(), hostSR, ttsOutMono);
        // mix to all channels softly
        for (int c = 0; c < numCh; ++c) {
            auto* ch = buffer.getWritePointer(c);
            for (size_t i = 0; i < std::min<size_t>(ttsOutMono.size(), (size_t)N); ++i) {
                ch[i] += ttsOutMono[i]; // simple add; later use gain/env
            }
        }
    }
}

void LiveTranslatorAudioProcessor::setLanguages(const juce::String& in, const juce::String& out)
{
    if (pipeline) pipeline->setLanguages(in, out);
}

void LiveTranslatorAudioProcessor::setAutoDetect(bool enabled)
{
    autoDetect.store(enabled);
    if (pipeline) pipeline->setAutoDetect(enabled); // add stub in Pipeline (no-op ok)
}

juce::String LiveTranslatorAudioProcessor::getLastTranscript() const
{
    //std::scoped_lock lock(textMx);
    const juce::ScopedLock sl(textMx);
    return lastTranscript;
}

void LiveTranslatorAudioProcessor::appendDebug(const juce::String& line)
{
    /*std::scoped_lock lock(textMx);
    pendingDebug << line << "\n";*/
    const juce::ScopedLock sl(textMx);
    pendingDebug << line << "\n";
}

juce::String LiveTranslatorAudioProcessor::pullDebugSinceLast()
{
    //std::scoped_lock lock(textMx);
    const juce::ScopedLock sl(textMx);
    auto out = pendingDebug;
    pendingDebug.clear();
    return out;
}

juce::AudioProcessorValueTreeState::ParameterLayout LiveTranslatorAudioProcessor::createParameterLayout()
{
    return {};
}

juce::AudioProcessorEditor* LiveTranslatorAudioProcessor::createEditor() 
{
    return new LiveTranslatorAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LiveTranslatorAudioProcessor();
}