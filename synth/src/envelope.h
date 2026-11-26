#ifndef _envelope_h_
#define _envelope_h_

#include <stdbool.h>
#include <stdint.h>

#include "../include/beepbox_synth.h"

#define FADE_OUT_RANGE 11
#define FADE_OUT_MIN -4
#define FADE_OUT_MAX 6
#define NOTE_SLIDE_TICKS 6

enum {
    ENV_COMPUTER_FLAG_DO_RESET = (1 << 0),
    ENV_COMPUTER_FLAG_IS_SLIDING = (1 << 1),
};

typedef struct {
    uint8_t flags;

    double envelope_starts[BBXS_ENV_INDEX_COUNT];
    double envelope_ends[BBXS_ENV_INDEX_COUNT];
    double tick;
    double note_secs_start;
    double note_secs_end;
    double prev_note_secs_end; // for the slide transition

    double slide_ratio_start, slide_ratio_end;

    // lowpassCutoffDecayVolumeCompensation
    // TODO: implement this. Currently it's always 1.0
    double lp_cutoff_decay_volume_compensation;

    double note_size[2];
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

double bbxs__secs_fade_in(double setting);
double bbxs__ticks_fade_out(double setting);

typedef struct {
    const char *name;
    envelope_curve_type_e curve_type;
    double speed;
} envelope_curve_preset_s;

void bbxs__envelope_computer_init(envelope_computer_s *env_computer,
                                  double note_size, double mod_x, double mod_y,
                                  double mod_w);

void bbxs__update_envelope_modulation(envelope_computer_s *env_computer,
                                      double note_size, double mod_x,
                                      double mod_y, double mod_w);

void bbxs__compute_envelopes(const bbxs_synth_s *instrument,
                             envelope_computer_s *env_computer,
                             double beat_start, double tick_time_start,
                             double secs_per_tick);

extern const envelope_curve_preset_s
    bbxs__envelope_curve_presets[BBXS_ENVELOPE_CURVE_PRESET_COUNT];
extern const char
    *bbxs__envelope_curve_preset_names[BBXS_ENVELOPE_CURVE_PRESET_COUNT + 1];

#endif