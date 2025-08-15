#ifndef _fm_h_
#define _fm_h_

#include <stdint.h>
#include <stddef.h>
#include "../../include/beepbox_synth.h"
#include "../../include/beepbox_instrument_data.h"
#include "../wavetables.h"
#include "instrument.h"

#define FM_OP_COUNT 4
#define FM_MOD_COUNT 9

typedef struct {
    double phase;
    double phase_delta;
    double phase_delta_scale;
    double expression;
    double expression_delta;
    double prev_pitch_expression;
    double output;

    uint8_t has_prev_pitch_expression;
} fm_voice_opstate_s;

typedef struct {
    inst_base_voice_s base;

    double feedback_mult;
    double feedback_delta;

    fm_voice_opstate_s op_states[FM_OP_COUNT];
} fm_voice_s;

typedef struct {
    bpbxsyn_synth_s base;

    uint8_t algorithm;
    uint8_t freq_ratios[FM_OP_COUNT];
    double amplitudes[FM_OP_COUNT];

    uint8_t feedback_type;
    double feedback;

    int carrier_count;
    fm_voice_s voices[BPBXSYN_SYNTH_MAX_VOICES];
} fm_inst_s;

static inline double fm_calc_op(const double phase_mix) {
    const int phase_int = (int) phase_mix;
    const int index = phase_int & (SINE_WAVE_LENGTH - 1);
    const double sample = sine_wave_d[index];
    return sample + (sine_wave_d[index+1] - sample) * (phase_mix - phase_int);
}

void bpbxsyn_synth_init_fm(fm_inst_s *inst);
bpbxsyn_voice_id fm_note_on(bpbxsyn_synth_s *inst, int key, double velocity, int32_t length);
void fm_note_off(bpbxsyn_synth_s *inst, bpbxsyn_voice_id id);
void fm_note_all_off(bpbxsyn_synth_s *inst);
void fm_tick(bpbxsyn_synth_s *src_inst, const bpbxsyn_tick_ctx_s *tick_ctx);
void fm_run(bpbxsyn_synth_s *src_inst, float *samples, size_t frame_count);

extern const inst_vtable_s inst_fm_vtable;

#endif