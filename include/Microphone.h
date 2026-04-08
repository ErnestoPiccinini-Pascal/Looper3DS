#ifndef MICROPHONE_H
#define MICROPHONE_H

#include <3ds.h>

class Microphone {
public:
    Microphone();
    ~Microphone();

    bool init(int seconds);
    void start();
    void stop();
    
    u32 getLastOffset() const; 
    int16_t* getBuffer() { return (int16_t*)micBuffer; }
    size_t getSampleCount() const { return currentSamples; }

private:
    u8* micBuffer;
    size_t currentSamples;
    bool initialized;
};

#endif