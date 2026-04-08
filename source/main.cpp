#include <3ds.h>
#include <cstdio>
#include <malloc.h>
#include <cstring>

#define SAMPLE_RATE 16360
#define MAX_SECONDS 30
#define ALIGN_PAGE(size) (((size) + 0xFFF) & ~0xFFF)
#define MAX_BUF_SIZE ALIGN_PAGE(MAX_SECONDS * SAMPLE_RATE * 2)

int main() {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    u8* mic_buffer = (u8*)memalign(0x1000, MAX_BUF_SIZE);
    int16_t* play_buffer = (int16_t*)linearAlloc(MAX_BUF_SIZE);
    
    if (!mic_buffer || !play_buffer) {
        printf("Errore RAM!\n");
        svcSleepThread(2000000000);
        return 0;
    }

    memset(mic_buffer, 0, MAX_BUF_SIZE);
    memset(play_buffer, 0, MAX_BUF_SIZE);

    ndspInit();
    micInit(mic_buffer, MAX_BUF_SIZE);
    
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, (float)SAMPLE_RATE);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);

    int selectedSeconds = 5;
    bool timerConfirmed = false;
    bool is_recording = false;
    
    u64 startTime = 0;
    u32 startPosSamples = 0;
    
    size_t active_buf_size = 0;
    size_t total_samples = 0;

    printf("\x1b[1;36mLooper V55 - Time-Stamp Sync\x1b[0m\n\n");

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kUp = hidKeysUp();

        if (kDown & KEY_START) break;

        if (!timerConfirmed) {
            printf("\rLoop: %d sec | A: Conferma ", selectedSeconds);
            if (kDown & KEY_DUP && selectedSeconds < MAX_SECONDS) selectedSeconds++;
            if (kDown & KEY_DDOWN && selectedSeconds > 1) selectedSeconds--;
            
            if (kDown & KEY_A) {
                active_buf_size = ALIGN_PAGE(selectedSeconds * SAMPLE_RATE * 2);
                total_samples = active_buf_size / 2;
                timerConfirmed = true;
                
                MICU_SetPower(true);
                MICU_StartSampling(MICU_ENCODING_PCM16_SIGNED, MICU_SAMPLE_RATE_16360, 0, active_buf_size, true);
                
                // Segnamo il tempo zero dell'avvio del microfono
                startTime = osGetTime();
                
                printf("\n\n\x1b[32mPRONTO (Sync via Timer)\x1b[0m\n");
            }
        } 
        else {
            if (kDown & KEY_A) {
                // Calcoliamo quanti millisecondi sono passati dall'avvio del mic
                u64 currentTime = osGetTime();
                u64 elapsedMs = currentTime - startTime;
                
                // Convertiamo i millisecondi in numero di campioni
                // (ms * SAMPLE_RATE) / 1000
                startPosSamples = (u32)((elapsedMs * SAMPLE_RATE) / 1000) % total_samples;
                
                is_recording = true;
                printf("\r\x1b[42m REC \x1b[0m           "); 
            }

            if (kUp & KEY_A && is_recording) {
                is_recording = false;
                
                // Non fermiamo il mic, invalidiamo solo la cache
                DSP_InvalidateDataCache(mic_buffer, active_buf_size);
                int16_t* mic_data = (int16_t*)mic_buffer;

                for (size_t i = 0; i < total_samples; i++) {
                    // Usiamo lo startPos calcolato matematicamente
                    size_t idx = (startPosSamples + i) % total_samples;
                    
                    int32_t existing = (int32_t)play_buffer[i];
                    int32_t new_sample = (int32_t)mic_data[idx] * 12; // Boost x12
                    
                    int32_t mixed = existing + new_sample;
                    if (mixed > 32767) mixed = 32767;
                    if (mixed < -32768) mixed = -32768;
                    
                    play_buffer[i] = (int16_t)mixed;
                }

                DSP_FlushDataCache(play_buffer, active_buf_size);
                
                ndspChnReset(0);
                ndspChnSetRate(0, (float)SAMPLE_RATE);
                
                ndspWaveBuf waveBuf;
                memset(&waveBuf, 0, sizeof(ndspWaveBuf));
                waveBuf.data_vaddr = play_buffer;
                waveBuf.nsamples = total_samples;
                waveBuf.looping = true;
                
                ndspChnWaveBufAdd(0, &waveBuf);
                printf("\r\x1b[44m LOOP \x1b[0m          \n"); 
            }

            if (kDown & KEY_B) {
                ndspChnReset(0);
                memset(play_buffer, 0, active_buf_size);
                DSP_FlushDataCache(play_buffer, active_buf_size);
                printf("\x1b[31mRESET\x1b[0m\n");
            }
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    MICU_StopSampling();
    micExit();
    ndspExit();
    linearFree(play_buffer);
    free(mic_buffer);
    gfxExit();
    return 0;
}