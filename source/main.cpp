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
// GUI BOTTOM SCREEN
// ============================================================

static const int TRACK_X[NUM_TRACKS] = {
    32,
    92,
    152,
    212
};

#define MASTER_X 282

#define REC_TOP 18
#define REC_BOTTOM 48
#define REC_HALF_W 22

#define FADER_TOP 72
#define FADER_BOTTOM 200
#define FADER_HALF_W 22


// ============================================================
// COLORI
// ============================================================

static const u32 COLOR_BG =
    C2D_Color32(46, 43, 42, 255);

static const u32 COLOR_PANEL =
    C2D_Color32(57, 53, 51, 255);

static const u32 COLOR_PANEL_SELECTED =
    C2D_Color32(67, 62, 59, 255);

static const u32 COLOR_LINE =
    C2D_Color32(18, 17, 17, 255);

static const u32 COLOR_TEXT =
    C2D_Color32(235, 229, 211, 255);

static const u32 COLOR_TEXT_DIM =
    C2D_Color32(170, 165, 152, 255);

static const u32 COLOR_RED =
    C2D_Color32(185, 45, 36, 255);

static const u32 COLOR_RED_ACTIVE =
    C2D_Color32(255, 55, 45, 255);

static const u32 COLOR_GREEN =
    C2D_Color32(70, 190, 85, 255);

static const u32 COLOR_KNOB =
    C2D_Color32(220, 205, 166, 255);

static const u32 COLOR_METER =
    C2D_Color32(215, 210, 185, 255);

static const u32 COLOR_NEEDLE =
    C2D_Color32(35, 25, 20, 255);


// ============================================================
// TRACK
// ============================================================

typedef struct {

    int16_t* buffer;

    bool active;

    int16_t peak;

    int volume;

} Track;


// ============================================================
// WAVE BUFFER NDSP PERSISTENTE
// ============================================================

ndspWaveBuf masterWave;


// ============================================================
// TESTO
// ============================================================

void drawText(
    C2D_TextBuf textBuf,
    const char* str,
    float x,
    float y,
    float scale,
    u32 color
) {

    C2D_Text text;

    C2D_TextParse(
        &text,
        textBuf,
        str
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

        int32_t value =
            (int32_t)buf[i];

        if (value < 0)
            value = -value;

        if (value > max_v)
            max_v = value;
    }

    if (max_v > 32767)
        max_v = 32767;

    return (int16_t)max_v;
}


// ============================================================
// SOFTWARE MIXER
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


        // ----------------------------------------------------
        // MIX DELLE 4 TRACCE
        // ----------------------------------------------------

        for (
            int t = 0;
            t < NUM_TRACKS;
            t++
        ) {

            if (tracks[t].active) {

                int32_t sample =
                    (int32_t)
                    tracks[t].buffer[i];


                // Volume singola traccia
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
        }


        // ----------------------------------------------------
        // MASTER
        // ----------------------------------------------------

        mixed =
            (
                mixed
                *
                masterVolume
            )
            /
            100;


        // ----------------------------------------------------
        // BOOST DELLA VERSIONE ORIGINALE
        // ----------------------------------------------------

        mixed *= 12;


        // ----------------------------------------------------
        // CLIPPING
        // ----------------------------------------------------

        if (mixed > 32767)
            mixed = 32767;

        if (mixed < -32768)
            mixed = -32768;


        play_buffer[i] =
            (int16_t)mixed;
    }


    // NDSP deve vedere le modifiche al buffer
    DSP_FlushDataCache(
        play_buffer,
        buf_size
    );
}


// ============================================================
// AVVIA / RIAVVIA NDSP
//
// IMPORTANTE:
// questa funzione NON viene usata quando muoviamo un fader.
//
// Altrimenti il loop ricomincerebbe da zero.
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
        total_samples
        *
        sizeof(int16_t)
    );


    ndspChnWaveBufAdd(
        0,
        &masterWave
    );
}


// ============================================================
// RILEVA PULSANTE REC
//
// Restituisce:
// 0 = T1
// 1 = T2
// 2 = T3
// 3 = T4
// -1 = nessun REC
// ============================================================

