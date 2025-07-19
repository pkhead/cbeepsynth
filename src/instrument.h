#ifndef _public_h_
#define _public_h_

#include <stddef.h>
#include "../include/beepbox_synth.h"
#include "envelope.h"
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
    double pitch_shift; // aka coarse detune
    double detune; // aka fine detune

    uint8_t vibrato_preset;
    bpbx_vibrato_params_s vibrato;

    // current state of note filter 
    filter_group_s note_filter;
    filter_group_s eq;

    // previous state of note filter
    // interpolated per-tick as usual
    filter_group_s last_note_filter;
    filter_group_s last_eq;

    int frame_counter;

    // envelopes
    uint8_t envelope_count;
    bpbx_envelope_s envelopes[BPBX_MAX_ENVELOPE_COUNT];

    // this is in seconds
    double vibrato_time_start;
    double vibrato_time_end;

    double mod_x;
    double mod_y;
    double mod_wheel; // last stored state of modulation wheel
} bpbx_inst_s;

typedef struct {
    uint8_t triggered; // triggered, but not active until the next tick
    uint8_t active;
    uint8_t released;
    uint8_t is_on_last_tick;
    uint16_t key;
    
    float volume;

    uint8_t has_prev_vibrato;
    double prev_vibrato;
    
    double expression;
    double expression_delta;

    // how long the voice has been active in ticks
    // time2_secs is seconds to end of current run.
    double time_secs;
    double time2_secs;

    // how long the voice has been active in ticks
    // time2_secs is ticks to the end of the current run.
    double time_ticks;
    double time2_ticks;

    double secs_since_release;
    double ticks_since_release;

    uint8_t filters_enabled;
    double note_filter_input[2]; // x[-1] and x[-2]
    dyn_biquad_s note_filters[FILTER_GROUP_COUNT];

    envelope_computer_s env_computer;
} inst_base_voice_s;

typedef struct {
    const bpbx_inst_s *inst;

    double fade_in;
    double fade_out;
    double samples_per_tick;
    double sample_rate;
    double cur_beat;
    double mod_x, mod_y, mod_w;
    bpbx_vibrato_params_s *vibrato_params;
} voice_compute_constants_s;

typedef struct {
    double interval_start;
    double interval_end;
    double expr_start;
    double expr_end;

    double sample_len;
    double samples_per_tick;
    double rounded_samples_per_tick;
} voice_compute_varying_s;

typedef struct {
    voice_compute_constants_s constants;
    voice_compute_varying_s varying;

    // internal variables
    uint8_t _released;
} voice_compute_s;

void inst_init(bpbx_inst_s *inst, bpbx_inst_type_e type);

void compute_voice_pre(inst_base_voice_s *const voice, voice_compute_s *compute_data);
void compute_voice_post(inst_base_voice_s *const voice, voice_compute_s *compute_data);
int trigger_voice(bpbx_inst_s *inst, void *voices, size_t sizeof_voice, int key, int velocity);
void release_voice(bpbx_inst_s *inst, void *voices, size_t sizeof_voice, int key, int velocity);

double calc_samples_per_tick(double bpm, double sample_rate);
double note_size_to_volume_mult(double size);
double inst_volume_to_mult(double inst_volume);
double get_lfo_amplitude(bpbx_vibrato_type_e type, double secs_into_bar);

extern bpbx_inst_param_info_s base_param_info[BPBX_BASE_PARAM_COUNT];
extern size_t base_param_offsets[BPBX_BASE_PARAM_COUNT];

#endif