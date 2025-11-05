#pragma once
#include <atomic>
#include <cstddef>
#include <cstring>

// Single-Producer / Single-Consumer lock-free ring for float audio
class LockFreeRingBuffer
{
public:
    explicit LockFreeRingBuffer(size_t capacityFrames, int numChannels)
        : channels(numChannels),
          capacity(capacityFrames),
          writeIndex(0), readIndex(0)
    {
        buffer.resize(capacity * channels);
    }

    size_t push(const float* interleaved, size_t frames)
    {
        size_t written = 0;
        while (written < frames)
        {
            size_t w = writeIndex.load(std::memory_order_relaxed);
            size_t r = readIndex.load(std::memory_order_acquire);
            size_t freeFrames = capacity - (w - r);
            if (freeFrames == 0) break;

            size_t chunk = std::min(frames - written, freeFrames);
            size_t wPos = w % capacity;

            size_t firstPart = std::min(chunk, capacity - wPos);
            copyFrames(&buffer[(wPos)*channels], &interleaved[written*channels], firstPart);

            if (chunk > firstPart)
                copyFrames(&buffer[0], &interleaved[(written+firstPart)*channels], chunk - firstPart);

            writeIndex.store(w + chunk, std::memory_order_release);
            written += chunk;
        }
        return written;
    }

    size_t pop(float* interleaved, size_t frames)
    {
        size_t read = 0;
        while (read < frames)
        {
            size_t r = readIndex.load(std::memory_order_relaxed);
            size_t w = writeIndex.load(std::memory_order_acquire);
            size_t available = (w - r);
            if (available == 0) break;

            size_t chunk = std::min(frames - read, available);
            size_t rPos = r % capacity;

            size_t firstPart = std::min(chunk, capacity - rPos);
            copyFrames(&interleaved[read*channels], &buffer[(rPos)*channels], firstPart);

            if (chunk > firstPart)
                copyFrames(&interleaved[(read+firstPart)*channels], &buffer[0], chunk - firstPart);

            readIndex.store(r + chunk, std::memory_order_release);
            read += chunk;
        }
        return read;
    }

    void clear() {
        writeIndex.store(0, std::memory_order_release);
        readIndex.store(0, std::memory_order_release);
    }

    size_t capacityFrames() const { return capacity; }

private:
    void copyFrames(float* dst, const float* src, size_t frames)
    {
        std::memcpy(dst, src, frames * channels * sizeof(float));
    }

    int channels;
    size_t capacity;
    std::vector<float> buffer;
    std::atomic<size_t> writeIndex, readIndex;
};