int getRecTrack(
    int x,
    int y
) {

    if (
        y < REC_TOP
        ||
        y > REC_BOTTOM
    ) {

        return -1;
    }


    for (
        int i = 0;
        i < NUM_TRACKS;
        i++
    ) {

        if (
            x >= TRACK_X[i] - REC_HALF_W
            &&
            x <= TRACK_X[i] + REC_HALF_W
        ) {

            return i;
        }
    }


    return -1;
}


// ============================================================
// RILEVA FADER
//
// 0 = T1
// 1 = T2
// 2 = T3
// 3 = T4
// 4 = MASTER
// ============================================================

int getFader(
    int x,
    int y
) {

    if (
        y < FADER_TOP - 10
        ||
        y > FADER_BOTTOM + 10
    ) {

        return -1;
    }


    for (
        int i = 0;
        i < NUM_TRACKS;
        i++
    ) {

        if (
            x >= TRACK_X[i] - FADER_HALF_W
            &&
            x <= TRACK_X[i] + FADER_HALF_W
        ) {

            return i;
        }
    }


    if (
        x >= MASTER_X - FADER_HALF_W
        &&
        x <= MASTER_X + FADER_HALF_W
    ) {

        return 4;
    }


    return -1;
}


// ============================================================
// TOUCH Y -> VOLUME
//
// Alto  = 120%
// Basso = 0%
// ============================================================

int volumeFromY(
    int y
) {

    if (y < FADER_TOP)
        y = FADER_TOP;

    if (y > FADER_BOTTOM)
        y = FADER_BOTTOM;


    int range =
        FADER_BOTTOM
        -
        FADER_TOP;


    int volume =
        (
            (
                FADER_BOTTOM
                -
                y
            )
            *
            MAX_VOLUME
        )
        /
        range;


    if (volume < 0)
        volume = 0;

    if (volume > MAX_VOLUME)
        volume = MAX_VOLUME;


    return volume;
}


// ============================================================
// VOLUME -> Y GRAFICO
// ============================================================

float volumeToY(
    int volume
) {

    if (volume < 0)
        volume = 0;

    if (volume > MAX_VOLUME)
        volume = MAX_VOLUME;


    float range =
        (float)(
            FADER_BOTTOM
            -
            FADER_TOP
        );


    return
        (float)FADER_BOTTOM
        -
        (
            (
                (float)volume
                /
                (float)MAX_VOLUME
            )
            *
            range
        );
}


// ============================================================
// CALCOLO VU
// ============================================================

