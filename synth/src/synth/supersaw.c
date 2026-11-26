#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "../alloc.h"
#include "../log.h"
#include "../param_util.h"
#include "../util.h"
#include "pwm.h"
#include "synth.h"

// It's actually closer to half of this, the synthesized sawtooth amplitude
// range is only .5 to -.5.
#define BASE_EXPRESSION 0.061425

#define SUPERSAW_VOICE_COUNT 7
#define SUPERSAW_MOD_COUNT 4

#define DELAY_LINE_COUNT BBXS_SYNTH_MAX_VOICES

typedef struct supersaw_voice {
    inst_base_voice_s base;

    float *delay_line;
    int delay_index;

    double phases[SUPERSAW_VOICE_COUNT];
    double phase_delta;
    double phase_delta_scale;
    double dynamism;
    double dynamism_delta;
    double unison_detunes[SUPERSAW_VOICE_COUNT];
    double shape;
    double shape_delta;
    double delay_length;
    double delay_length_delta;

    double prev_pitch_expression;
    bool has_prev_pitch_expression;

    double prev_phase_delta;
    bool has_prev_phase_delta;
} supersaw_voice_s;

typedef struct supersaw_synth {
    bbxs_synth_s base;

    // [0] = prev
    // [1] = cur
    double dynamism[2];
    double spread[2];
    double shape[2];
    double pulse_width[2];

    bool aliases;

    prng_state_s prng_state;

    supersaw_voice_s voices[BBXS_SYNTH_MAX_VOICES];

    // TODO: what is a good size for a delay line?
    // a contiguous array of delay lines of size delay_line_size.
    float *delay_line_alloc;
    int delay_line_size;
} supersaw_synth_s;

static void supersaw_init(bbxs_context_s *ctx, bbxs_synth_s *p_inst) {
    supersaw_synth_s *synth = (supersaw_synth_s *)p_inst;
    *synth = (supersaw_synth_s){0};
    bbxs__inst_init(ctx, (void *)synth, BBXS_SYNTH_SUPERSAW);

    synth->prng_state = bbxs__random_seeded_state(bbxs__random(&ctx->prng));

    for (int i = 0; i < 2; ++i) {
        synth->dynamism[i] = BBXS_SUPERSAW_DYNAMISM_MAX;
        synth->spread[i] = BBXS_SUPERSAW_SPREAD_MAX / 2.0;
        synth->shape[i] = 0.0;
        synth->pulse_width[i] = BBXS_PULSE_WIDTH_MAX;
    }
    synth->aliases = false;
}

static void supersaw_destroy(bbxs_synth_s *p_inst) {
    assert(p_inst);
    assert(p_inst->type == BBXS_SYNTH_SUPERSAW);
    supersaw_synth_s *inst = (supersaw_synth_s *)p_inst;

    bbxs__free(inst->base.ctx, inst->delay_line_alloc);
}

static bbxs_voice_id supersaw_note_on(bbxs_synth_s *p_inst, int key,
                                      double velocity, int32_t length) {
    assert(p_inst);
    assert(p_inst->type == BBXS_SYNTH_SUPERSAW);
    supersaw_synth_s *inst = (supersaw_synth_s *)p_inst;

    bool continuation;
    bbxs_voice_id id =
        bbxs__trigger_voice(p_inst, GENERIC_LIST(inst->voices), key, velocity,
                            length, &continuation);

    if (!continuation) {
        supersaw_voice_s *voice = &inst->voices[id];
        voice->delay_index = -1;
        memset(voice->phases, 0, sizeof(voice->phases));
        voice->has_prev_pitch_expression = false;
        voice->has_prev_phase_delta = false;
    }

    return id;
}

static void supersaw_note_off(bbxs_synth_s *p_inst, bbxs_voice_id id) {
    assert(p_inst);
    assert(p_inst->type == BBXS_SYNTH_SUPERSAW);
    supersaw_synth_s *inst = (supersaw_synth_s *)p_inst;

    bbxs__release_voice(p_inst, GENERIC_LIST(inst->voices), id);
}

static void supersaw_note_all_off(bbxs_synth_s *p_inst) {
    assert(p_inst);
    assert(p_inst->type == BBXS_SYNTH_SUPERSAW);
    supersaw_synth_s *inst = (supersaw_synth_s *)p_inst;

    bbxs__release_all_voices(p_inst, GENERIC_LIST(inst->voices));
}

