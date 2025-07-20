#ifndef _chip_h_
#define _chip_h_

#include <stdint.h>
#include <stddef.h>
#include "../include/beepbox_synth.h"
#include "../include/beepbox_instrument_data.h"
#include "instrument.h"

#define CHIP_MOD_COUNT 1

typedef struct {
    inst_base_voice_s base;

    double phase[UNISON_MAX_VOICES];
    double phase_delta[UNISON_MAX_VOICES];
    double phase_delta_scale[UNISON_MAX_VOICES];

    double prev_pitch_expression;
    uint8_t has_prev_pitch_expression;
} chip_voice_s;

typedef struct {
    bpbx_inst_s base;

    uint8_t waveform;
    uint8_t unison_type;

    chip_voice_s voices[BPBX_INST_MAX_VOICES];
} chip_inst_s;

void chip_init(chip_inst_s *inst);
int chip_midi_on(bpbx_inst_s *inst, int key, int velocity);
void chip_midi_off(bpbx_inst_s *inst, int key, int velocity);
void chip_run(bpbx_inst_s *src_inst, const bpbx_run_ctx_s *const run_ctx);

extern const bpbx_inst_param_info_s chip_param_info[BPBX_CHIP_PARAM_COUNT];
extern const bpbx_envelope_compute_index_e chip_env_targets[CHIP_MOD_COUNT];
extern const size_t chip_param_addresses[BPBX_CHIP_PARAM_COUNT];

#endif