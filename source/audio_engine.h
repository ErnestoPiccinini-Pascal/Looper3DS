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

    ndspWaveBuf waveBuf[2];
    int16_t mixBuffers[2][512];

    int currentBuf = 0;
};