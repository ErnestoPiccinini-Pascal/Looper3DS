#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

#include <cstdio>
#include <malloc.h>
#include <cstring>
#include <cmath>

#define SAMPLE_RATE 16360
#define MAX_SECONDS 15

#define ALIGN_PAGE(size) (((size) + 0xFFF) & ~0xFFF)
#define MAX_BUF_SIZE ALIGN_PAGE(MAX_SECONDS * SAMPLE_RATE * 2)

#define NUM_TRACKS 4
#define MAX_VOLUME 120


// ============================================================
// INDICI UI.T3S
// ============================================================

enum UIImage {

    IMG_TOP_BG = 0,
    IMG_BOTTOM_BG,

    IMG_SEL_SPEED,
    IMG_SEL_REC,

    IMG_SPEED_CENTRO,
    IMG_SPEED_DX,
    IMG_SPEED_SX,

    IMG_CURSORE,
    IMG_LANCETTA,

    IMG_REC_LED_TOP,
    IMG_REC_LED_BOTTOM,

    IMG_SEL_QUADRATO,
    IMG_SEL_SAVE,

    IMG_COUNT
};


// ============================================================
// CONTROLLI TOP
// ============================================================

enum ControlType {

    CONTROL_REC = 0,
    CONTROL_SPEED,
    CONTROL_REV,
    CONTROL_ERS
};


// ============================================================
// TRACK
// ============================================================

typedef struct {

    int16_t* buffer;

    bool active;

    int16_t peak;

    int volume;

    // 0 = sx
    // 1 = x1
    // 2 = dx
    int speedState;

} Track;


// ============================================================
// GLOBALI
// ============================================================

C2D_SpriteSheet uiSheet = NULL;

ndspWaveBuf masterWave;


// ============================================================
// COORDINATE TOP
// ============================================================

static const float CONTROL_SELECT_X[NUM_TRACKS] = {

    12.0f,
    71.0f,
    132.0f,
    195.0f
};


static const float SPEED_SPRITE_X[NUM_TRACKS] = {

    5.0f,
    64.0f,
    125.0f,
    188.0f
};


#define SEL_REC_Y       58.0f
#define SEL_SPEED_Y    108.0f
#define SEL_REV_Y      164.0f
#define SEL_ERS_Y      186.0f

#define SPEED_SPRITE_Y  97.0f

#define REC_LED_TOP_Y    22.0f


// ============================================================
// VU
//
// V92:
// VU_PIVOT_Y = 67
//
// V93:
// spostato 7 pixel più in basso.
// ============================================================

#define VU_PIVOT_X 324.0f
#define VU_PIVOT_Y  74.0f

#define VU_CENTER_X 0.94f
#define VU_CENTER_Y 0.97f

#define VU_MIN_ROTATION -18.0f
#define VU_MAX_ROTATION  92.0f


// ============================================================
// BOTTOM
// ============================================================

static const float FADER_X[5] = {

    25.0f,
    87.0f,
    149.0f,
    211.0f,
    273.0f
};


static const float BOTTOM_LED_X[NUM_TRACKS] = {

    36.0f,
    98.0f,
    160.0f,
    222.0f
};


#define BOTTOM_LED_Y 14.0f

#define FADER_TOP     35
#define FADER_BOTTOM 203


// ============================================================
// TESTO
// ============================================================

void drawText(
    C2D_TextBuf buf,
    const char* string,
    float x,
    float y,
    float scale,
    u32 color
) {

    C2D_Text text;

    C2D_TextParse(
        &text,
        buf,
        string
    );

    C2D_TextOptimize(
        &text
    );

    C2D_DrawText(
        &text,
        C2D_WithColor,
        x,
        y,
        0.5f,
        scale,
        scale,
        color
    );
}


// ============================================================
// IMAGE
// ============================================================

void drawImage(
    int imageIndex,
    float x,
    float y,
    float z = 0.0f
) {

    C2D_Image img =
        C2D_SpriteSheetGetImage(
            uiSheet,
            imageIndex
        );

    C2D_DrawImageAt(
        img,
        x,
        y,
        z,
        NULL,
        1.0f,
        1.0f
    );
}


