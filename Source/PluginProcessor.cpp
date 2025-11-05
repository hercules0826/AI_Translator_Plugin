// #include "PluginProcessor.h"
// #include "PluginEditor.h"

// //==============================================================================
// #define WHISPER_USE_DEPRECATED_API 1

// PluginAudioProcessor::PluginAudioProcessor()
//     : AudioProcessor(
//         BusesProperties()
// #if !JucePlugin_IsSynth
//         .withInput("Input", juce::AudioChannelSet::stereo(), true)
// #endif
//         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
// {
//     // Init audio FIFO buffer
//     audioFIFO.resize(fifoSize);
//     // --- Whisper CPU-only context ---
//     whisper_context_params wparams = whisper_context_default_params();
//     wparams.use_gpu = false;               // <— force CPU
//     wparams.dtw_token_timestamps = false;  // safer defaults for now
//     wparams.flash_attn = false;

//     // Init Whisper context (CPU backend)
//     whisperCtx = whisper_init_from_file_with_params("external/whisper.cpp/models/ggml-base.en.bin", wparams);

//     // Create & start background worker thread
//     whisperThread = std::make_unique<WhisperThread>(whisperCtx, fifo, audioFIFO);
//     whisperThread->startThread();
// }

// PluginAudioProcessor::~PluginAudioProcessor()
// {
//     // stop thread safely
//     whisperThread->stopThread(2000);

//     // free whisper memory
//     whisper_free(whisperCtx);
// }

// //==============================================================================

// void PluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
// {
//     juce::ignoreUnused(samplesPerBlock);
//     currentHostSR = sampleRate;
//     resampleRatio = whisperSampleRate / currentHostSR; // e.g., 16000/48000 = 0.3333
//     resampler.reset();
//     fifo.reset();
//     resampleBuf.resize((size_t)std::ceil(samplesPerBlock * resampleRatio) + 512);
// }

// void PluginAudioProcessor::releaseResources()
// {
// }

// bool PluginAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
// {
//     return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
// }

// //==============================================================================
// // Audio processing callback
// void PluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
//     juce::MidiBuffer& midiMessages)
// {
//     juce::ignoreUnused(midiMessages);

//     const int numSamples = buffer.getNumSamples();
//     const int numCh = buffer.getNumChannels();

//     // 1) downmix to mono
//     static thread_local std::vector<float> mono;
//     mono.resize((size_t)numSamples);
//     if (numCh == 1) {
//         memcpy(mono.data(), buffer.getReadPointer(0), numSamples * sizeof(float));
//     } else {
//         const float* L = buffer.getReadPointer(0);
//         const float* R = buffer.getReadPointer(1);
//         for (int i = 0; i < numSamples; ++i) mono[(size_t)i] = 0.5f * (L[i] + R[i]);
//     }

//     // 2) resample hostSR -> 16k
//     int outProduced = resampler.process(
//         resampleRatio,
//         mono.data(),
//         resampleBuf.data(),
//         numSamples
//     );

//     // 3) push into lock-free FIFO buffer
//     int s1, s2, e1, e2;
//     fifo.prepareToWrite(outProduced, s1, e1, s2, e2);

//     if (e1 > 0) memcpy(audioFIFO.data() + s1, resampleBuf.data(), e1 * sizeof(float));
//     if (e2 > 0) memcpy(audioFIFO.data() + s2, resampleBuf.data() + e1, e2 * sizeof(float));
//     fifo.finishedWrite(outProduced);

//     // 4) notify worker
//     whisperThread->newAudioAvailable.store(true, std::memory_order_release);
// }

// //==============================================================================
// // Plugin state � not used yet
// juce::AudioProcessorValueTreeState::ParameterLayout PluginAudioProcessor::createParameterLayout()
// {
//     return {};
// }

// juce::AudioProcessorEditor* PluginAudioProcessor::createEditor() 
// {
//     return new PluginAudioProcessorEditor (*this);
// }

// bool PluginAudioProcessor::acceptsMidi() const 
// {
//     return false;
// }

// bool PluginAudioProcessor::producesMidi() const 
// {
//     return false;
// }

// bool PluginAudioProcessor::isMidiEffect() const 
// {
//     return false;
// }

// //==============================================================================

// // Mandatory JUCE factory method for plugin export
// juce::AudioProcessor* JUCE_CALLTYPE  createPluginFilter()
// {
//     return new PluginAudioProcessor();
// }

#include "PluginProcessor.h"
#include "PluginEditor.h"

LiveTranslatorAudioProcessor::LiveTranslatorAudioProcessor()
: AudioProcessor (BusesProperties()
    .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
    .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{}

LiveTranslatorAudioProcessor::~LiveTranslatorAudioProcessor() = default;

void LiveTranslatorAudioProcessor::prepareToPlay (double sr, int)
{
    sampleRateHz = sr;

    pipeline = std::make_unique<Pipeline>(inFifo, outFifo);
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
}

void LiveTranslatorAudioProcessor::setLanguages(const juce::String& in, const juce::String& out)
{
    if (pipeline) pipeline->setLanguages(in, out);
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