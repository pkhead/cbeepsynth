#include "audio.h"

#include <assert.h>
#include <math.h>
#include "util.h"

double calc_samples_per_tick(double bpm, double sample_rate) {
    const double beats_per_sec = bpm / 60.0;
    const double parts_per_sec = PARTS_PER_BEAT * beats_per_sec;
    const double ticks_per_sec = TICKS_PER_PART * parts_per_sec;
    return sample_rate / ticks_per_sec;
}

void sanitize_delay_line(float *buffer, int last_index, int mask) {
    while (1) {
        int index = (--last_index) & mask;
        float sample = fabsf(buffer[index]);
        if (isfinite(sample) && (sample == 0.0 || sample >= FLUSH_ZERO_EPSILON))
            break;

        buffer[index] = 0.f;
    }
}

void sanitize_delay_line_mod(float *buffer, int last_index, int wrap) {
    while (1) {
        int index = (--last_index) % wrap;
        float sample = fabsf(buffer[index]);
        if (isfinite(sample) && (sample == 0.0 || sample >= FLUSH_ZERO_EPSILON))
            break;

        buffer[index] = 0.f;
    }
}

double find_random_zero_crossing(prng_state_s *rng_state, float *wave,
                                 int wave_length) {
    assert(is_power_of_two(wave_length) && "wave length must be a power of 2!");

    double phase = bbsyn_frandom(rng_state) * wave_length;
    const int phaseMask = wave_length - 1;

    // Spectrum and drumset waves sounds best when they start at a zero
    // crossing, otherwise they pop. Try to find a zero crossing.
    int indexPrev = (int)phase & phaseMask;
    double wavePrev = (double)wave[indexPrev];
    const int stride = 16;

    for (int attemptsRemaining = 128; attemptsRemaining > 0; --attemptsRemaining) {
        const int indexNext = (indexPrev + stride) & phaseMask;
        const double waveNext = (double)wave[indexNext];

        if (wavePrev * waveNext <= 0.0) {
            // Found a zero crossing! Now let's narrow it down to two adjacent
            // sample indices.
            for (int i = 0; i < stride; ++i) {
                const int innerIndexNext = (indexPrev + 1) & phaseMask;
                const double innerWaveNext = (double)wave[innerIndexNext];

                if (wavePrev * innerWaveNext <= 0.0) {
                    // Found the zero crossing again! Now let's find the exact
                    // intersection.
                    const double slope = innerWaveNext - wavePrev;
                    phase = indexPrev;
                    if (fabs(slope) > 0.00000001) {
                        phase += -wavePrev / slope;
                    }
                    phase = fmod(max(0, phase), wave_length);
                    break;
                } else {
                    indexPrev = innerIndexNext;
                    wavePrev = innerWaveNext;
                }
            }
            break;
        } else {
            indexPrev = indexNext;
            wavePrev = waveNext;
        }
    }

    return phase;
}