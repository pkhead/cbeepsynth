/**
    This contains the definitions for all the wavetable-type instruments

    It includes these instruments:
    - Chip
    - Custom chip
    - Harmonics (this dynamically generates a wavetable to be used)
**/
#ifndef _wave_h_
#define _wave_h_

#include <stdint.h>
#include <stddef.h>
#include "../../include/beepbox_synth.h"
#include "../wavetables.h"
#include "synth.h"

#define CHIP_MOD_COUNT 1
#define HARMONICS_MOD_COUNT 1

typedef struct {
    inst_base_voice_s base;

    double phase[UNISON_MAX_VOICES];
    double phase_delta[UNISON_MAX_VOICES];
    double phase_delta_scale[UNISON_MAX_VOICES];

    double prev_pitch_expression;
    uint8_t has_prev_pitch_expression;
} wave_voice_s;

typedef struct {
    bpbxsyn_synth_s base;

    uint8_t unison_type;
    uint8_t waveform;

    wave_voice_s voices[BPBXSYN_SYNTH_MAX_VOICES];
} chip_inst_s;

typedef struct {
    bpbxsyn_synth_s base;

    uint8_t unison_type;
    uint8_t controls[BPBXSYN_HARMONICS_CONTROL_COUNT];
    uint8_t last_controls[BPBXSYN_HARMONICS_CONTROL_COUNT];

    wave_voice_s voices[BPBXSYN_SYNTH_MAX_VOICES];

    float wave[HARMONICS_WAVE_LENGTH + 1];
} harmonics_inst_s;

void bpbxsyn_synth_init_chip(bpbxsyn_context_s *ctx, chip_inst_s *inst);
bpbxsyn_voice_id chip_note_on(bpbxsyn_synth_s *inst, int key, double velocity, int32_t length);
void chip_note_off(bpbxsyn_synth_s *inst, bpbxsyn_voice_id id);
void chip_note_all_off(bpbxsyn_synth_s *inst);
void chip_tick(bpbxsyn_synth_s *src_inst, const bpbxsyn_tick_ctx_s *tick_ctx);
void chip_run(bpbxsyn_synth_s *src_inst, float *samples, size_t frame_count);

void bpbxsyn_synth_init_harmonics(bpbxsyn_context_s *ctx, harmonics_inst_s *inst);
bpbxsyn_voice_id harmonics_note_on(bpbxsyn_synth_s *inst, int key, double velocity, int32_t length);
void harmonics_note_off(bpbxsyn_synth_s *inst, bpbxsyn_voice_id id);
void harmonics_note_all_off(bpbxsyn_synth_s *inst);
void harmonics_tick(bpbxsyn_synth_s *src_inst, const bpbxsyn_tick_ctx_s *tick_ctx);
void harmonics_run(bpbxsyn_synth_s *src_inst, float *samples, size_t frame_count);

extern const inst_vtable_s inst_chip_vtable;
extern const inst_vtable_s inst_harmonics_vtable;

#endif