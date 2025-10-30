#include <assert.h>
#include <string.h>
#include "synth.h"
#include "../util.h"
#include "../context.h"


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

///////////////
//  GENERIC  //
///////////////

static inline bpbxsyn_voice_id wave_note_on(
    bpbxsyn_synth_s *inst, wave_voice_s *voice_list,
    int key, double velocity, int32_t length
) {
    bool continuation;
    bpbxsyn_voice_id voice_index = bbsyn_trigger_voice(
        inst, GENERIC_LIST(voice_list), key, velocity, length, &continuation);

    if (!continuation) {
        wave_voice_s *voice = &voice_list[voice_index];
        *voice = (wave_voice_s) {
            .base = voice->base
        };
    }

    return voice_index;
}

static inline void wave_note_off(bpbxsyn_synth_s *inst, wave_voice_s *voice_list, bpbxsyn_voice_id id) {
    bbsyn_release_voice(inst, GENERIC_LIST(voice_list), id);
}

static void compute_wave_voice(
    const bpbxsyn_synth_s *const base_inst, inst_base_voice_s *voice, voice_compute_s *compute_data,
    const double base_expression
) {
    const chip_inst_s *const inst = (chip_inst_s*) base_inst;
    wave_voice_s *const wave_voice = (wave_voice_s*) voice;

    const double sample_len = compute_data->varying.sample_len;
    const double rounded_samples_per_tick = compute_data->varying.rounded_samples_per_tick;

    voice_compute_varying_s *const varying = &compute_data->varying;

    const unison_desc_s unison = bbsyn_unison_info[inst->unison_type];

    double settings_expression_mult = base_expression;
    settings_expression_mult *= unison.expression * unison.voices / 2.0;

    const double interval_start = compute_data->varying.interval_start;
    const double interval_end = compute_data->varying.interval_end;
    const double start_pitch = voice->current_key + interval_start;
    const double end_pitch = voice->current_key + interval_end;

    // pitch expression
    double pitch_expression_start;
    if (wave_voice->has_prev_pitch_expression) {
        pitch_expression_start = wave_voice->prev_pitch_expression;
    } else {
        pitch_expression_start = calc_pitch_expression(start_pitch);
    }
    const double pitch_expression_end = calc_pitch_expression(end_pitch);
    wave_voice->has_prev_pitch_expression = TRUE;
    wave_voice->prev_pitch_expression = pitch_expression_end;

    // calculate final expression
    const double expr_start = varying->expr_start * settings_expression_mult * pitch_expression_start;
    const double expr_end = varying->expr_end * settings_expression_mult * pitch_expression_end;
    
    const double unison_env_start = voice->env_computer.envelope_starts[BPBXSYN_ENV_INDEX_UNISON];
    const double unison_env_end = voice->env_computer.envelope_ends[BPBXSYN_ENV_INDEX_UNISON];

    const double freq_end_ratio = pow(2.0, (interval_end - interval_start) * 1.0 / 12.0);
    const double base_phase_delta_scale = pow(freq_end_ratio, 1.0 / rounded_samples_per_tick);

    const double start_freq = key_to_hz_d(start_pitch);

    // TODO: specialIntervalMult has to do with arpeggios/custom interval
    // But if there is none it will always be 1.0
    assert(UNISON_MAX_VOICES == 2);
    double unison_starts[UNISON_MAX_VOICES];
    double unison_ends[UNISON_MAX_VOICES];

    unison_starts[0] = pow(2.0, (unison.offset + unison.spread) * unison_env_start / 12.0);
    unison_ends[0] = pow(2.0, (unison.offset + unison.spread) * unison_env_end / 12.0);
    unison_starts[1] = pow(2.0, (unison.offset - unison.spread) * unison_env_start / 12.0)/* * specialIntervalMult*/;
    unison_ends[1] = pow(2.0, (unison.offset - unison.spread) * unison_env_end / 12.0)/* * specialIntervalMult*/;

    for (int i = 0; i < UNISON_MAX_VOICES; i++) {
        wave_voice->phase_delta[i] = start_freq * sample_len * unison_starts[i];
        wave_voice->phase_delta_scale[i] = base_phase_delta_scale * pow(unison_ends[i] / unison_starts[i], 1.0 / rounded_samples_per_tick);
    }
    
    voice->expression = expr_start;
    voice->expression_delta = (expr_end - expr_start) / rounded_samples_per_tick;
}

