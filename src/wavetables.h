#ifndef _wavetables_h_
#define _wavetables_h_

#include "../include/beepbox_synth.h"

// SINE_WAVE_LENGTH must be a power of 2!
// add an extra element to the wavetable in case
// synthesizer code uses the last index of the wavetable,
// and then gets the next element for interpolation purposes.
#define SINE_WAVE_LENGTH 256
extern float sine_wave_f[SINE_WAVE_LENGTH + 1];
extern double sine_wave_d[SINE_WAVE_LENGTH + 1];

// chip wave tables
typedef struct {
    double expression;
    const double *values;
    size_t length;
} wavetable_desc_s;

extern wavetable_desc_s chip_wavetables[BPBX_CHIP_WAVE_COUNT];

extern void init_wavetables(void);

#endif