static void supersaw_sample_rate_changed(bbxs_synth_s *p_inst, double old,
                                         double new) {
    if (new == old) return;

    assert(p_inst);
    assert(p_inst->type == BBXS_SYNTH_SUPERSAW);
    supersaw_synth_s *inst = (supersaw_synth_s *)p_inst;
    const bbxs_context_s *const ctx = inst->base.ctx;

    if (inst->delay_line_alloc) bbxs__free(ctx, inst->delay_line_alloc);

    // The delay line buffer will get reused for other tones so might as well
    // start off with a buffer size that is big enough for most notes.
    // const int likely_maximum_length = (int) ceil(4 * new /
    // key_to_hz_d(12.0)); int dl_size =
    // bbxs__fitting_power_of_two(likely_maximum_length);

    // the min_buffer_length of MIDI E(-1) is 4660; i think the intended value
    // of likelyMaximumLength (16,384) is higher than reasonable. and I should
    // prefer conserving memory.
    int dl_size = 8192;

    float *dl_alloc =
        bbxs__malloc(ctx, DELAY_LINE_COUNT * dl_size * sizeof(float));
    if (!dl_alloc) {
        bbxs__logmsgf(ctx, BBXS_LOG_ERROR,
                      "supersaw could not allocate delay lines!");

        // umm i don't know how the program would recover from this. uhhh.
        inst->delay_line_alloc = NULL;
        inst->delay_line_size = 0;
        return;
    }

    bbxs__logmsgf(ctx, BBXS_LOG_DEBUG,
                  "allocated %zu bytes of delay line buffers",
                  (size_t)dl_size * DELAY_LINE_COUNT * sizeof(float));

    memset(dl_alloc, 0, DELAY_LINE_COUNT * dl_size * sizeof(float));

    inst->delay_line_size = dl_size;
    inst->delay_line_alloc = dl_alloc;
    for (int i = 0; i < BBXS_SYNTH_MAX_VOICES; ++i) {
        inst->voices[i].delay_line = dl_alloc;
        dl_alloc += dl_size;
    }
}

