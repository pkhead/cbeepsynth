#include "pwm.h"

#include <assert.h>
#include <string.h>
#include "../util.h"

// It's actually closer to half of this, the synthesized pulse amplitude range
// is only .5 to -.5, but also note that the fundamental sine partial amplitude
// of a square wave is 4/π times the measured square wave amplitude.
#define BASE_EXPRESSION 0.04725

#define pulse_width_ratio(x)                                                   \
    ((double)(x) / (BPBXSYN_PULSE_WIDTH_RANGE * 2))

void bpbxsyn_synth_init_pwm(bpbxsyn_context_s *ctx, pwm_inst_s *inst) {
    *inst = (pwm_inst_s){0};
    inst_init(ctx, &inst->base, BPBXSYN_SYNTH_PULSE_WIDTH);

    inst->pulse_width_param[0] = BPBXSYN_PULSE_WIDTH_MAX;
    inst->pulse_width_param[1] = inst->pulse_width_param[0];
}

bpbxsyn_voice_id pwm_note_on(bpbxsyn_synth_s *p_inst, int key,
                              double velocity, int32_t length) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PULSE_WIDTH);
    pwm_inst_s *inst = (pwm_inst_s*)p_inst;

    bool continuation;
    bpbxsyn_voice_id id = trigger_voice(p_inst, GENERIC_LIST(inst->voices), key,
                                        velocity, length, &continuation);

    if (!continuation) {
        pwm_voice_s *voice = &inst->voices[id];
        *voice = (pwm_voice_s) {
            .base = voice->base
        };
    }

    return id;
}

void pwm_note_off(bpbxsyn_synth_s *p_inst, bpbxsyn_voice_id id) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PULSE_WIDTH);
    pwm_inst_s *inst = (pwm_inst_s*)p_inst;

    release_voice(p_inst, GENERIC_LIST(inst->voices), id);
}

void pwm_note_all_off(bpbxsyn_synth_s *p_inst) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PULSE_WIDTH);
    pwm_inst_s *inst = (pwm_inst_s*)p_inst;

    release_all_voices(p_inst, GENERIC_LIST(inst->voices));
}

static void compute_voice(const bpbxsyn_synth_s *const base_inst,
                          inst_base_voice_s *base_voice,
                          voice_compute_s *compute_data) {
    const pwm_inst_s *const inst = (pwm_inst_s*)base_inst;
    pwm_voice_s *voice = (pwm_voice_s*)base_voice;
    
    const double sample_len = compute_data->varying.sample_len;
    const double rounded_samples_per_tick =
        compute_data->varying.rounded_samples_per_tick;

    voice_compute_varying_s *const varying = &compute_data->varying;

    const double interval_start = compute_data->varying.interval_start;
    const double interval_end = compute_data->varying.interval_end;
    const double start_pitch = voice->base.current_key + interval_start;
    const double end_pitch = voice->base.current_key + interval_end;

    // pitch expression
    double pitch_expression_start;
    if (voice->has_prev_pitch_expression) {
        pitch_expression_start = voice->prev_pitch_expression;
    } else {
        pitch_expression_start = calc_pitch_expression(start_pitch);
    }
    const double pitch_expression_end = calc_pitch_expression(end_pitch);
    voice->has_prev_pitch_expression = true;
    voice->prev_pitch_expression = pitch_expression_end;

    const double expr_start =
        BASE_EXPRESSION * varying->expr_start * pitch_expression_start;
    const double expr_end =
        BASE_EXPRESSION * varying->expr_end * pitch_expression_end;

    const double start_freq = key_to_hz_d(start_pitch);
    const double end_freq = key_to_hz_d(end_pitch);

    voice->phase_delta = start_freq * sample_len;
    voice->phase_delta_scale =
        pow(end_freq / start_freq, 1.0 / rounded_samples_per_tick);
    
    voice->base.expression = expr_start;
    voice->base.expression_delta =
        (expr_end - expr_start) / rounded_samples_per_tick;
    
    // pulse width modulation calculation
    const double pulse_width_start =
        pulse_width_ratio(inst->pulse_width_param[0]) *
        voice->base.env_computer.envelope_starts[BPBXSYN_ENV_INDEX_PULSE_WIDTH];
    const double pulse_width_end =
        pulse_width_ratio(inst->pulse_width_param[1]) *
        voice->base.env_computer.envelope_ends[BPBXSYN_ENV_INDEX_PULSE_WIDTH];

    voice->pulse_width = pulse_width_start;
    voice->pulse_width_delta = (pulse_width_end - pulse_width_start) /
        rounded_samples_per_tick;
}

void pwm_tick(bpbxsyn_synth_s *p_inst, const bpbxsyn_tick_ctx_s *tick_ctx) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PULSE_WIDTH);
    pwm_inst_s *inst = (pwm_inst_s*)p_inst;

    inst_tick(p_inst, tick_ctx, &(audio_compute_s) {
        .voice_list = inst->voices,
        .sizeof_voice = sizeof(*inst->voices),
        .compute_voice = compute_voice,
        .userdata = NULL
    });

    inst->pulse_width_param[0] = inst->pulse_width_param[1];
}

