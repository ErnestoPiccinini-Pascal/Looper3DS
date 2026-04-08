#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <3ds.h>

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool init(int seconds);
    void addSample(size_t index, int16_t newSample);
    void clearBuffer();
    void play();

    size_t getSampleCount() const { return totalSamples; }

private:
    int16_t* loopBuffer;
    size_t totalSamples;
    size_t bufferSizeInBytes;
    ndspWaveBuf waveBuf;
};

#endif