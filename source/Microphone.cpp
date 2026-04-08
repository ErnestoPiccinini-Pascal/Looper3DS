#include "../include/Microphone.h"
#include <malloc.h>
#include <cstring>

Microphone::Microphone() : micBuffer(nullptr), currentSamples(0), initialized(false) {}

Microphone::~Microphone() {
    if (initialized) {
        MICU_StopSampling();
        micExit();
    }
    if (micBuffer) linearFree(micBuffer); 
}

bool Microphone::init(int seconds) {
    if (micBuffer) {
        linearFree(micBuffer);
        micBuffer = nullptr;
    }
    
    // Frequenza 32728Hz
    size_t size = seconds * 32730 * 2; // Cambiato a 32730
    currentSamples = size / 2;
    
    micBuffer = (u8*)linearAlloc(size);
    if (!micBuffer) return false;
    
    memset(micBuffer, 0, size);
    micExit(); 
    
    Result res = micInit(micBuffer, size);
    if (R_FAILED(res)) return false;
    
    initialized = true;
    return true;
}

void Microphone::start() {
    if (!initialized) return;
    
    // Pulizia buffer
    memset(micBuffer, 0, currentSamples * 2);
    DSP_FlushDataCache(micBuffer, currentSamples * 2);

    MICU_SetPower(true);
    svcSleepThread(100000000); // 100ms stabilità hardware
    
    u32 dataSize = currentSamples * 2;
    // Uilizza MICU_SAMPLE_RATE_32730
    MICU_StartSampling(MICU_ENCODING_PCM16_SIGNED, MICU_SAMPLE_RATE_32730, 0, dataSize, true);
}

void Microphone::stop() {
    if (!initialized) return;
    MICU_StopSampling();
    // Forza la CPU a leggere i dati scritti dal microfono nella RAM
    DSP_InvalidateDataCache(micBuffer, currentSamples * 2);
}

u32 Microphone::getLastOffset() const {
    if (!initialized) return 0;
    return micGetLastSampleOffset();
}