#include "../include/Microphone.h"
#include <3ds.h>
#include <malloc.h>
#include <cstring>
#include <cstdio>

Microphone::Microphone() : micBuffer(nullptr), micActualSize(0), initialized(false), isSampling(false) {
    bufferSizeBytes = 0x30000;
    length = bufferSizeBytes / sizeof(int16_t);
    micBuffer = (int16_t*)memalign(0x1000, bufferSizeBytes);
    if (micBuffer) memset(micBuffer, 0, bufferSizeBytes);
}

Microphone::~Microphone() {
    if (isSampling) MICU_StopSampling();
    if (initialized) micExit();
    if (micBuffer) free(micBuffer);
}

bool Microphone::init() {
    if (initialized) return true;
    micExit(); 
    Result res = micInit((u8*)micBuffer, bufferSizeBytes);
    if (R_FAILED(res)) return false;
    micActualSize = micGetSampleDataSize();
    initialized = true;
    return true;
}

bool Microphone::start() {
    if (!initialized) return false;
    if (isSampling) return true;
    
    MICU_SetPower(true);
    svcSleepThread(50000000); 

    Result res = MICU_StartSampling(MICU_ENCODING_PCM16_SIGNED, MICU_SAMPLE_RATE_16360, 0, micActualSize, true);
    if (R_SUCCEEDED(res)) {
        isSampling = true;
        return true;
    }
    return false;
}

void Microphone::stop() {
    if (micBuffer) DSP_InvalidateDataCache(micBuffer, bufferSizeBytes);
}

int16_t Microphone::getSample(size_t index) const {
    if (!micBuffer || index >= length) return 0;
    return micBuffer[index];
}
u32 Microphone::getCurrentOffset() const {
    if (!initialized) return 0;
    // Questa funzione dice esattamente dove sta scrivendo il microfono ADESSO
    return micGetLastSampleOffset();
}