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
#include "../include/beepbox_synth.h"
#include "instrument.h"
#include "wavetables.h"

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
    bpbx_inst_s base;

    uint8_t unison_type;
    uint8_t waveform;

    wave_voice_s voices[BPBX_INST_MAX_VOICES];
} chip_inst_s;

typedef struct {
    bpbx_inst_s base;

    uint8_t unison_type;
    uint8_t controls[BPBX_HARMONICS_CONTROL_COUNT];
    uint8_t last_controls[BPBX_HARMONICS_CONTROL_COUNT];

    wave_voice_s voices[BPBX_INST_MAX_VOICES];

    float wave[HARMONICS_WAVE_LENGTH + 1];
} harmonics_inst_s;

void bpbx_inst_init_chip(chip_inst_s *inst);
int chip_midi_on(bpbx_inst_s *inst, int key, int velocity);
void chip_midi_off(bpbx_inst_s *inst, int key, int velocity);
void chip_tick(bpbx_inst_s *src_inst, const bpbx_tick_ctx_s *tick_ctx);
void chip_run(bpbx_inst_s *src_inst, float *samples, size_t frame_count);

void bpbx_inst_init_harmonics(harmonics_inst_s *inst);
int harmonics_midi_on(bpbx_inst_s *inst, int key, int velocity);
void harmonics_midi_off(bpbx_inst_s *inst, int key, int velocity);
void harmonics_tick(bpbx_inst_s *src_inst, const bpbx_tick_ctx_s *tick_ctx);
void harmonics_run(bpbx_inst_s *src_inst, float *samples, size_t frame_count);

extern const inst_vtable_s inst_chip_vtable;
extern const inst_vtable_s inst_harmonics_vtable;

#endif