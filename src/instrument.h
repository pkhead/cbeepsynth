#ifndef _instrument_h_
#define _instrument_h_

#include <stddef.h>
#include <math.h>
#include "../include/beepbox_synth.h"
#include "envelope.h"
#include "filtering.h"

#define NOTE_SIZE_MAX 3
#define PARTS_PER_BEAT 24
#define TICKS_PER_PART 2
#define UNISON_MAX_VOICES 2
#define EXPRESSION_REFERENCE_PITCH 16 // A low "E" as a MIDI pitch.
#define PITCH_DAMPING 48

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
    uint8_t active_chord_id;
    uint8_t last_active_chord_id;

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

typedef void (*inst_init_f)(bpbx_inst_s *inst);
typedef void (*inst_destroy_f)(bpbx_inst_s *inst);

// (showing this to someone who isn't too familiar with C code)
// this is a really elegant solution i swear!!
typedef struct {
    const size_t            struct_size;
    const inst_init_f       inst_init;
    const inst_destroy_f    inst_destroy;
    const uint32_t          param_count;
    const bpbx_inst_param_info_s* param_info;
    const size_t*           param_addresses;

    const size_t            envelope_target_count;
    const bpbx_envelope_compute_index_e* envelope_targets;
    
    int (*const inst_midi_on)
            (bpbx_inst_s *inst, int key, int velocity);
    void (*const inst_midi_off)
            (bpbx_inst_s *inst, int key, int velocity);
    void (*const inst_tick)
            (bpbx_inst_s *inst, const bpbx_tick_ctx_s *tick_ctx);
    void (*const inst_run)
            (bpbx_inst_s *inst, float *samples, size_t frame_count);
} inst_vtable_s;
// (for c, at least)
// was using a switch statement cus i was lazy and thought "eh maybe it won't be that bad"
// then got to the third switch case and was like yeah you know what i don't want to edit 5
// switch statements everytime i add a new instrument LOL. i'm going to use the concept of
// those newfangled "virtual tables"

typedef struct {
    // TODO: consolidate these fields into a singular "flags" bitfield
    uint8_t triggered; // triggered, but not computing until the next tick
    uint8_t active; // this voice is currently reserved
    uint8_t released;
    uint8_t computing; // this voice is elligble being computed.
                       // set to true on the first frame after it was triggered.
    uint8_t is_on_last_tick; // this voice will be freed on the start of the next tick
    
    // unique identifier for the chord this note belongs to
    uint8_t chord_id;
    // chronological index of the note within the chord
    uint8_t chord_index;

    uint16_t key;
    
    double current_key; // modified by chord type
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

typedef struct {
    void *voice_list;
    size_t sizeof_voice;

    void (*compute_voice)(const bpbx_inst_s *const inst, inst_base_voice_s *const voice, voice_compute_s *compute_data);

    void *userdata;
} audio_compute_s;

typedef struct {
    int voices; // maximum of two
    double spread;
    double offset;
    double expression;
    double sign;
} unison_desc_s;

void inst_init(bpbx_inst_s *inst, bpbx_inst_type_e type);

int trigger_voice(bpbx_inst_s *inst, void *voices, size_t sizeof_voice, int key, int velocity);
void release_voice(bpbx_inst_s *inst, void *voices, size_t sizeof_voice, int key, int velocity);
void inst_tick(bpbx_inst_s *inst, const bpbx_tick_ctx_s *run_ctx, const audio_compute_s *params);

double calc_samples_per_tick(double bpm, double sample_rate);
double note_size_to_volume_mult(double size);
double inst_volume_to_mult(double inst_volume);
double get_lfo_amplitude(bpbx_vibrato_type_e type, double secs_into_bar);
// double calc_pitch_expression(double pitch);
#define calc_pitch_expression(pitch) (pow(2.0, -((pitch) - EXPRESSION_REFERENCE_PITCH) / PITCH_DAMPING))

extern bpbx_inst_param_info_s base_param_info[BPBX_BASE_PARAM_COUNT];
extern size_t base_param_offsets[BPBX_BASE_PARAM_COUNT];
extern const unison_desc_s unison_info[BPBX_UNISON_COUNT];

#endif