static void wave_audio_render_callback(
    float *output_buffer, size_t frames_to_compute, bool aliases,
    unison_desc_s unison, const float *wave, size_t wave_length, wave_voice_s *voice_list
) {
    assert(UNISON_MAX_VOICES == 2);
    assert(unison.voices <= UNISON_MAX_VOICES);

    memset(output_buffer, 0, frames_to_compute * sizeof(float));

    for (int i = 0; i < BPBXSYN_SYNTH_MAX_VOICES; i++) {
        wave_voice_s *voice = voice_list + i;
        if (!voice_is_computing(&voice->base)) continue;
        float *out = output_buffer;

        if (unison.voices == 1) voice->phase[1] = voice->phase[0];
        
        // convert to operable values
        double phase[UNISON_MAX_VOICES];
        double phase_delta[UNISON_MAX_VOICES];
        double prev_wave_integral[UNISON_MAX_VOICES];

        for (int i = 0; i < UNISON_MAX_VOICES; i++) {
            phase[i] = fmod(voice->phase[i], 1.0) * wave_length;
            phase_delta[i] = voice->phase_delta[i] * wave_length;
            prev_wave_integral[i] = 0.0;

            if (!aliases) {
                const int phase_int = (int)phase[i];
                const int index = phase_int % wave_length;
                const double phase_ratio = phase[i] - phase_int;
                prev_wave_integral[i] = (double)wave[index];
                prev_wave_integral[i] += ((double)wave[index + 1] - prev_wave_integral[i]) * phase_ratio;
            }
        }
        
        double x1 = voice->base.note_filter_input[0];
        double x2 = voice->base.note_filter_input[1];

        for (size_t frame = 0; frame < frames_to_compute; frame++) {
            double x0[UNISON_MAX_VOICES];

            for (int i = 0; i < UNISON_MAX_VOICES; i++) {
                x0[i] = 0;
                phase[i] += phase_delta[i];

                if (aliases) {
                    x0[i] = (double)wave[(int)phase[i] % wave_length];
                } else {
                    const int phase_int = (int)phase[i];
                    const int index = phase_int % wave_length;
                    double next_wave_integral = (double)wave[index];
                    const double phase_ratio = phase[i] - phase_int;
                    next_wave_integral += ((double)wave[index + 1] - next_wave_integral) * phase_ratio;
                    x0[i] = (next_wave_integral - prev_wave_integral[i]) / phase_delta[i];
                    prev_wave_integral[i] = next_wave_integral;
                }

                phase_delta[i] *= voice->phase_delta_scale[i];
            }

            double x0_sum = x0[0] + x0[1] * unison.sign;

            x0_sum *= voice->base.expression * voice->base.volume;

            float final_sample;
            if (voice->base.filters_enabled) {
                final_sample = (float) bbsyn_apply_filters(x0_sum, x1, x2, voice->base.note_filters);
            } else {
                final_sample = (float) x0_sum;
            }

            x2 = x1;
            x1 = x0_sum;

            voice->base.expression += voice->base.expression_delta;

            // output frame
            *out++ += final_sample;
        }

        voice->base.note_filter_input[0] = x1;
        voice->base.note_filter_input[1] = x2;

        bbsyn_sanitize_filters(voice->base.note_filters, FILTER_GROUP_COUNT);

        for (int i = 0; i < UNISON_MAX_VOICES; i++) {
            voice->phase[i] = phase[i] / wave_length;
            voice->phase_delta[i] = phase_delta[i] / wave_length;
        }
    }
}













///////////////////////
//  CHIP INSTRUMENT  //
///////////////////////

#define CHIP_VOICE_BASE_EXPRESSION 0.03375

static void chip_init(bpbxsyn_context_s *ctx, bpbxsyn_synth_s *p_inst) {
    chip_inst_s *inst = (chip_inst_s*)p_inst;
    *inst = (chip_inst_s){0};
    bbsyn_inst_init(ctx, &inst->base, BPBXSYN_SYNTH_CHIP);

    for (int i = 0; i < BPBXSYN_SYNTH_MAX_VOICES; i++) {
        inst->voices[i].base.flags = 0;
    }

    inst->waveform = 2;
    inst->unison_type = 0;
}

static bpbxsyn_voice_id chip_note_on(bpbxsyn_synth_s *inst, int key,
                                     double velocity, int32_t length) {
    assert(inst);
    assert(inst->type == BPBXSYN_SYNTH_CHIP);
    chip_inst_s *const chip = (chip_inst_s*)inst;
    return wave_note_on(inst, chip->voices, key, velocity, length);
}

static void chip_note_off(bpbxsyn_synth_s *inst, bpbxsyn_voice_id id) {
    assert(inst);
    assert(inst->type == BPBXSYN_SYNTH_CHIP);
    chip_inst_s *const chip = (chip_inst_s*)inst;
    wave_note_off(inst, chip->voices, id);
}

