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

    void setSample(size_t index, int16_t value);
    void resetPlayback();
    void flush();

private:
    int16_t* loopBuffer;
    size_t bufferSize;
    size_t playhead = 0;

    ndspWaveBuf waveBuf[1];
    int currentBuf = 0;
};
