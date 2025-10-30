#ifndef _noise_h_
#define _noise_h_

#include "../../include/beepbox_synth.h"
#include "synth.h"
#include "../wavetables.h"
#include "../util.h"

typedef struct noise_voice {
    inst_base_voice_s base;

    double phase;
    double phase_delta;
    double phase_delta_scale;
    double noise_sample;

    double prev_pitch_expression;
    bool has_prev_pitch_expression;
} noise_voice_s;

typedef struct noise_inst {
    bpbxsyn_synth_s base;
    uint8_t noise_type;
    const noise_wavetable_s *wavetable;
    noise_voice_s voices[BPBXSYN_SYNTH_MAX_VOICES];

    prng_state_s prng_state;
} noise_inst_s;

extern const inst_vtable_s bbsyn_inst_noise_vtable;

#endif