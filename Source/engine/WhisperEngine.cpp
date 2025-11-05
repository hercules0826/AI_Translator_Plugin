#include "WhisperEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "whisper.h"
}

static bool energyGate(const float* x, size_t n, float thr) {
    double e = 0.0;
    for (size_t i = 0; i < n; ++i) { double s = x[i]; e += s*s; }
    e /= (double)std::max<size_t>(1, n);
    return e > thr;
}

WhisperEngine::WhisperEngine(LockFreeRingBuffer& ring16k,
                             MessageBus& b,
                             ITranslator& tr,
                             ITts& t,
                             const WhisperParams& p)
: ring16k(ring16k), bus(b), translator(tr), tts(t), params(p)
{
    whisper_context_params wcp = whisper_context_default_params();
    ctx = whisper_init_from_file_with_params(params.modelPath.c_str(), wcp);

    windowSamples = (size_t)(params.windowSec * 16000.0f);
    hopSamples    = (size_t)(params.hopSec    * 16000.0f);
    window.resize(windowSamples, 0.0f);
}

WhisperEngine::~WhisperEngine() { 
    stop();
    if (ctx) whisper_free(ctx);
}

void WhisperEngine::start() {
    if (running.exchange(true)) return;
    worker = std::thread(&WhisperEngine::threadFn, this);
}

void WhisperEngine::stop() {
    if (!running.exchange(false)) return;
    if (worker.joinable()) worker.join();
}

void WhisperEngine::threadFn() {
    constexpr size_t frame = 320; // 20 ms @ 16k
    std::vector<float> tmp(frame);

    size_t filled = 0;

    while (running.load()) {
        // pull 20ms when available (non-blocking)
        size_t got = ring16k.pop(tmp.data(), frame);
        if (got < frame) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        // shift window left by 'frame', append new frame
        if (filled + frame <= windowSamples) {
            std::memcpy(window.data() + filled, tmp.data(), frame * sizeof(float));
            filled += frame;
        } else {
            std::memmove(window.data(), window.data() + frame, (windowSamples - frame) * sizeof(float));
            std::memcpy(window.data() + (windowSamples - frame), tmp.data(), frame * sizeof(float));
            filled = windowSamples;
        }

        // Only fire when we've accumulated at least hop size since last decode
        static size_t sinceLast = 0;
        sinceLast += frame;
        if (sinceLast < hopSamples) continue;
        sinceLast = 0;

        if (filled < windowSamples) continue; // need full window initially

        // VAD-ish energy gate
        if (!energyGate(window.data(), window.size(), params.vadEnergy)) continue;

        // Run whisper on the 2s window
        whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
        wparams.print_realtime = false;
        wparams.print_progress = false;
        wparams.print_timestamps = false;
        wparams.no_context = true;       // streaming friendliness
        wparams.single_segment = true;   // one segment per call
        wparams.translate = false;       // do not auto-translate here
        wparams.language = "auto";       // autodetect

        if (whisper_full(ctx, wparams, window.data(), (int)window.size()) != 0)
            continue;

        int n = whisper_full_n_segments(ctx);
        if (n <= 0) continue;

        // Take the last (most recent) segment
        int si = n - 1;
        const char* ctext = whisper_full_get_segment_text(ctx, si);
        if (!ctext || !ctext[0]) continue;

        TranscriptMsg tmsg;
        tmsg.isFinal = true; // simple model: segment treated as final
        tmsg.text = ctext;
        bus.pushTranscript(tmsg);

        // Translate (blocking, quick)
        TranslateRequest tr;
        tr.text = tmsg.text;
        tr.srcLang = "auto";
        tr.dstLang = params.dstLang;
        std::string outText = translator.translate(tr);

        // Kick TTS (asynchronous chunks pushed to bus)
        TtsRequest req{ outText };
        tts.synthesize(req, [this](const std::vector<float>& chunk, bool eof){
            TtsPcmMsg m; m.pcm16k = chunk; m.eof = eof; bus.pushTts(m);
        });
    }
}

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
