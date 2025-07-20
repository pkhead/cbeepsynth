#include "chip.h"

#include <assert.h>
#include "util.h"
#include "wavetables.h"

#define VOICE_BASE_EXPRESSION 0.03375

void chip_init(chip_inst_s *inst) {
    *inst = (chip_inst_s){0};
    inst_init(&inst->base, BPBX_INSTRUMENT_CHIP);

    for (int i = 0; i < BPBX_INST_MAX_VOICES; i++) {
        inst->voices[i].base.active = FALSE;
        inst->voices[i].base.triggered = FALSE;
    }

    inst->waveform = 2;
    inst->unison_type = 0;
}

int chip_midi_on(bpbx_inst_s *inst, int key, int velocity) {
    assert(inst);
    assert(inst->type == BPBX_INSTRUMENT_CHIP);
    chip_inst_s *const chip = (chip_inst_s*)inst;

    int voice_index = trigger_voice(inst, chip->voices, sizeof(*chip->voices), key, velocity);
    chip_voice_s *voice = &chip->voices[voice_index];

    return voice_index;
}

void chip_midi_off(bpbx_inst_s *inst, int key, int velocity) {
    assert(inst);
    assert(inst->type == BPBX_INSTRUMENT_CHIP);
    chip_inst_s *const chip = (chip_inst_s*)inst;

    release_voice(inst, chip->voices, sizeof(*chip->voices), key, velocity);
}

static void compute_chip_voice(const bpbx_inst_s *const base_inst, inst_base_voice_s *voice, voice_compute_s *compute_data) {
    const chip_inst_s *const inst = (chip_inst_s*) base_inst;
    chip_voice_s *const chip_voice = (chip_voice_s*) voice;

    const double sample_len = compute_data->varying.sample_len;
    const double samples_per_tick = compute_data->varying.samples_per_tick;
    const double rounded_samples_per_tick = compute_data->varying.rounded_samples_per_tick;

    voice_compute_varying_s *const varying = &compute_data->varying;

    wavetable_desc_s wavetable = chip_wavetables[inst->waveform];
    const unison_desc_s unison = unison_info[inst->unison_type];

    double settings_expression_mult = VOICE_BASE_EXPRESSION * wavetable.expression;
    settings_expression_mult *= unison.expression * unison.voices / 2.0;

    const double interval_start = compute_data->varying.interval_start;
    const double interval_end = compute_data->varying.interval_end;
    const double start_pitch = (double)voice->key + interval_start;
    const double end_pitch = (double)voice->key + interval_end;

    // pitch expression
    double pitch_expression_start;
    if (chip_voice->has_prev_pitch_expression) {
        pitch_expression_start = chip_voice->prev_pitch_expression;
    } else {
        pitch_expression_start = calc_pitch_expression(start_pitch);
    }
    const double pitch_expression_end = calc_pitch_expression(end_pitch);
    chip_voice->has_prev_pitch_expression = TRUE;
    chip_voice->prev_pitch_expression = pitch_expression_end;

    // calculate final expression
    const double expr_start = VOICE_BASE_EXPRESSION * varying->expr_start * settings_expression_mult * pitch_expression_start;
    const double expr_end = VOICE_BASE_EXPRESSION * varying->expr_end * settings_expression_mult * pitch_expression_end;
    
    const double unison_env_start = voice->env_computer.envelope_starts[BPBX_ENV_INDEX_UNISON];
    const double unison_env_end = voice->env_computer.envelope_ends[BPBX_ENV_INDEX_UNISON];

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
        chip_voice->phase_delta[i] = start_freq * sample_len * unison_starts[i];
        chip_voice->phase_delta_scale[i] = base_phase_delta_scale * pow(unison_ends[i] / unison_starts[i], 1.0 / rounded_samples_per_tick);
    }
    
    voice->expression = expr_start;
    voice->expression_delta = (expr_end - expr_start) / rounded_samples_per_tick;
}

