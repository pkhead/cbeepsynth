#ifndef _harmonics_h_
#define _harmonics_h_

#include <stdint.h>
#include <stddef.h>
#include "../include/beepbox_synth.h"
#include "../include/beepbox_instrument_data.h"
#include "instrument.h"

#define HARMONICS_MOD_COUNT 1
#define HARMONICS_WAVE_LENGTH 2048

typedef struct {
    inst_base_voice_s base;

    double phase[UNISON_MAX_VOICES];
    double phase_delta[UNISON_MAX_VOICES];
    double phase_delta_scale[UNISON_MAX_VOICES];

    double prev_pitch_expression;
    uint8_t has_prev_pitch_expression;
} harmonics_voice_s;

typedef struct {
    bpbx_inst_s base;

    uint8_t controls[BPBX_HARMONICS_CONTROL_COUNT];
    uint8_t unison_type;

    harmonics_voice_s voices[BPBX_INST_MAX_VOICES];

    // float instead of double to take up less space
    float wave[HARMONICS_WAVE_LENGTH + 1];
} harmonics_inst_s;

void harmonics_init(harmonics_inst_s *inst);
int harmonics_midi_on(bpbx_inst_s *inst, int key, int velocity);
void harmonics_midi_off(bpbx_inst_s *inst, int key, int velocity);
void harmonics_run(bpbx_inst_s *src_inst, const bpbx_run_ctx_s *const run_ctx);

extern const bpbx_inst_param_info_s harmonics_param_info[BPBX_HARMONICS_PARAM_COUNT];
extern const bpbx_envelope_compute_index_e harmonics_env_targets[HARMONICS_MOD_COUNT];
extern const size_t harmonics_param_addresses[BPBX_HARMONICS_PARAM_COUNT];

#endif