#pragma once
#include <3ds.h>
#include <vector>
#include <cstdint>

class AudioEngine {
public:
    AudioEngine(size_t loopSamples);
    ~AudioEngine();

    void init();
    void update();
    void fillTestTone();
    void clear();

private:
    std::vector<int16_t> loopBuffer;
    size_t playhead = 0;

    ndspWaveBuf waveBufs[2];

    // buffer NDSP in linear memory
    int16_t* mixBuffers[2] = { nullptr, nullptr };

    int currentBuf = 0;
};