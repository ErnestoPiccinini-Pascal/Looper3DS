#include <3ds.h>
#include <stdio.h>
#include "audio_engine.h"

int main() {
    // inizializza i servizi base del 3DS
    gfxInitDefault();

    // apre una console testuale sullo schermo superiore
    consoleInit(GFX_TOP, nullptr);

    // crea un loop di 4 secondi
    AudioEngine engine(22050 * 4);

    // inizializza NDSP
    engine.init();

    // riempie il loop con una sinusoide 440Hz
    engine.fillTestTone();

    //printf("Loop test attivo!\n");
    //printf("Dovresti sentire un LA continuo.\n");
    //printf("START = esci\n");

    // loop principale app
    while (aptMainLoop()) {
        // aggiorna input tasti
        hidScanInput();

        // se premi START esce
        if (hidKeysDown() & KEY_START)
            break;

        // aggiorna il motore audio
        engine.update();

        // aspetta il prossimo frame video
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}