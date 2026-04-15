#include <3ds.h>
#include <cstdio>
#include <malloc.h>
#include <cstring>
#include <cmath>

#define SAMPLE_RATE 16360
#define MAX_SECONDS 15 
#define ALIGN_PAGE(size) (((size) + 0xFFF) & ~0xFFF)
#define MAX_BUF_SIZE ALIGN_PAGE(MAX_SECONDS * SAMPLE_RATE * 2)

typedef struct {
    int16_t* buffer;
    bool active;
    int16_t peak;
} Track;

// Ricalcolo del picco per l'interfaccia
int16_t calculate_peak(int16_t* buf, size_t samples) {
    int16_t max_v = 0;
    for(size_t i = 0; i < samples; i += 200) {
        int16_t val = abs(buf[i]);
        if(val > max_v) max_v = val;
    }
    return max_v;
}

// Software Mixer
void update_master_mix(Track* tracks, int16_t* play_buffer, size_t total_samples, size_t buf_size) {
    memset(play_buffer, 0, buf_size);
    for (size_t i = 0; i < total_samples; i++) {
        int32_t mixed = 0;
        for (int t = 0; t < 4; t++) {
            if (tracks[t].active) mixed += (int32_t)tracks[t].buffer[i];
        }
        mixed *= 12; // Boost volume
        if (mixed > 32767) mixed = 32767;
        if (mixed < -32768) mixed = -32768;
        play_buffer[i] = (int16_t)mixed;
    }
    DSP_FlushDataCache(play_buffer, buf_size);
}

int main() {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    u8* mic_buffer = (u8*)memalign(0x1000, MAX_BUF_SIZE);
    int16_t* play_buffer = (int16_t*)linearAlloc(MAX_BUF_SIZE);
    
    Track tracks[4];
    for(int i=0; i<4; i++) {
        tracks[i].buffer = (int16_t*)linearAlloc(MAX_BUF_SIZE);
        tracks[i].active = false;
        tracks[i].peak = 0;
        if(tracks[i].buffer) memset(tracks[i].buffer, 0, MAX_BUF_SIZE);
    }

    ndspInit();
    micInit(mic_buffer, MAX_BUF_SIZE);
    ndspChnReset(0);
    ndspChnSetRate(0, (float)SAMPLE_RATE);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);

    int selectedSeconds = 5;
    int currentTrack = 0;
    bool timerConfirmed = false;
    bool is_recording = false;
    u64 globalStartTime = 0; 
    size_t active_buf_size = 0;
    size_t total_samples = 0;

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_START) break;

        if (!timerConfirmed) {
            printf("\x1b[1;1H\x1b[36m--- TOGGLE LOOPER V79 ---\x1b[0m");
            printf("\x1b[3;1HLoop Globale: %d sec (Su/Giu)", selectedSeconds);
            if (kDown & KEY_DUP && selectedSeconds < MAX_SECONDS) selectedSeconds++;
            if (kDown & KEY_DDOWN && selectedSeconds > 1) selectedSeconds--;
            if (kDown & KEY_A) {
                active_buf_size = ALIGN_PAGE(selectedSeconds * SAMPLE_RATE * 2);
                total_samples = active_buf_size / 2;
                timerConfirmed = true;
                MICU_SetPower(true);
                MICU_SetGain(7);
                MICU_StartSampling(MICU_ENCODING_PCM16_SIGNED, MICU_SAMPLE_RATE_16360, 0, active_buf_size, true);
                globalStartTime = osGetTime();
                consoleClear();
            }
        } else {
            if (kDown & KEY_DRIGHT) { if(currentTrack < 3) currentTrack++; }
            if (kDown & KEY_DLEFT) { if(currentTrack > 0) currentTrack--; }

            // POSIZIONE GLOBALE (Timeline)
            u64 currentTime = osGetTime();
            u64 elapsedMs = (currentTime - globalStartTime);
            u64 loopMs = (u64)selectedSeconds * 1000;
            u32 currentLoopPosMs = (u32)(elapsedMs % loopMs);
            float progress = (float)currentLoopPosMs / (float)loopMs;

            printf("\x1b[1;1H\x1b[33m--- SYNC MIXER ---\x1b[0m");
            printf("\x1b[2;1HLoop: [");
            int dotPos = (int)(progress * 20);
            for(int i=0; i<20; i++) printf(i == dotPos ? ">" : "-");
            printf("] %.1fs", (float)currentLoopPosMs/1000.0f);

            for(int i=0; i<4; i++) {
                printf("\x1b[%d;1HT%d: %s %s", i+4, i+1, 
                    tracks[i].active ? "\x1b[32m[LIVE]\x1b[0m" : "\x1b[30m[EMPTY]\x1b[0m",
                    (i == currentTrack) ? "<-- SEL" : "       ");
            }

            // LOGICA TOGGLE (A per Start, A per Stop)
            if (kDown & KEY_A) {
                if (!is_recording) {
                    // --- INIZIO REGISTRAZIONE ---
                    is_recording = true;
                    // Pulizia istantanea per sovrascrittura totale
                    memset(tracks[currentTrack].buffer, 0, active_buf_size);
                    tracks[currentTrack].active = false; 
                    update_master_mix(tracks, play_buffer, total_samples, active_buf_size);
                } else {
                    // --- FINE REGISTRAZIONE ---
                    is_recording = false;
                    DSP_InvalidateDataCache(mic_buffer, active_buf_size);
                    int16_t* mic_ptr = (int16_t*)mic_buffer;

                    // Copia l'audio: essendo un buffer circolare, se hai superato i 5 secondi, 
                    // il microfono ha già sovrascritto se stesso nel mic_buffer.
                    for (size_t i = 0; i < total_samples; i++) {
                        tracks[currentTrack].buffer[i] = mic_ptr[i]; 
                    }

                    tracks[currentTrack].active = true;
                    tracks[currentTrack].peak = calculate_peak(tracks[currentTrack].buffer, total_samples);
                    update_master_mix(tracks, play_buffer, total_samples, active_buf_size);

                    // Aggiorna l'audio NDSP
                    ndspChnReset(0);
                    ndspChnSetRate(0, (float)SAMPLE_RATE);
                    ndspWaveBuf wave;
                    memset(&wave, 0, sizeof(ndspWaveBuf));
                    wave.data_vaddr = play_buffer;
                    wave.nsamples = total_samples;
                    wave.looping = true;
                    ndspChnWaveBufAdd(0, &wave);
                }
            }

            if (is_recording) {
                printf("\x1b[9;1H\x1b[41m REC T%d - (A per fermare) \x1b[0m", currentTrack + 1);
            } else {
                printf("\x1b[9;1H\x1b[0m[A] Registra/Stop | [B] Clear     ");
            }

            if (kDown & KEY_B) {
                tracks[currentTrack].active = false;
                memset(tracks[currentTrack].buffer, 0, active_buf_size);
                update_master_mix(tracks, play_buffer, total_samples, active_buf_size);
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
    for(int i=0; i<4; i++) linearFree(tracks[i].buffer);
    free(mic_buffer);
    gfxExit();
    return 0;
}