// TODO: don't make base_inst in compute_voice signature a constant
static void compute_voice(const bbxs_synth_s *const base_inst,
                          inst_base_voice_s *base_voice,
                          voice_compute_s *compute_data) {
    supersaw_synth_s *const inst = (supersaw_synth_s *)base_inst;
    supersaw_voice_s *voice = (supersaw_voice_s *)base_voice;
    const envelope_computer_s *envc = &voice->base.env_computer;

    const double sample_len = compute_data->varying.sample_len;
    const double rounded_samples_per_tick =
        compute_data->varying.rounded_samples_per_tick;

    voice_compute_varying_s *const varying = &compute_data->varying;

    const double interval_start = compute_data->varying.interval_start;
    const double interval_end = compute_data->varying.interval_end;
    const double start_pitch = voice->base.current_key + interval_start;
    const double end_pitch = voice->base.current_key + interval_end;

    const double start_freq = key_to_hz_d(start_pitch);
    const double end_freq = key_to_hz_d(end_pitch);

    const double interval_scale = 1.0; // 6.0 if this is a noise channel
    const double freqEndRatio =
        pow(2.0, (interval_end - interval_start) * interval_scale / 12.0);

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

    double supersaw_expr_start = 1.0;
    double supersaw_expr_end = 1.0;

    double min_first_voice_amp = 1.0 / sqrt(SUPERSAW_VOICE_COUNT);

    double use_dynamism_start = inst->dynamism[0] / BBXS_SUPERSAW_DYNAMISM_MAX;
    double use_dynamism_end = inst->dynamism[1] / BBXS_SUPERSAW_DYNAMISM_MAX;

    double curved_dynamism_start =
        1.0 - pow(max(0.0, 1.0 - use_dynamism_start *
                                     envc->envelope_starts
                                         [BBXS_ENV_INDEX_SUPERSAW_DYNAMISM]),
                  0.2);
    double curved_dynamism_end =
        1.0 -
        pow(max(0.0,
                1.0 -
                    use_dynamism_end *
                        envc->envelope_ends[BBXS_ENV_INDEX_SUPERSAW_DYNAMISM]),
            0.2);
    double first_voice_amp_start =
        pow(2.0, log2(min_first_voice_amp) * curved_dynamism_start);
    double first_voice_amp_end =
        pow(2.0, log2(min_first_voice_amp) * curved_dynamism_end);

    double dynamism_start = sqrt((1.0 / pow(first_voice_amp_start, 2.0) - 1.0) /
                                 (SUPERSAW_VOICE_COUNT - 1.0));
    double dynamism_end = sqrt((1.0 / pow(first_voice_amp_end, 2.0) - 1.0) /
                               (SUPERSAW_VOICE_COUNT - 1.0));

    voice->dynamism = dynamism_start;
    voice->dynamism_delta =
        (dynamism_end - dynamism_start) / rounded_samples_per_tick;

    bool init_supersaw = (voice->delay_index == -1);
    if (init_supersaw) {
        // Goal: generate sawtooth phases such that the combined initial
        // amplitude cancel out to minimize pop. Algorithm: generate sorted
        // phases, iterate over their sawtooth drop points to find a combined
        // zero crossing, then offset the phases so they start there.

        // Generate random phases in ascending order by adding positive randomly
        // sized gaps between adjacent phases. For a proper distribution of
        // random events, the gaps sizes should be an "exponential
        // distribution", which is just: -Math.log(Math.random()). At the end,
        // normalize the phases to a 0-1 range by dividing by the final value of
        // the accumulator.
        double accumulator = 0.0;
        for (int i = 0; i < SUPERSAW_VOICE_COUNT; i++) {
            voice->phases[i] = accumulator;
            accumulator += -log(bbxs__frandom(&inst->prng_state));
        }

        double amplitude_sum =
            1.0 + (SUPERSAW_VOICE_COUNT - 1.0) * dynamism_start;
        double slope = amplitude_sum;

        // Find the initial amplitude of the sum of sawtooths with the
        // normalized set of phases.
        double sample = 0.0;
        for (int i = 0; i < SUPERSAW_VOICE_COUNT; i++) {
            double amplitude = (i == 0) ? 1.0 : dynamism_start;
            double normalized_phase = voice->phases[i] / accumulator;
            voice->phases[i] = normalized_phase;
            sample += (normalized_phase - 0.5) * amplitude;
        }

        // Find the phase of the zero crossing of the sum of the sawtooths. You
        // can use a constant slope and the distance between sawtooth drops to
        // determine if the zero crossing occurs between them. Note that a small
        // phase means that the corresponding drop for that wave is far away,
        // and a big phase means the drop is nearby, so to iterate forward
        // through the drops we iterate backward through the phases.
        double zero_crossing_phase = 1.0;
        double prev_drop = 0.0;
        for (int i = SUPERSAW_VOICE_COUNT - 1; i >= 0; i--) {
            double next_drop = 1.0 - voice->phases[i];
            double phase_delta = next_drop - prev_drop;
            if (sample < 0.0) {
                double dist_to_zero_crossing = -sample / slope;
                if (dist_to_zero_crossing < phase_delta) {
                    zero_crossing_phase = prev_drop + dist_to_zero_crossing;
                    break;
                }
            }
            double amplitude = (i == 0) ? 1.0 : dynamism_start;
            sample += phase_delta * slope - amplitude;
            prev_drop = next_drop;
        }
        for (int i = 0; i < SUPERSAW_VOICE_COUNT; i++) {
            voice->phases[i] += zero_crossing_phase;
        }

        // Randomize the (initially sorted) order of the phases (aside from the
        // first one) so that they don't correlate to the detunes that are also
        // based on index.
        for (int i = 1; i < SUPERSAW_VOICE_COUNT - 1; i++) {
            int swapped_index =
                i + (int)floor(bbxs__frandom(&inst->prng_state) *
                               (SUPERSAW_VOICE_COUNT - i));
            double temp = voice->phases[i];
            voice->phases[i] = voice->phases[swapped_index];
            voice->phases[swapped_index] = temp;
        }
    }

    // Spread mods
    double use_spread_start = inst->spread[0] / BBXS_SUPERSAW_SPREAD_MAX;
    double use_spread_end = inst->spread[1] / BBXS_SUPERSAW_SPREAD_MAX;

    double spread_slider_start =
        use_spread_start *
        envc->envelope_starts[BBXS_ENV_INDEX_SUPERSAW_SPREAD];
    double spread_slider_end =
        use_spread_end * envc->envelope_ends[BBXS_ENV_INDEX_SUPERSAW_SPREAD];

    // Just use the average detune for the current tick in the below loop.
    double average_spread_slider =
        (spread_slider_start + spread_slider_end) * 0.5;
    double curved_spread =
        pow(1.0 - sqrt(max(0.0, 1.0 - average_spread_slider)), 1.75);
    for (int i = 0; i < SUPERSAW_VOICE_COUNT; i++) {
        // Spread out the detunes around the center
        // pkhead: i know i didn't bother splitting up the other long formulas
        // but this one was just so complicated. so when clang-format wraps it
        // to 80 columns it's illegible.
        double offset;
        if (i == 0) {
            offset = 0.0;
        } else {
            double spread = (((i + 1) >> 1) - 0.5 + 0.025 * ((i & 2) - 1));
            double factor = ((i & 1) * 2 - 1);
            offset = pow(spread / (SUPERSAW_VOICE_COUNT >> 1), 1.1) * factor;
        }

        voice->unison_detunes[i] = pow(2.0, curved_spread * offset / 12.0);
    }

    // Saw shape mods
    double use_shape_start =
        (inst->shape[0] / BBXS_SUPERSAW_SHAPE_MAX) *
        envc->envelope_starts[BBXS_ENV_INDEX_SUPERSAW_SHAPE];
    double use_shape_end = (inst->shape[1] / BBXS_SUPERSAW_SHAPE_MAX) *
                           envc->envelope_ends[BBXS_ENV_INDEX_SUPERSAW_SHAPE];

    double shape_start =
        use_shape_start *
        envc->envelope_starts[BBXS_ENV_INDEX_SUPERSAW_SHAPE]; // sic
    double shape_end =
        use_shape_end *
        envc->envelope_ends[BBXS_ENV_INDEX_SUPERSAW_SHAPE]; // sic
    voice->shape = shape_start;
    voice->shape_delta = (shape_end - shape_start) / rounded_samples_per_tick;

    // Check for PWM mods to this instrument
    double pulse_width_mod_start = pulse_width_ratio(inst->pulse_width[0]);
    double pulse_width_mod_end = pulse_width_ratio(inst->pulse_width[1]);

    double pulse_width_start =
        pulse_width_mod_start *
        envc->envelope_starts[BBXS_ENV_INDEX_PULSE_WIDTH];
    double pulse_width_end =
        pulse_width_mod_end * envc->envelope_ends[BBXS_ENV_INDEX_PULSE_WIDTH];
    double phase_delta_start = (voice->has_prev_phase_delta)
                                   ? voice->prev_phase_delta
                                   : start_freq * sample_len;
    double phase_delta_end = start_freq * sample_len * freqEndRatio;
    voice->prev_phase_delta = phase_delta_end;
    voice->has_prev_phase_delta = true;

    double delay_length_start = pulse_width_start / phase_delta_start;
    double delay_length_end = pulse_width_end / phase_delta_end;
    voice->delay_length = delay_length_start;
    voice->delay_length_delta =
        (delay_length_end - delay_length_start) / rounded_samples_per_tick;
    int min_buffer_length =
        (int)ceil(max(delay_length_start, delay_length_end)) + 2;
    assert(inst->delay_line_size > min_buffer_length);
    (void)min_buffer_length;
    // TODO: what do i do if min_buffer_length <= delay_line_size ?

    if (init_supersaw) {
        for (int i = 0; i < inst->delay_line_size; i++)
            voice->delay_line[i] = 0.0f;
        voice->delay_index = inst->delay_line_size;
    }

    double pulse_expression_ratio = PWM_BASE_EXPRESSION / BASE_EXPRESSION;
    supersaw_expr_start *=
        (1.0 + (pulse_expression_ratio - 1.0) * shape_start) /
        sqrt(1.0 +
             (SUPERSAW_VOICE_COUNT - 1.0) * dynamism_start * dynamism_start);
    supersaw_expr_end *=
        (1.0 + (pulse_expression_ratio - 1.0) * shape_end) /
        sqrt(1.0 + (SUPERSAW_VOICE_COUNT - 1.0) * dynamism_end * dynamism_end);

    double expression_start = BASE_EXPRESSION * varying->expr_start *
                              pitch_expression_start * supersaw_expr_start;
    double expression_end = BASE_EXPRESSION * varying->expr_end *
                            pitch_expression_end * supersaw_expr_end;

    voice->base.expression = expression_start;
    voice->base.expression_delta =
        (expression_end - expression_start) / rounded_samples_per_tick;

    voice->phase_delta = start_freq * sample_len;
    voice->phase_delta_scale =
        pow(end_freq / start_freq, 1.0 / rounded_samples_per_tick);
}

