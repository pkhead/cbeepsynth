#ifndef _wavetables_h_
#define _wavetables_h_

#include <stdbool.h>
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
    const float *samples;
    size_t length;
} wavetable_desc_s;

// chip noise tables
#define NOISE_WAVETABLE_LENGTH ((1 << 15)) // 32768
typedef struct {
    double expression;
    int base_pitch;
    double pitch_filter_mult;
    bool is_soft;

    // float instead of double for space efficiency
    float samples[NOISE_WAVETABLE_LENGTH+1];
} noise_wavetable_s;

#define HARMONICS_WAVE_LENGTH 2048

extern noise_wavetable_s noise_wavetables[BPBX_NOISE_COUNT];

// raw chip wavetables
extern wavetable_desc_s raw_chip_wavetables[BPBX_CHIP_WAVE_COUNT];

// chip wavetables integrated for the purposes of anti-aliasing
extern wavetable_desc_s chip_wavetables[BPBX_CHIP_WAVE_COUNT];

void init_wavetables(void);

// size is assumed to be HARMONICS_WAVE_LENGTH + 1
void generate_harmonics(uint8_t controls[BPBX_HARMONICS_CONTROL_COUNT], int harmonics_rendered, float *out);

#endif