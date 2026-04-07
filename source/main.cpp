#include <3ds.h>
#include <cstdio>
#include <malloc.h>
#include <cstring>

// Parametri fissi come nel tuo main.c funzionante
#define MIC_BUF_SIZE 0x30000
#define SAMPLE_RATE 16360

int main() {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    // 1. Allocazione Memoria
    u8* mic_buffer = (u8*)memalign(0x1000, MIC_BUF_SIZE);
    int16_t* play_buffer = (int16_t*)linearAlloc(MIC_BUF_SIZE);
    
    if (!mic_buffer || !play_buffer) {
        printf("Errore memoria!\n");
        svcSleepThread(2000000000);
        return 0;
    }

    memset(mic_buffer, 0, MIC_BUF_SIZE);
    memset(play_buffer, 0, MIC_BUF_SIZE);

    // 2. Inizializzazione Servizi
    Result res = ndspInit();
    if (R_FAILED(res)) printf("NDSP fallito\n");

    res = micInit(mic_buffer, MIC_BUF_SIZE);
    if (R_FAILED(res)) printf("MIC fallito\n");

    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, SAMPLE_RATE);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);

    ndspWaveBuf waveBuf;
    memset(&waveBuf, 0, sizeof(ndspWaveBuf));
    waveBuf.data_vaddr = play_buffer;
    waveBuf.nsamples = MIC_BUF_SIZE / 2;
    waveBuf.looping = true;
    waveBuf.status = NDSP_WBUF_DONE;

    u32 start_offset = 0;
    bool is_recording = false;

    printf("\x1b[36m--- LOOPER V26 ---\x1b[0m\n");
    printf("Premi A per registrare\n");

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kUp = hidKeysUp();

        if (kDown & KEY_START) break;

        // SE PREMI A
        if (kDown & KEY_A) {
            MICU_SetPower(true);
            u32 data_size = micGetSampleDataSize();
            res = MICU_StartSampling(MICU_ENCODING_PCM16_SIGNED, MICU_SAMPLE_RATE_16360, 0, data_size, true);
            
            if (R_SUCCEEDED(res)) {
                start_offset = micGetLastSampleOffset();
                is_recording = true;
                printf("\x1b[42m  REC  \x1b[0m\r"); // Sfondo verde
            }
        }

        // SE RILASCI A
        if (kUp & KEY_A && is_recording) {
            is_recording = false;
            printf("\x1b[43m  STOP \x1b[0m\r"); // Sfondo giallo

            // Invalida cache per leggere i dati scritti dal MIC (DMA)
            DSP_InvalidateDataCache(mic_buffer, MIC_BUF_SIZE);

            int16_t* mic_data = (int16_t*)mic_buffer;
            size_t total_samples = MIC_BUF_SIZE / 2;

            for (size_t i = 0; i < total_samples; i++) {
                size_t idx = (start_offset + i) % total_samples;
                // Amplificazione brutale x16
                int32_t val = (int32_t)mic_data[idx] * 16;
                if (val > 32767) val = 32767;
                if (val < -32768) val = -32768;
                play_buffer[i] = (int16_t)val;
            }

            // Flush cache per inviare dati al DSP
            DSP_FlushDataCache(play_buffer, MIC_BUF_SIZE);
            
            if (waveBuf.status == NDSP_WBUF_DONE) {
                ndspChnWaveBufAdd(0, &waveBuf);
            }
            printf("\x1b[44m  PLAY \x1b[0m\n"); // Sfondo blu
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    // Cleanup
    MICU_StopSampling();
    micExit();
    ndspExit();
    linearFree(play_buffer);
    free(mic_buffer);
    gfxExit();
    return 0;
}