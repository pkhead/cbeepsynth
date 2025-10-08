#ifndef _spectrum_h_
#define _spectrum_h_

#include "../../include/beepbox_synth.h"
#include "../wavetables.h"
#include "synth.h"

typedef struct spectrum_voice {
    inst_base_voice_s base;

    double phase;
    double phase_delta;
    double phase_delta_scale;

    double noise_sample;

    double prev_pitch_expression;
    bool has_prev_pitch_expression;
} spectrum_voice_s;

typedef struct spectrum_inst {
    bpbxsyn_synth_s base;
    
    uint8_t is_noise_channel; // a boolean
    uint8_t controls[BPBXSYN_SPECTRUM_CONTROL_COUNT];
    int control_hash;

    spectrum_voice_s voices[BPBXSYN_SYNTH_MAX_VOICES];

    float wave[SPECTRUM_WAVE_LENGTH + 1];
    prng_state_s prng_state;
} spectrum_inst_s;

void bpbxsyn_synth_init_spectrum(bpbxsyn_context_s *ctx, spectrum_inst_s *inst);

bpbxsyn_voice_id bbsyn_spectrum_note_on(bpbxsyn_synth_s *inst, int key,
                                        double velocity, int32_t length);
void bbsyn_spectrum_note_off(bpbxsyn_synth_s *inst, bpbxsyn_voice_id id);
void bbsyn_spectrum_note_all_off(bpbxsyn_synth_s *inst);
void bbsyn_spectrum_tick(bpbxsyn_synth_s *src_inst,
                         const bpbxsyn_tick_ctx_s *tick_ctx);
void bbsyn_spectrum_run(bpbxsyn_synth_s *src_inst, float *samples,
                        size_t frame_count);

extern const inst_vtable_s bbsyn_inst_spectrum_vtable;

#endif