#ifndef MICROPHONE_H
#define MICROPHONE_H

#include <3ds.h>
#include <cstdint>

class Microphone {
public:
    Microphone();
    ~Microphone();

    bool init();     
    bool start();    
    void stop();     
    u32 getCurrentOffset() const; // <--- AGGIUNTA
    
    int16_t getSample(size_t index) const; 
    size_t getBufferLength() const { return length; }

private:
    int16_t* micBuffer;  
    u32 micActualSize;
    size_t bufferSizeBytes;
    size_t length;
    bool initialized;
    bool isSampling; 
};

#endif