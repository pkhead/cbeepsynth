#include <limits.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "instrument.h"
#include "util.h"

#define VOLUME_LOG_SCALE 0.1428
#define GET_VOICE(voice_list, size, idx) ((inst_base_voice_s*)((uint8_t*)(voice_list) + (idx) * (size)))

void inst_init(bpbx_inst_s *inst, bpbx_inst_type_e type) {
    *inst = (bpbx_inst_s) {
        .type = type,
        .sample_rate = 0.0,

        .volume = 0.0,
        .panning = 50.0,
        .fade_in = 0.0,
        .fade_out = 0.0
    };

    for (int i = 0; i < BPBX_FILTER_GROUP_COUNT; i++) {
        inst->note_filter.gain_idx[i] = BPBX_FILTER_GAIN_CENTER;
        inst->note_filter.freq_idx[i] = 20;
    }
}

double calc_samples_per_tick(double bpm, double sample_rate) {
    const double beats_per_sec = bpm / 60.0;
    const double parts_per_sec = PARTS_PER_BEAT * beats_per_sec;
    const double ticks_per_sec = TICKS_PER_PART * parts_per_sec;
    return sample_rate / ticks_per_sec;
}

// double calc_pitch_expression(double pitch) {
//     return pow(2.0, -(pitch - EXPRESSION_REFERENCE_PITCH) / PITCH_DAMPING);
// }

double note_size_to_volume_mult(double size) {
    return pow(max(0.0, size) / NOTE_SIZE_MAX, 1.5);
}

double inst_volume_to_mult(double inst_volume) {
    if (inst_volume <= -25.0) return 0.0;
    return pow(2.0, VOLUME_LOG_SCALE * inst_volume);
}

const double vibrato_normal_periods_secs[1] = {0.14};
const double vibrato_shaky_periods_secs[3] = {0.11, 1.618 * 0.11, 3 * 0.11};

typedef struct {
    int size_periods_secs;
    const double *periods_secs;
} vibrato_type_def_s;

static vibrato_type_def_s vibrato_types[2] = {
    {
        .size_periods_secs = 1,
        .periods_secs = vibrato_normal_periods_secs
    },
    {
        .size_periods_secs = 3,
        .periods_secs = vibrato_shaky_periods_secs
    }
};

double get_lfo_amplitude(bpbx_vibrato_type_e type, double secs_into_bar) {
    assert(0 <= type && type <= 1);

    double effect = 0.0;
    vibrato_type_def_s type_config = vibrato_types[type];
    for (int i = 0; i < type_config.size_periods_secs; i++) {
        const double vibrato_period_secs = type_config.periods_secs[i];
        effect += sin(PI2 * secs_into_bar / vibrato_period_secs);
    }
    return effect;
}

int trigger_voice(bpbx_inst_s *inst, void *voices, size_t sizeof_voice, int key, int velocity) {
    int voice_index = 0;

    for (int i = 0; i < BPBX_INST_MAX_VOICES; i++) {
        if (GET_VOICE(voices, sizeof_voice, i)->triggered) continue;
        voice_index = i;
        break;
    }

    inst_base_voice_s *voice = GET_VOICE(voices, sizeof_voice, voice_index);
    memset(voice, 0, sizeof_voice);
    *voice = (inst_base_voice_s) {
        .triggered = TRUE,
        .released = FALSE,
        .key = key < 0 ? 0 : (uint16_t)key,
        .volume = velocity / 127.f,
        .has_prev_vibrato = FALSE
    };

    for (int i = 0; i < FILTER_GROUP_COUNT; i++) {
        dyn_biquad_reset_output(voice->note_filters + i);
    }

    envelope_computer_init(&voice->env_computer, inst->mod_x, inst->mod_y, inst->mod_wheel);

    return voice_index;
}

void release_voice(bpbx_inst_s *inst, void *voices, size_t sizeof_voice, int key, int velocity) {
    (void)inst;
    (void)velocity;

    for (int i = 0; i < BPBX_INST_MAX_VOICES; i++) {
        inst_base_voice_s *voice = GET_VOICE(voices, sizeof_voice, i);
        if (voice->triggered && !voice->released && voice->key == key) {
            voice->released = 1;
            break;
        }
    }
}

