#include "wavetables.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "util.h"
#include "fft.h"
#include "context.h"
#include "alloc.h"

// performIntegralOld
static void harmonics_perform_integral(float *wave, size_t length) {
    // Old ver used in harmonics/picked string instruments, manipulates wave in place.
    double cumulative = 0.0;
    for (size_t i = 0; i < length; i++) {
        const double temp = (double)wave[i];
        wave[i] += cumulative;
        cumulative += temp;
    }
}

void generate_harmonics(const wavetables_s *tables,
                        uint8_t controls[BPBXSYN_HARMONICS_CONTROL_COUNT],
                        int harmonics_rendered,
                        float wave[HARMONICS_WAVE_LENGTH + 1])
{
    memset(wave, 0, HARMONICS_WAVE_LENGTH * sizeof(float));

    const float *retro_wave = tables->noise_wavetables[BPBXSYN_NOISE_RETRO].samples;

    const double overall_slope = -0.25;
    double combined_control_point_amp = 1;

    for (int harmonic_index = 0; harmonic_index < harmonics_rendered; harmonic_index++) {
        const int harmonic_freq = harmonic_index + 1;
        double control_value =
            harmonic_index < BPBXSYN_HARMONICS_CONTROL_COUNT ?
            (double)controls[harmonic_index] : 
            (double)controls[BPBXSYN_HARMONICS_CONTROL_COUNT - 1];

        if (harmonic_index >= BPBXSYN_HARMONICS_CONTROL_COUNT) {
            control_value *= 1.0 -
                (double)(harmonic_index - BPBXSYN_HARMONICS_CONTROL_COUNT) /
                (harmonics_rendered - BPBXSYN_HARMONICS_CONTROL_COUNT);
        }

        const double normalized_value = control_value / BPBXSYN_HARMONICS_CONTROL_MAX;
        double amplitude =
            pow(2.0, control_value - BPBXSYN_HARMONICS_CONTROL_MAX + 1) *
            sqrt(normalized_value);
        
        if (harmonic_index < BPBXSYN_HARMONICS_CONTROL_COUNT) {
            combined_control_point_amp += amplitude;
        }
        amplitude *= pow((double)harmonic_freq, overall_slope);
        
        // Multiply all the sine wave amplitudes by 1 or -1 based on the LFSR
        // retro wave (effectively random) to avoid egregiously tall spikes.
        amplitude *= retro_wave[harmonic_index + 589];

        wave[HARMONICS_WAVE_LENGTH - harmonic_freq] = amplitude;
    }

    fft_inverse_real_fourier_transform(wave, HARMONICS_WAVE_LENGTH);

    // Limit the maximum wave amplitude
    const double mult = 1.0 / pow(combined_control_point_amp, 0.7);
    for (int i = 0; i < HARMONICS_WAVE_LENGTH; i++) wave[i] *= mult;

    harmonics_perform_integral(wave, HARMONICS_WAVE_LENGTH);

    // The first sample should be zero, and we'll duplicate it at the end for
    // easier interpolation.
    wave[HARMONICS_WAVE_LENGTH] = wave[0];
}

static double draw_noise_spectrum(const wavetables_s *tables, float *wave,
                                  int wave_length, double low_octave,
                                  double high_octave, double low_power,
                                  double high_power, double overall_slope)
{
    const int reference_octave = 11;
    const int reference_index = 1 << reference_octave;
    const int low_index = (int)pow(2, low_octave);
    const int high_index = mini(wave_length >> 1, (int)pow(2, high_octave));
    const float *retro_wave =
        tables->noise_wavetables[BPBXSYN_NOISE_RETRO].samples;
    
    double combined_amplitude = 0.0;
    for (int i = low_index; i < high_index; ++i) {
        double lerped = low_power +
            (high_power - low_power) * (log2(i) - low_octave) /
            (high_octave - low_octave);
        
        double amplitude = pow(2, (lerped - 1.0) * 7.0 + 1.0) * lerped;

        amplitude *= pow((double)i / reference_index, overall_slope);
        combined_amplitude += amplitude;

        // Add two different sources of psuedo-randomness to the noise
        // (individually they aren't random enough) but in a deterministic
        // way so that live spectrum editing doesn't result in audible pops.
        // Multiply all the sine wave amplitudes by 1 or -1 based on the
        // LFSR retro wave (effectively random), and also rotate the phase
        // of each sine wave based on the golden angle to disrupt the symmetry.
        amplitude *= retro_wave[i];
        const double radians = 0.61803398875 * i * i * PI2;

        wave[i] = cos(radians) * amplitude;
        wave[wave_length - i] = sin(radians) * amplitude;
    }

    return combined_amplitude;
}