static void supersaw_tick(bbxs_synth_s *p_inst,
                          const bbxs_tick_ctx_s *tick_ctx) {
    assert(p_inst);
    assert(p_inst->type == BBXS_SYNTH_SUPERSAW);
    supersaw_synth_s *inst = (supersaw_synth_s *)p_inst;

    bbxs__inst_tick(&inst->base, tick_ctx,
                    &(audio_compute_s){.voice_list = inst->voices,
                                       .sizeof_voice = sizeof(*inst->voices),
                                       .compute_voice = compute_voice,
                                       .userdata = NULL});

    inst->dynamism[0] = inst->dynamism[1];
    inst->spread[0] = inst->spread[1];
    inst->shape[0] = inst->shape[1];
    inst->pulse_width[0] = inst->pulse_width[1];
}

static void supersaw_run(bbxs_synth_s *p_inst, float *samples,
                         size_t frame_count) {
    assert(p_inst);
    assert(p_inst->type == BBXS_SYNTH_SUPERSAW);
    supersaw_synth_s *inst = (supersaw_synth_s *)p_inst;

    memset(samples, 0, frame_count * sizeof(float));

    const int voice_count = SUPERSAW_VOICE_COUNT;

    for (int i = 0; i < BBXS_SYNTH_MAX_VOICES; ++i) {
        supersaw_voice_s *voice = &inst->voices[i];
        if (!voice_is_computing(&voice->base)) continue;

        double phase_delta = voice->phase_delta;
        const double phase_delta_scale = voice->phase_delta_scale;
        double expression = voice->base.expression;
        const double expression_delta = voice->base.expression_delta;
        double *phases = voice->phases;

        double dynamism = voice->dynamism;
        const double dynamism_delta = voice->dynamism_delta;
        const double *unison_detunes = voice->unison_detunes;
        double shape = voice->shape;
        const double shape_delta = voice->shape_delta;
        double delayLength = voice->delay_length;
        const double delay_length_delta = voice->delay_length_delta;
        float *const delay_line = voice->delay_line;
        const int delay_buffer_mask = inst->delay_line_size - 1;
        assert(is_power_of_two(delay_buffer_mask + 1));
        int delay_index = voice->delay_index;
        delay_index = (delay_index & delay_buffer_mask) + inst->delay_line_size;

        double x1 = voice->base.note_filter_input[0];
        double x2 = voice->base.note_filter_input[1];

        for (size_t smp = 0; smp < frame_count; ++smp) {
            // The phase initially starts at a zero crossing so apply
            // the delta before first sample to get a nonzero value.
            double phase = fmod((phases[0] + phase_delta), 1.0);
            double supersaw_sample =
                phase - 0.5 * (1.0 + (voice_count - 1.0) * dynamism);

            // This is a PolyBLEP, which smooths out discontinuities at any
            // frequency to reduce aliasing.
            if (!inst->aliases) {
                if (phase < phase_delta) {
                    double t = phase / phase_delta;
                    supersaw_sample -= (t + t - t * t - 1) * 0.5;
                } else if (phase > 1.0 - phase_delta) {
                    double t = (phase - 1.0) / phase_delta;
                    supersaw_sample -= (t + t + t * t + 1) * 0.5;
                }
            }

            phases[0] = phase;

            for (int i = 1; i < voice_count; i++) {
                const double detuned_phase_delta =
                    phase_delta * unison_detunes[i];
                // The phase initially starts at a zero crossing so apply
                // the delta before first sample to get a nonzero value.
                double phase = fmod((phases[i] + detuned_phase_delta), 1.0);
                supersaw_sample += phase * dynamism;

                // This is a PolyBLEP, which smooths out discontinuities at any
                // frequency to reduce aliasing.
                if (!inst->aliases) {
                    if (phase < detuned_phase_delta) {
                        const double t = phase / detuned_phase_delta;
                        supersaw_sample -= (t + t - t * t - 1) * 0.5 * dynamism;
                    } else if (phase > 1.0 - detuned_phase_delta) {
                        const double t = (phase - 1.0) / detuned_phase_delta;
                        supersaw_sample -= (t + t + t * t + 1) * 0.5 * dynamism;
                    }
                }

                phases[i] = phase;
            }

            delay_line[delay_index & delay_buffer_mask] =
                (float)supersaw_sample;
            const double delay_sample_time = delay_index - delayLength;
            const int lower_index = (int)delay_sample_time;
            const int upper_index = lower_index + 1;
            const double delay_ratio = delay_sample_time - lower_index;
            const double prev_delay_sample =
                delay_line[lower_index & delay_buffer_mask];
            const double next_delay_sample =
                delay_line[upper_index & delay_buffer_mask];
            const double delay_sample =
                prev_delay_sample +
                (next_delay_sample - prev_delay_sample) * delay_ratio;
            delay_index++;

            const double x0 = supersaw_sample - delay_sample * shape;
            double sample =
                bbxs__apply_filters(x0, x1, x2, voice->base.note_filters);
            x2 = x1;
            x1 = x0;

            phase_delta *= phase_delta_scale;
            dynamism += dynamism_delta;
            shape += shape_delta;
            delayLength += delay_length_delta;

            const double output = sample * expression;
            expression += expression_delta;

            samples[smp] += (float)output;
        }

        voice->phase_delta = phase_delta;
        voice->base.expression = expression;
        voice->dynamism = dynamism;
        voice->shape = shape;
        voice->delay_length = delayLength;
        voice->delay_index = delay_index;

        bbxs__sanitize_filters(voice->base.note_filters, FILTER_GROUP_COUNT);
        voice->base.note_filter_input[0] = x1;
        voice->base.note_filter_input[1] = x2;
    }
}