float calculateVU(
    Track* tracks,
    int masterVolume,
    size_t samplePosition,
    size_t total_samples
) {

    if (total_samples == 0)
        return 0.0f;


    if (samplePosition >= total_samples)
        samplePosition %= total_samples;


    int32_t mixed = 0;


    // Usiamo un piccolo gruppo di campioni attorno
    // alla posizione corrente, invece di uno solo.
    const size_t windowSize = 120;


    int32_t maxLevel = 0;


    for (
        size_t offset = 0;
        offset < windowSize;
        offset += 4
    ) {

        size_t pos =
            (
                samplePosition
                +
                offset
            )
            %
            total_samples;


        mixed = 0;


        for (
            int t = 0;
            t < NUM_TRACKS;
            t++
        ) {

            if (tracks[t].active) {

                int32_t sample =
                    (int32_t)
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


        int32_t level = mixed;

        if (level < 0)
            level = -level;


        if (level > maxLevel)
            maxLevel = level;
    }


    float result =
        (float)maxLevel
        /
        32767.0f;


    if (result > 1.0f)
        result = 1.0f;


    return result;
}


// ============================================================
// VU METER
// ============================================================

void drawVUMeter(
    C2D_TextBuf textBuf,
    float vu
) {

    // --------------------------------------------------------
    // CORPO
    // --------------------------------------------------------

    C2D_DrawRectSolid(
        267,
        35,
        0.1f,
        120,
        92,
        COLOR_PANEL
    );


    C2D_DrawRectSolid(
        276,
        43,
        0.2f,
        102,
        72,
        COLOR_METER
    );


    float cx =
        327.0f;

    float cy =
        105.0f;


    const float startAngle =
        -2.60f;

    const float endAngle =
        -0.54f;


    // --------------------------------------------------------
    // TACCHE
    // --------------------------------------------------------

    for (
        int i = 0;
        i <= 10;
        i++
    ) {

        float amount =
            (float)i
            /
            10.0f;


        float angle =
            startAngle
            +
            (
                endAngle
                -
                startAngle
            )
            *
            amount;


        float x1 =
            cx
            +
            cosf(angle)
            *
            43.0f;


        float y1 =
            cy
            +
            sinf(angle)
            *
            43.0f;


        float x2 =
            cx
            +
            cosf(angle)
            *
            36.0f;


        float y2 =
            cy
            +
            sinf(angle)
            *
            36.0f;


        C2D_DrawLine(
            x1,
            y1,
            COLOR_LINE,

            x2,
            y2,
            COLOR_LINE,

            1.5f,
            0.4f
        );
    }


    // --------------------------------------------------------
    // LIMITI VU
    // --------------------------------------------------------

    if (vu < 0.0f)
        vu = 0.0f;

    if (vu > 1.0f)
        vu = 1.0f;


    // --------------------------------------------------------
    // LANCETTA
    // --------------------------------------------------------

    float needleAngle =
        startAngle
        +
        (
            endAngle
            -
            startAngle
        )
        *
        vu;


    float needleX =
        cx
        +
        cosf(
            needleAngle
        )
        *
        38.0f;


    float needleY =
        cy
        +
        sinf(
            needleAngle
        )
        *
        38.0f;


    C2D_DrawLine(
        cx,
        cy,
        COLOR_NEEDLE,

        needleX,
        needleY,
        COLOR_NEEDLE,

        2.0f,
        0.7f
    );


    C2D_DrawCircleSolid(
        cx,
        cy,
        0.8f,
        3.0f,
        COLOR_NEEDLE
    );


    drawText(
        textBuf,
        "VU",
        318,
        91,
        0.45f,
        COLOR_LINE
    );


    drawText(
        textBuf,
        "BUSS LR",
        304,
        17,
        0.42f,
        COLOR_TEXT
    );
}


// ============================================================
// TOP SCREEN
//
// NOTA IMPORTANTE:
// questa funzione NON pulisce il render target.
// Il target viene già pulito nel main.
// ============================================================

void drawTop(
    C2D_TextBuf textBuf,
    Track* tracks,
    bool is_recording,
    int recordingTrack,
    float vu,
    float progress
) {

    // --------------------------------------------------------
    // QUATTRO COLONNE
    // --------------------------------------------------------

    for (
        int i = 0;
        i < NUM_TRACKS;
        i++
    ) {

        float x =
            7.0f
            +
            i
            *
            62.0f;


        // pannello
        C2D_DrawRectSolid(
            x,
            8,
            0.1f,
            58,
            205,
            COLOR_PANEL
        );


        // numero traccia
        char number[8];

        snprintf(
            number,
            sizeof(number),
            "%d",
            i + 1
        );


        drawText(
            textBuf,
            number,
            x + 27,
            16,
            0.45f,
            COLOR_TEXT
        );


        // ----------------------------------------------------
        // REC
        // ----------------------------------------------------

        drawText(
            textBuf,
            "REC",
            x + 7,
            43,
            0.35f,
            COLOR_TEXT
        );


        u32 recColor =
            (
                is_recording
                &&
                recordingTrack == i
            )
            ?
            COLOR_RED_ACTIVE
            :
            COLOR_RED;


        C2D_DrawRectSolid(
            x + 9,
            58,
            0.3f,
            34,
            18,
            COLOR_KNOB
        );


        C2D_DrawCircleSolid(
            x + 18,
            67,
            0.5f,
            5,
            recColor
        );


        // LED stato traccia
        C2D_DrawCircleSolid(
            x + 48,
            48,
            0.5f,
            2.5f,

            tracks[i].active
            ?
            COLOR_GREEN
            :
            COLOR_TEXT_DIM
        );


        // ----------------------------------------------------
        // SPEED
        // solo grafica per ora
        // ----------------------------------------------------

        drawText(
            textBuf,
            "SPEED",
            x + 7,
            90,
            0.32f,
            COLOR_TEXT
        );


        C2D_DrawRectSolid(
            x + 10,
            105,
            0.3f,
            35,
            7,
            COLOR_LINE
        );


        C2D_DrawRectSolid(
            x + 25,
            102,
            0.4f,
            8,
            13,
            COLOR_KNOB
        );


        drawText(
            textBuf,
            "- x1 +",
            x + 8,
            119,
            0.30f,
            COLOR_TEXT
        );


        // ----------------------------------------------------
        // REV
        // solo grafica
        // ----------------------------------------------------

        C2D_DrawRectSolid(
            x + 9,
            148,
            0.3f,
            11,
            11,
            COLOR_KNOB
        );


        drawText(
            textBuf,
            "REV",
            x + 24,
            147,
            0.30f,
            COLOR_TEXT
        );


        // ----------------------------------------------------
        // ERS
        // solo grafica
        // ----------------------------------------------------

        C2D_DrawRectSolid(
            x + 9,
            170,
            0.3f,
            11,
            11,
            COLOR_KNOB
        );


        drawText(
            textBuf,
            "ERS",
            x + 24,
            169,
            0.30f,
            COLOR_TEXT
        );
    }


    // --------------------------------------------------------
    // VU
    // --------------------------------------------------------

    drawVUMeter(
        textBuf,
        vu
    );


    // --------------------------------------------------------
    // FILE / LOOP
    // solo grafica
    // --------------------------------------------------------

    drawText(
        textBuf,
        "File",
        304,
        143,
        0.37f,
        COLOR_TEXT_DIM
    );


    drawText(
        textBuf,
        "Loop 1",
        304,
        160,
        0.34f,
        COLOR_TEXT_DIM
    );


    drawText(
        textBuf,
        "Loop 2",
        304,
        174,
        0.34f,
        COLOR_TEXT_DIM
    );


    drawText(
        textBuf,
        "Loop 3",
        304,
        188,
        0.34f,
        COLOR_TEXT_DIM
    );


    drawText(
        textBuf,
        "Loop 4",
        304,
        202,
        0.34f,
        COLOR_TEXT_DIM
    );


    // --------------------------------------------------------
    // TIMELINE
    // --------------------------------------------------------

    C2D_DrawRectSolid(
        268,
        224,
        0.2f,
        120,
        4,
        COLOR_LINE
    );


    C2D_DrawRectSolid(
        268,
        224,
        0.4f,
        120.0f
        *
        progress,
        4,
        COLOR_KNOB
    );
}


// ============================================================
// FADER
// ============================================================

void drawFader(
    C2D_TextBuf textBuf,
    int x,
    int volume,
    const char* label,
    bool selected
) {

    // --------------------------------------------------------
    // PANNELLO
    // --------------------------------------------------------

    C2D_DrawRectSolid(
        x - 26,
        57,
        0.1f,
        52,
        174,

        selected
        ?
        COLOR_PANEL_SELECTED
        :
        COLOR_PANEL
    );


    // --------------------------------------------------------
    // BINARIO
    // --------------------------------------------------------

    C2D_DrawRectSolid(
        x - 3,
        FADER_TOP,
        0.3f,
        6,
        FADER_BOTTOM
        -
        FADER_TOP,
        COLOR_LINE
    );


    // --------------------------------------------------------
    // TACCHE
    // --------------------------------------------------------

    for (
        int i = 0;
        i <= 6;
        i++
    ) {

        float y =
            FADER_BOTTOM
            -
            (
                (
                    FADER_BOTTOM
                    -
                    FADER_TOP
                )
                /
                6.0f
            )
            *
            i;


        C2D_DrawLine(
            x + 9,
            y,
            COLOR_TEXT_DIM,

            x + 15,
            y,
            COLOR_TEXT_DIM,

            1.0f,
            0.3f
        );
    }


    // --------------------------------------------------------
    // LEVETTA
    // --------------------------------------------------------

    float knobY =
        volumeToY(
            volume
        );


    C2D_DrawRectSolid(
        x - 14,
        knobY - 4,
        0.6f,
        28,
        8,
        COLOR_KNOB
    );


    // --------------------------------------------------------
    // LABEL
    // --------------------------------------------------------

    drawText(
        textBuf,
        label,
        x - 4,
        211,
        0.38f,
        COLOR_TEXT
    );


    // --------------------------------------------------------
    // VOLUME NUMERICO
    // --------------------------------------------------------

    char volumeText[16];

    snprintf(
        volumeText,
        sizeof(volumeText),
        "%d%%",
        volume
    );


    drawText(
        textBuf,
        volumeText,
        x - 15,
        51,
        0.30f,
        COLOR_TEXT
    );
}


// ============================================================
// BOTTOM SCREEN
// ============================================================

void drawBottom(
    C2D_TextBuf textBuf,
    Track* tracks,
    int masterVolume,
    bool is_recording,
    int recordingTrack,
    int selectedTrack
) {

    // --------------------------------------------------------
    // 4 FADER
    // --------------------------------------------------------

    for (
        int i = 0;
        i < NUM_TRACKS;
        i++
    ) {

        char label[4];

        snprintf(
            label,
            sizeof(label),
            "%d",
            i + 1
        );


        drawFader(
            textBuf,
            TRACK_X[i],
            tracks[i].volume,
            label,
            selectedTrack == i
        );
    }


    // --------------------------------------------------------
    // MASTER
    // --------------------------------------------------------

    drawFader(
        textBuf,
        MASTER_X,
        masterVolume,
        "M",
        false
    );


    // --------------------------------------------------------
    // PULSANTI REC
    // --------------------------------------------------------

    for (
        int i = 0;
        i < NUM_TRACKS;
        i++
    ) {

        bool recording =
            (
                is_recording
                &&
                recordingTrack == i
            );


        // Corpo pulsante
        C2D_DrawRectSolid(
            TRACK_X[i] - 20,
            REC_TOP,
            0.5f,
            40,
            REC_BOTTOM
            -
            REC_TOP,
            COLOR_KNOB
        );


        // Pulsante rosso
        C2D_DrawCircleSolid(
            TRACK_X[i],
            31,
            0.7f,
            8,

            recording
            ?
            COLOR_RED_ACTIVE
            :
            COLOR_RED
        );


        // LED superiore
        C2D_DrawCircleSolid(
            TRACK_X[i] + 19,
            16,
            0.8f,
            3,

            recording
            ?
            COLOR_RED_ACTIVE
            :
            (
                tracks[i].active
                ?
                COLOR_GREEN
                :
                COLOR_TEXT_DIM
            )
        );
    }


    drawText(
        textBuf,
        "MASTER",
        263,
        218,
        0.28f,
        COLOR_TEXT
    );
}


// ============================================================
// MAIN
// ============================================================

int main() {

    // ========================================================
    // GRAFICA
    // ========================================================

    gfxInitDefault();


    if (
        !C3D_Init(
            C3D_DEFAULT_CMDBUF_SIZE
        )
    ) {

        gfxExit();

        return 1;
    }


    if (
        !C2D_Init(
            C2D_DEFAULT_MAX_OBJECTS
        )
    ) {

        C3D_Fini();

        gfxExit();

        return 1;
    }


    C2D_Prepare();


    // --------------------------------------------------------
    // CREAZIONE TARGET
    //
    // Questi sono gli unici target che useremo.
    // NON esiste C2D_GetScreenTarget().
    // --------------------------------------------------------

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
            4096
        );


    // ========================================================
    // BUFFER AUDIO
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


    // ========================================================
    // TRACK
    // ========================================================

    Track tracks[
        NUM_TRACKS
    ];


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


        if (
            tracks[i].buffer
        ) {

            memset(
                tracks[i].buffer,
                0,
                MAX_BUF_SIZE
            );
        }
    }


    // --------------------------------------------------------
    // CONTROLLO BUFFER
    // --------------------------------------------------------

    bool allocationError =
        (
            mic_buffer == NULL
            ||
            play_buffer == NULL
        );


    for (
        int i = 0;
        i < NUM_TRACKS;
        i++
    ) {

        if (
            tracks[i].buffer == NULL
        ) {

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


        C2D_TextBufDelete(
            textBuf
        );

        C2D_Fini();

        C3D_Fini();

        gfxExit();

        return 1;
    }


    memset(
        play_buffer,
        0,
        MAX_BUF_SIZE
    );


    // ========================================================
    // AUDIO INIT
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


    memset(
        &masterWave,
        0,
        sizeof(ndspWaveBuf)
    );


    // ========================================================
    // STATO
    // ========================================================

    int selectedSeconds =
        5;


    bool timerConfirmed =
        false;


    bool is_recording =
        false;


    int recordingTrack =
        -1;


    int selectedTrack =
        0;


    int masterVolume =
        100;


    u64 globalStartTime =
        0;


    size_t active_buf_size =
        0;


    size_t total_samples =
        0;


    float vuLevel =
        0.0f;


    // ========================================================
    // MAIN LOOP
    // ========================================================

    while (
        aptMainLoop()
    ) {

        // ----------------------------------------------------
        // INPUT
        // ----------------------------------------------------

        hidScanInput();


        u32 kDown =
            hidKeysDown();


        u32 kHeld =
            hidKeysHeld();


        if (
            kDown
            &
            KEY_START
        ) {

            break;
        }


        // ====================================================
        // SCELTA DURATA LOOP
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

                active_buf_size =
                    ALIGN_PAGE(
                        selectedSeconds
                        *
                        SAMPLE_RATE
                        *
                        2
                    );


                total_samples =
                    active_buf_size
                    /
                    2;


                timerConfirmed =
                    true;


                // --------------------------------------------
                // MICROFONO
                // --------------------------------------------

                MICU_SetPower(
                    true
                );


                MICU_SetGain(
                    7
                );


                MICU_StartSampling(
                    MICU_ENCODING_PCM16_SIGNED,
                    MICU_SAMPLE_RATE_16360,
                    0,
                    active_buf_size,
                    true
                );


                globalStartTime =
                    osGetTime();
            }
        }


        // ====================================================
        // LOOP STATION
        // ====================================================

        else {

            touchPosition touch;


            // =================================================
            // NUOVO TOCCO
            //
            // Controlliamo i pulsanti rossi REC.
            // =================================================

            if (
                kDown
                &
                KEY_TOUCH
            ) {

                hidTouchRead(
                    &touch
                );


                int recTrack =
                    getRecTrack(
                        touch.px,
                        touch.py
                    );


                if (
                    recTrack >= 0
                ) {

                    selectedTrack =
                        recTrack;


                    // =========================================
                    // INIZIO RECORD
                    // =========================================

                    if (
                        !is_recording
                    ) {

                        recordingTrack =
                            recTrack;


                        is_recording =
                            true;


                        // Elimina l'eventuale vecchia
                        // registrazione della traccia.
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


                        // Togliamo la vecchia traccia dal mix.
                        //
                        // IMPORTANTE:
                        // NON chiamiamo update_ndsp().
                        //
                        // Quindi il loop NON ricomincia da 0.
                        update_master_mix(
                            tracks,
                            masterVolume,
                            play_buffer,
                            total_samples,
                            active_buf_size
                        );
                    }


                    // =========================================
                    // STOP RECORD
                    //
                    // Devi toccare di nuovo il pulsante
                    // rosso della stessa traccia.
                    // =========================================

                    else if (
                        recordingTrack
                        ==
                        recTrack
                    ) {

                        is_recording =
                            false;


                        DSP_InvalidateDataCache(
                            mic_buffer,
                            active_buf_size
                        );


                        int16_t* mic_ptr =
                            (int16_t*)
                            mic_buffer;


                        // -------------------------------------
                        // COPIA AUDIO
                        // -------------------------------------

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


                        // -------------------------------------
                        // RICREA MASTER MIX
                        // -------------------------------------

                        update_master_mix(
                            tracks,
                            masterVolume,
                            play_buffer,
                            total_samples,
                            active_buf_size
                        );


                        // -------------------------------------
                        // AVVIA AUDIO
                        // -------------------------------------

                        update_ndsp(
                            play_buffer,
                            total_samples
                        );


                        recordingTrack =
                            -1;
                    }
                }
            }


            // =================================================
            // FADER TOUCH
            //
            // KEY_TOUCH deve essere tenuto premuto,
            // così il fader segue il pennino.
            // =================================================

            if (
                kHeld
                &
                KEY_TOUCH
            ) {

                hidTouchRead(
                    &touch
                );


                int fader =
                    getFader(
                        touch.px,
                        touch.py
                    );


                if (
                    fader >= 0
                ) {

                    int newVolume =
                        volumeFromY(
                            touch.py
                        );


                    // =========================================
                    // FADER T1-T4
                    // =========================================

                    if (
                        fader
                        <
                        NUM_TRACKS
                    ) {

                        selectedTrack =
                            fader;


                        if (
                            tracks[fader].volume
                            !=
                            newVolume
                        ) {

                            tracks[fader].volume =
                                newVolume;


                            // ---------------------------------
                            // DURANTE LA REGISTRAZIONE
                            // NON TOCCHIAMO PLAY_BUFFER.
                            // ---------------------------------

                            if (
                                !is_recording
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


                    // =========================================
                    // MASTER
                    // =========================================

                    else {

                        if (
                            masterVolume
                            !=
                            newVolume
                        ) {

                            masterVolume =
                                newVolume;


                            // Anche il master durante REC
                            // non modifica il playback.
                            if (
                                !is_recording
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


            // =================================================
            // B = CLEAR
            //
            // Per ora resta fisico.
            // =================================================

            if (
                (
                    kDown
                    &
                    KEY_B
                )
                &&
                !is_recording
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


                // NON resetta NDSP.
                update_master_mix(
                    tracks,
                    masterVolume,
                    play_buffer,
                    total_samples,
                    active_buf_size
                );
            }
        }


        // ====================================================
        // TIMELINE + VU
        // ====================================================

        float progress =
            0.0f;


        if (
            timerConfirmed
        ) {

            u64 currentTime =
                osGetTime();


            u64 elapsedMs =
                currentTime
                -
                globalStartTime;


            u64 loopMs =
                (u64)
                selectedSeconds
                *
                1000;


            u32 currentLoopPosMs =
                (u32)(
                    elapsedMs
                    %
                    loopMs
                );


            progress =
                (float)
                currentLoopPosMs
                /
                (float)
                loopMs;


            size_t samplePosition =
                (
                    (size_t)
                    currentLoopPosMs
                    *
                    SAMPLE_RATE
                )
                /
                1000;


            // ------------------------------------------------
            // TARGET DEL VU
            // ------------------------------------------------

            float targetVU =
                calculateVU(
                    tracks,
                    masterVolume,
                    samplePosition,
                    total_samples
                );


            // Attack veloce
            if (
                targetVU
                >
                vuLevel
            ) {

                vuLevel =
                    targetVU;
            }

            // Release lento
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
        // TOP SCREEN
        // ====================================================

        C2D_TargetClear(
            top,
            COLOR_BG
        );


        C2D_SceneBegin(
            top
        );


        // ----------------------------------------------------
        // SCHERMATA SCELTA DURATA
        // ----------------------------------------------------

        if (
            !timerConfirmed
        ) {

            drawText(
                textBuf,
                "TOGGLE LOOPER V89",
                100,
                55,
                0.75f,
                COLOR_TEXT
            );


            drawText(
                textBuf,
                "Durata loop",
                142,
                105,
                0.55f,
                COLOR_TEXT_DIM
            );


            char timeText[32];

            snprintf(
                timeText,
                sizeof(timeText),
                "%d secondi",
                selectedSeconds
            );


            drawText(
                textBuf,
                timeText,
                155,
                135,
                0.65f,
                COLOR_KNOB
            );


            drawText(
                textBuf,
                "SU/GIU cambia - A conferma",
                105,
                180,
                0.45f,
                COLOR_TEXT_DIM
            );
        }


        // ----------------------------------------------------
        // MIXER
        // ----------------------------------------------------

        else {

            drawTop(
                textBuf,
                tracks,
                is_recording,
                recordingTrack,
                vuLevel,
                progress
            );
        }


        // ====================================================
        // BOTTOM SCREEN
        // ====================================================

        C2D_TargetClear(
            bottom,
            COLOR_BG
        );


        C2D_SceneBegin(
            bottom
        );


        if (
            timerConfirmed
        ) {

            drawBottom(
                textBuf,
                tracks,
                masterVolume,
                is_recording,
                recordingTrack,
                selectedTrack
            );
        }

        else {

            drawText(
                textBuf,
                "Premi A per entrare nel mixer",
                55,
                105,
                0.48f,
                COLOR_TEXT
            );
        }


        // ====================================================
        // FINE FRAME
        // ====================================================

        C3D_FrameEnd(0);
    }


    // ========================================================
    // CLEANUP
    // ========================================================

    MICU_StopSampling();


    micExit();


    ndspExit();


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


    C2D_TextBufDelete(
        textBuf
    );


    C2D_Fini();


    C3D_Fini();


    gfxExit();


    return 0;
}