static void compute_voice_pre(inst_base_voice_s *const voice, voice_compute_s *compute_struct) {
    const voice_compute_constants_s *const compute_data = &compute_struct->constants;
    const bpbx_inst_s *const inst = compute_data->inst;

    const double sample_len = compute_struct->varying.sample_len
        = 1.f / compute_data->sample_rate;
    const double samples_per_tick = compute_struct->varying.samples_per_tick
        = compute_data->samples_per_tick;
    const double rounded_samples_per_tick = compute_struct->varying.rounded_samples_per_tick
        = ceil(samples_per_tick);

    voice->time_ticks = voice->time2_ticks;
    voice->time2_ticks = voice->time_ticks + 1.0;
    voice->time_secs = voice->time2_secs;
    voice->time2_secs = voice->time_secs + samples_per_tick / compute_data->sample_rate;

    // const double ticks_into_bar = compute_data.cur_beat * PARTS_PER_BEAT / TICKS_PER_PART;
    // const double part_time_start = (double)ticks_into_bar / TICKS_PER_PART;
    // const double part_time_end = (double)(ticks_into_bar + 1) / TICKS_PER_PART;

    // update envelope computer modulation
    update_envelope_modulation(&voice->env_computer, compute_data->mod_x, compute_data->mod_y, compute_data->mod_w);

    compute_envelopes(inst, &voice->env_computer,
        compute_data->cur_beat, voice->time_secs, samples_per_tick * sample_len
    );

    const double fade_in_secs = secs_fade_in(compute_data->fade_in);

    double interval_start = 0.0;
    double interval_end = 0.0;

    // precalculation/volume balancing/etc
    double fade_expr_start = 1.0;
    double fade_expr_end = 1.0;

    const uint8_t released = compute_struct->_released = voice->time_secs >= fade_in_secs && voice->released;
    if (released) {
        const double ticks = fabs(ticks_fade_out(compute_data->fade_out));
        fade_expr_start = note_size_to_volume_mult((1.0 - voice->ticks_since_release / ticks) * NOTE_SIZE_MAX);
        fade_expr_end = note_size_to_volume_mult((1.0 - (voice->ticks_since_release + 1.0) / ticks) * NOTE_SIZE_MAX);

        // code should be voice->ticks_since_release + 1 >= ticks
        // but i decide to end it one tick earlier to mitigate release click.
        // How tf does beepbox not have that.
        if (voice->ticks_since_release >= ticks) {
            voice->is_on_last_tick = TRUE;
        }
    } else {
        // fade in beginning of note
        if (fade_in_secs > 0) {
            fade_expr_start *= min(1.0, voice->time_secs / fade_in_secs);
            fade_expr_end *= min(1.0, voice->time2_secs / fade_in_secs);
        }
    }

    // pitch shift
    if (inst->active_effects[BPBX_INSTFX_PITCH_SHIFT]) {
        const double env_start = voice->env_computer.envelope_starts[BPBX_ENV_INDEX_PITCH_SHIFT];
        const double env_end = voice->env_computer.envelope_ends[BPBX_ENV_INDEX_PITCH_SHIFT];

        interval_start += inst->pitch_shift * env_start;
        interval_end += inst->pitch_shift * env_end;
    }

    // detune
    if (inst->active_effects[BPBX_INSTFX_DETUNE]) {
        const double env_start = voice->env_computer.envelope_starts[BPBX_ENV_INDEX_DETUNE];
        const double env_end = voice->env_computer.envelope_ends[BPBX_ENV_INDEX_DETUNE];

        interval_start += inst->detune * env_start / 100.0;
        interval_end += inst->detune * env_end / 100.0;
    }

    // vibrato
    if (inst->active_effects[BPBX_INSTFX_VIBRATO]) {
        const bpbx_vibrato_params_s vibrato_params = *compute_data->vibrato_params;

        int delay_ticks = vibrato_params.delay * 2;

        // i don't get the calculations beepbox/jummbox does.
        // i just did my own thing. It sounds the same i think.
        double vibrato_phase_tick = compute_data->cur_beat * PARTS_PER_BEAT * TICKS_PER_PART;
        vibrato_phase_tick *= vibrato_params.speed;

        const double vibrato_time_start = inst->vibrato_time_start;
        const double vibrato_time_end = inst->vibrato_time_end;

        double vibrato_start;
        if (voice->has_prev_vibrato) {
            vibrato_start = voice->prev_vibrato;
        } else {
            double lfo_start = get_lfo_amplitude(vibrato_params.type, vibrato_time_start);
            const double vibrato_depth_envelope_start = voice->env_computer.envelope_starts[BPBX_ENV_INDEX_VIBRATO_DEPTH];
            vibrato_start = vibrato_params.depth * lfo_start * vibrato_depth_envelope_start;

            if (delay_ticks > 0.0) {
                const int ticks_until_vibrato_start = delay_ticks - voice->time_ticks;
                vibrato_start *= max(0.0, min(1.0, 1.0 - ticks_until_vibrato_start / 2.0));
            }
        }

        double lfo_end = get_lfo_amplitude(vibrato_params.type, vibrato_time_end);
        const double vibrato_depth_envelope_end = voice->env_computer.envelope_ends[BPBX_ENV_INDEX_VIBRATO_DEPTH];
        double vibrato_end = vibrato_params.depth * lfo_end * vibrato_depth_envelope_end;
        if (delay_ticks > 0.0) {
            const int ticks_until_vibrato_end = delay_ticks - voice->time2_ticks;
            vibrato_end *= max(0.0, min(1.0, 1.0 - ticks_until_vibrato_end / 2.0));
        }

        voice->has_prev_vibrato = TRUE;
        voice->prev_vibrato = vibrato_end;

        interval_start += vibrato_start;
        interval_end += vibrato_end;
    }

    // note filter
    double note_filter_expression = voice->env_computer.lp_cutoff_decay_volume_compensation;
    voice->filters_enabled = inst->active_effects[BPBX_INSTFX_NOTE_FILTER];
    if (voice->filters_enabled) {
        // get modulation for all freqs
        const double note_all_freqs_envelope_start =
            voice->env_computer.envelope_starts[BPBX_ENV_INDEX_NOTE_FILTER_ALL_FREQS];
        const double note_all_freqs_envelope_end=
            voice->env_computer.envelope_ends[BPBX_ENV_INDEX_NOTE_FILTER_ALL_FREQS];
        
        for (int i = 0; i < FILTER_GROUP_COUNT; i++) {
            const filter_group_s *filter_group_start = &inst->last_note_filter;
            const filter_group_s *filter_group_end = &inst->note_filter;

            // If switching dot type, do it all at once and do not try to interpolate since no valid interpolation exists.
            if (filter_group_start->type[i] != filter_group_end->type[i]) {
                filter_group_start = filter_group_end;
            }

            if (filter_group_start->type[i] == BPBX_FILTER_TYPE_OFF) {
                voice->note_filters[i].enabled = FALSE;
            } else {
                // get freq modulation
                const double note_freq_envelope_start =
                    voice->env_computer.envelope_starts[BPBX_ENV_INDEX_NOTE_FILTER_FREQ0 + i];
                const double note_freq_envelope_end =
                    voice->env_computer.envelope_ends[BPBX_ENV_INDEX_NOTE_FILTER_FREQ0 + i];

                // get gain modulation
                const double note_peak_envelope_start =
                    voice->env_computer.envelope_starts[BPBX_ENV_INDEX_NOTE_FILTER_GAIN0 + i];
                const double note_peak_envelope_end =
                    voice->env_computer.envelope_ends[BPBX_ENV_INDEX_NOTE_FILTER_GAIN0 + i];
                
                voice->note_filters[i].enabled = TRUE;

                filter_coefs_s start_coefs = filter_to_coefficients(
                    filter_group_start, i,
                    compute_data->sample_rate,
                    note_all_freqs_envelope_start * note_freq_envelope_start,
                    note_peak_envelope_start);

                filter_coefs_s end_coefs = filter_to_coefficients(
                    filter_group_end, i,
                    compute_data->sample_rate,
                    note_all_freqs_envelope_end * note_freq_envelope_end,
                    note_peak_envelope_end);
                
                dyn_biquad_load(&voice->note_filters[i],
                    start_coefs, end_coefs, 1.0 / rounded_samples_per_tick,
                    filter_group_start->type[i] == BPBX_FILTER_TYPE_LP);

                note_filter_expression *= filter_get_volume_compensation_mult(filter_group_start, i);
            }
        }
    }

    if (note_filter_expression > 3.0)
        note_filter_expression = 3.0;

    const double expr_start = note_filter_expression * fade_expr_start *
        voice->env_computer.envelope_starts[BPBX_ENV_INDEX_NOTE_VOLUME];
    const double expr_end = note_filter_expression * fade_expr_end *
        voice->env_computer.envelope_ends[BPBX_ENV_INDEX_NOTE_VOLUME];

    compute_struct->varying.expr_start = expr_start;
    compute_struct->varying.expr_end = expr_end;
    compute_struct->varying.interval_start = interval_start;
    compute_struct->varying.interval_end = interval_end;
}