//////////
// DATA //
//////////

static const bbxs_param_info_s param_info[BBXS_SUPERSAW_PARAM_COUNT] = {
    {
        .type = BBXS_PARAM_DOUBLE,
        .id = "sswDynan",
        .group = "Supersaw",
        .name = "Dynamism",
        .min_value = 0.0,
        .max_value = BBXS_SUPERSAW_DYNAMISM_MAX,
        .default_value = BBXS_SUPERSAW_DYNAMISM_MAX,
    },
    {
        .type = BBXS_PARAM_DOUBLE,
        .id = "sswSpred",
        .group = "Supersaw",
        .name = "Spread",
        .min_value = 0.0,
        .max_value = BBXS_SUPERSAW_SPREAD_MAX,
        .default_value = BBXS_SUPERSAW_SPREAD_MAX / 2.0,
    },
    {
        .type = BBXS_PARAM_DOUBLE,
        .id = "sswShape",
        .group = "Supersaw",
        .name = "Saw↔Pulse", // should be UTF-8
        .min_value = 0.0,
        .max_value = BBXS_SUPERSAW_SHAPE_MAX,
        .default_value = 0.0,
    },
    {
        .type = BBXS_PARAM_DOUBLE,
        .id = "sswWidth",
        .group = "Supersaw",
        .name = "Pulse Width",
        .min_value = BBXS_PULSE_WIDTH_MIN,
        .max_value = BBXS_PULSE_WIDTH_MAX,
        .default_value = BBXS_PULSE_WIDTH_MAX,
    },
    {
        .type = BBXS_PARAM_UINT8,
        .flags = BBXS_PARAM_FLAG_NO_AUTOMATION,
        .id = "sswAlias",
        .group = "Pulse Width",
        .name = "Aliases?",
        .min_value = 0,
        .max_value = 1,
        .default_value = 0,
        .enum_values = bbxs__yes_no_enum_values,
    },
};

