#include "envelope.h"

#include <assert.h>
#include "util.h"
#include "inst/instrument.h"

double secs_fade_in(double setting) {
    return 0.0125 * (0.95 * setting + 0.05 * setting * setting);
}

// neutral index is at idx4
static double fade_out_ticks[] = { -24.0, -12.0, -6.0, -3.0, -1.0, 6.0, 12.0, 24.0, 48.0, 72.0, 96.0 };

double ticks_fade_out(double setting) {
    setting += 4.0;

    // not possible in normal beepbox; extrapolation
    // using an upward-opening quadratic which intersects with the points (5,6) and (10,96)
    if (setting < 0.0) return fade_out_ticks[0];
    if (setting > FADE_OUT_RANGE - 1) return 1.68 * setting * setting - 7.2 * setting * setting;

    // fractional settings are not possible in normal beepbox; linear interpolation
    int index = (int) setting;
    double result = fade_out_ticks[index];
    return (fade_out_ticks[index+1] - result) * (setting - index) + result;
}

static double get_lp_cutoff_decay_volume_compensation(const envelope_curve_preset_s *curve) {
    // This is a little hokey in the details, but I designed it a while ago and keep it 
    // around for compatibility. This decides how much to increase the volume (or
    // expression) to compensate for a decaying lowpass cutoff to maintain perceived
    // volume overall.
    if (curve->curve_type == ENV_CURVE_DECAY) return 1.25 + 0.025 * curve->speed;
    if (curve->curve_type == ENV_CURVE_TWANG) return 1.0 + 0.02 * curve->speed;
    return 1.0;
}

static double compute_envelope(const envelope_curve_preset_s *curve, double time, double beats, double note_size, double mod_x, double mod_y, double mod_w) {
    switch (curve->curve_type) {
        case ENV_CURVE_NOTE_SIZE:       return note_size_to_volume_mult(note_size);
        case ENV_CURVE_NONE:            return 1.0;
        case ENV_CURVE_TWANG:           return 1.0 / (1.0 + time * curve->speed);
        case ENV_CURVE_SWELL:           return 1.0 - 1.0 / (1.0 + time * curve->speed);
        case ENV_CURVE_TREMOLO:         return 0.5 - cos(beats * 2.0 * PI * curve->speed) * 0.5;
        case ENV_CURVE_TREMOLO2:        return 0.75 - cos(beats * 2.0 * PI * curve->speed) * 0.25;
        case ENV_CURVE_PUNCH:           return max(1.0, 2.0 - time * 10.0);
        case ENV_CURVE_DECAY:           return pow(2.0, -curve->speed * time);
        case ENV_CURVE_BLIP:            return 1.0 * +(time < (0.25 / sqrt(curve->speed)));

        case ENV_CURVE_FLARE: {
            const double attack = 0.25 / sqrt(curve->speed);
            return time < attack ? time / attack : 1.0 / (1.0 + (time - attack) * curve->speed);
        }

        case ENV_CURVE_MOD_X:     return clampd(mod_x, 0.0, 1.0);
        case ENV_CURVE_MOD_Y:     return clampd(mod_y, 0.0, 1.0);
        case ENV_CURVE_MOD_WHEEL: return clampd(mod_w, 0.0, 1.0);

        default:
            assert(0 && "unrecognized operator envelope type");
            return 0.0;
    }
}

void envelope_computer_init(envelope_computer_s *env_computer, double mod_x, double mod_y, double mod_w) {
    for (int i = 0; i < BPBXSYN_ENV_INDEX_COUNT; i++) {
        env_computer->envelope_starts[i] = 1.0;
        env_computer->envelope_ends[i] = 1.0;
    }

    env_computer->lp_cutoff_decay_volume_compensation = 1.0;

    env_computer->mod_x[0] = mod_x;
    env_computer->mod_y[0] = mod_y;
    env_computer->mod_wheel[0] = mod_w;

    env_computer->mod_x[1] = mod_x;
    env_computer->mod_y[1] = mod_y;
    env_computer->mod_wheel[1] = mod_w;
}

void update_envelope_modulation(envelope_computer_s *env_computer, double mod_x, double mod_y, double mod_w) {
    env_computer->mod_x[0] = env_computer->mod_x[1];
    env_computer->mod_y[0] = env_computer->mod_y[1];
    env_computer->mod_wheel[0] = env_computer->mod_wheel[1];

    env_computer->mod_x[1] = mod_x;
    env_computer->mod_y[1] = mod_y;
    env_computer->mod_wheel[1] = mod_w;
}

