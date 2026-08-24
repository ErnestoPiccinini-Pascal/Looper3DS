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
    int volume;                 // 0-100
} Track;


// ============================================================
// NDSP WAVE BUFFER
// Deve rimanere valido per tutta la durata del playback
// ============================================================

ndspWaveBuf masterWave;


// ============================================================
// CALCOLO DEL PICCO
// ============================================================

int16_t calculate_peak(int16_t* buf, size_t samples) {

    int16_t max_v = 0;

    for (size_t i = 0; i < samples; i += 200) {

        int16_t val = abs(buf[i]);

        if (val > max_v)
            max_v = val;
    }

    return max_v;
}


// ============================================================
// CONTROLLA SE ESISTE ALMENO UNA TRACCIA ATTIVA
// ============================================================

bool has_active_track(Track* tracks) {

    for (int i = 0; i < 4; i++) {

        if (tracks[i].active)
            return true;
    }

    return false;
}


// ============================================================
// SOFTWARE MIXER
//
// Il volume viene applicato individualmente:
//
// 100 = 100%
// 75  = 75%
// 50  = 50%
// 25  = 25%
// 0   = mute
// ============================================================

void update_master_mix(
    Track* tracks,
    int16_t* play_buffer,
    size_t total_samples,
    size_t buf_size
) {

    memset(play_buffer, 0, buf_size);

    for (size_t i = 0; i < total_samples; i++) {

        int32_t mixed = 0;

        for (int t = 0; t < 4; t++) {

            if (tracks[t].active) {

                int32_t sample =
                    (int32_t)tracks[t].buffer[i];

                // Volume individuale
                sample =
                    (sample * tracks[t].volume) / 100;

                mixed += sample;
            }
        }

        // Boost originale
        mixed *= 12;

        // Clipping
        if (mixed > 32767)
            mixed = 32767;

        if (mixed < -32768)
            mixed = -32768;

        play_buffer[i] =
            (int16_t)mixed;
    }

    // Aggiorna la cache del buffer audio
    DSP_FlushDataCache(
        play_buffer,
        buf_size
    );
}


// ============================================================
// AVVIO / RIAVVIO PLAYBACK NDSP
//
// Questa funzione viene chiamata SOLO quando dobbiamo
// iniziare un nuovo playback.
//
// NON viene chiamata quando cambiamo volume o facciamo Clear,
// così la posizione del loop non viene resettata.
// ============================================================

void update_ndsp(
    int16_t* play_buffer,
    size_t total_samples
) {

    ndspChnReset(0);

    ndspChnSetRate(
        0,
        (float)SAMPLE_RATE
    );

    ndspChnSetFormat(
        0,
        NDSP_FORMAT_MONO_PCM16
    );

    memset(
        &masterWave,
        0,
        sizeof(ndspWaveBuf)
    );

    masterWave.data_vaddr =
        play_buffer;

    masterWave.nsamples =
        total_samples;

    masterWave.looping =
        true;

    DSP_FlushDataCache(
        play_buffer,
        total_samples * sizeof(int16_t)
    );

    ndspChnWaveBufAdd(
        0,
        &masterWave
    );
}


// ============================================================
// MAIN
// ============================================================

