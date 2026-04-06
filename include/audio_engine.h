#pragma once
#include <3ds.h>
#include <cstdint>

class AudioEngine {
public:
    AudioEngine(size_t loopSamples);
    ~AudioEngine();

    void init();
    void update();
    void clear();
    // void fillTestTone(); // commentato, useremo microfono
    void setSample(size_t index, int16_t value);
private:
    int16_t* loopBuffer = nullptr;   // Allocazione lineare
    size_t loopSize = 0;
    size_t playhead = 0;

    ndspWaveBuf waveBuf[2];
    int16_t mixBuffers[2][512];
    int currentBuf = 0;
};