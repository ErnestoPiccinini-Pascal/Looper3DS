#include "../include/Microphone.h"
#include <3ds.h>
#include <cstring> // per memset

Microphone::Microphone(size_t samples)
    : bufferSize(samples)
{
    // Alloca memoria allineata per il microfono (128-byte aligned)
    micBuffer = (u32*)linearMemAlign(bufferSize * sizeof(u32), 0x80);
}

Microphone::~Microphone()
{
    if(recording) stop();

    if(micBuffer) {
        linearFree(micBuffer);
        micBuffer = nullptr;
    }
}

void Microphone::start()
{
    if(recording) return;

    // Inizializza il microfono
    Result r = micInit((u8*)micBuffer, static_cast<u32>(bufferSize * sizeof(u32)));
    if(r != 0) return; // gestione semplice errori

    // Avvia il sampling
    r = MICU_StartSampling(
        MICU_ENCODING_PCM16_SIGNED,       // Codifica PCM16
        MICU_SAMPLE_RATE_16360,           // Frequenza 16.36 kHz
        0,                                // Flags
        *micBuffer,                        // Buffer u32
        static_cast<u32>(bufferSize * sizeof(u32))  // Dimensione in byte
    );

    if(r == 0) recording = true;
}

void Microphone::stop()
{
    if(!recording) return;

    MICU_StopSampling();   // Ferma il microfono
    micExit();             // Chiude il servizio microfono

    recording = false;
}
size_t Microphone::getBufferLength() {
    return bufferSize;
}
int16_t Microphone::getSample(size_t index){ 
    if(index < bufferSize) return micBuffer[index]; 
    return 0; 
    }