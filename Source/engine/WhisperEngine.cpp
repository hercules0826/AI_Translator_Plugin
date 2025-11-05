#include "WhisperEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "whisper.h"
}

WhisperEngine::WhisperEngine() {}
WhisperEngine::~WhisperEngine() { reset(); }

bool WhisperEngine::loadModel(const juce::File& path)
{
    reset();
    if (! path.existsAsFile()) return false;

    ctx = whisper_init_from_file(path.getFullPathName().toRawUTF8());
    ready.store(ctx != nullptr);
    return ready.load();
}

void WhisperEngine::reset()
{
    if (ctx)
    {
        whisper_free(ctx);
        ctx = nullptr;
    }
    ready.store(false);
    ring.setSize(1, 0);
    ringWrite = 0;
}

void WhisperEngine::setLanguage(const juce::String& langCode)
{
    currentLang = langCode; // "auto" handled in transcribe
}

void WhisperEngine::ensureCapacity(int samples)
{
    if (ring.getNumSamples() < samples)
        ring.setSize(1, samples, true, true, true);
}

void WhisperEngine::pushAudio(const float* interleaved, int frames, int numChannels, double sr)
{
    if (! ready.load() || frames <= 0) return;

    // Downmix to mono, resample to 16k (cheap, acceptable for now)
    const int monoSamples = frames;
    downmixMono.setSize(1, monoSamples, false, false, true);
    auto* mono = downmixMono.getWritePointer(0);

    if (numChannels == 1)
        std::memcpy(mono, interleaved, sizeof(float) * monoSamples);
    else
    {
        for (int i = 0; i < monoSamples; ++i)
            mono[i] = 0.5f * (interleaved[2*i] + interleaved[2*i + 1]);
    }

    // crude resample to 16k using JUCE Lagrange
    static juce::LagrangeInterpolator resampler;
    const double ratio = 16000.0 / std::max(1.0, sr);
    const int target = (int) std::ceil(monoSamples * ratio) + 8;

    juce::AudioBuffer<float> resOut(1, target);
    const int done = resampler.process(ratio, mono, resOut.getWritePointer(0), monoSamples);

    const int n = done;
    const juce::ScopedLock sl(ringLock);
    ensureCapacity(ringWrite + n + 1);
    auto* w = ring.getWritePointer(0);
    std::memcpy(&w[ringWrite], resOut.getReadPointer(0), sizeof(float) * n);
    ringWrite += n;
}

void WhisperEngine::process(int maxMsPerTick)
{
    if (! ready.load()) return;

    int kChunk = 16000 * 1; // 1 second chunks at 16k
    int localWrite;
    {
        const juce::ScopedLock sl(ringLock);
        localWrite = ringWrite;
        if (localWrite < kChunk) return;
    }

    // copy out one chunk to avoid holding lock during inference
    juce::HeapBlock<float> chunk(kChunk);
    {
        const juce::ScopedLock sl(ringLock);
        std::memcpy(chunk.getData(), ring.getReadPointer(0), sizeof(float) * kChunk);
        // shift remaining
        auto* w = ring.getWritePointer(0);
        std::memmove(w, &w[kChunk], sizeof(float) * (ringWrite - kChunk));
        ringWrite -= kChunk;
    }

    transcribeChunk(chunk.getData(), kChunk);
}

void WhisperEngine::transcribeChunk(const float* mono16k, int numSamples)
{
    whisper_full_params p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    p.n_threads = std::max(1u, std::thread::hardware_concurrency()/2);
    p.print_realtime = false;
    p.print_progress = false;
    p.single_segment = true;    // treat this chunk as one segment
    p.no_timestamps  = true;

    if (currentLang != "auto")
        p.language = currentLang.toRawUTF8();
    else
        p.language = nullptr; // auto-detect

    if (whisper_full(ctx, p, mono16k, numSamples) != 0)
        return;

    const int n = whisper_full_n_segments(ctx);
    juce::String out;
    for (int i = 0; i < n; ++i)
        out += juce::String(whisper_full_get_segment_text(ctx, i));

    if (onTranscript && out.isNotEmpty())
        onTranscript(out.trim(), currentLang == "auto" ? "auto" : currentLang);
}