static double control_point_to_octave(int point, const double *pitch_tweak,
                                      double lowest_octave
) {
    return lowest_octave
        + (int)(point / SPECTRUM_CONTROL_POINTS_PER_OCTAVE)
        + pitch_tweak[(point + SPECTRUM_CONTROL_POINTS_PER_OCTAVE) % SPECTRUM_CONTROL_POINTS_PER_OCTAVE];
}

void generate_spectrum_wave(const wavetables_s *tables,
                            uint8_t controls[BPBXSYN_SPECTRUM_CONTROL_COUNT],
                            double lowest_octave,
                            float wave[SPECTRUM_WAVE_LENGTH + 1]
) {
    memset(wave, 0, sizeof(float) * SPECTRUM_WAVE_LENGTH);

    const double highest_octave = 14;
    const double falloff_ratio = 0.25;

    // Nudge the 2/7 and 4/7 control points so that they form harmonic
    // intervals.
    const double pitch_tweak[] = {
        0.0,
        1.0 / 7.0,
        log2(5.0 / 4.0),
        3.0 / 7.0,
        log2(3.0 / 2.0),
        5.0 / 7.0,
        6.0 / 7.0
    };

    double combined_amplitude = 1;

    for (int i = 0; i < BPBXSYN_SPECTRUM_CONTROL_COUNT + 1; ++i) {
        const uint8_t value1 = (i <= 0) ? 0 : controls[i - 1];
        const uint8_t value2 = (i >= BPBXSYN_SPECTRUM_CONTROL_COUNT)
            ? controls[BPBXSYN_SPECTRUM_CONTROL_COUNT - 1]
            : controls[i];
        
        const double octave1 = control_point_to_octave(i - 1, pitch_tweak,
                                                       lowest_octave);
        double octave2 = control_point_to_octave(i, pitch_tweak, lowest_octave);

        if (i >= BPBXSYN_SPECTRUM_CONTROL_COUNT)
            octave2 = highest_octave +
                      (octave2 - highest_octave) * falloff_ratio;
        
        if (value1 == 0 && value2 == 0) continue;

        double spectrum =
            draw_noise_spectrum(tables, wave, SPECTRUM_WAVE_LENGTH, octave1,
                                octave2,
                                (double)value1 / BPBXSYN_SPECTRUM_CONTROL_MAX,
                                (double)value2 / BPBXSYN_SPECTRUM_CONTROL_MAX,
                                -0.5);
        
        combined_amplitude += 0.02 * spectrum;
    }

    if (controls[BPBXSYN_SPECTRUM_CONTROL_COUNT - 1] > 0) {
        double low_octave = highest_octave + (control_point_to_octave(BPBXSYN_SPECTRUM_CONTROL_COUNT, pitch_tweak, lowest_octave) - highest_octave) * falloff_ratio;
        double low_power = (double)controls[BPBXSYN_SPECTRUM_CONTROL_COUNT - 1] / BPBXSYN_SPECTRUM_CONTROL_MAX;

        combined_amplitude += 0.02 *
            draw_noise_spectrum(tables, wave, SPECTRUM_WAVE_LENGTH, low_octave,
                                highest_octave, low_power, 0, -0.5);
    }

    fft_inverse_real_fourier_transform(wave, SPECTRUM_WAVE_LENGTH);
    fft_scale_array(wave,
                    SPECTRUM_WAVE_LENGTH + 1,
                    5.0 / (sqrt(SPECTRUM_WAVE_LENGTH) * pow(combined_amplitude, 0.75)));

    // Duplicate the first sample at the end for easier wrap-around interpolation.
    wave[SPECTRUM_WAVE_LENGTH] = wave[0];
}

