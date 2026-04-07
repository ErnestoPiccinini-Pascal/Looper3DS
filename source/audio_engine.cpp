#include "../include/audio_engine.h"
#include <3ds.h>
#include <cstring>
#include <malloc.h>

AudioEngine::AudioEngine(size_t size) : loopBuffer(nullptr), bufferSize(size) {
    // Allocazione in memoria lineare (necessaria per il DSP)
    loopBuffer = (int16_t*)linearAlloc(bufferSize * sizeof(int16_t));
    if (loopBuffer) memset(loopBuffer, 0, bufferSize * sizeof(int16_t));
}

AudioEngine::~AudioEngine() {
    ndspExit();
    if (loopBuffer) linearFree(loopBuffer);
}

void AudioEngine::init() {
    ndspInit();
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, 16360.0f);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);

    // Inizializzazione della struct waveBuf
    memset(&waveBuf[0], 0, sizeof(ndspWaveBuf));
    waveBuf[0].data_vaddr = loopBuffer;
    waveBuf[0].nsamples = bufferSize;
    waveBuf[0].looping = true;
    waveBuf[0].status = NDSP_WBUF_DONE; 
}

void AudioEngine::setSample(size_t index, int16_t sample) {
    if (loopBuffer && index < bufferSize) {
        loopBuffer[index] = sample;
    }
}

void AudioEngine::flush() {
    if (!loopBuffer) return;
    
    // Sincronizza la cache prima di dare i dati al DSP
    DSP_FlushDataCache(loopBuffer, bufferSize * sizeof(int16_t));
    
    // Aggiunge il buffer al canale se è libero
    if (waveBuf[0].status == NDSP_WBUF_DONE) {
        ndspChnWaveBufAdd(0, &waveBuf[0]);
    }
}

// Questa è la funzione che mancava o era scritta male!
void AudioEngine::update() {
    // NDSP gestisce il loop hardware, quindi qui non serve logica pesante,
    // ma la funzione DEVE esistere perché il main la chiama.
}