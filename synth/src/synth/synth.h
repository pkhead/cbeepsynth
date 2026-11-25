#ifndef _instrument_h_
#define _instrument_h_

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#include "../audio.h"
#include "../envelope.h"
#include "../filtering.h"
#include "../include/beepbox_synth.h"

#define UNISON_MAX_VOICES 2
#define EXPRESSION_REFERENCE_PITCH 16 // A low "E" as a MIDI pitch.
#define PITCH_DAMPING 48
#define ARPEGGIO_SPEED_SETTING_COUNT 51

struct bbxs_synth {
    bbxs_synth_type_e type;
    const bbxs_context_s *ctx;
    double sample_rate;

    double fade_in; // double in range of 0-9
    double fade_out;

    uint8_t active_effects[6];

    uint8_t transition_type;
    uint8_t chord_type;
    uint8_t fast_two_note_arpeggio;

    uint8_t active_chord_id;
    uint8_t last_active_chord_id;

    // if a chord contiuation was triggered, relevant for transition types.
    // at the start of the next tick, active_chord_id will be set to
    // last_active_chord_Id.
    bool chord_continuation;

    double pitch_shift; // aka coarse detune
    double detune;      // aka fine detune
    double arpeggio_speed;
    double strum_speed;

    uint8_t vibrato_preset;
    bbxs_vibrato_params_s vibrato;

    // current state of note filter
    filter_group_s note_filter;

    // previous state of note filter
    // interpolated per-tick as usual
    filter_group_s last_note_filter;

    // envelopes
    uint8_t envelope_count;
    bbxs_envelope_s envelopes[BBXS_MAX_ENVELOPE_COUNT];

    // this is in seconds
    double vibrato_time_start;
    double vibrato_time_end;
    double arp_time;

    double mod_x;
    double mod_y;
    double mod_wheel; // last stored state of modulation wheel

    bbxs_synth_callbacks_s callbacks;
    void *userdata;
};

typedef void (*inst_init_f)(bbxs_context_s *ctx, bbxs_synth_s *inst);
typedef void (*inst_destroy_f)(bbxs_synth_s *inst);

// (showing this to someone who isn't too familiar with C code)
// this is a really elegant solution i swear!!
typedef struct {
    const size_t struct_size;
    const size_t exstate_size;
    const inst_init_f inst_init;
    const inst_destroy_f inst_destroy;
    const uint32_t param_count;
    const bbxs_param_info_s *param_info;
    const size_t *param_addresses;

    const size_t envelope_target_count;
    const bbxs_envelope_compute_index_e *envelope_targets;

    bbxs_voice_id (*const inst_note_on)(bbxs_synth_s *inst, int key,
                                        double velocity, int32_t length);
    void (*const inst_note_off)(bbxs_synth_s *inst, bbxs_voice_id id);
    void (*const inst_note_choke)(bbxs_synth_s *inst, bbxs_voice_id id);
    void (*const inst_note_modulate)(bbxs_synth_s *inst, bbxs_voice_id,
                                     uint32_t param, double value);
    void (*const inst_note_all_off)(bbxs_synth_s *inst);

    void (*const inst_sample_rate_changed)(bbxs_synth_s *inst, double old,
                                           double new);

    void (*const inst_tick)(bbxs_synth_s *inst,
                            const bbxs_tick_ctx_s *tick_ctx);
    void (*const inst_run)(bbxs_synth_s *inst, float *samples,
                           size_t frame_count);

    void (*const inst_get_exstate)(bbxs_synth_s *inst, void *out);
    void (*const inst_set_exstate)(bbxs_synth_s *inst, const void *in);
    int (*const inst_serialize_exstate)(bbxs_synth_s *inst,
                                        bbxs_writer_f writer, void *ud);
    int (*const inst_deserialize_exstate)(bbxs_synth_s *inst,
                                          bbxs_reader_f reader, void *ud);
} synth_vtable_s;
// (for c, at least)
// was using a switch statement cus i was lazy and thought "eh maybe it won't be
// that bad" then got to the third switch case and was like yeah you know what i
// don't want to edit 5 switch statements everytime i add a new instrument LOL.
// i'm going to use the concept of those newfangled "virtual tables"

// clang-format off
enum {
    VOICE_FLAG_ACTIVE              = (1 << 0), // the voice is currently reserved
    VOICE_FLAG_TRIGGERED           = (1 << 1), // triggered, but not computing
                                               // until the next tick.
    VOICE_FLAG_RELEASED            = (1 << 2),
    VOICE_FLAG_RELEASE_TRIGGERED   = (1 << 3), // released flag will be set on
                                               // the next tick, and this will
                                               // be unset.
    VOICE_FLAG_COMPUTING           = (1 << 4), // voice is fully active
    VOICE_FLAG_IS_ON_LAST_TICK     = (1 << 5), // this voice will be freed on
                                               // the start of the next tick
    VOICE_FLAG_USE_NOTE_SIZE       = (1 << 6), // value of note size for this
                                               // voice updates on every tick
    VOICE_FLAG_HAS_PREV_VIBRATO    = (1 << 7), // for vibrato continuity
    VOICE_FLAG_HAS_PREV_PITCH_EXPR = (1 << 8),
    VOICE_FLAG_HAS_PREV_NOTE       = (1 << 9),
};
// clang-format on

