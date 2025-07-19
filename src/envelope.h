#ifndef _envelope_h_
#define _envelope_h_

#include <stdint.h>
#include "../include/beepbox_synth.h"

#define FADE_OUT_RANGE 11
#define FADE_OUT_MIN -4
#define FADE_OUT_MAX 6

typedef struct {
    double envelope_starts[BPBX_ENV_INDEX_COUNT];
    double envelope_ends[BPBX_ENV_INDEX_COUNT];
    double tick;
    double note_secs_start;
    double note_secs_end;

    // lowpassCutoffDecayVolumeCompensation
    // TODO: implement this. Currently it's always 1.0
    double lp_cutoff_decay_volume_compensation;

    double mod_x[2];
    double mod_y[2];
    double mod_wheel[2];
    double pitch_wheel[2];
} envelope_computer_s;

typedef enum {
    ENV_CURVE_NONE,
    ENV_CURVE_NOTE_SIZE, // aka note size
    ENV_CURVE_PUNCH,
    ENV_CURVE_FLARE,
    ENV_CURVE_TWANG,
    ENV_CURVE_SWELL,
    ENV_CURVE_TREMOLO,
    ENV_CURVE_TREMOLO2,
    ENV_CURVE_DECAY,
    ENV_CURVE_BLIP, // from jummbox

    ENV_CURVE_MOD_X,
    ENV_CURVE_MOD_Y,
    ENV_CURVE_MOD_WHEEL // MIDI CC 1 (Modulation wheel)
} envelope_curve_type_e;

double secs_fade_in(double setting);
double ticks_fade_out(double setting);

typedef struct {
    const char *name;
    envelope_curve_type_e curve_type;
    double speed;
} envelope_curve_preset_s;

void envelope_computer_init(envelope_computer_s *env_computer, double mod_x, double mod_y, double mod_w);

void update_envelope_modulation(envelope_computer_s *env_computer, double mod_x, double mod_y, double mod_w);

void compute_envelopes(
    envelope_computer_s *env_computer,
    const bpbx_envelope_s *envelopes, unsigned int envelope_count,
    double beat_start, double tick_time_start, double secs_per_tick
);

extern const envelope_curve_preset_s envelope_curve_presets[BPBX_ENVELOPE_CURVE_PRESET_COUNT];

#endif