static void compute_voice_post(inst_base_voice_s *const voice, voice_compute_s *compute_data) {
    if (compute_data->_released) {
        voice->ticks_since_release += 1.0;
        voice->secs_since_release += compute_data->constants.samples_per_tick;
    }
}

void inst_tick(bpbx_inst_s *inst, const bpbx_tick_ctx_s *run_ctx, const audio_compute_s *params)
{
    const double sample_rate = inst->sample_rate;
    const double sample_len = 1.0 / sample_rate;
    const double beat = run_ctx->beat;

    const double samples_per_tick = calc_samples_per_tick(run_ctx->bpm, sample_rate);
    const double fade_in = inst->fade_in;
    const double fade_out = inst->fade_out;

    const double mod_x = inst->mod_x;
    const double mod_y = inst->mod_y;
    const double mod_w = run_ctx->mod_wheel;    

    // update vibrato lfo
    bpbx_vibrato_params_s vibrato = inst->vibrato;
    bpbx_vibrato_preset_params(inst->vibrato_preset, &vibrato);

    inst->vibrato_time_start = inst->vibrato_time_end;
    inst->vibrato_time_end += samples_per_tick * sample_len * vibrato.speed;

    for (int i = 0; i < BPBX_INST_MAX_VOICES; i++) {
        inst_base_voice_s *voice = GET_VOICE(params->voice_list, params->sizeof_voice, i);
        
        if (!voice->triggered) continue;
        if (voice->is_on_last_tick) {
            voice->triggered = FALSE;
            voice->active = FALSE;
            continue;
        }
        
        voice->active = TRUE;

        voice_compute_s compute_data = {
            .constants = {
                .inst = inst,

                .samples_per_tick = samples_per_tick,
                .sample_rate = sample_rate,
                .fade_in = fade_in,
                .fade_out = fade_out,
                .cur_beat = beat,
                .vibrato_params = &vibrato,

                .mod_x = mod_x,
                .mod_y = mod_y,
                .mod_w = mod_w
            }
        };

        compute_voice_pre(voice, &compute_data);
        params->compute_voice(inst, voice, &compute_data);
        compute_voice_post(voice, &compute_data);
    }

    inst->last_eq = inst->eq;
    inst->last_note_filter = inst->note_filter;
}










