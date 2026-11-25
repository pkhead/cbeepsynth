#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "../param_util.h"
#include "../util.h"
#include "synth.h"


// It's actually closer to half of this, the synthesized sawtooth amplitude
// range is only .5 to -.5.
#define BASE_EXPRESSION 0.061425

#define SUPERSAW_VOICE_COUNT 7
#define SUPERSAW_MOD_COUNT 4

typedef struct supersaw_voice {
    inst_base_voice_s base;

    float *delay_line;
    int delay_index;

    double phases[SUPERSAW_VOICE_COUNT];

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
} supersaw_synth_s;

static void supersaw_init(bbxs_context_s *ctx, bbxs_synth_s *p_inst) {
    supersaw_synth_s *synth = (supersaw_synth_s *)p_inst;
    *synth = (supersaw_synth_s){0};
    bbxs__inst_init(ctx, (void *)synth, BBXS_SYNTH_SUPERSAW);

    for (int i = 0; i < 2; ++i) {
        synth->dynamism[i] = BBXS_SUPERSAW_DYNAMISM_MAX;
        synth->spread[i] = BBXS_SUPERSAW_SPREAD_MAX / 2.0;
        synth->shape[i] = 0.0;
        synth->pulse_width[i] = BBXS_PULSE_WIDTH_MAX;
    }
    synth->aliases = false;
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
        *voice = (supersaw_voice_s){.base = voice->base};
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
    const double freqEndRatio = pow(2.0, (interval_end - interval_start) * interval_scale / 12.0);

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

    // TODO: make expressionStart and expressionEnd variables earlier and modify
    // those instead of these supersawExpression variables.
    double supersawExpressionStart = 1.0;
    double supersawExpressionEnd = 1.0;

    double minFirstVoiceAmplitude = 1.0 / sqrt(SUPERSAW_VOICE_COUNT);
				
    double useDynamismStart = inst->dynamism[0] / BBXS_SUPERSAW_DYNAMISM_MAX;
    double useDynamismEnd = inst->dynamism[1] / BBXS_SUPERSAW_DYNAMISM_MAX;

    double curvedDynamismStart = 1.0 - pow(max(0.0, 1.0 - useDynamismStart * envc->envelope_starts[BBXS_ENV_INDEX_SUPERSAW_DYNAMISM]), 0.2);
    double curvedDynamismEnd = 1.0 - pow(max(0.0, 1.0 - useDynamismEnd * envc->envelope_ends[BBXS_ENV_INDEX_SUPERSAW_DYNAMISM]), 0.2);
    double firstVoiceAmplitudeStart = pow(2.0, log2(minFirstVoiceAmplitude) * curvedDynamismStart);
    double firstVoiceAmplitudeEnd = pow(2.0, log2(minFirstVoiceAmplitude) * curvedDynamismEnd);

    double dynamismStart = sqrt((1.0 / pow(firstVoiceAmplitudeStart, 2.0) - 1.0) / (SUPERSAW_VOICE_COUNT - 1.0));
    double dynamismEnd = sqrt((1.0 / pow(firstVoiceAmplitudeEnd, 2.0) - 1.0) / (SUPERSAW_VOICE_COUNT - 1.0));

    voice->dynamism = dynamismStart;
    voice->dynamism_delta = (dynamismEnd - dynamismStart) / rounded_samples_per_tick;
    
    bool initializeSupersaw = (voice->delay_index == -1);
    if (initializeSupersaw) {
        // Goal: generate sawtooth phases such that the combined initial
        // amplitude cancel out to minimize pop. Algorithm: generate sorted phases, iterate over
        // their sawtooth drop points to find a combined zero crossing, then offset the
        // phases so they start there.
        
        // Generate random phases in ascending order by adding positive randomly
        // sized gaps between adjacent phases. For a proper distribution of random
        // events, the gaps sizes should be an "exponential distribution", which is
        // just: -Math.log(Math.random()). At the end, normalize the phases to a 0-1
        // range by dividing by the final value of the accumulator.
        double accumulator = 0.0;
        for (int i = 0; i < SUPERSAW_VOICE_COUNT; i++) {
            voice->phases[i] = accumulator;
            accumulator += -log(bbxs__frandom(&inst->prng_state));
        }
        
        double amplitudeSum = 1.0 + (SUPERSAW_VOICE_COUNT - 1.0) * dynamismStart;
        double slope = amplitudeSum;
        
        // Find the initial amplitude of the sum of sawtooths with the normalized
        // set of phases.
        double sample = 0.0;
        for (int i = 0; i < SUPERSAW_VOICE_COUNT; i++) {
            double amplitude = (i == 0) ? 1.0 : dynamismStart;
            double normalizedPhase = voice->phases[i] / accumulator;
            voice->phases[i] = normalizedPhase;
            sample += (normalizedPhase - 0.5) * amplitude;
        }
        
        // Find the phase of the zero crossing of the sum of the sawtooths. You can
        // use a constant slope and the distance between sawtooth drops to determine if
        // the zero crossing occurs between them. Note that a small phase means that
        // the corresponding drop for that wave is far away, and a big phase means the
        // drop is nearby, so to iterate forward through the drops we iterate backward
        // through the phases.
        double zeroCrossingPhase = 1.0;
        double prevDrop = 0.0;
        for (int i = SUPERSAW_VOICE_COUNT - 1; i >= 0; i--) {
            double nextDrop = 1.0 - voice->phases[i];
            double phaseDelta = nextDrop - prevDrop;
            if (sample < 0.0) {
                double distanceToZeroCrossing = -sample / slope;
                if (distanceToZeroCrossing < phaseDelta) {
                    zeroCrossingPhase = prevDrop + distanceToZeroCrossing;
                    break;
                }
            }
            double amplitude = (i == 0) ? 1.0 : dynamismStart;
            sample += phaseDelta * slope - amplitude;
            prevDrop = nextDrop;
        }
        for (int i = 0; i < SUPERSAW_VOICE_COUNT; i++) {
            voice->phases[i] += zeroCrossingPhase;
        }
        
        // Randomize the (initially sorted) order of the phases (aside from the
        // first one) so that they don't correlate to the detunes that are also
        // based on index.
        for (int i = 1; i < SUPERSAW_VOICE_COUNT - 1; i++) {
            int swappedIndex = i + (int)floor(bbxs__frandom(&inst->prng_state) * (SUPERSAW_VOICE_COUNT - i));
            double temp = voice->phases[i];
            voice->phases[i] = voice->phases[swappedIndex];
            voice->phases[swappedIndex] = temp;
        }
    }
    
    // Spread mods
    double useSpreadStart = inst->spread[0] / BBXS_SUPERSAW_SPREAD_MAX;
    double useSpreadEnd = inst->spread[1] / BBXS_SUPERSAW_SPREAD_MAX;
    
    double spreadSliderStart = useSpreadStart * envc->envelope_starts[BBXS_ENV_INDEX_SUPERSAW_SPREAD];
    double spreadSliderEnd = useSpreadEnd * envc->envelope_ends[BBXS_ENV_INDEX_SUPERSAW_SPREAD];

    // Just use the average detune for the current tick in the below loop.
    double averageSpreadSlider = (spreadSliderStart + spreadSliderEnd) * 0.5;
    double curvedSpread = pow(1.0 - sqrt(max(0.0, 1.0 - averageSpreadSlider)), 1.75);
    for (int i = 0; i < SUPERSAW_VOICE_COUNT; i++) {
        // Spread out the detunes around the center;
        double offset = (i == 0) ? 0.0 : pow((((i + 1) >> 1) - 0.5 + 0.025 * ((i & 2) - 1)) / (SUPERSAW_VOICE_COUNT >> 1), 1.1) * ((i & 1) * 2 - 1);
        voice->unison_detunes[i] = pow(2.0, curvedSpread * offset / 12.0);
    }
    
    double baseShape = voice->shape / BBXS_SUPERSAW_SHAPE_MAX;
    // Saw shape mods
    double useShapeStart = (inst->shape[0] / BBXS_SUPERSAW_SHAPE_MAX) * envc->envelope_starts[BBXS_ENV_INDEX_SUPERSAW_SHAPE];
    double useShapeEnd = (inst->shape[1] / BBXS_SUPERSAW_SHAPE_MAX) * envc->envelope_ends[BBXS_ENV_INDEX_SUPERSAW_SHAPE];

    double shapeStart = useShapeStart * envc->envelope_starts[BBXS_ENV_INDEX_SUPERSAW_SHAPE]; // sic
    double shapeEnd = useShapeEnd * envc->envelope_ends[BBXS_ENV_INDEX_SUPERSAW_SHAPE]; // sic
    voice->shape = shapeStart;
    voice->shape_delta = (shapeEnd - shapeStart) / rounded_samples_per_tick;
        
    // Check for PWM mods to this instrument
    double pulseWidthModStart = pulse_width_ratio(inst->pulse_width[0]);
    double pulseWidthModEnd = pulse_width_ratio(inst->pulse_width[1]);

    double pulseWidthStart = pulseWidthModStart * envc->envelope_starts[BBXS_ENV_INDEX_PULSE_WIDTH];
    double pulseWidthEnd = pulseWidthModEnd * envc->envelope_ends[BBXS_ENV_INDEX_PULSE_WIDTH];
    double phaseDeltaStart = (voice->has_prev_phase_delta) ? voice->prev_phase_delta : start_freq * varying->sample_len;
    double phaseDeltaEnd = start_freq * varying->sample_len * freqEndRatio;
    voice->prev_phase_delta = phaseDeltaEnd;
    voice->has_prev_phase_delta = true;
    const delayLengthStart = pulseWidthStart / phaseDeltaStart;
    const delayLengthEnd = pulseWidthEnd / phaseDeltaEnd;
    tone.supersawDelayLength = delayLengthStart;
    tone.supersawDelayLengthDelta = (delayLengthEnd - delayLengthStart) / roundedSamplesPerTick;
    const minBufferLength: number = Math.ceil(Math.max(delayLengthStart, delayLengthEnd)) + 2;
    
    if (tone.supersawDelayLine == null || tone.supersawDelayLine.length <= minBufferLength) {
        // The delay line buffer will get reused for other tones so might as well
        // start off with a buffer size that is big enough for most notes.
        const likelyMaximumLength: number = Math.ceil(0.5 * this.samplesPerSecond / Instrument.frequencyFromPitch(24));
        const newDelayLine: Float32Array = new Float32Array(Synth.fittingPowerOfTwo(Math.max(likelyMaximumLength, minBufferLength)));
        if (!initializeSupersaw && tone.supersawDelayLine != null) {
            // If the tone has already started but the buffer needs to be reallocated,
            // transfer the old data to the new buffer.
            const oldDelayBufferMask: number = (tone.supersawDelayLine.length - 1) >> 0;
            const startCopyingFromIndex: number = tone.supersawDelayIndex;
            for (let i: number = 0; i < tone.supersawDelayLine.length; i++) {
                newDelayLine[i] = tone.supersawDelayLine[(startCopyingFromIndex + i) & oldDelayBufferMask];
            }
        }
        tone.supersawDelayLine = newDelayLine;
        tone.supersawDelayIndex = tone.supersawDelayLine.length;
    } else if (initializeSupersaw) {
        tone.supersawDelayLine.fill(0.0);
        tone.supersawDelayIndex = tone.supersawDelayLine.length;
    }
    
    const pulseExpressionRatio: number = Config.pwmBaseExpression / Config.supersawBaseExpression;
    supersawExpressionStart *= (1.0 + (pulseExpressionRatio - 1.0) * shapeStart) / Math.sqrt(1.0 + (Config.supersawVoiceCount - 1.0) * dynamismStart * dynamismStart);
    supersawExpressionEnd *= (1.0 + (pulseExpressionRatio - 1.0) * shapeEnd) / Math.sqrt(1.0 + (Config.supersawVoiceCount - 1.0) * dynamismEnd * dynamismEnd);

    /*
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
    */
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

    for (int i = 0; i < BBXS_SYNTH_MAX_VOICES; ++i) {
        supersaw_voice_s *voice = &inst->voices[i];
        if (!voice_is_computing(&voice->base)) continue;

        double x1 = voice->base.note_filter_input[0];
        double x2 = voice->base.note_filter_input[1];

        for (size_t smp = 0; smp < frame_count; ++smp) {
            // double sample =
            //     bbxs__apply_filters(x0, x1, x2, voice->base.note_filters);
            // x2 = x1;
            // x1 = x0;

            // const double output = sample * expression;
            // expression += expression_delta;

            // samples[smp] += (float)output;
        }

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
    .inst_note_on = supersaw_note_on,
    .inst_note_off = supersaw_note_off,
    .inst_note_all_off = supersaw_note_all_off,

    .inst_tick = supersaw_tick,
    .inst_run = supersaw_run,
};