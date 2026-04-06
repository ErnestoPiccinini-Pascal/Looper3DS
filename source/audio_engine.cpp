#include "../include/audio_engine.h"
#include <cmath>
#include <cstring>
#include <cstdio>

AudioEngine::AudioEngine(size_t loopSamples)
    : loopBuffer(loopSamples, 0) {}

AudioEngine::~AudioEngine() {
    if (mixBuffers[0]) linearFree(mixBuffers[0]);
    if (mixBuffers[1]) linearFree(mixBuffers[1]);

    ndspExit();
}

void AudioEngine::init() {
    ndspInit();

    ndspSetOutputMode(NDSP_OUTPUT_MONO);
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, 22050);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);

    mixBuffers[0] = (int16_t*)linearAlloc(sizeof(int16_t) * 512);
    mixBuffers[1] = (int16_t*)linearAlloc(sizeof(int16_t) * 512);

    memset(mixBuffers[0], 0, sizeof(int16_t) * 512);
    memset(mixBuffers[1], 0, sizeof(int16_t) * 512);

    memset(waveBufs, 0, sizeof(waveBufs));
}

void AudioEngine::fillTestTone() {
    for (size_t i = 0; i < loopBuffer.size(); i++) {
        float t = static_cast<float>(i) / 22050.0f;
        loopBuffer[i] = static_cast<int16_t>(
            std::sin(2.0f * 3.14159f * 440.0f * t) * 12000
        );
    }
}

void AudioEngine::clear() {
    std::fill(loopBuffer.begin(), loopBuffer.end(), 0);
}

void AudioEngine::update() {
    ndspWaveBuf* buf = &waveBufs[currentBuf];

    if (buf->status == NDSP_WBUF_DONE || buf->status == NDSP_WBUF_FREE) {
        for (int i = 0; i < 512; i++) {
            mixBuffers[currentBuf][i] = loopBuffer[playhead];
            playhead = (playhead + 1) % loopBuffer.size();
        }

        DSP_FlushDataCache(mixBuffers[currentBuf], sizeof(int16_t) * 512);

        buf->data_pcm16 = mixBuffers[currentBuf];
        buf->nsamples = 512;

        ndspChnWaveBufAdd(0, buf);

        currentBuf ^= 1;
    }
}