////////////
//  DATA  //
////////////

static const char *bool_enum_values[] = {"Off", "On"};
static const char *transition_type_values[] = {"Normal", "Interrupt", "Continue", "Slide"};
static const char *filt_type_enum[] = {"Off", "Low pass", "High pass", "Notch"};
static const char *chord_type_values[] = {"Simultaneous", "Strum", "Arpeggio", "Custom Interval"};
static const char *vibrato_preset_values[] = {"None", "Light", "Delayed", "Heavy", "Shaky", "Custom"};
static const char *vibrato_values[] = {"Normal", "Shaky"};

#define FILTER_MAX_FREQ 33
#define FILTER_MAX_GAIN 14

const unison_desc_s unison_info[BPBX_UNISON_COUNT] = {
    // none
    { .voices = 1, .spread = 0.0, .offset = 0.0, .expression = 1.4, .sign = 1.0 },
    // shimmer
    { .voices = 2, .spread = 0.018, .offset = 0.0, .expression = 0.8, .sign = 1.0 },
    // hum
    { .voices = 2, .spread = 0.045, .offset = 0.0, .expression = 1.0, .sign = 1.0 },
    // honky tonk
    { .voices = 2, .spread = 0.09, .offset = 0.0, .expression = 1.0, .sign = 1.0 },
    // dissonant
    { .voices = 2, .spread = 0.25, .offset = 0.0, .expression = 0.9, .sign = 1.0 },
    // fifth
    { .voices = 2, .spread = 3.5, .offset = 3.5, .expression = 0.9, .sign = 1.0 },
    // octave
    { .voices = 2, .spread = 6.0, .offset = 6.0, .expression = 0.8, .sign = 1.0 },
    // bowed
    { .voices = 2, .spread = 0.02, .offset = 0.0, .expression = 1.0, .sign = -1.0 },
    // piano
    { .voices = 2, .spread = 0.01, .offset = 0.0, .expression = 1.0, .sign = 0.7 },
    // warbled
    { .voices = 2, .spread = 0.25, .offset = 0.05, .expression = 0.9, .sign = -0.8 },
};