static void audio_render_callback(
    float *output_buffer, size_t frames_to_compute,
    double inst_volume, void *userdata_ptr
) {
    chip_inst_s *const chip = userdata_ptr;
    const uint8_t aliases = FALSE;

    wavetable_desc_s wavetable;
    if (aliases) {
        wavetable = raw_chip_wavetables[chip->waveform];
    } else {
        wavetable = chip_wavetables[chip->waveform];
    }

    unison_desc_s unison = unison_info[chip->unison_type];
    assert(UNISON_MAX_VOICES == 2);
    assert(unison.voices <= UNISON_MAX_VOICES);

    const double *const wave = wavetable.values;
    const size_t wave_length = wavetable.length -1 ;

    for (int i = 0; i < BPBX_INST_MAX_VOICES; i++) {
        chip_voice_s *voice = chip->voices + i;
        if (!voice->base.active) continue;
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
                prev_wave_integral[i] = wave[index];
                prev_wave_integral[i] += (wave[index + 1] - prev_wave_integral[i]) * phase_ratio;
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
                    x0[i] = wave[(int)phase[i] % wave_length];
                } else {
                    const int phase_int = (int)phase[i];
                    const int index = phase_int % wave_length;
                    double next_wave_integral = wave[index];
                    const double phase_ratio = phase[i] - phase_int;
                    next_wave_integral += (wave[index + 1] - next_wave_integral) * phase_ratio;
                    x0[i] = (next_wave_integral - prev_wave_integral[i]) / phase_delta[i];
                    prev_wave_integral[i] = next_wave_integral;
                }

                x0[i] *= wavetable.expression * inst_volume * voice->base.expression * voice->base.volume;

                phase_delta[i] *= voice->phase_delta_scale[i];
            }

            double x0_sum;
            if (!aliases)
                x0_sum = x0[0] + x0[1] * unison.sign;

            float final_sample;
            if (voice->base.filters_enabled) {
                final_sample = (float) apply_filters(x0_sum, x1, x2, voice->base.note_filters);
            } else {
                final_sample = (float) x0_sum;
            }

            x2 = x1;
            x1 = x0_sum;

            voice->base.expression += voice->base.expression_delta;

            // output frame
            *out++ += final_sample;
            *out++ += final_sample;
        }

        voice->base.note_filter_input[0] = x1;
        voice->base.note_filter_input[1] = x2;

        for (int i = 0; i < UNISON_MAX_VOICES; i++) {
            voice->phase[i] = phase[i] / wave_length;
            voice->phase_delta[i] = phase_delta[i] / wave_length;
        }
        
        // process this frame
        // double x0 = (algo_func(voice, voice->feedback_mult) * voice->base.expression * inst_volume) * voice->base.volume;
        // double x1 = voice->base.note_filter_input[0];
        // double x2 = voice->base.note_filter_input[1];
        
        // float sample;
        // if (voice->base.filters_enabled) {
        //     sample = (float) apply_filters(x0, x1, x2, voice->base.note_filters);
        // } else {
        //     sample = (float) x0;
        // }

        // x2 = x1;
        // x1 = x0;

        // voice->base.note_filter_input[0] = x1;
        // voice->base.note_filter_input[1] = x2;
        
        // convert from operable values
        // for (int op = 0; op < FM_OP_COUNT; op++) {
        //     voice->op_states[op].phase /= SINE_WAVE_LENGTH;
        //     voice->op_states[op].phase_delta /= SINE_WAVE_LENGTH;
        // }
    }
}

void chip_run(bpbx_inst_s *src_inst, const bpbx_run_ctx_s *const run_ctx) {
    assert(src_inst);
    assert(src_inst->type == BPBX_INSTRUMENT_CHIP);

    chip_inst_s *const chip = (chip_inst_s*)src_inst;

    inst_audio_process(src_inst, run_ctx, &(audio_compute_s) {
        .voice_list = chip->voices,
        .sizeof_voice = sizeof(*chip->voices),

        .compute_voice = compute_chip_voice,
        .render_block = audio_render_callback,

        .userdata = chip
    });
}

































////////////
//  DATA  //
////////////

static const char *waveform_enum_values[BPBX_CHIP_WAVE_COUNT] = {
    "rounded", "triangle", "square", "1/4 pulse", "1/8 pulse", "sawtooth",
    "double saw", "double pulse", "spiky", "sine", "flute", "harp",
    "sharp clarinet", "soft clarinet", "alto sax", "bassoon",
    "trumpet", "electric guitar", "organ", "pan flute", "glitch"
};

static const char *unison_enum_values[BPBX_UNISON_COUNT] = {
    "none", "shimmer", "hum", "honky tonk", "dissonant",
    "fifth", "octave", "bowed", "piano", "warbled"
};

const bpbx_inst_param_info_s chip_param_info[BPBX_CHIP_PARAM_COUNT] = {
    {
        .type = BPBX_PARAM_UINT8,
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .name = "Wave",
        .group = "Chip",
        .min_value = 0,
        .max_value = BPBX_CHIP_WAVE_COUNT - 1,
        .default_value = BPBX_CHIP_WAVE_SQUARE,

        .enum_values = waveform_enum_values
    },

    {
        .type = BPBX_PARAM_UINT8,
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .name = "Unison",
        .group = "Chip",
        .min_value = 0,
        .max_value = BPBX_UNISON_COUNT - 1,
        .default_value = BPBX_UNISON_NONE,

        .enum_values = unison_enum_values
    },
};

const bpbx_envelope_compute_index_e chip_env_targets[CHIP_MOD_COUNT] = {
    BPBX_ENV_INDEX_UNISON
};

const size_t chip_param_addresses[BPBX_CHIP_PARAM_COUNT] = {
    offsetof(chip_inst_s, waveform),
    offsetof(chip_inst_s, unison_type)
};