static void chip_note_all_off(bpbxsyn_synth_s *inst) {
    assert(inst);
    assert(inst->type == BPBXSYN_SYNTH_CHIP);
    chip_inst_s *const chip = (chip_inst_s*)inst;
    bbsyn_release_all_voices(inst, GENERIC_LIST(chip->voices));
}

static void compute_chip_voice(const bpbxsyn_synth_s *const base_inst,
                               inst_base_voice_s *voice,
                               voice_compute_s *compute_data) {
    const chip_inst_s *const inst = (chip_inst_s*) base_inst;

    wavetable_desc_s wavetable = base_inst->ctx->wavetables.chip_wavetables[inst->waveform];
    double settings_expression_mult = CHIP_VOICE_BASE_EXPRESSION * wavetable.expression;

    compute_wave_voice(base_inst, voice, compute_data, settings_expression_mult);
}

static void chip_tick(bpbxsyn_synth_s *src_inst,
                      const bpbxsyn_tick_ctx_s *tick_ctx) {
    assert(src_inst);
    assert(src_inst->type == BPBXSYN_SYNTH_CHIP);

    chip_inst_s *const chip = (chip_inst_s*)src_inst;

    bbsyn_inst_tick(src_inst, tick_ctx, &(audio_compute_s) {
        .voice_list = chip->voices,
        .sizeof_voice = sizeof(*chip->voices),
        .compute_voice = compute_chip_voice,
        .userdata = chip
    });
}

static void chip_run(bpbxsyn_synth_s *src_inst, float *samples,
                     size_t frame_count) {
    chip_inst_s *const chip = (chip_inst_s*) src_inst;
    const bool aliases = FALSE;

    wavetable_desc_s wavetable;
    if (aliases) {
        wavetable = src_inst->ctx->wavetables.raw_chip_wavetables[chip->waveform];
    } else {
        wavetable = src_inst->ctx->wavetables.chip_wavetables[chip->waveform];
    }

    unison_desc_s unison = bbsyn_unison_info[chip->unison_type];

    wave_audio_render_callback(
        samples, frame_count,
        aliases, unison, wavetable.samples, wavetable.length - 1, chip->voices);
}























////////////////////////////
//  HARMONICS INSTRUMENT  //
////////////////////////////

#define HARMONICS_VOICE_BASE_EXPRESSION 0.025

static void harmonics_init(bpbxsyn_context_s *ctx, bpbxsyn_synth_s *p_inst) {
    harmonics_inst_s *inst = (harmonics_inst_s*)p_inst;
    *inst = (harmonics_inst_s){0};
    bbsyn_inst_init(ctx, &inst->base, BPBXSYN_SYNTH_HARMONICS);

    for (int i = 0; i < BPBXSYN_SYNTH_MAX_VOICES; i++) {
        inst->voices[i].base.flags = 0;
    }

    inst->unison_type = BPBXSYN_UNISON_NONE;
    inst->controls[0] = BPBXSYN_HARMONICS_CONTROL_MAX;

    bbsyn_generate_harmonics(&inst->base.ctx->wavetables, inst->controls, 64, inst->wave);

    memcpy(inst->last_controls, inst->controls, sizeof(inst->controls));
}

static bpbxsyn_voice_id harmonics_note_on(bpbxsyn_synth_s *inst, int key,
                                          double velocity, int32_t length) {
    assert(inst);
    assert(inst->type == BPBXSYN_SYNTH_HARMONICS);
    harmonics_inst_s *const harmonics = (harmonics_inst_s*)inst;

    return wave_note_on(inst, harmonics->voices, key, velocity, length);
}

static void harmonics_note_off(bpbxsyn_synth_s *inst, bpbxsyn_voice_id id) {
    assert(inst);
    assert(inst->type == BPBXSYN_SYNTH_HARMONICS);
    harmonics_inst_s *const harmonics = (harmonics_inst_s*)inst;
    wave_note_off(inst, harmonics->voices, id);
}

static void harmonics_note_all_off(bpbxsyn_synth_s *inst) {
    assert(inst);
    assert(inst->type == BPBXSYN_SYNTH_HARMONICS);
    harmonics_inst_s *const harmonics = (harmonics_inst_s*)inst;

    bbsyn_release_all_voices(inst, harmonics->voices, sizeof(*harmonics->voices));
}