bpbx_inst_param_info_s base_param_info[BPBX_BASE_PARAM_COUNT] = {
    // general
    {
        .name = "Volume",
        .group = "General",

        .type = BPBX_PARAM_DOUBLE,
        .min_value = -25.0,
        .max_value = 25.0,
        .default_value = 0.0
    },
    {
        .name = "Fade In",
        .group = "General",

        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0.0,
        .max_value = 9.0,
        .default_value = 0.0
    },
    {
        .name = "Fade Out",
        .group = "General",

        .type = BPBX_PARAM_DOUBLE,
        .min_value = FADE_OUT_MIN,
        .max_value = FADE_OUT_MAX,
        .default_value = 0.0
    },

    // modulation params
    {
        .name = "Modulation X",
        .group = "Modulation",

        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0.0,
        .max_value = 1.0,
        .default_value = 0.0
    },
    {
        .name = "Modulation Y",
        .group = "Modulation",

        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0.0,
        .max_value = 1.0,
        .default_value = 0.0
    },

    // transition type
    {
        .name = "Transition Type Toggle",
        .group = "Effects/Transition Type",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .default_value = 0,
        .enum_values = bool_enum_values
    },
    {
        .name = "Transition Type",
        .group = "Effects/Transition Type",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .default_value = 0,
        .enum_values = transition_type_values
    },

    // chord type
    {
        .name = "Chord Type Toggle",
        .group = "Effects/Chord Type",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .default_value = 0,
        .enum_values = bool_enum_values
    },
    {
        .name = "Chord Type",
        .group = "Effects/Chord Type",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .default_value = 0,
        .enum_values = chord_type_values
    },

    // pitch shift
    {
        .name = "Pitch Shift Toggle",
        .group = "Effects/Pitch Shift",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .default_value = 0,
        .enum_values = bool_enum_values
    },
    {
        .name = "Pitch Shift (st)",
        .group = "Effects/Pitch Shift",

        .type = BPBX_PARAM_DOUBLE,
        .min_value = -12,
        .max_value = 12,
        .default_value = 0,
    },

    // detune
    {
        .name = "Detune Toggle",
        .group = "Effects/Detune",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .default_value = 0,
        .enum_values = bool_enum_values
    },
    {
        .name = "Detune (c)",
        .group = "Effects/Detune",

        .type = BPBX_PARAM_DOUBLE,
        .min_value = -200,
        .max_value = 200,
        .default_value = 0,
    },

    // vibrato
    {
        .name = "Vibrato Toggle",
        .group = "Effects/Vibrato",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .default_value = 0,
        .enum_values = bool_enum_values
    },
    {
        .name = "Vibrato Preset",
        .group = "Effects/Vibrato",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 5,
        .default_value = 0,
        .enum_values = vibrato_preset_values
    },
    {
        .name = "Vibrato Depth",
        .group = "Effects/Vibrato",

        // in beepbox code, this was quantized to increments of 0.04
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = 2,
        .default_value = 0,
        .enum_values = bool_enum_values
    },
    {
        .name = "Vibrato Speed",
        .group = "Effects/Vibrato",

        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = 3.0,
        .default_value = 1.0,
    },
    {
        .name = "Vibrato Delay",
        .group = "Effects/Vibrato",

        .type = BPBX_PARAM_INT,
        .min_value = 0,
        .max_value = 50,
        .default_value = 0,
    },
    {
        .name = "Vibrato Type",
        .group = "Effects/Vibrato",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .default_value = 0,
        .enum_values = vibrato_values
    },

    // note filter
    /*
    local template = [[{
        .name = "Note Filter # Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter # Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter # Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    ]]

    local build = {}
    for i=1, 8 do
        local repl = template:gsub("#", tostring(i))
        table.insert(build, repl)
    end

    local str = table.concat(build)

    local p = assert(io.popen("clip.exe", "w"))
    p:write(str)
    p:close()
    */
    {
        .name = "Note Filter Toggle",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "Note Filter 1 Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter 1 Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter 1 Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "Note Filter 2 Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter 2 Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter 2 Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "Note Filter 3 Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter 3 Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter 3 Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "Note Filter 4 Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter 4 Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter 4 Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "Note Filter 5 Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter 5 Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter 5 Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "Note Filter 6 Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter 6 Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter 6 Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "Note Filter 7 Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter 7 Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter 7 Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "Note Filter 8 Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter 8 Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter 8 Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },

    
    
    // eq filter
    /*
    local template = [[{
        .name = "EQ # Type",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "EQ # Freq.",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "EQ # Gain",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    ]]

    local build = {}
    for i=1, 8 do
        local repl = template:gsub("#", tostring(i))
        table.insert(build, repl)
    end

    local str = table.concat(build)

    local p = assert(io.popen("clip.exe", "w"))
    p:write(str)
    p:close()
    */
    {
        .name = "EQ 1 Type",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "EQ 1 Freq.",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "EQ 1 Gain",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "EQ 2 Type",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "EQ 2 Freq.",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "EQ 2 Gain",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "EQ 3 Type",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "EQ 3 Freq.",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "EQ 3 Gain",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "EQ 4 Type",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "EQ 4 Freq.",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "EQ 4 Gain",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "EQ 5 Type",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "EQ 5 Freq.",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "EQ 5 Gain",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "EQ 6 Type",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "EQ 6 Freq.",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "EQ 6 Gain",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "EQ 7 Type",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "EQ 7 Freq.",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "EQ 7 Gain",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "EQ 8 Type",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "EQ 8 Freq.",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "EQ 8 Gain",
        .group = "EQ",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
};

size_t base_param_offsets[BPBX_BASE_PARAM_COUNT] = {
    // general
    offsetof(bpbx_inst_s, volume),
    offsetof(bpbx_inst_s, fade_in),
    offsetof(bpbx_inst_s, fade_out),

    // modulation
    offsetof(bpbx_inst_s, mod_x),
    offsetof(bpbx_inst_s, mod_y),

    // transition type
    offsetof(bpbx_inst_s, active_effects[BPBX_INSTFX_TRANSITION_TYPE]),
    offsetof(bpbx_inst_s, transition_type),

    // chord type
    offsetof(bpbx_inst_s, active_effects[BPBX_INSTFX_CHORD_TYPE]),
    offsetof(bpbx_inst_s, chord_type),

    // pitch shift
    offsetof(bpbx_inst_s, active_effects[BPBX_INSTFX_PITCH_SHIFT]),
    offsetof(bpbx_inst_s, pitch_shift),

    // detune
    offsetof(bpbx_inst_s, active_effects[BPBX_INSTFX_DETUNE]),
    offsetof(bpbx_inst_s, detune),

    // vibrato
    offsetof(bpbx_inst_s, active_effects[BPBX_INSTFX_VIBRATO]),
    offsetof(bpbx_inst_s, vibrato_preset),
    offsetof(bpbx_inst_s, vibrato.depth),
    offsetof(bpbx_inst_s, vibrato.speed),
    offsetof(bpbx_inst_s, vibrato.delay),
    offsetof(bpbx_inst_s, vibrato.type),

    // note filter params
    offsetof(bpbx_inst_s, active_effects[BPBX_INSTFX_NOTE_FILTER]),
    offsetof(bpbx_inst_s, note_filter.type    [0]),
    offsetof(bpbx_inst_s, note_filter.freq_idx[0]),
    offsetof(bpbx_inst_s, note_filter.gain_idx[0]),
    offsetof(bpbx_inst_s, note_filter.type    [1]),
    offsetof(bpbx_inst_s, note_filter.freq_idx[1]),
    offsetof(bpbx_inst_s, note_filter.gain_idx[1]),
    offsetof(bpbx_inst_s, note_filter.type    [2]),
    offsetof(bpbx_inst_s, note_filter.freq_idx[2]),
    offsetof(bpbx_inst_s, note_filter.gain_idx[2]),
    offsetof(bpbx_inst_s, note_filter.type    [3]),
    offsetof(bpbx_inst_s, note_filter.freq_idx[3]),
    offsetof(bpbx_inst_s, note_filter.gain_idx[3]),
    offsetof(bpbx_inst_s, note_filter.type    [4]),
    offsetof(bpbx_inst_s, note_filter.freq_idx[4]),
    offsetof(bpbx_inst_s, note_filter.gain_idx[4]),
    offsetof(bpbx_inst_s, note_filter.type    [5]),
    offsetof(bpbx_inst_s, note_filter.freq_idx[5]),
    offsetof(bpbx_inst_s, note_filter.gain_idx[5]),
    offsetof(bpbx_inst_s, note_filter.type    [6]),
    offsetof(bpbx_inst_s, note_filter.freq_idx[6]),
    offsetof(bpbx_inst_s, note_filter.gain_idx[6]),
    offsetof(bpbx_inst_s, note_filter.type    [7]),
    offsetof(bpbx_inst_s, note_filter.freq_idx[7]),
    offsetof(bpbx_inst_s, note_filter.gain_idx[7]),

    // eq filter params
    offsetof(bpbx_inst_s, eq.type    [0]),
    offsetof(bpbx_inst_s, eq.freq_idx[0]),
    offsetof(bpbx_inst_s, eq.gain_idx[0]),
    offsetof(bpbx_inst_s, eq.type    [1]),
    offsetof(bpbx_inst_s, eq.freq_idx[1]),
    offsetof(bpbx_inst_s, eq.gain_idx[1]),
    offsetof(bpbx_inst_s, eq.type    [2]),
    offsetof(bpbx_inst_s, eq.freq_idx[2]),
    offsetof(bpbx_inst_s, eq.gain_idx[2]),
    offsetof(bpbx_inst_s, eq.type    [3]),
    offsetof(bpbx_inst_s, eq.freq_idx[3]),
    offsetof(bpbx_inst_s, eq.gain_idx[3]),
    offsetof(bpbx_inst_s, eq.type    [4]),
    offsetof(bpbx_inst_s, eq.freq_idx[4]),
    offsetof(bpbx_inst_s, eq.gain_idx[4]),
    offsetof(bpbx_inst_s, eq.type    [5]),
    offsetof(bpbx_inst_s, eq.freq_idx[5]),
    offsetof(bpbx_inst_s, eq.gain_idx[5]),
    offsetof(bpbx_inst_s, eq.type    [6]),
    offsetof(bpbx_inst_s, eq.freq_idx[6]),
    offsetof(bpbx_inst_s, eq.gain_idx[6]),
    offsetof(bpbx_inst_s, eq.type    [7]),
    offsetof(bpbx_inst_s, eq.freq_idx[7]),
    offsetof(bpbx_inst_s, eq.gain_idx[7]),
};