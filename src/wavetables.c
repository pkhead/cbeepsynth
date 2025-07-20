#include <math.h>
#include "util.h"
#include "wavetables.h"

float sine_wave_f[SINE_WAVE_LENGTH + 1];
double sine_wave_d[SINE_WAVE_LENGTH + 1];
static int need_init_wavetables = 1;

wavetable_desc_s chip_wavetables[BPBX_CHIP_WAVE_COUNT];

#define INIT_WAVETABLE(INDEX, EXPR, ...)                \
    {                                                   \
        static double arr[] = {__VA_ARGS__, 0};         \
        center_wave(arr, sizeof(arr)/sizeof(*arr));     \
        chip_wavetables[INDEX] = (wavetable_desc_s) {   \
            .expression = EXPR,                         \
            .values = arr,                              \
            .length = sizeof(arr)/sizeof(*arr)          \
        };                                              \
    }

#define INIT_WAVETABLE_N(INDEX, EXPR, ...)                          \
    {                                                               \
        static double arr[] = {__VA_ARGS__};                        \
        center_and_normalize_wave(arr, sizeof(arr)/sizeof(*arr));   \
        chip_wavetables[INDEX] = (wavetable_desc_s) {               \
            .expression = EXPR,                                     \
            .values = arr,                                          \
            .length = sizeof(arr)/sizeof(*arr)                      \
        };                                                          \
    }

static void center_wave(double *wave, size_t length) {
    length--;

    double sum = 0.0;
    for (size_t i = 0; i < length; i++) {
        sum += wave[i];
    }
    const double average = sum / length;
    for (size_t i = 0; i < length; i++) {
        wave[i] -= average;
    }
    // perform_intergal(wave);  // what is the point of this?

    // The first sample should be zero, and we'll duplicate it at the end for easier interpolation.
    // (adding the 0 is done in the macro. this is why i subtract 1 from length.)
}

static void center_and_normalize_wave(double *wave, size_t length) {
    double magn = 0.0;
    center_wave(wave, length);

    // Going to length-1 because an extra 0 sample is added on the end as part of centerWave, which shouldn't impact magnitude calculation.
    for (size_t i = 0; i < length - 1; i++) {
        magn += fabs(wave[i]);
    }
    const double magn_avg = magn / (length - 1);

    for (size_t i = 0; i < length - 1; i++) {
        wave[i] = wave[i] / magn_avg;
    }
}

