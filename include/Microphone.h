#pragma once
#include <3ds.h>
#include <cstdint>
#include <vector>

class Microphone {
public:
    Microphone(size_t samples);
    ~Microphone();

    void start();   // Inizia la registrazione
    void stop();    // Ferma la registrazione
    bool isRecording() const { return recording; }

    u32* getBuffer() const { return micBuffer; }
    size_t getBufferSize() const { return bufferSize; }
    size_t getBufferLength() ;
    int16_t getSample(size_t index);


private:
    u32* micBuffer = nullptr;      // Buffer audio (u32 per MICU)
    size_t bufferSize = 0;         // Numero di campioni
    bool recording = false;
};