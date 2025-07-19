#include <math.h>
#include "util.h"
#include "wavetables.h"

float sine_wave_f[SINE_WAVE_LENGTH + 1];
double sine_wave_d[SINE_WAVE_LENGTH + 1];
static int need_init_wavetables = 1;

void init_wavetables() {
    if (need_init_wavetables) {
        need_init_wavetables = 0;

        for (int i = 0; i < SINE_WAVE_LENGTH + 1; i++) {
            sine_wave_d[i] = sin((double)i / SINE_WAVE_LENGTH * PI2);
            sine_wave_f[i] = sinf((float)i / SINE_WAVE_LENGTH * PI2f);
        }
    }
}