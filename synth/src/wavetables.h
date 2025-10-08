#ifndef _wavetables_h_
#define _wavetables_h_

// wavetable data is stored in the bpbxsyn_context struct

#include <stdbool.h>
#include "../include/beepbox_synth.h"

// SINE_WAVE_LENGTH must be a power of 2!
// add an extra element to the wavetable in case
// synthesizer code uses the last index of the wavetable,
// and then gets the next element for interpolation purposes.
#define SINE_WAVE_LENGTH 256

#define HARMONICS_WAVE_LENGTH 2048
#define SPECTRUM_WAVE_LENGTH 32768
#define SPECTRUM_CONTROL_POINTS_PER_OCTAVE 7

// chip wave tables
typedef struct {
    double expression;
    float *samples;
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

typedef struct wavetables {
    float sine_wave[SINE_WAVE_LENGTH + 1];

    noise_wavetable_s noise_wavetables[BPBXSYN_NOISE_COUNT];

    // raw chip wavetables
    wavetable_desc_s raw_chip_wavetables[BPBXSYN_CHIP_WAVE_COUNT];

    // chip wavetables integrated for the purposes of anti-aliasing
    wavetable_desc_s chip_wavetables[BPBXSYN_CHIP_WAVE_COUNT];
} wavetables_s;

bool bbsyn_init_wavetables_for_context(bpbxsyn_context_s *ctx);

void bbsyn_generate_harmonics(const wavetables_s *wavetables,
                              uint8_t controls[BPBXSYN_HARMONICS_CONTROL_COUNT],
                              int harmonics_rendered,
                              float out[HARMONICS_WAVE_LENGTH + 1]);

void bbsyn_generate_spectrum_wave(const wavetables_s *wavetables,
                                  uint8_t controls[BPBXSYN_SPECTRUM_CONTROL_COUNT],
                                  double lowest_octave,
                                  float out[SPECTRUM_WAVE_LENGTH + 1]);

#endif