#define ARRLEN(arr) (sizeof(arr)/sizeof(*arr))

#define INIT_WAVETABLE_GENERIC(INDEX, EXPR, TRANSFORM, ...)                    \
    {                                                                          \
        static const float arrdata[] = {__VA_ARGS__, 0};                       \
        float *raw = bpbxsyn_malloc(ctx, sizeof(arrdata));                     \
        assert(raw);                                                           \
        if (!raw) return false;                                                \
        memcpy(raw, arrdata, sizeof(arrdata));                                 \
        TRANSFORM(raw, ARRLEN(arrdata));                                       \
        float *use = bpbxsyn_malloc(ctx, sizeof(arrdata));                     \
        assert(use);                                                           \
        if (!use) return false;                                                \
        perform_integral(raw, use, ARRLEN(arrdata));                           \
        wavetables->raw_chip_wavetables[INDEX] = (wavetable_desc_s) {          \
            .expression = EXPR,                                                \
            .samples = raw,                                                    \
            .length = ARRLEN(arrdata)                                          \
        };                                                                     \
        wavetables->chip_wavetables[INDEX] = (wavetable_desc_s) {              \
            .expression = EXPR,                                                \
            .samples = use,                                                    \
            .length = ARRLEN(arrdata)                                          \
        };                                                                     \
    }

#define INIT_WAVETABLE(INDEX, EXPR, ...) \
    INIT_WAVETABLE_GENERIC(INDEX, EXPR, center_wave, __VA_ARGS__)

#define INIT_WAVETABLE_N(INDEX, EXPR, ...) \
    INIT_WAVETABLE_GENERIC(INDEX, EXPR, center_and_normalize_wave, __VA_ARGS__)

#define RANDOM() ((float)rand() / RAND_MAX)

static void center_wave(float *wave, size_t length) {
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

    // The first sample should be zero, and we'll duplicate it at the end for
    // easier interpolation. (adding the 0 is done in the macro. this is why i
    // subtract 1 from length.)
}

static void center_and_normalize_wave(float *wave, size_t length) {
    double magn = 0.0;
    center_wave(wave, length);

    // Going to length-1 because an extra 0 sample is added on the end as part
    // of centerWave, which shouldn't impact magnitude calculation.
    for (size_t i = 0; i < length - 1; i++) {
        magn += fabs(wave[i]);
    }
    const double magn_avg = magn / (length - 1);

    for (size_t i = 0; i < length - 1; i++) {
        wave[i] = wave[i] / magn_avg;
    }
}

static void perform_integral(float *wave, float *new_wave, size_t length) {
    // Perform the integral on the wave. The synth function will perform the
    // derivative to get the original wave back but with antialiasing.
    double cumulative = 0.0;
    for (size_t i = 0; i < length; i++) {
        new_wave[i] = cumulative;
        cumulative += wave[i];
    }
}

