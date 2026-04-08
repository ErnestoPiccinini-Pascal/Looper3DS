#include "../include/audio_engine.h"
#include <cstring>

AudioEngine::AudioEngine() : loopBuffer(nullptr), totalSamples(0) {
    ndspInit();
}

AudioEngine::~AudioEngine() {
    if (loopBuffer) linearFree(loopBuffer);
    ndspExit();
}

bool AudioEngine::init(int seconds) {
    if (loopBuffer) {
        linearFree(loopBuffer);
        loopBuffer = nullptr;
    }

    bufferSizeInBytes = seconds * 32730 * sizeof(int16_t); // Cambiato a 32730
    totalSamples = bufferSizeInBytes / 2;
    
    loopBuffer = (int16_t*)linearAlloc(bufferSizeInBytes);
    if (!loopBuffer) return false;
    
    memset(loopBuffer, 0, bufferSizeInBytes);

    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, 32730.0f); // Cambiato a 32730.0f
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);

    // Imposta volume massimo
    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0f; mix[1] = 1.0f;
    ndspChnSetMix(0, mix);

    memset(&waveBuf, 0, sizeof(ndspWaveBuf));
    waveBuf.data_vaddr = loopBuffer;
    waveBuf.nsamples = totalSamples;
    waveBuf.looping = true;
    waveBuf.status = NDSP_WBUF_DONE;
    
    return true;
}

void AudioEngine::addSample(size_t index, int16_t newSample) {
    if (index >= totalSamples) return;
    // Boost volume x12 per sentire bene la registrazione
    int32_t mixed = (int32_t)loopBuffer[index] + ((int32_t)newSample * 12);
    if (mixed > 32767) mixed = 32767;
    if (mixed < -32768) mixed = -32768;
    loopBuffer[index] = (int16_t)mixed;
}

void AudioEngine::clearBuffer() {
    if (loopBuffer) {
        memset(loopBuffer, 0, bufferSizeInBytes);
        DSP_FlushDataCache(loopBuffer, bufferSizeInBytes);
    }
}

void AudioEngine::play() {
    if (!loopBuffer) return;
    DSP_FlushDataCache(loopBuffer, bufferSizeInBytes);
    if (waveBuf.status == NDSP_WBUF_DONE) {
        ndspChnWaveBufAdd(0, &waveBuf);
    }
}