// static int is_filter_target(bpbxsyn_envelope_compute_index_e index) {
//     return
//         (index >= BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ0 &&
//         index <= BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ7) ||
//         (index >= BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN0 &&
//         index <= BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN7) ||
//         index == BPBXSYN_ENV_INDEX_NOTE_FILTER_ALL_FREQS;
// }

static int get_filter_target(bpbxsyn_envelope_compute_index_e index) {
    // i'd rather make a huge switch statement than write the bounds checking
    // because i can multicursor
    switch (index) {
        case BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ0: return 0;
        case BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ1: return 1;
        case BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ2: return 2;
        case BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ3: return 3;
        case BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ4: return 4;
        case BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ5: return 5;
        case BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ6: return 6;
        case BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ7: return 7;

        case BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN0: return 0;
        case BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN1: return 1;
        case BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN2: return 2;
        case BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN3: return 3;
        case BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN4: return 4;
        case BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN5: return 5;
        case BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN6: return 6;
        case BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN7: return 7;

        default: return -1;
    }
}

void compute_envelopes(
    const bpbxsyn_synth_s *inst, envelope_computer_s *env_computer,
    double beat_start, double tick_time_start, double secs_per_tick
) {
    (void)tick_time_start;

    const bool slide_transition =
        inst->active_effects[BPBXSYN_SYNTHFX_TRANSITION_TYPE] &&
        inst->transition_type == BPBXSYN_TRANSITION_TYPE_SLIDE;
    
    if (env_computer->flags & ENV_COMPUTER_FLAG_DO_RESET) {
        env_computer->prev_note_secs_end = env_computer->note_secs_end;
        env_computer->note_secs_end = 0.0;
        env_computer->flags &= ~ENV_COMPUTER_FLAG_DO_RESET;
        env_computer->tick = 0;

        if (slide_transition) {
            env_computer->flags |= ENV_COMPUTER_FLAG_IS_SLIDING;
            env_computer->slide_ratio_start = 0.0;
            env_computer->slide_ratio_end = 0.0;
        }
    }
    
    env_computer->note_secs_start = env_computer->note_secs_end;
    env_computer->note_secs_end = env_computer->note_secs_start + secs_per_tick;
    env_computer->lp_cutoff_decay_volume_compensation = 1.0;

    // const double tick_time_end = tick_time_start + 1.0;

    const double beats_per_tick = 1.0 / (TICKS_PER_PART * PARTS_PER_BEAT);
    const double beats_time_start = beat_start;
    const double beats_time_end = beat_start + beats_per_tick;

    for (int i = 0; i < BPBXSYN_ENV_INDEX_COUNT; i++) {
        env_computer->envelope_starts[i] = 1.0;
        env_computer->envelope_ends[i] = 1.0;
    }

    bool sliding = env_computer->flags & ENV_COMPUTER_FLAG_IS_SLIDING;
    if (sliding) {
        const double tick_start = env_computer->tick;
        const double tick_end = tick_start + 1.0;

        if (tick_start >= NOTE_SLIDE_TICKS) {
            env_computer->flags &= ~ENV_COMPUTER_FLAG_IS_SLIDING;
            sliding = false;
        } else {
            env_computer->slide_ratio_start = tick_start / NOTE_SLIDE_TICKS;
            env_computer->slide_ratio_end = tick_end / NOTE_SLIDE_TICKS;
        }
    }

    for (unsigned int i = 0; i < inst->envelope_count; i++) {
        const bpbxsyn_envelope_s *env = &inst->envelopes[i];
        const envelope_curve_preset_s curve = envelope_curve_presets[env->curve_preset];

        double envelope_start = compute_envelope(
            &curve, env_computer->note_secs_start, beats_time_start, NOTE_SIZE_MAX,
            env_computer->mod_x[0], env_computer->mod_y[0], env_computer->mod_wheel[0]
        );

        double envelope_end = compute_envelope(
            &curve, env_computer->note_secs_end, beats_time_end, NOTE_SIZE_MAX,
            env_computer->mod_x[1], env_computer->mod_y[1], env_computer->mod_wheel[1]
        );

        if (sliding) {
            const double other_start = compute_envelope(
                &curve,
                env_computer->prev_note_secs_end, beats_time_start, NOTE_SIZE_MAX,
                env_computer->mod_x[0], env_computer->mod_y[0], env_computer->mod_wheel[0]
            );

            const double other_end = compute_envelope(
                &curve,
                env_computer->prev_note_secs_end, beats_time_end, NOTE_SIZE_MAX,
                env_computer->mod_x[0], env_computer->mod_y[0], env_computer->mod_wheel[0]
            );

            envelope_start =
                (envelope_start - other_start) * env_computer->slide_ratio_start + other_start;
            envelope_end =
                (envelope_end - other_end) * env_computer->slide_ratio_end + other_end;
        }

        env_computer->envelope_starts[env->index] *= envelope_start;
        env_computer->envelope_ends[env->index] *= envelope_end;

        int filter_target = get_filter_target(env->index);
        if (filter_target != -1 && inst->note_filter.type[filter_target] == BPBXSYN_FILTER_TYPE_LP) {
            double v = get_lp_cutoff_decay_volume_compensation(&curve);
            if (v > env_computer->lp_cutoff_decay_volume_compensation)
                env_computer->lp_cutoff_decay_volume_compensation = v;
        }
    }

    env_computer->tick += 1.0;
}