void pwm_run(bpbxsyn_synth_s *p_inst, float *samples, size_t frame_count) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PULSE_WIDTH);
    pwm_inst_s *inst = (pwm_inst_s*)p_inst;

    memset(samples, 0, frame_count * sizeof(float));

    for (int i = 0; i < BPBXSYN_SYNTH_MAX_VOICES; ++i) {
        pwm_voice_s *voice = &inst->voices[i];
        if (!voice_is_computing(&voice->base)) continue;

        // pre
        double phase_delta = voice->phase_delta;
        const double phase_delta_scale = voice->phase_delta_scale;
        double expression = voice->base.expression;
        const double expression_delta = voice->base.expression_delta;

        double phase = fmod(voice->phase, 1.0);
        double pulse_width = voice->pulse_width;
        const double pulse_width_delta = voice->pulse_width_delta;

        double x1 = voice->base.note_filter_input[0];
        double x2 = voice->base.note_filter_input[1];

        for (size_t smp = 0; smp < frame_count; ++smp) {
            const double saw_phase_a = fmod(phase, 1.0);
            const double saw_phase_b = fmod(phase + pulse_width, 1);

            double pulse_wave = saw_phase_b - saw_phase_a;
            
            // This is a PolyBLEP, which smooths out discontinuities at any
            // frequency to reduce aliasing.
            if (!inst->aliases) {
                if (saw_phase_a < phase_delta) {
                    double t = saw_phase_a / phase_delta;
                    pulse_wave += (t + t - t * t - 1) * 0.5;
                } else if (saw_phase_a > 1.0 - phase_delta) {
                    double t = (saw_phase_a - 1.0) / phase_delta;
                    pulse_wave += (t + t + t * t + 1) * 0.5;
                }
                if (saw_phase_b < phase_delta) {
                    double t = saw_phase_b / phase_delta;
                    pulse_wave -= (t + t - t * t - 1) * 0.5;
                } else if (saw_phase_b > 1.0 - phase_delta) {
                    double t = (saw_phase_b - 1.0) / phase_delta;
                    pulse_wave -= (t + t + t * t + 1) * 0.5;
                }
            }

            const double x0 = pulse_wave;
            double sample = apply_filters(x0, x1, x2, voice->base.note_filters);
            x2 = x1;
            x1 = x0;

            phase += phase_delta;
            phase_delta *= phase_delta_scale;
            pulse_width += pulse_width_delta;

            const double output = sample * expression;
            expression += expression_delta;

            samples[smp] += (float)output;
        }

        // post
        voice->phase = phase;
        voice->phase_delta = phase_delta;
        voice->base.expression = expression;
        voice->pulse_width = pulse_width;

        sanitize_filters(voice->base.note_filters, FILTER_GROUP_COUNT);
        voice->base.note_filter_input[0] = x1;
        voice->base.note_filter_input[1] = x2;
    }
}











//////////
// DATA //
//////////

const bpbxsyn_param_info_s pwm_param_info[BPBXSYN_PULSE_WIDTH_PARAM_COUNT] = {
    {
        .type = BPBXSYN_PARAM_DOUBLE,
        .id = "pwmWidth",
        .group = "Pulse Width",
        .name = "Pulse Width",
        .min_value = BPBXSYN_PULSE_WIDTH_MIN,
        .max_value = BPBXSYN_PULSE_WIDTH_MAX,
        .default_value = BPBXSYN_PULSE_WIDTH_MAX,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "pwmAlias",
        .group = "Pulse Width",
        .name = "Aliases",
        .min_value = 0,
        .max_value = 1,
        .default_value = 0,
        .enum_values = yes_no_enum_values
    },
};

const size_t pwm_param_addresses[BPBXSYN_PULSE_WIDTH_PARAM_COUNT] = {
    offsetof(pwm_inst_s, pulse_width_param[1]),
    offsetof(pwm_inst_s, aliases),
};

static const bpbxsyn_envelope_compute_index_e pwm_env_targets[PWM_MOD_COUNT] = {
    BPBXSYN_ENV_INDEX_PULSE_WIDTH,
};

const inst_vtable_s inst_pwm_vtable = {
    .struct_size = sizeof(pwm_inst_s),

    .param_count = BPBXSYN_PULSE_WIDTH_PARAM_COUNT,
    .param_info = pwm_param_info,
    .param_addresses = pwm_param_addresses,

    .envelope_target_count = 1,
    .envelope_targets = pwm_env_targets,

    .inst_init = (inst_init_f)bpbxsyn_synth_init_pwm,
    .inst_note_on = pwm_note_on,
    .inst_note_off = pwm_note_off,
    .inst_note_all_off = pwm_note_all_off,

    .inst_tick = pwm_tick,
    .inst_run = pwm_run
};