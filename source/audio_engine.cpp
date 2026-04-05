#include "audio_engine.h"
#include <cmath>
#include <cstring>

AudioEngine::AudioEngine(size_t loopSamples)
    : loopBuffer(loopSamples, 0) {}

AudioEngine::~AudioEngine() {
    ndspExit();
}

void AudioEngine::init() {
    ndspInit();

    ndspSetOutputMode(NDSP_OUTPUT_MONO);
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, 22050);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);

    memset(waveBuf, 0, sizeof(waveBuf));
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
    ndspWaveBuf* buf = &waveBuf[currentBuf];

    if (buf->status == NDSP_WBUF_DONE || buf->status == 0) {
        for (int i = 0; i < 512; i++) {
            mixBuffers[currentBuf][i] = loopBuffer[playhead];
            playhead = (playhead + 1) % loopBuffer.size();
        }

        DSP_FlushDataCache(mixBuffers[currentBuf], sizeof(mixBuffers[currentBuf]));

        buf->data_vaddr = mixBuffers[currentBuf];
        buf->nsamples = 512;

        ndspChnWaveBufAdd(0, buf);

        currentBuf ^= 1;
    }
}