// ============================================================
// PEAK
// ============================================================

int16_t calculate_peak(
    int16_t* buf,
    size_t samples
) {

    int32_t max_v = 0;

    for (
        size_t i = 0;
        i < samples;
        i += 200
    ) {

        int32_t v =
            (int32_t)buf[i];

        if (v < 0)
            v = -v;

        if (v > max_v)
            max_v = v;
    }

    if (max_v > 32767)
        max_v = 32767;

    return (int16_t)max_v;
}


// ============================================================
// MIXER
// ============================================================

void update_master_mix(
    Track* tracks,
    int masterVolume,
    int16_t* play_buffer,
    size_t total_samples,
    size_t buf_size
) {

    memset(
        play_buffer,
        0,
        buf_size
    );


    for (
        size_t i = 0;
        i < total_samples;
        i++
    ) {

        int32_t mixed = 0;


        for (
            int t = 0;
            t < NUM_TRACKS;
            t++
        ) {

            if (!tracks[t].active)
                continue;


            int32_t sample =
                (int32_t)tracks[t].buffer[i];


            sample =
                (
                    sample
                    *
                    tracks[t].volume
                )
                /
                100;


            mixed += sample;
        }


        // Master

        mixed =
            (
                mixed
                *
                masterVolume
            )
            /
            100;


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


    DSP_FlushDataCache(
        play_buffer,
        buf_size
    );
}


// ============================================================
// NDSP
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
        sizeof(masterWave)
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
// VOLUME -> Y
// ============================================================

float volumeToY(
    int volume
) {

    if (volume < 0)
        volume = 0;

    if (volume > MAX_VOLUME)
        volume = MAX_VOLUME;


    float amount =
        (float)volume
        /
        (float)MAX_VOLUME;


    return
        (float)FADER_BOTTOM
        -
        amount
        *
        (
            FADER_BOTTOM
            -
            FADER_TOP
        );
}


// ============================================================
// Y -> VOLUME
// ============================================================

int yToVolume(
    int y
) {

    if (y < FADER_TOP)
        y = FADER_TOP;

    if (y > FADER_BOTTOM)
        y = FADER_BOTTOM;


    float amount =
        (
            FADER_BOTTOM
            -
            y
        )
        /
        (float)(
            FADER_BOTTOM
            -
            FADER_TOP
        );


    int volume =
        (int)(
            amount
            *
            MAX_VOLUME
        );


    if (volume < 0)
        volume = 0;

    if (volume > MAX_VOLUME)
        volume = MAX_VOLUME;


    return volume;
}


// ============================================================
// TOUCH FADER
// ============================================================

int findTouchedFader(
    int px,
    int py
) {

    if (
        py < FADER_TOP - 15
        ||
        py > FADER_BOTTOM + 15
    ) {

        return -1;
    }


    for (
        int i = 0;
        i < 5;
        i++
    ) {

        if (
            px >= FADER_X[i] - 24
            &&
            px <= FADER_X[i] + 24
        ) {

            return i;
        }
    }


    return -1;
}


// ============================================================
// VU
// ============================================================

float calculateVU(
    Track* tracks,
    int masterVolume,
    size_t samplePosition,
    size_t totalSamples
) {

    if (totalSamples == 0)
        return 0.0f;


    samplePosition %=
        totalSamples;


    int32_t maxLevel =
        0;


    for (
        size_t offset = 0;
        offset < 128;
        offset += 4
    ) {

        size_t pos =
            (
                samplePosition
                +
                offset
            )
            %
            totalSamples;


        int32_t mixed =
            0;


        for (
            int t = 0;
            t < NUM_TRACKS;
            t++
        ) {

            if (!tracks[t].active)
                continue;


            int32_t sample =
                tracks[t].buffer[pos];


            sample =
                (
                    sample
                    *
                    tracks[t].volume
                )
                /
                100;


            mixed += sample;
        }


        mixed =
            (
                mixed
                *
                masterVolume
            )
            /
            100;


        mixed *= 12;


        if (mixed > 32767)
            mixed = 32767;

        if (mixed < -32768)
            mixed = -32768;


        if (mixed < 0)
            mixed = -mixed;


        if (mixed > maxLevel)
            maxLevel = mixed;
    }


    float level =
        (float)maxLevel
        /
        32767.0f;


    if (level > 1.0f)
        level = 1.0f;


    return level;
}


// ============================================================
// LANCETTA
// ============================================================

void drawNeedle(
    float vu
) {

    if (vu < 0.0f)
        vu = 0.0f;

    if (vu > 1.0f)
        vu = 1.0f;


    C2D_Sprite needle;


    C2D_SpriteFromSheet(
        &needle,
        uiSheet,
        IMG_LANCETTA
    );


    C2D_SpriteSetCenter(
        &needle,
        VU_CENTER_X,
        VU_CENTER_Y
    );


    C2D_SpriteSetPos(
        &needle,
        VU_PIVOT_X,
        VU_PIVOT_Y
    );


    float rotation =
        VU_MIN_ROTATION
        +
        vu
        *
        (
            VU_MAX_ROTATION
            -
            VU_MIN_ROTATION
        );


    C2D_SpriteRotateDegrees(
        &needle,
        rotation
    );


    C2D_DrawSprite(
        &needle
    );
}


// ============================================================
// SPEED
// ============================================================

void drawTrackSpeed(
    Track* tracks,
    int track
) {

    int sprite =
        IMG_SPEED_CENTRO;


    if (
        tracks[track].speedState == 0
    ) {

        sprite =
            IMG_SPEED_SX;
    }

    else if (
        tracks[track].speedState == 2
    ) {

        sprite =
            IMG_SPEED_DX;
    }


    drawImage(
        sprite,
        SPEED_SPRITE_X[track],
        SPEED_SPRITE_Y,
        0.3f
    );
}


// ============================================================
// TOP SELECTION
// ============================================================

void drawTopSelection(
    int selectedTrack,
    ControlType selectedControl
) {

    float x =
        CONTROL_SELECT_X[
            selectedTrack
        ];


    switch (
        selectedControl
    ) {

        case CONTROL_REC:

            drawImage(
                IMG_SEL_REC,
                x,
                SEL_REC_Y,
                0.8f
            );

            break;


        case CONTROL_SPEED:

            drawImage(
                IMG_SEL_SPEED,
                x,
                SEL_SPEED_Y,
                0.8f
            );

            break;


        case CONTROL_REV:

            drawImage(
                IMG_SEL_QUADRATO,
                x,
                SEL_REV_Y,
                0.8f
            );

            break;


        case CONTROL_ERS:

            drawImage(
                IMG_SEL_QUADRATO,
                x,
                SEL_ERS_Y,
                0.8f
            );

            break;
    }
}


// ============================================================
// TOP
// ============================================================

void drawTop(
    Track* tracks,

    int selectedTrack,
    ControlType selectedControl,

    bool isRecording,
    int recordingTrack,

    float vu
) {

    drawImage(
        IMG_TOP_BG,
        0,
        0,
        0.0f
    );


    // SPEED

    for (
        int i = 0;
        i < NUM_TRACKS;
        i++
    ) {

        drawTrackSpeed(
            tracks,
            i
        );
    }


    // LED TOP:
    // solamente durante la registrazione.

    if (
        isRecording
        &&
        recordingTrack >= 0
    ) {

        drawImage(
            IMG_REC_LED_TOP,

            CONTROL_SELECT_X[
                recordingTrack
            ],

            REC_LED_TOP_Y,

            0.9f
        );
    }


    // Selezione

    drawTopSelection(
        selectedTrack,
        selectedControl
    );


    // VU

    drawNeedle(
        vu
    );
}


// ============================================================
// BOTTOM
// ============================================================

void drawBottom(
    Track* tracks,

    int masterVolume,

    bool isRecording,
    int recordingTrack
) {

    drawImage(
        IMG_BOTTOM_BG,
        0,
        0,
        0.0f
    );


    // ========================================================
    // FADER 1-4
    // ========================================================

    for (
        int i = 0;
        i < NUM_TRACKS;
        i++
    ) {

        float y =
            volumeToY(
                tracks[i].volume
            );


        drawImage(
            IMG_CURSORE,

            FADER_X[i]
            -
            11.5f,

            y
            -
            7.0f,

            0.6f
        );
    }


    // ========================================================
    // MASTER
    // ========================================================

    float masterY =
        volumeToY(
            masterVolume
        );


    drawImage(
        IMG_CURSORE,

        FADER_X[4]
        -
        11.5f,

        masterY
        -
        7.0f,

        0.6f
    );


    // ========================================================
    // LED
    //
    // acceso se active oppure se sta registrando.
    // ========================================================

    for (
        int i = 0;
        i < NUM_TRACKS;
        i++
    ) {

        bool ledOn =
            tracks[i].active
            ||
            (
                isRecording
                &&
                recordingTrack == i
            );


        if (
            ledOn
        ) {

            drawImage(
                IMG_REC_LED_BOTTOM,

                BOTTOM_LED_X[i]
                -
                5.0f,

                BOTTOM_LED_Y
                -
                5.0f,

                0.9f
            );
        }
    }
}


// ============================================================
// MAIN
// ============================================================

int main() {

    // ========================================================
    // GRAFICA
    // ========================================================

    gfxInitDefault();

    romfsInit();


    if (
        !C3D_Init(
            C3D_DEFAULT_CMDBUF_SIZE
        )
    ) {

        romfsExit();
        gfxExit();

        return 1;
    }


    if (
        !C2D_Init(
            C2D_DEFAULT_MAX_OBJECTS
        )
    ) {

        C3D_Fini();
        romfsExit();
        gfxExit();

        return 1;
    }


    C2D_Prepare();


    C3D_RenderTarget* top =
        C2D_CreateScreenTarget(
            GFX_TOP,
            GFX_LEFT
        );


    C3D_RenderTarget* bottom =
        C2D_CreateScreenTarget(
            GFX_BOTTOM,
            GFX_LEFT
        );


    C2D_TextBuf textBuf =
        C2D_TextBufNew(
            2048
        );


    // ========================================================
    // SPRITESHEET
    // ========================================================

    uiSheet =
        C2D_SpriteSheetLoad(
            "romfs:/gfx/ui.t3x"
        );


    if (
        !uiSheet
        ||
        C2D_SpriteSheetCount(
            uiSheet
        )
        <
        IMG_COUNT
    ) {

        if (uiSheet)
            C2D_SpriteSheetFree(
                uiSheet
            );

        C2D_TextBufDelete(
            textBuf
        );

        C2D_Fini();
        C3D_Fini();

        romfsExit();
        gfxExit();

        return 1;
    }


    // ========================================================
    // BUFFER
    // ========================================================

    u8* mic_buffer =
        (u8*)memalign(
            0x1000,
            MAX_BUF_SIZE
        );


    int16_t* play_buffer =
        (int16_t*)linearAlloc(
            MAX_BUF_SIZE
        );


    Track tracks[
        NUM_TRACKS
    ];


    bool allocationError =
        (
            !mic_buffer
            ||
            !play_buffer
        );


    for (
        int i = 0;
        i < NUM_TRACKS;
        i++
    ) {

        tracks[i].buffer =
            (int16_t*)linearAlloc(
                MAX_BUF_SIZE
            );


        tracks[i].active =
            false;

        tracks[i].peak =
            0;

        tracks[i].volume =
            100;

        tracks[i].speedState =
            1;


        if (
            tracks[i].buffer
        ) {

            memset(
                tracks[i].buffer,
                0,
                MAX_BUF_SIZE
            );
        }

        else {

            allocationError =
                true;
        }
    }


    if (
        allocationError
    ) {

        if (play_buffer)
            linearFree(
                play_buffer
            );

        if (mic_buffer)
            free(
                mic_buffer
            );


        for (
            int i = 0;
            i < NUM_TRACKS;
            i++
        ) {

            if (
                tracks[i].buffer
            ) {

                linearFree(
                    tracks[i].buffer
                );
            }
        }


        C2D_SpriteSheetFree(
            uiSheet
        );

        C2D_TextBufDelete(
            textBuf
        );

        C2D_Fini();
        C3D_Fini();

        romfsExit();
        gfxExit();

        return 1;
    }


    memset(
        play_buffer,
        0,
        MAX_BUF_SIZE
    );


    // ========================================================
    // MICROFONO / AUDIO
    // ========================================================

    ndspInit();


    Result micInitResult =
        micInit(
            mic_buffer,
            MAX_BUF_SIZE
        );


    if (
        R_FAILED(
            micInitResult
        )
    ) {

        ndspExit();

        // cleanup minimale
        linearFree(
            play_buffer
        );

        for (
            int i = 0;
            i < NUM_TRACKS;
            i++
        ) {

            linearFree(
                tracks[i].buffer
            );
        }

        free(
            mic_buffer
        );

        C2D_SpriteSheetFree(
            uiSheet
        );

        C2D_TextBufDelete(
            textBuf
        );

        C2D_Fini();
        C3D_Fini();

        romfsExit();
        gfxExit();

        return 1;
    }


    // Dimensione reale utilizzabile dal MIC.
    //
    // micGetSampleDataSize() è prevista proprio
    // per conoscere l'area dati disponibile.

    u32 micDataSize =
        micGetSampleDataSize();


    MICU_SetPower(
        true
    );


    MICU_SetGain(
        7
    );


    // NOTA:
    //
    // NON chiamiamo MICU_StartSampling() qui.
    //
    // Il microfono è inizializzato e alimentato,
    // ma NON sta ancora registrando.


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
        sizeof(masterWave)
    );


    // ========================================================
    // STATO
    // ========================================================

    int selectedSeconds =
        5;


    bool timerConfirmed =
        false;


    size_t active_buf_size =
        0;


    size_t total_samples =
        0;


    u64 globalStartTime =
        0;


    int selectedTrack =
        0;


    ControlType selectedControl =
        CONTROL_REC;


    bool editingSpeed =
        false;


    bool isRecording =
        false;


    int recordingTrack =
        -1;


    int masterVolume =
        100;


    float vuLevel =
        0.0f;


    // ========================================================
    // MAIN LOOP
    // ========================================================

    while (
        aptMainLoop()
    ) {

        hidScanInput();


        u32 kDown =
            hidKeysDown();


        u32 kHeld =
            hidKeysHeld();


        // ====================================================
        // START
        // ====================================================

        if (
            kDown
            &
            KEY_START
        ) {

            break;
        }


        // ====================================================
        // SCELTA DURATA
        // ====================================================

        if (
            !timerConfirmed
        ) {

            if (
                kDown
                &
                KEY_DUP
            ) {

                if (
                    selectedSeconds
                    <
                    MAX_SECONDS
                ) {

                    selectedSeconds++;
                }
            }


            if (
                kDown
                &
                KEY_DDOWN
            ) {

                if (
                    selectedSeconds
                    >
                    1
                ) {

                    selectedSeconds--;
                }
            }


            if (
                kDown
                &
                KEY_A
            ) {

                // IMPORTANTE:
                //
                // La dimensione AUDIO è quella reale,
                // non quella allineata della memoria.

                active_buf_size =
                    selectedSeconds
                    *
                    SAMPLE_RATE
                    *
                    sizeof(int16_t);


                // Sicurezza rispetto all'area MIC reale.

                if (
                    active_buf_size
                    >
                    micDataSize
                ) {

                    active_buf_size =
                        micDataSize;

                    // PCM16 = multiplo di 2

                    active_buf_size &=
                        ~1;
                }


                total_samples =
                    active_buf_size
                    /
                    sizeof(int16_t);


                globalStartTime =
                    osGetTime();


                timerConfirmed =
                    true;
            }
        }


        // ====================================================
        // MIXER
        // ====================================================

        else {

            // =================================================
            // SPEED EDIT
            // =================================================

            if (
                editingSpeed
            ) {

                if (
                    kDown
                    &
                    KEY_DLEFT
                ) {

                    if (
                        tracks[
                            selectedTrack
                        ].speedState
                        >
                        0
                    ) {

                        tracks[
                            selectedTrack
                        ].speedState--;
                    }
                }


                if (
                    kDown
                    &
                    KEY_DRIGHT
                ) {

                    if (
                        tracks[
                            selectedTrack
                        ].speedState
                        <
                        2
                    ) {

                        tracks[
                            selectedTrack
                        ].speedState++;
                    }
                }


                if (
                    kDown
                    &
                    KEY_A
                ) {

                    editingSpeed =
                        false;
                }


                if (
                    kDown
                    &
                    KEY_B
                ) {

                    editingSpeed =
                        false;
                }
            }


            // =================================================
            // NAVIGAZIONE NORMALE
            // =================================================

            else {

                // TRACK

                if (
                    kDown
                    &
                    KEY_DLEFT
                ) {

                    if (
                        selectedTrack
                        >
                        0
                    ) {

                        selectedTrack--;
                    }
                }


                if (
                    kDown
                    &
                    KEY_DRIGHT
                ) {

                    if (
                        selectedTrack
                        <
                        NUM_TRACKS - 1
                    ) {

                        selectedTrack++;
                    }
                }


                // CONTROLLO

                if (
                    kDown
                    &
                    KEY_DUP
                ) {

                    if (
                        selectedControl
                        >
                        CONTROL_REC
                    ) {

                        selectedControl =
                            (ControlType)(
                                selectedControl
                                -
                                1
                            );
                    }
                }


                if (
                    kDown
                    &
                    KEY_DDOWN
                ) {

                    if (
                        selectedControl
                        <
                        CONTROL_ERS
                    ) {

                        selectedControl =
                            (ControlType)(
                                selectedControl
                                +
                                1
                            );
                    }
                }


                // =================================================
                // A
                // =================================================

                if (
                    kDown
                    &
                    KEY_A
                ) {

                    // =============================================
                    // REC
                    // =============================================

                    if (
                        selectedControl
                        ==
                        CONTROL_REC
                    ) {

                        // =========================================
                        // START REC
                        // =========================================

                        if (
                            !isRecording
                        ) {

                            // -------------------------------------
                            // PULISCE IL BUFFER MICROFONO
                            //
                            // Così non può esistere audio
                            // precedente alla pressione REC.
                            // -------------------------------------

                            memset(
                                mic_buffer,
                                0,
                                MAX_BUF_SIZE
                            );


                            // -------------------------------------
                            // START SAMPLING
                            //
                            // IL MICROFONO COMINCIA DAVVERO QUI.
                            // -------------------------------------

                            Result startResult =
                                MICU_StartSampling(
                                    MICU_ENCODING_PCM16_SIGNED,
                                    MICU_SAMPLE_RATE_16360,
                                    0,
                                    active_buf_size,
                                    true
                                );


                            // Procediamo SOLO se il MIC
                            // ha iniziato correttamente.

                            if (
                                R_SUCCEEDED(
                                    startResult
                                )
                            ) {

                                recordingTrack =
                                    selectedTrack;


                                isRecording =
                                    true;


                                // Cancella vecchia registrazione
                                // di questa traccia.

                                memset(
                                    tracks[
                                        recordingTrack
                                    ].buffer,
                                    0,
                                    active_buf_size
                                );


                                tracks[
                                    recordingTrack
                                ].active =
                                    false;


                                tracks[
                                    recordingTrack
                                ].peak =
                                    0;


                                // Rimuove la vecchia traccia
                                // dal master SENZA reset NDSP.

                                update_master_mix(
                                    tracks,
                                    masterVolume,
                                    play_buffer,
                                    total_samples,
                                    active_buf_size
                                );
                            }
                        }


                        // =========================================
                        // STOP REC
                        // =========================================

                        else if (
                            recordingTrack
                            ==
                            selectedTrack
                        ) {

                            // -------------------------------------
                            // IL MICROFONO SMETTE DI REGISTRARE QUI
                            // -------------------------------------

                            MICU_StopSampling();


                            isRecording =
                                false;


                            // Ora il CPU deve rileggere i dati
                            // scritti dal servizio MIC.

                            DSP_InvalidateDataCache(
                                mic_buffer,
                                active_buf_size
                            );


                            int16_t* mic_ptr =
                                (int16_t*)
                                mic_buffer;


                            // =====================================
                            // COPIA AUDIO
                            // =====================================

                            for (
                                size_t i = 0;
                                i < total_samples;
                                i++
                            ) {

                                tracks[
                                    recordingTrack
                                ].buffer[i]
                                    =
                                    mic_ptr[i];
                            }


                            // =====================================
                            // ATTIVA TRACCIA
                            // =====================================

                            tracks[
                                recordingTrack
                            ].active =
                                true;


                            tracks[
                                recordingTrack
                            ].peak =
                                calculate_peak(
                                    tracks[
                                        recordingTrack
                                    ].buffer,

                                    total_samples
                                );


                            // =====================================
                            // MIX
                            // =====================================

                            update_master_mix(
                                tracks,
                                masterVolume,
                                play_buffer,
                                total_samples,
                                active_buf_size
                            );


                            // =====================================
                            // PLAYBACK
                            // =====================================

                            update_ndsp(
                                play_buffer,
                                total_samples
                            );


                            // Riallinea la timeline al playback.

                            globalStartTime =
                                osGetTime();


                            recordingTrack =
                                -1;
                        }
                    }


                    // =============================================
                    // SPEED
                    // =============================================

                    else if (
                        selectedControl
                        ==
                        CONTROL_SPEED
                    ) {

                        editingSpeed =
                            true;
                    }


                    // =============================================
                    // REV
                    // =============================================

                    else if (
                        selectedControl
                        ==
                        CONTROL_REV
                    ) {

                        // futuro
                    }


                    // =============================================
                    // ERS
                    // =============================================

                    else if (
                        selectedControl
                        ==
                        CONTROL_ERS
                    ) {

                        if (
                            !isRecording
                        ) {

                            tracks[
                                selectedTrack
                            ].active =
                                false;


                            tracks[
                                selectedTrack
                            ].peak =
                                0;


                            memset(
                                tracks[
                                    selectedTrack
                                ].buffer,
                                0,
                                active_buf_size
                            );


                            update_master_mix(
                                tracks,
                                masterVolume,
                                play_buffer,
                                total_samples,
                                active_buf_size
                            );
                        }
                    }
                }
            }


            // =================================================
            // TOUCH FADER
            // =================================================

            if (
                kHeld
                &
                KEY_TOUCH
            ) {

                touchPosition touch;


                hidTouchRead(
                    &touch
                );


                int fader =
                    findTouchedFader(
                        touch.px,
                        touch.py
                    );


                if (
                    fader >= 0
                ) {

                    int newVolume =
                        yToVolume(
                            touch.py
                        );


                    // TRACK

                    if (
                        fader
                        <
                        NUM_TRACKS
                    ) {

                        if (
                            tracks[
                                fader
                            ].volume
                            !=
                            newVolume
                        ) {

                            tracks[
                                fader
                            ].volume =
                                newVolume;


                            if (
                                !isRecording
                            ) {

                                update_master_mix(
                                    tracks,
                                    masterVolume,
                                    play_buffer,
                                    total_samples,
                                    active_buf_size
                                );
                            }
                        }
                    }


                    // MASTER

                    else {

                        if (
                            masterVolume
                            !=
                            newVolume
                        ) {

                            masterVolume =
                                newVolume;


                            if (
                                !isRecording
                            ) {

                                update_master_mix(
                                    tracks,
                                    masterVolume,
                                    play_buffer,
                                    total_samples,
                                    active_buf_size
                                );
                            }
                        }
                    }
                }
            }
        }


        // ====================================================
        // VU
        // ====================================================

        if (
            timerConfirmed
        ) {

            u64 elapsed =
                osGetTime()
                -
                globalStartTime;


            u64 loopMs =
                (u64)
                selectedSeconds
                *
                1000;


            u32 positionMs =
                (u32)(
                    elapsed
                    %
                    loopMs
                );


            size_t samplePosition =
                (
                    (size_t)
                    positionMs
                    *
                    SAMPLE_RATE
                )
                /
                1000;


            float targetVU =
                calculateVU(
                    tracks,
                    masterVolume,
                    samplePosition,
                    total_samples
                );


            // Attack

            if (
                targetVU
                >
                vuLevel
            ) {

                vuLevel =
                    targetVU;
            }


            // Release

            else {

                vuLevel *=
                    0.88f;


                if (
                    vuLevel
                    <
                    0.001f
                ) {

                    vuLevel =
                        0.0f;
                }
            }
        }


        // ====================================================
        // RENDER
        // ====================================================

        C2D_TextBufClear(
            textBuf
        );


        C3D_FrameBegin(
            C3D_FRAME_SYNCDRAW
        );


        // ====================================================
        // TOP
        // ====================================================

        C2D_TargetClear(
            top,
            C2D_Color32(
                0,
                0,
                0,
                255
            )
        );


        C2D_SceneBegin(
            top
        );


        if (
            timerConfirmed
        ) {

            drawTop(
                tracks,

                selectedTrack,
                selectedControl,

                isRecording,
                recordingTrack,

                vuLevel
            );
        }

        else {

            drawText(
                textBuf,
                "LOOPING STATION V93",
                88,
                60,
                0.75f,

                C2D_Color32(
                    255,
                    255,
                    255,
                    255
                )
            );


            char buffer[32];


            snprintf(
                buffer,
                sizeof(buffer),
                "Loop: %d secondi",
                selectedSeconds
            );


            drawText(
                textBuf,
                buffer,
                125,
                120,
                0.60f,

                C2D_Color32(
                    255,
                    255,
                    255,
                    255
                )
            );


            drawText(
                textBuf,
                "SU/GIU cambia - A conferma",
                95,
                170,
                0.45f,

                C2D_Color32(
                    200,
                    200,
                    200,
                    255
                )
            );
        }


        // ====================================================
        // BOTTOM
        // ====================================================

        C2D_TargetClear(
            bottom,
            C2D_Color32(
                0,
                0,
                0,
                255
            )
        );


        C2D_SceneBegin(
            bottom
        );


        if (
            timerConfirmed
        ) {

            drawBottom(
                tracks,
                masterVolume,

                isRecording,
                recordingTrack
            );
        }


        C3D_FrameEnd(0);
    }


    // ========================================================
    // CLEANUP
    // ========================================================

    // Se usciamo mentre REC è ancora attivo,
    // fermiamo prima il campionamento.

    if (
        isRecording
    ) {

        MICU_StopSampling();
    }


    MICU_SetPower(
        false
    );


    micExit();


    ndspExit();


    if (
        uiSheet
    ) {

        C2D_SpriteSheetFree(
            uiSheet
        );
    }


    C2D_TextBufDelete(
        textBuf
    );


    if (
        play_buffer
    ) {

        linearFree(
            play_buffer
        );
    }


    for (
        int i = 0;
        i < NUM_TRACKS;
        i++
    ) {

        if (
            tracks[i].buffer
        ) {

            linearFree(
                tracks[i].buffer
            );
        }
    }


    if (
        mic_buffer
    ) {

        free(
            mic_buffer
        );
    }


    C2D_Fini();

    C3D_Fini();

    romfsExit();

    gfxExit();


    return 0;
}