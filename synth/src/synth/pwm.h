#ifndef _pulse_h_
#define _pulse_h_

#include "../../include/beepbox_synth.h"
#include "synth.h"

#define PWM_MOD_COUNT 1

typedef struct pwm_voice {
    inst_base_voice_s base;

    double phase;
    double phase_delta;
    double phase_delta_scale;

    double pulse_width;
    double pulse_width_delta;

    double prev_pitch_expression;
    bool has_prev_pitch_expression;
} pwm_voice_s;

typedef struct pwm_inst {
    bpbxsyn_synth_s base;

    // [0] = prev
    // [1] = cur
    double pulse_width_param[2];
    uint8_t aliases;

    pwm_voice_s voices[BPBXSYN_SYNTH_MAX_VOICES];
} pwm_inst_s;

void bpbxsyn_synth_init_pwm(bpbxsyn_context_s *ctx, pwm_inst_s *inst);

bpbxsyn_voice_id bbsyn_pwm_note_on(bpbxsyn_synth_s *inst, int key,
                                   double velocity, int32_t length);
void bbsyn_pwm_note_off(bpbxsyn_synth_s *inst, bpbxsyn_voice_id id);
void bbsyn_pwm_note_all_off(bpbxsyn_synth_s *inst);
void bbsyn_pwm_tick(bpbxsyn_synth_s *src_inst,
                    const bpbxsyn_tick_ctx_s *tick_ctx);
void bbsyn_pwm_run(bpbxsyn_synth_s *src_inst, float *samples,
                     size_t frame_count);

extern const inst_vtable_s bbsyn_inst_pwm_vtable;

#endif