static const size_t param_addresses[BBXS_SUPERSAW_PARAM_COUNT] = {
    offsetof(supersaw_synth_s, dynamism[1]),
    offsetof(supersaw_synth_s, spread[1]),
    offsetof(supersaw_synth_s, shape[1]),
    offsetof(supersaw_synth_s, pulse_width[1]),
    offsetof(supersaw_synth_s, aliases),
};

static const bbxs_envelope_compute_index_e env_targets[SUPERSAW_MOD_COUNT] = {
    BBXS_ENV_INDEX_PULSE_WIDTH,
    BBXS_ENV_INDEX_SUPERSAW_DYNAMISM,
    BBXS_ENV_INDEX_SUPERSAW_SPREAD,
    BBXS_ENV_INDEX_SUPERSAW_SHAPE,
};

const synth_vtable_s bbxs__synth_supersaw_vtable = {
    .struct_size = sizeof(supersaw_synth_s),

    .param_count = BBXS_SUPERSAW_PARAM_COUNT,
    .param_info = param_info,
    .param_addresses = param_addresses,

    .envelope_target_count = SUPERSAW_MOD_COUNT,
    .envelope_targets = env_targets,

    .inst_init = supersaw_init,
    .inst_destroy = supersaw_destroy,
    .inst_note_on = supersaw_note_on,
    .inst_note_off = supersaw_note_off,
    .inst_note_all_off = supersaw_note_all_off,
    .inst_sample_rate_changed = supersaw_sample_rate_changed,

    .inst_tick = supersaw_tick,
    .inst_run = supersaw_run,
};