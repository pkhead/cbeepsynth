#ifndef _fm_h_
#define _fm_h_

#include <stdint.h>
#include <stddef.h>
#include "../../include/beepbox_synth.h"
#include "../../include/beepbox_instrument_data.h"
#include "../wavetables.h"
#include "synth.h"

#define FM_OP_COUNT 4
#define FM_MOD_COUNT 9

// runtime compilation of FM algorithms is only supported on x86-64 and AArch64
#if defined(__x86_64__) || defined(_M_X64) \
    || defined(__aarch64__) || defined(_M_ARM64)
#define BBSYN_SUPPORT_FMGEN
#endif

typedef struct fm_voice_opstate fm_voice_opstate_s;
typedef struct fm_algoc fm_algoc_s;

typedef double (*fm_algo_f)(fm_voice_opstate_s *ops, double feedback_amp,
                            const void *userdata);

typedef struct fm_voice_opstate {
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

    uint8_t prev_algorithm;
    uint8_t algorithm;
    uint8_t freq_ratios[FM_OP_COUNT];
    double amplitudes[FM_OP_COUNT];

    uint8_t prev_feedback_type;
    uint8_t feedback_type;
    double feedback;

    int carrier_count;
    fm_voice_s voices[BPBXSYN_SYNTH_MAX_VOICES];

    bpbxsyn_mcalloc_id mcalloc_id;
    void *mcode_rw;
    const void *mcode_x;
    fm_algoc_s *algoc;
    fm_algo_f compiled_algo;
} fm_inst_s;

typedef struct fm_desc {
    uint8_t operator_count;
    uint8_t carrier_count;

    uint8_t mod[8]; // bitfield of modulation inputs per operator
    uint8_t fdb[8]; // bitfield of feedback inputs per operator
} fm_desc_s;

fm_algoc_s *bbsyn_fm_algoc_new(const bpbxsyn_context_s *ctx);
void bbsyn_fm_algoc_destroy(fm_algoc_s *algoc);
fm_algo_f bbsyn_fm_algoc_compile(fm_algoc_s *algoc, const fm_desc_s *desc,
                                 const float *sine_wave, void *code_rw,
                                 const void *code_x, size_t max_size);

#endif