#include <3ds.h>
#include <cstdio>
#include "../include/audio_engine.h"
#include "../include/Microphone.h"


int main() {
    // Inizializza grafica
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    // Inizializza audio engine
    AudioEngine audioEngine(512 * 1024); // Loop buffer da 512k campioni
    audioEngine.init();

    // Inizializza microfono
    Microphone mic(32 * 1024); // Buffer microfono da 32k campioni

    bool recording = false;

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_START)
            break; // Esci dal programma

        if (kDown & KEY_A) {
            if (!recording) {
                printf("Start recording...\n");
                mic.start();
                recording = true;
            } else {
                printf("Stop recording...\n");
                mic.stop();

                // Copia dati dal microfono nel loop buffer dell'audio engine
                for (size_t i = 0; i < mic.getBufferLength(); i++) {
                    audioEngine.setSample(i, mic.getSample(i));
                }

                recording = false;
            }
        }

        // Aggiorna l'audio engine
        audioEngine.update();

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    mic.stop();
    gfxExit();
    return 0;
}