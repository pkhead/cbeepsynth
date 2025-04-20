#ifndef _public_h_
#define _public_h_

#include <stddef.h>
#include "../include/beepbox_synth.h"
#include "envelope.h"
#include "fm.h"
#include "filtering.h"

#define NOTE_SIZE_MAX 3
#define PARTS_PER_BEAT 24
#define TICKS_PER_PART 2

typedef struct bpbx_inst_s {
    bpbx_inst_type_e type;
    double sample_rate;

    double volume;
    double panning;
    double fade_in; // double in range of 0-9
    double fade_out;

    uint8_t active_effects[6];

    uint8_t transition_type;
    uint8_t chord_type;
    int pitch_shift; // aka coarse detune
    double detune; // aka fine detune

    struct {
        double depth;
        double speed;
        double delay;
        uint8_t type;
    } vibrato;

    filter_group_s note_filter;
    filter_group_s eq;

    union {
        fm_inst_s *fm;
    };

    // envelopes
    uint8_t envelope_count;
    bpbx_envelope_s envelopes[BPBX_MAX_ENVELOPE_COUNT];
} bpbx_inst_s;

double calc_samples_per_tick(double bpm, double sample_rate);
double note_size_to_volume_mult(double size);
double inst_volume_to_mult(double inst_volume);

bpbx_inst_param_info_s base_param_info[BPBX_BASE_PARAM_COUNT];
size_t base_param_offsets[BPBX_BASE_PARAM_COUNT];

#endif