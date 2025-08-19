#include "audio.h"

#include <math.h>

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