bool init_wavetables_for_context(bpbxsyn_context_s *ctx) {
    wavetables_s *const wavetables = &ctx->wavetables;

    // init sine wavetable
    for (int i = 0; i < SINE_WAVE_LENGTH + 1; i++) {
        wavetables->sine_wave[i] = sin((double)i / SINE_WAVE_LENGTH * PI2);
    }

    // set up chip wavetables
    INIT_WAVETABLE(
        BPBXSYN_CHIP_WAVE_ROUNDED,
        0.94,
        0.0, 0.2, 0.4, 0.5, 0.6, 0.7, 0.8, 0.85, 0.9, 0.95, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.95, 0.9, 0.85, 0.8, 0.7, 0.6, 0.5, 0.4, 0.2, 0.0, -0.2, -0.4, -0.5, -0.6, -0.7, -0.8, -0.85, -0.9, -0.95, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -0.95, -0.9, -0.85, -0.8, -0.7, -0.6, -0.5, -0.4, -0.2
    );
    INIT_WAVETABLE(
        BPBXSYN_CHIP_WAVE_TRIANGLE,
        1.0,
        1.0 / 15.0, 3.0 / 15.0, 5.0 / 15.0, 7.0 / 15.0, 9.0 / 15.0, 11.0 / 15.0, 13.0 / 15.0, 15.0 / 15.0, 15.0 / 15.0, 13.0 / 15.0, 11.0 / 15.0, 9.0 / 15.0, 7.0 / 15.0, 5.0 / 15.0, 3.0 / 15.0, 1.0 / 15.0, -1.0 / 15.0, -3.0 / 15.0, -5.0 / 15.0, -7.0 / 15.0, -9.0 / 15.0, -11.0 / 15.0, -13.0 / 15.0, -15.0 / 15.0, -15.0 / 15.0, -13.0 / 15.0, -11.0 / 15.0, -9.0 / 15.0, -7.0 / 15.0, -5.0 / 15.0, -3.0 / 15.0, -1.0 / 15.0
    );
    INIT_WAVETABLE(
        BPBXSYN_CHIP_WAVE_SQUARE,
        0.5,
        1.0, -1.0
    );
    INIT_WAVETABLE(
        BPBXSYN_CHIP_WAVE_PULSE4,
        0.5,
        1.0, -1.0, -1.0, -1.0
    );
    INIT_WAVETABLE(
        BPBXSYN_CHIP_WAVE_PULSE8,
        0.5,
        1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0
    );
    INIT_WAVETABLE(
        BPBXSYN_CHIP_WAVE_SAWTOOTH,
        0.65,
        1.0 / 31.0, 3.0 / 31.0, 5.0 / 31.0, 7.0 / 31.0, 9.0 / 31.0, 11.0 / 31.0, 13.0 / 31.0, 15.0 / 31.0, 17.0 / 31.0, 19.0 / 31.0, 21.0 / 31.0, 23.0 / 31.0, 25.0 / 31.0, 27.0 / 31.0, 29.0 / 31.0, 31.0 / 31.0, -31.0 / 31.0, -29.0 / 31.0, -27.0 / 31.0, -25.0 / 31.0, -23.0 / 31.0, -21.0 / 31.0, -19.0 / 31.0, -17.0 / 31.0, -15.0 / 31.0, -13.0 / 31.0, -11.0 / 31.0, -9.0 / 31.0, -7.0 / 31.0, -5.0 / 31.0, -3.0 / 31.0, -1.0 / 31.0
    );
    INIT_WAVETABLE(
        BPBXSYN_CHIP_WAVE_DOUBLE_SAW,
        0.5,
        0.0, -0.2, -0.4, -0.6, -0.8, -1.0, 1.0, -0.8, -0.6, -0.4, -0.2, 1.0, 0.8, 0.6, 0.4, 0.2
    );
    INIT_WAVETABLE(
        BPBXSYN_CHIP_WAVE_DOUBLE_PULSE,
        0.4,
        1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0
    );
    INIT_WAVETABLE(
        BPBXSYN_CHIP_WAVE_SPIKY,
        0.4,
        1.0, -1.0, 1.0, -1.0, 1.0, 0.0
    );
    INIT_WAVETABLE_N(
        BPBXSYN_CHIP_WAVE_SINE,
        0.88,
        8.0, 9.0, 11.0, 12.0, 13.0, 14.0, 15.0, 15.0, 15.0, 15.0, 14.0, 14.0, 13.0, 11.0, 10.0, 9.0, 7.0, 6.0, 4.0, 3.0, 2.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 2.0, 4.0, 5.0, 6.0
    );
    INIT_WAVETABLE_N(
        BPBXSYN_CHIP_WAVE_FLUTE,
        0.8,
        3.0, 4.0, 6.0, 8.0, 10.0, 11.0, 13.0, 14.0, 15.0, 15.0, 14.0, 13.0, 11.0, 8.0, 5.0, 3.0
    );
    INIT_WAVETABLE_N(
        BPBXSYN_CHIP_WAVE_HARP,
        0.8,
        0.0, 3.0, 3.0, 3.0, 4.0, 5.0, 5.0, 6.0, 7.0, 8.0, 9.0, 11.0, 11.0, 13.0, 13.0, 15.0, 15.0, 14.0, 12.0, 11.0, 10.0, 9.0, 8.0, 7.0, 7.0, 5.0, 4.0, 3.0, 2.0, 1.0, 0.0, 0.0
    );
    INIT_WAVETABLE_N(
        BPBXSYN_CHIP_WAVE_SHARP_CLARINET,
        0.38,
        0.0, 0.0, 0.0, 1.0, 1.0, 8.0, 8.0, 9.0, 9.0, 9.0, 8.0, 8.0, 8.0, 8.0, 8.0, 9.0, 9.0, 7.0, 9.0, 9.0, 10.0, 4.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
    );
    INIT_WAVETABLE_N(
        BPBXSYN_CHIP_WAVE_SOFT_CLARINET,
        0.45,
        0.0, 1.0, 5.0, 8.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 11.0, 11.0, 12.0, 13.0, 12.0, 10.0, 9.0, 7.0, 6.0, 4.0, 3.0, 3.0, 3.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0
    );
    INIT_WAVETABLE_N(
        BPBXSYN_CHIP_WAVE_ALTO_SAX,
        0.3,
        5.0, 5.0, 6.0, 4.0, 3.0, 6.0, 8.0, 7.0, 2.0, 1.0, 5.0, 6.0, 5.0, 4.0, 5.0, 7.0, 9.0, 11.0, 13.0, 14.0, 14.0, 14.0, 14.0, 13.0, 10.0, 8.0, 7.0, 7.0, 4.0, 3.0, 4.0, 2.0
    );
    INIT_WAVETABLE_N(
        BPBXSYN_CHIP_WAVE_BASSOON,
        0.35,
        9.0, 9.0, 7.0, 6.0, 5.0, 4.0, 4.0, 4.0, 4.0, 5.0, 7.0, 8.0, 9.0, 10.0, 11.0, 13.0, 13.0, 11.0, 10.0, 9.0, 7.0, 6.0, 4.0, 2.0, 1.0, 1.0, 1.0, 2.0, 2.0, 5.0, 11.0, 14.0
    );
    INIT_WAVETABLE_N(
        BPBXSYN_CHIP_WAVE_TRUMPET,
        0.22,
        10.0, 11.0, 8.0, 6.0, 5.0, 5.0, 5.0, 6.0, 7.0, 7.0, 7.0, 7.0, 6.0, 6.0, 7.0, 7.0, 7.0, 7.0, 7.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 7.0, 8.0, 9.0, 11.0, 14.0
    );
    INIT_WAVETABLE_N(
        BPBXSYN_CHIP_WAVE_ELECTRIC_GUITAR,
        0.2,
        11.0, 12.0, 12.0, 10.0, 6.0, 6.0, 8.0, 0.0, 2.0, 4.0, 8.0, 10.0, 9.0, 10.0, 1.0, 7.0, 11.0, 3.0, 6.0, 6.0, 8.0, 13.0, 14.0, 2.0, 0.0, 12.0, 8.0, 4.0, 13.0, 11.0, 10.0, 13.0
    );
    INIT_WAVETABLE_N(
        BPBXSYN_CHIP_WAVE_ORGAN,
        0.2,
        11.0, 10.0, 12.0, 11.0, 14.0, 7.0, 5.0, 5.0, 12.0, 10.0, 10.0, 9.0, 12.0, 6.0, 4.0, 5.0, 13.0, 12.0, 12.0, 10.0, 12.0, 5.0, 2.0, 2.0, 8.0, 6.0, 6.0, 5.0, 8.0, 3.0, 2.0, 1.0
    );
    INIT_WAVETABLE_N(
        BPBXSYN_CHIP_WAVE_PAN_FLUTE,
        0.35,
        1.0, 4.0, 7.0, 6.0, 7.0, 9.0, 7.0, 7.0, 11.0, 12.0, 13.0, 15.0, 13.0, 11.0, 11.0, 12.0, 13.0, 10.0, 7.0, 5.0, 3.0, 6.0, 10.0, 7.0, 3.0, 3.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0
    );
    INIT_WAVETABLE(
        BPBXSYN_CHIP_WAVE_GLITCH,
        0.5,
        1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0
    );

    // generate noise wavetables

    // there is an extra 0 at the end of each wavetable.
    // This is just for interpolation purposes.
    for (int i = 0; i < BPBXSYN_NOISE_COUNT; i++) {
        wavetables->noise_wavetables[i].samples[NOISE_WAVETABLE_LENGTH] = 0.0;
    }

    // The "retro" drum uses a "Linear Feedback Shift Register" similar to the
    // NES noise channel.
    {
        noise_wavetable_s *wavetable =
            &wavetables->noise_wavetables[BPBXSYN_NOISE_RETRO];
        wavetable->expression = 0.25;
        wavetable->base_pitch = 69;
        wavetable->pitch_filter_mult = 1024.0;
        wavetable->is_soft = false;

        float *wave = wavetable->samples;
        int drum_buffer = 1;
        for (int i = 0; i < NOISE_WAVETABLE_LENGTH; i++) {
            wave[i] = (float)(drum_buffer & 1) * 2.0 - 1.0;
            int new_buffer = drum_buffer >> 1;
            if (((drum_buffer + new_buffer) & 1) == 1) {
                new_buffer += 1 << 14;
            }
            drum_buffer = new_buffer;
        }
    }

    // White noise is just random values for each sample.
    {
        noise_wavetable_s *wavetable =
            &wavetables->noise_wavetables[BPBXSYN_NOISE_WHITE];
        wavetable->expression = 1.0;
        wavetable->base_pitch = 69;
        wavetable->pitch_filter_mult = 8.0;
        wavetable->is_soft = true;
        
        float *wave = wavetable->samples;
        for (int i = 0; i < NOISE_WAVETABLE_LENGTH; i++) {
            wave[i] = RANDOM() * 2.f - 1.f;
        }
    }

    // The "clang" noise wave is based on a similar noise wave in the modded
    // beepbox made by DAzombieRE.
    {
        noise_wavetable_s *wavetable =
            &wavetables->noise_wavetables[BPBXSYN_NOISE_CLANG];
        wavetable->expression = 0.4;
        wavetable->base_pitch = 69;
        wavetable->pitch_filter_mult = 1024.0;
        wavetable->is_soft = false;
        
        float *wave = wavetable->samples;
        int drum_buffer = 1;
        for (int i = 0; i < NOISE_WAVETABLE_LENGTH; i++) {
            wave[i] = (drum_buffer & 1) * 2.0 - 1.0;
            int new_buffer = drum_buffer >> 1;
            if (((drum_buffer + new_buffer) & 1) == 1) {
                new_buffer += 2 << 14;
            }
            drum_buffer = new_buffer;
        }
    }

    // The "buzz" noise wave is based on a similar noise wave in the modded
    // beepbox made by DAzombieRE.
    {
        noise_wavetable_s *wavetable =
            &wavetables->noise_wavetables[BPBXSYN_NOISE_BUZZ];
        wavetable->expression = 0.3;
        wavetable->base_pitch = 69;
        wavetable->pitch_filter_mult = 1024.0;
        wavetable->is_soft = false;
        
        float *wave = wavetable->samples;
        int drum_buffer = 1;
        for (int i = 0; i < NOISE_WAVETABLE_LENGTH; i++) {
            wave[i] = (drum_buffer & 1) * 2.0 - 1.0;
            int new_buffer = drum_buffer >> 1;
            if (((drum_buffer + new_buffer) & 1) == 1) {
                new_buffer += 10 << 2;
            }
            drum_buffer = new_buffer;
        }
    }

    // TODO: hollow drums
    // "hollow" drums, designed in frequency space and then converted via FFT:
    {
        noise_wavetable_s *wavetable =
            &wavetables->noise_wavetables[BPBXSYN_NOISE_HOLLOW];
        wavetable->expression = 1.5;
        wavetable->base_pitch = 96;
        wavetable->pitch_filter_mult = 1.0;
        wavetable->is_soft = true;
        
        // TODO: generation
    }

    // "Shine" drums from modbox!
    // (it's the same as buzz but louder)
    {
        noise_wavetable_s *wavetable =
            &wavetables->noise_wavetables[BPBXSYN_NOISE_SHINE];
        wavetable->expression = 1.0;
        wavetable->base_pitch = 69;
        wavetable->pitch_filter_mult = 1024.0;
        wavetable->is_soft = false;
        
        // TODO: generation
        float *wave = wavetable->samples;
        int drum_buffer = 1;
        for (int i = 0; i < NOISE_WAVETABLE_LENGTH; i++) {
            wave[i] = (drum_buffer & 1) * 2.0 - 1.0;
            int new_buffer = drum_buffer >> 1;
            if (((drum_buffer + new_buffer) & 1) == 1) {
                new_buffer += 10 << 2;
            }
            drum_buffer = new_buffer;
        }
    }

    // TODO: deep drums
    // "Deep" drums from modbox!
    {
        noise_wavetable_s *wavetable =
            &wavetables->noise_wavetables[BPBXSYN_NOISE_DEEP];
        wavetable->expression = 1.5;
        wavetable->base_pitch = 120;
        wavetable->pitch_filter_mult = 1024.0;
        wavetable->is_soft = true;
        
        // TODO: generation
    }

    // "Cutter" drums from modbox!
    {
        noise_wavetable_s *wavetable =
            &wavetables->noise_wavetables[BPBXSYN_NOISE_CUTTER];
        wavetable->expression = 0.005;
        wavetable->base_pitch = 96;
        wavetable->pitch_filter_mult = 1024.0;
        wavetable->is_soft = false;
        
        float *wave = wavetable->samples;
        int drum_buffer = 1;
        for (int i = 0; i < NOISE_WAVETABLE_LENGTH; i++) {
            wave[i] = (drum_buffer & 1) * 4.f * (RANDOM() * 14.f + 1) - 8.f;
            int new_buffer = drum_buffer >> 1;
            if (((drum_buffer + new_buffer) & 1) == 1) {
                new_buffer += 15 << 2;
            }
            drum_buffer = new_buffer;
        }
    }

    // "Metallic" drums from modbox!
    {
        noise_wavetable_s *wavetable =
            &wavetables->noise_wavetables[BPBXSYN_NOISE_METALLIC];
        wavetable->expression = 1.0;
        wavetable->base_pitch = 96;
        wavetable->pitch_filter_mult = 1024.0;
        wavetable->is_soft = false;
        
        float *wave = wavetable->samples;
        int drum_buffer = 1;
        for (int i = 0; i < NOISE_WAVETABLE_LENGTH; i++) {
            wave[i] = (drum_buffer & 1) / 2.f - 0.5f;
            int new_buffer = drum_buffer >> 1;
            if (((drum_buffer + new_buffer) & 1) == 1) {
                new_buffer -= 10 << 2;
            }
            drum_buffer = new_buffer;
        }
    }

    return true;
}