static void compute_harmonics_voice(const bpbxsyn_synth_s *const base_inst,
                                    inst_base_voice_s *voice,
                                    voice_compute_s *compute_data) {
    compute_wave_voice(base_inst, voice, compute_data, HARMONICS_VOICE_BASE_EXPRESSION);
}

static void harmonics_tick(bpbxsyn_synth_s *src_inst,
                           const bpbxsyn_tick_ctx_s *tick_ctx) {
    assert(src_inst);
    assert(src_inst->type == BPBXSYN_SYNTH_HARMONICS);

    harmonics_inst_s *const harmonics = (harmonics_inst_s*)src_inst;

    bbsyn_inst_tick(src_inst, tick_ctx, &(audio_compute_s) {
        .voice_list = harmonics->voices,
        .sizeof_voice = sizeof(*harmonics->voices),
        .compute_voice = compute_harmonics_voice,
        .userdata = harmonics
    });
}

static void harmonics_run(bpbxsyn_synth_s *src_inst, float *samples,
                          size_t frame_count) {
    harmonics_inst_s *const harmonics = (harmonics_inst_s*)src_inst;
    const bool aliases = FALSE;

    unison_desc_s unison = bbsyn_unison_info[harmonics->unison_type];

    // if harmonic controls changed, rebuild the wave
    if (memcmp(harmonics->controls, harmonics->last_controls, sizeof(harmonics->controls))) {
        memcpy(harmonics->last_controls, harmonics->controls, sizeof(harmonics->controls));
        bbsyn_generate_harmonics(&src_inst->ctx->wavetables, harmonics->controls, 64, harmonics->wave);
    }

    wave_audio_render_callback(
        samples, frame_count,
        aliases, unison, harmonics->wave, HARMONICS_WAVE_LENGTH, harmonics->voices);
}









////////////
//  DATA  //
////////////

static const char *waveform_enum_values[BPBXSYN_CHIP_WAVE_COUNT] = {
    "rounded", "triangle", "square", "1/4 pulse", "1/8 pulse", "sawtooth",
    "double saw", "double pulse", "spiky", "sine", "flute", "harp",
    "sharp clarinet", "soft clarinet", "alto sax", "bassoon",
    "trumpet", "electric guitar", "organ", "pan flute", "glitch"
};

static const char *unison_enum_values[BPBXSYN_UNISON_COUNT] = {
    "none", "shimmer", "hum", "honky tonk", "dissonant",
    "fifth", "octave", "bowed", "piano", "warbled"
};

static const bpbxsyn_param_info_s chip_param_info[BPBXSYN_CHIP_PARAM_COUNT] = {
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "wvWavfrm",
        .name = "Wave",
        .group = "Chip",
        .min_value = 0,
        .max_value = BPBXSYN_CHIP_WAVE_COUNT - 1,
        .default_value = BPBXSYN_CHIP_WAVE_SQUARE,

        .enum_values = waveform_enum_values
    },

    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "wvUnison",
        .name = "Unison",
        .group = "Chip",
        .min_value = 0,
        .max_value = BPBXSYN_UNISON_COUNT - 1,
        .default_value = BPBXSYN_UNISON_NONE,

        .enum_values = unison_enum_values
    },
};

static const bpbxsyn_envelope_compute_index_e chip_env_targets[CHIP_MOD_COUNT] = {
    BPBXSYN_ENV_INDEX_UNISON
};

static const size_t chip_param_addresses[BPBXSYN_CHIP_PARAM_COUNT] = {
    offsetof(chip_inst_s, waveform),
    offsetof(chip_inst_s, unison_type)
};










/*
import subprocess

template = """{
    .type = BPBXSYN_PARAM_UINT8,
    .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

    .id = "hmHarm@",
    .name = "Harmonics #x",
    .group = "Harmonics",
    .min_value = 0,
    .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
    .default_value = 0.0,
},
"""

out = ""
for i in range(28):
    out += template.replace("#", str(i+1)).replace("@", str(i+1).zfill(2))

with subprocess.Popen("clip.exe", shell=True, stdin=subprocess.PIPE) as proc:
    proc.stdin.write(bytes(out, "utf-8"))
*/
static const bpbxsyn_param_info_s harmonics_param_info[] = {
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmUnison",
        .name = "Unison",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_UNISON_COUNT - 1,
        .default_value = BPBXSYN_UNISON_NONE,

        .enum_values = unison_enum_values
    },
    
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm01",
        .name = "Harmonics 1x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm02",
        .name = "Harmonics 2x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm03",
        .name = "Harmonics 3x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm04",
        .name = "Harmonics 4x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm05",
        .name = "Harmonics 5x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm06",
        .name = "Harmonics 6x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm07",
        .name = "Harmonics 7x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm08",
        .name = "Harmonics 8x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm09",
        .name = "Harmonics 9x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm10",
        .name = "Harmonics 10x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm11",
        .name = "Harmonics 11x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm12",
        .name = "Harmonics 12x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm13",
        .name = "Harmonics 13x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm14",
        .name = "Harmonics 14x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm15",
        .name = "Harmonics 15x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm16",
        .name = "Harmonics 16x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm17",
        .name = "Harmonics 17x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm18",
        .name = "Harmonics 18x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm19",
        .name = "Harmonics 19x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm20",
        .name = "Harmonics 20x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm21",
        .name = "Harmonics 21x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm22",
        .name = "Harmonics 22x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm23",
        .name = "Harmonics 23x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm24",
        .name = "Harmonics 24x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm25",
        .name = "Harmonics 25x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm26",
        .name = "Harmonics 26x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm27",
        .name = "Harmonics 27x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "hmHarm28",
        .name = "Harmonics 28x",
        .group = "Harmonics",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },

};