#define voice_is_active(voice) ((voice)->flags & VOICE_FLAG_ACTIVE)
#define voice_is_triggered(voice) ((voice)->flags & VOICE_FLAG_TRIGGERED)
#define voice_is_released(voice) ((voice)->flags & VOICE_FLAG_RELEASED)
#define voice_is_computing(voice) ((voice)->flags & VOICE_FLAG_COMPUTING)

// ugh, horrible name.
#define voice_is_releasedt(voice)                                              \
    ((voice)->flags & (VOICE_FLAG_RELEASED | VOICE_FLAG_RELEASE_TRIGGERED))

typedef struct {
    uint16_t flags; // VOICE_FLAG_*
    uint16_t key;

    // unique identifier for the chord this note belongs to
    uint8_t chord_id;
    // chronological index of the note within the chord
    uint8_t chord_index;

    int note_length;

    // current note size: a value from 0-1. this is copied from the tick run
    // context on every tick the voice is active. this is initialized to -1 when
    // the voice has just begun before its first tick without an initial
    // velocity
    double note_size;

    double current_key; // modified by chord type
    double prev_pitch;  // used by slide transition

    double prev_vibrato;
    double expression;
    double expression_delta;

    // how long the voice has been active in ticks
    // time2_secs is seconds to end of current run.
    double time_secs;
    double time2_secs;

    // how long the voice has been active in ticks
    // time2_secs is ticks to the end of the current run.
    int time_ticks;
    int time2_ticks;

    double secs_since_release;
    double ticks_since_release;

    double note_filter_input[2]; // x[-1] and x[-2]
    bool filters_enabled;
    dyn_biquad_s note_filters[FILTER_GROUP_COUNT];

    envelope_computer_s env_computer;
} inst_base_voice_s;

typedef struct {
    const bbxs_synth_s *inst;

    double fade_in;
    double fade_out;
    double samples_per_tick;
    double sample_rate;
    double cur_beat;
    double note_size;
    double mod_x, mod_y, mod_w;
    bbxs_vibrato_params_s *vibrato_params;
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

    void (*compute_voice)(const bbxs_synth_s *const inst,
                          inst_base_voice_s *const voice,
                          voice_compute_s *compute_data);

    void *userdata;
} audio_compute_s;

typedef struct {
    int voices; // maximum of two
    double spread;
    double offset;
    double expression;
    double sign;
} unison_desc_s;

void bbxs__inst_init(bbxs_context_s *ctx, bbxs_synth_s *inst,
                     bbxs_synth_type_e type);

bbxs_voice_id bbxs__trigger_voice(bbxs_synth_s *inst, void *voices,
                                  size_t sizeof_voice, int key, double velocity,
                                  int32_t length, bool *continuation);
void bbxs__release_voice(bbxs_synth_s *inst, void *voices, size_t sizeof_voice,
                         bbxs_voice_id id);
// void choke_voice(bbxs_synth_s *inst, void *voices, size_t sizeof_voice,
// bbxs_voice_id id);
void bbxs__release_all_voices(bbxs_synth_s *inst, void *voices,
                              size_t sizeof_voice);
void bbxs__inst_tick(bbxs_synth_s *inst, const bbxs_tick_ctx_s *run_ctx,
                     const audio_compute_s *params);
double bbxs__inst_calc_arp_speed(double arp_speed_setting);

#define GENERIC_LIST(list) (list), sizeof(*(list))

double bbxs__note_size_to_volume_mult(double size);
double bbxs__get_lfo_amplitude(bbxs_vibrato_type_e type, double secs_into_bar);
#define calc_pitch_expression_ex(pitch, refpitch, damping)                     \
    (pow(2.0, -((pitch) - (refpitch)) / (damping)))
#define calc_pitch_expression(pitch)                                           \
    calc_pitch_expression_ex(pitch, EXPRESSION_REFERENCE_PITCH, PITCH_DAMPING)

extern bbxs_param_info_s bbxs__base_param_info[BBXS_BASE_PARAM_COUNT];
extern size_t bbxs__base_param_offsets[BBXS_BASE_PARAM_COUNT];
extern const unison_desc_s bbxs__unison_info[BBXS_UNISON_COUNT];

extern const synth_vtable_s bbxs__synth_fm_vtable;
extern const synth_vtable_s bbxs__synth_chip_vtable;
extern const synth_vtable_s bbxs__synth_pwm_vtable;
extern const synth_vtable_s bbxs__synth_harmonics_vtable;
extern const synth_vtable_s bbxs__synth_spectrum_vtable;
extern const synth_vtable_s bbxs__synth_noise_vtable;
extern const synth_vtable_s bbxs__synth_picked_vtable;
extern const synth_vtable_s bbxs__synth_custom_chip_vtable;

#endif