////////////
//  DATA  //
////////////

const envelope_curve_preset_s envelope_curve_presets[BPBXSYN_ENVELOPE_CURVE_PRESET_COUNT] = {
    {
        .name = "none",
        .curve_type = ENV_CURVE_NONE,
    },
    {
        .name = "note size",
        .curve_type = ENV_CURVE_NOTE_SIZE,
    },
    {
        .name = "mod x",
        .curve_type = ENV_CURVE_MOD_X
    },
    {
        .name = "mod y",
        .curve_type = ENV_CURVE_MOD_Y
    },
    {
        .name = "punch",
        .curve_type = ENV_CURVE_PUNCH,
    },
    {
        .name = "flare 1",
        .curve_type = ENV_CURVE_FLARE,
        .speed = 32.0
    },
    {
        .name = "flare 2",
        .curve_type = ENV_CURVE_FLARE,
        .speed = 8.0
    },

    {
        .name = "flare 3",
        .curve_type = ENV_CURVE_FLARE,
        .speed = 2.0
    },
    {
        .name = "twang 1",
        .curve_type = ENV_CURVE_TWANG,
        .speed = 32.0
    },
    {
        .name = "twang 2",
        .curve_type = ENV_CURVE_TWANG,
        .speed = 8.0
    },
    {
        .name = "twang 3",
        .curve_type = ENV_CURVE_TWANG,
        .speed = 2.0
    },
    {
        .name = "swell 1",
        .curve_type = ENV_CURVE_SWELL,
        .speed = 32.0
    },
    {
        .name = "swell 2",
        .curve_type = ENV_CURVE_SWELL,
        .speed = 8.0
    },
    {
        .name = "swell 3",
        .curve_type = ENV_CURVE_SWELL,
        .speed = 2.0
    },
    {
        .name = "tremolo 1",
        .curve_type = ENV_CURVE_TREMOLO,
        .speed = 4.0
    },
    {
        .name = "tremolo 2",
        .curve_type = ENV_CURVE_TREMOLO,
        .speed = 2.0
    },
    {
        .name = "tremolo 3",
        .curve_type = ENV_CURVE_TREMOLO,
        .speed = 1.0,
    },
    {
        .name = "tremolo 4",
        .curve_type = ENV_CURVE_TREMOLO2,
        .speed = 4.0
    },
    {
        .name = "tremolo 5",
        .curve_type = ENV_CURVE_TREMOLO2,
        .speed = 2.0
    },
    {
        .name = "tremolo 6",
        .curve_type = ENV_CURVE_TREMOLO2,
        .speed = 1.0
    },
    {
        .name = "decay 1",
        .curve_type = ENV_CURVE_DECAY,
        .speed = 10.0
    },
    {
        .name = "decay 2",
        .curve_type = ENV_CURVE_DECAY,
        .speed = 7.0
    },
    {
        .name = "decay 3",
        .curve_type = ENV_CURVE_DECAY,
        .speed = 4.0
    },
    {
        .name = "blip 1",
        .curve_type = ENV_CURVE_BLIP,
        .speed = 6.0
    },
    {
        .name = "blip 2",
        .curve_type = ENV_CURVE_BLIP,
        .speed = 16.0
    },
    {
        .name = "blip 3",
        .curve_type = ENV_CURVE_BLIP,
        .speed = 32.0
    },
};