static const bpbxsyn_envelope_compute_index_e harmonics_env_targets[] = {
    BPBXSYN_ENV_INDEX_UNISON
};

/*
import subprocess

template = """    offsetof(harmonics_inst_s, controls[#]),
"""

out = ""
for i in range(28):
    out += template.replace("#", str(i))

with subprocess.Popen("clip.exe", shell=True, stdin=subprocess.PIPE) as proc:
    proc.stdin.write(bytes(out, "utf-8"))
*/
static const size_t harmonics_param_addresses[] = {
    offsetof(harmonics_inst_s, unison_type),
    offsetof(harmonics_inst_s, controls[0]),
    offsetof(harmonics_inst_s, controls[1]),
    offsetof(harmonics_inst_s, controls[2]),
    offsetof(harmonics_inst_s, controls[3]),
    offsetof(harmonics_inst_s, controls[4]),
    offsetof(harmonics_inst_s, controls[5]),
    offsetof(harmonics_inst_s, controls[6]),
    offsetof(harmonics_inst_s, controls[7]),
    offsetof(harmonics_inst_s, controls[8]),
    offsetof(harmonics_inst_s, controls[9]),
    offsetof(harmonics_inst_s, controls[10]),
    offsetof(harmonics_inst_s, controls[11]),
    offsetof(harmonics_inst_s, controls[12]),
    offsetof(harmonics_inst_s, controls[13]),
    offsetof(harmonics_inst_s, controls[14]),
    offsetof(harmonics_inst_s, controls[15]),
    offsetof(harmonics_inst_s, controls[16]),
    offsetof(harmonics_inst_s, controls[17]),
    offsetof(harmonics_inst_s, controls[18]),
    offsetof(harmonics_inst_s, controls[19]),
    offsetof(harmonics_inst_s, controls[20]),
    offsetof(harmonics_inst_s, controls[21]),
    offsetof(harmonics_inst_s, controls[22]),
    offsetof(harmonics_inst_s, controls[23]),
    offsetof(harmonics_inst_s, controls[24]),
    offsetof(harmonics_inst_s, controls[25]),
    offsetof(harmonics_inst_s, controls[26]),
    offsetof(harmonics_inst_s, controls[27]),
};






const inst_vtable_s bbsyn_inst_chip_vtable = {
    .struct_size = sizeof(chip_inst_s),

    .param_count = BPBXSYN_CHIP_PARAM_COUNT,
    .param_info = chip_param_info,
    .param_addresses = chip_param_addresses,

    .envelope_target_count = CHIP_MOD_COUNT,
    .envelope_targets = chip_env_targets,

    .inst_init = chip_init,
    .inst_note_on = chip_note_on,
    .inst_note_off = chip_note_off,
    .inst_note_all_off = chip_note_all_off,

    .inst_tick = chip_tick,
    .inst_run = chip_run
};

const inst_vtable_s bbsyn_inst_harmonics_vtable = {
    .struct_size = sizeof(harmonics_inst_s),

    .param_count = BPBXSYN_HARMONICS_PARAM_COUNT,
    .param_info = harmonics_param_info,
    .param_addresses = harmonics_param_addresses,

    .envelope_target_count = HARMONICS_MOD_COUNT,
    .envelope_targets = harmonics_env_targets,

    .inst_init = harmonics_init,
    .inst_note_on = harmonics_note_on,
    .inst_note_off = harmonics_note_off,
    .inst_note_all_off = harmonics_note_all_off,

    .inst_tick = harmonics_tick,
    .inst_run = harmonics_run
};