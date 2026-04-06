#include "../include/audio_engine.h"
#include <cstring>




AudioEngine::AudioEngine(size_t loopSamples)
    : loopSize(loopSamples)
{
    // Allocazione memoria lineare per il loop
    loopBuffer = (int16_t*)linearMemAlign(loopSize * sizeof(int16_t), 0x80);
}

AudioEngine::~AudioEngine()
{
    if(loopBuffer) {
        linearFree(loopBuffer);
        loopBuffer = nullptr;
    }
}

void AudioEngine::init()
{
    ndspInit();
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    for(int i = 0; i < 2; i++) {
        waveBuf[i].data_vaddr = mixBuffers[i];
        waveBuf[i].nsamples = 512;
        waveBuf[i].status = NDSP_WBUF_DONE;
        ndspChnWaveBufAdd(0, &waveBuf[i]);
    }
}

void AudioEngine::update()
{
    // TODO: mix del buffer microfono nel loop
}
void AudioEngine::setSample(size_t index, int16_t value) {
    if(index < loopSize)
        loopBuffer[index] = value;
}
void AudioEngine::clear()
{
    if(loopBuffer)
        memset(loopBuffer, 0, loopSize * sizeof(int16_t));
}