void init_wavetables() {
    if (!need_init_wavetables) return;
    need_init_wavetables = 0;

    // init sine wavetable
    for (int i = 0; i < SINE_WAVE_LENGTH + 1; i++) {
        sine_wave_d[i] = sin((double)i / SINE_WAVE_LENGTH * PI2);
        sine_wave_f[i] = sinf((float)i / SINE_WAVE_LENGTH * PI2f);
    }

    INIT_WAVETABLE(
        BPBX_CHIP_WAVE_ROUNDED,
        0.94,
        0.0, 0.2, 0.4, 0.5, 0.6, 0.7, 0.8, 0.85, 0.9, 0.95, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.95, 0.9, 0.85, 0.8, 0.7, 0.6, 0.5, 0.4, 0.2, 0.0, -0.2, -0.4, -0.5, -0.6, -0.7, -0.8, -0.85, -0.9, -0.95, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -0.95, -0.9, -0.85, -0.8, -0.7, -0.6, -0.5, -0.4, -0.2
    );
    INIT_WAVETABLE(
        BPBX_CHIP_WAVE_TRIANGLE,
        1.0,
        1.0 / 15.0, 3.0 / 15.0, 5.0 / 15.0, 7.0 / 15.0, 9.0 / 15.0, 11.0 / 15.0, 13.0 / 15.0, 15.0 / 15.0, 15.0 / 15.0, 13.0 / 15.0, 11.0 / 15.0, 9.0 / 15.0, 7.0 / 15.0, 5.0 / 15.0, 3.0 / 15.0, 1.0 / 15.0, -1.0 / 15.0, -3.0 / 15.0, -5.0 / 15.0, -7.0 / 15.0, -9.0 / 15.0, -11.0 / 15.0, -13.0 / 15.0, -15.0 / 15.0, -15.0 / 15.0, -13.0 / 15.0, -11.0 / 15.0, -9.0 / 15.0, -7.0 / 15.0, -5.0 / 15.0, -3.0 / 15.0, -1.0 / 15.0
    );
    INIT_WAVETABLE(
        BPBX_CHIP_WAVE_SQUARE,
        0.5,
        1.0, -1.0
    );
    INIT_WAVETABLE(
        BPBX_CHIP_WAVE_PULSE4,
        0.5,
        1.0, -1.0, -1.0, -1.0
    );
    INIT_WAVETABLE(
        BPBX_CHIP_WAVE_PULSE8,
        0.5,
        1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0
    );
    INIT_WAVETABLE(
        BPBX_CHIP_WAVE_SAWTOOTH,
        0.65,
        1.0 / 31.0, 3.0 / 31.0, 5.0 / 31.0, 7.0 / 31.0, 9.0 / 31.0, 11.0 / 31.0, 13.0 / 31.0, 15.0 / 31.0, 17.0 / 31.0, 19.0 / 31.0, 21.0 / 31.0, 23.0 / 31.0, 25.0 / 31.0, 27.0 / 31.0, 29.0 / 31.0, 31.0 / 31.0, -31.0 / 31.0, -29.0 / 31.0, -27.0 / 31.0, -25.0 / 31.0, -23.0 / 31.0, -21.0 / 31.0, -19.0 / 31.0, -17.0 / 31.0, -15.0 / 31.0, -13.0 / 31.0, -11.0 / 31.0, -9.0 / 31.0, -7.0 / 31.0, -5.0 / 31.0, -3.0 / 31.0, -1.0 / 31.0
    );
    INIT_WAVETABLE(
        BPBX_CHIP_WAVE_DOUBLE_SAW,
        0.5,
        0.0, -0.2, -0.4, -0.6, -0.8, -1.0, 1.0, -0.8, -0.6, -0.4, -0.2, 1.0, 0.8, 0.6, 0.4, 0.2
    );
    INIT_WAVETABLE(
        BPBX_CHIP_WAVE_DOUBLE_PULSE,
        0.4,
        1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0
    );
    INIT_WAVETABLE(
        BPBX_CHIP_WAVE_SPIKY,
        0.4,
        1.0, -1.0, 1.0, -1.0, 1.0, 0.0
    );
    INIT_WAVETABLE_N(
        BPBX_CHIP_WAVE_SINE,
        0.88,
        8.0, 9.0, 11.0, 12.0, 13.0, 14.0, 15.0, 15.0, 15.0, 15.0, 14.0, 14.0, 13.0, 11.0, 10.0, 9.0, 7.0, 6.0, 4.0, 3.0, 2.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 2.0, 4.0, 5.0, 6.0
    );
    INIT_WAVETABLE_N(
        BPBX_CHIP_WAVE_FLUTE,
        0.8,
        3.0, 4.0, 6.0, 8.0, 10.0, 11.0, 13.0, 14.0, 15.0, 15.0, 14.0, 13.0, 11.0, 8.0, 5.0, 3.0
    );
    INIT_WAVETABLE_N(
        BPBX_CHIP_WAVE_HARP,
        0.8,
        0.0, 3.0, 3.0, 3.0, 4.0, 5.0, 5.0, 6.0, 7.0, 8.0, 9.0, 11.0, 11.0, 13.0, 13.0, 15.0, 15.0, 14.0, 12.0, 11.0, 10.0, 9.0, 8.0, 7.0, 7.0, 5.0, 4.0, 3.0, 2.0, 1.0, 0.0, 0.0
    );
    INIT_WAVETABLE_N(
        BPBX_CHIP_WAVE_SHARP_CLARINET,
        0.38,
        0.0, 0.0, 0.0, 1.0, 1.0, 8.0, 8.0, 9.0, 9.0, 9.0, 8.0, 8.0, 8.0, 8.0, 8.0, 9.0, 9.0, 7.0, 9.0, 9.0, 10.0, 4.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
    );
    INIT_WAVETABLE_N(
        BPBX_CHIP_WAVE_SOFT_CLARINET,
        0.45,
        0.0, 1.0, 5.0, 8.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 11.0, 11.0, 12.0, 13.0, 12.0, 10.0, 9.0, 7.0, 6.0, 4.0, 3.0, 3.0, 3.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0
    );
    INIT_WAVETABLE_N(
        BPBX_CHIP_WAVE_ALTO_SAX,
        0.3,
        5.0, 5.0, 6.0, 4.0, 3.0, 6.0, 8.0, 7.0, 2.0, 1.0, 5.0, 6.0, 5.0, 4.0, 5.0, 7.0, 9.0, 11.0, 13.0, 14.0, 14.0, 14.0, 14.0, 13.0, 10.0, 8.0, 7.0, 7.0, 4.0, 3.0, 4.0, 2.0
    );
    INIT_WAVETABLE_N(
        BPBX_CHIP_WAVE_BASSOON,
        0.35,
        9.0, 9.0, 7.0, 6.0, 5.0, 4.0, 4.0, 4.0, 4.0, 5.0, 7.0, 8.0, 9.0, 10.0, 11.0, 13.0, 13.0, 11.0, 10.0, 9.0, 7.0, 6.0, 4.0, 2.0, 1.0, 1.0, 1.0, 2.0, 2.0, 5.0, 11.0, 14.0
    );
    INIT_WAVETABLE_N(
        BPBX_CHIP_WAVE_TRUMPET,
        0.22,
        10.0, 11.0, 8.0, 6.0, 5.0, 5.0, 5.0, 6.0, 7.0, 7.0, 7.0, 7.0, 6.0, 6.0, 7.0, 7.0, 7.0, 7.0, 7.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 7.0, 8.0, 9.0, 11.0, 14.0
    );
    INIT_WAVETABLE_N(
        BPBX_CHIP_WAVE_ELECTRIC_GUITAR,
        0.2,
        11.0, 12.0, 12.0, 10.0, 6.0, 6.0, 8.0, 0.0, 2.0, 4.0, 8.0, 10.0, 9.0, 10.0, 1.0, 7.0, 11.0, 3.0, 6.0, 6.0, 8.0, 13.0, 14.0, 2.0, 0.0, 12.0, 8.0, 4.0, 13.0, 11.0, 10.0, 13.0
    );
    INIT_WAVETABLE_N(
        BPBX_CHIP_WAVE_ORGAN,
        0.2,
        11.0, 10.0, 12.0, 11.0, 14.0, 7.0, 5.0, 5.0, 12.0, 10.0, 10.0, 9.0, 12.0, 6.0, 4.0, 5.0, 13.0, 12.0, 12.0, 10.0, 12.0, 5.0, 2.0, 2.0, 8.0, 6.0, 6.0, 5.0, 8.0, 3.0, 2.0, 1.0
    );
    INIT_WAVETABLE_N(
        BPBX_CHIP_WAVE_PAN_FLUTE,
        0.35,
        1.0, 4.0, 7.0, 6.0, 7.0, 9.0, 7.0, 7.0, 11.0, 12.0, 13.0, 15.0, 13.0, 11.0, 11.0, 12.0, 13.0, 10.0, 7.0, 5.0, 3.0, 6.0, 10.0, 7.0, 3.0, 3.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0
    );
    INIT_WAVETABLE(
        BPBX_CHIP_WAVE_GLITCH,
        0.5,
        1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0
    );
}