int main() {

    gfxInitDefault();

    consoleInit(
        GFX_TOP,
        NULL
    );


    // ========================================================
    // BUFFER MICROFONO
    // ========================================================

    u8* mic_buffer =
        (u8*)memalign(
            0x1000,
            MAX_BUF_SIZE
        );


    // ========================================================
    // BUFFER MASTER
    // ========================================================

    int16_t* play_buffer =
        (int16_t*)linearAlloc(
            MAX_BUF_SIZE
        );


    // ========================================================
    // TRACK
    // ========================================================

    Track tracks[4];

    for (int i = 0; i < 4; i++) {

        tracks[i].buffer =
            (int16_t*)linearAlloc(
                MAX_BUF_SIZE
            );

        tracks[i].active = false;

        tracks[i].peak = 0;

        // Volume iniziale
        tracks[i].volume = 100;

        if (tracks[i].buffer) {

            memset(
                tracks[i].buffer,
                0,
                MAX_BUF_SIZE
            );
        }
    }


    // ========================================================
    // CONTROLLO ALLOCAZIONE
    // ========================================================

    if (!mic_buffer || !play_buffer) {

        printf(
            "Errore allocazione buffer!\n"
        );

        if (mic_buffer)
            free(mic_buffer);

        if (play_buffer)
            linearFree(play_buffer);

        for (int i = 0; i < 4; i++) {

            if (tracks[i].buffer)
                linearFree(
                    tracks[i].buffer
                );
        }

        gfxExit();

        return 1;
    }


    // ========================================================
    // AUDIO
    // ========================================================

    ndspInit();

    micInit(
        mic_buffer,
        MAX_BUF_SIZE
    );

    ndspChnReset(0);

    ndspChnSetRate(
        0,
        (float)SAMPLE_RATE
    );

    ndspChnSetFormat(
        0,
        NDSP_FORMAT_MONO_PCM16
    );


    // ========================================================
    // INIZIALIZZA NDSP WAVE BUFFER
    // ========================================================

    memset(
        &masterWave,
        0,
        sizeof(ndspWaveBuf)
    );


    // ========================================================
    // VARIABILI
    // ========================================================

    int selectedSeconds = 5;

    int currentTrack = 0;

    bool timerConfirmed = false;

    bool is_recording = false;

    u64 globalStartTime = 0;

    size_t active_buf_size = 0;

    size_t total_samples = 0;


    // ========================================================
    // MAIN LOOP
    // ========================================================

    while (aptMainLoop()) {

        hidScanInput();

        u32 kDown =
            hidKeysDown();


        // ====================================================
        // START = USCITA
        // ====================================================

        if (kDown & KEY_START)
            break;


        // ====================================================
        // SELEZIONE DURATA
        // ====================================================

        if (!timerConfirmed) {

            printf(
                "\x1b[1;1H"
                "\x1b[36m--- TOGGLE LOOPER V88 ---\x1b[0m"
            );

            printf(
                "\x1b[3;1H"
                "Loop Globale: %d sec (Su/Giu)",
                selectedSeconds
            );


            // ------------------------------------------------
            // DURATA +
            // ------------------------------------------------

            if (kDown & KEY_DUP) {

                if (selectedSeconds < MAX_SECONDS)
                    selectedSeconds++;
            }


            // ------------------------------------------------
            // DURATA -
            // ------------------------------------------------

            if (kDown & KEY_DDOWN) {

                if (selectedSeconds > 1)
                    selectedSeconds--;
            }


            // ------------------------------------------------
            // CONFERMA
            // ------------------------------------------------

            if (kDown & KEY_A) {

                active_buf_size =
                    ALIGN_PAGE(
                        selectedSeconds
                        * SAMPLE_RATE
                        * 2
                    );

                total_samples =
                    active_buf_size / 2;

                timerConfirmed = true;


                // --------------------------------------------
                // MICROFONO
                // --------------------------------------------

                MICU_SetPower(true);

                MICU_SetGain(7);

                MICU_StartSampling(
                    MICU_ENCODING_PCM16_SIGNED,
                    MICU_SAMPLE_RATE_16360,
                    0,
                    active_buf_size,
                    true
                );


                // --------------------------------------------
                // TIMELINE GLOBALE
                // --------------------------------------------

                globalStartTime =
                    osGetTime();

                consoleClear();
            }
        }


        // ====================================================
        // LOOP OPERATIVO
        // ====================================================

        else {

            // =================================================
            // SELEZIONE TRACCIA
            // =================================================

            if (kDown & KEY_DRIGHT) {

                if (currentTrack < 3)
                    currentTrack++;
            }

            if (kDown & KEY_DLEFT) {

                if (currentTrack > 0)
                    currentTrack--;
            }


            // =================================================
            // VOLUME +
            //
            // NON RIAVVIA NDSP
            // NON MODIFICA IL TIMING
            // DURANTE REC NON AGGIORNA IL MIX
            // =================================================

            if (kDown & KEY_DUP) {

                tracks[currentTrack].volume += 5;

                if (tracks[currentTrack].volume > 120)
                    tracks[currentTrack].volume = 120;

                if (!is_recording) {

                    update_master_mix(
                        tracks,
                        play_buffer,
                        total_samples,
                        active_buf_size
                    );
                }
            }


            // =================================================
            // VOLUME -
            //
            // NON RIAVVIA NDSP
            // NON MODIFICA IL TIMING
            // DURANTE REC NON AGGIORNA IL MIX
            // =================================================

            if (kDown & KEY_DDOWN) {

                tracks[currentTrack].volume -= 5;

                if (tracks[currentTrack].volume < 0)
                    tracks[currentTrack].volume = 0;


                if (!is_recording) {

                    update_master_mix(
                        tracks,
                        play_buffer,
                        total_samples,
                        active_buf_size
                    );
                }
            }


            // =================================================
            // POSIZIONE GLOBALE
            // =================================================

            u64 currentTime =
                osGetTime();

            u64 elapsedMs =
                currentTime - globalStartTime;

            u64 loopMs =
                (u64)selectedSeconds * 1000;

            u32 currentLoopPosMs =
                (u32)(
                    elapsedMs % loopMs
                );

            float progress =
                (float)currentLoopPosMs
                /
                (float)loopMs;


            // =================================================
            // INTERFACCIA
            // =================================================

            printf(
                "\x1b[1;1H"
                "\x1b[33m--- SYNC MIXER V88 ---\x1b[0m"
            );


            printf(
                "\x1b[2;1H"
                "Loop: ["
            );


            int dotPos =
                (int)(progress * 20);


            for (int i = 0; i < 20; i++) {

                printf(
                    i == dotPos
                    ? ">"
                    : "-"
                );
            }


            printf(
                "] %.1fs",
                (float)currentLoopPosMs
                / 1000.0f
            );


            // =================================================
            // TRACK DISPLAY
            // =================================================

            for (int i = 0; i < 4; i++) {

                printf(
                    "\x1b[%d;1H"
                    "T%d: %s VOL:%3d%% %s",
                    i + 4,

                    i + 1,

                    tracks[i].active
                    ? "\x1b[32m[LIVE]\x1b[0m"
                    : "\x1b[30m[EMPTY]\x1b[0m",

                    tracks[i].volume,

                    (i == currentTrack)
                    ? "<-- SEL"
                    : "       "
                );
            }


            // =================================================
            // CONTROLLI
            // =================================================

            if (is_recording) {

                printf(
                    "\x1b[9;1H"
                    "\x1b[41m REC T%d - "
                    "(A per fermare) \x1b[0m",
                    currentTrack + 1
                );

            } else {

                printf(
                    "\x1b[9;1H"
                    "\x1b[0m"
                    "[A] Registra/Stop | "
                    "[B] Clear | "
                    "[Su/Giu] Volume"
                );
            }


            // =================================================
            // A = RECORD / STOP
            // =================================================

            if (kDown & KEY_A) {


                // =================================================
                // INIZIO REGISTRAZIONE
                // =================================================

                if (!is_recording) {

                    is_recording = true;


                    // --------------------------------------------
                    // Pulisce completamente la traccia
                    // --------------------------------------------

                    memset(
                        tracks[currentTrack].buffer,
                        0,
                        active_buf_size
                    );


                    tracks[currentTrack].active =
                        false;


                    // --------------------------------------------
                    // Aggiorna il mix.
                    //
                    // NON riavviamo NDSP.
                    // --------------------------------------------

                    update_master_mix(
                        tracks,
                        play_buffer,
                        total_samples,
                        active_buf_size
                    );
                }


                // =================================================
                // FINE REGISTRAZIONE
                // =================================================

                else {

                    is_recording = false;


                    // --------------------------------------------
                    // INVALIDATE CACHE MICROFONO
                    // --------------------------------------------

                    DSP_InvalidateDataCache(
                        mic_buffer,
                        active_buf_size
                    );


                    int16_t* mic_ptr =
                        (int16_t*)mic_buffer;


                    // --------------------------------------------
                    // COPIA AUDIO
                    // --------------------------------------------

                    for (
                        size_t i = 0;
                        i < total_samples;
                        i++
                    ) {

                        tracks[currentTrack].buffer[i] =
                            mic_ptr[i];
                    }


                    // --------------------------------------------
                    // ATTIVA TRACCIA
                    // --------------------------------------------

                    tracks[currentTrack].active =
                        true;


                    // --------------------------------------------
                    // PEAK
                    // --------------------------------------------

                    tracks[currentTrack].peak =
                        calculate_peak(
                            tracks[currentTrack].buffer,
                            total_samples
                        );


                    // --------------------------------------------
                    // CREA NUOVO MIX
                    // --------------------------------------------

                    update_master_mix(
                        tracks,
                        play_buffer,
                        total_samples,
                        active_buf_size
                    );


                    // --------------------------------------------
                    // AVVIA NUOVO PLAYBACK
                    //
                    // Qui è voluto che il loop riparta da 0.
                    // --------------------------------------------

                    update_ndsp(
                        play_buffer,
                        total_samples
                    );
                }
            }


            // =================================================
            // B = CLEAR
            //
            // NON RIAVVIA IL LOOP
            // =================================================

            if (kDown & KEY_B) {

                tracks[currentTrack].active =
                    false;

                tracks[currentTrack].peak =
                    0;


                memset(
                    tracks[currentTrack].buffer,
                    0,
                    active_buf_size
                );


                if (!is_recording) {

                    update_master_mix(
                        tracks,
                        play_buffer,
                        total_samples,
                        active_buf_size
                    );
                }
            }
        }


        // ====================================================
        // GRAFICA
        // ====================================================

        gfxFlushBuffers();

        gfxSwapBuffers();

        gspWaitForVBlank();
    }


    // ========================================================
    // CLEANUP
    // ========================================================

    MICU_StopSampling();

    micExit();

    ndspExit();


    if (play_buffer)
        linearFree(play_buffer);


    for (int i = 0; i < 4; i++) {

        if (tracks[i].buffer)
            linearFree(
                tracks[i].buffer
            );
    }


    if (mic_buffer)
        free(mic_buffer);


    gfxExit();

    return 0;
}