#include <assert.h>
#include <string.h>
#include <time.h>
#include "synth.h"
#include "../context.h"
#include "../param_util.h"

// i feel like noise volume should be halved for pitch channels to be consistent
// with spectrum and the comment "drums tend to be loud but brief" for some
// drumset config thing. but several mods have implemented noise in pitch
// channels and the volume level is the same regardless of what type of channel
// the instrument is in. i should probably value compatibility than what i feel
// is shaktool's extrapolated intentions.
#define BASE_EXPRESSION 0.19

static_assert(UNISON_MAX_VOICES == 2, "UNISON_MAX_VOICES must be 2");

typedef struct noise_voice {
    inst_base_voice_s base;

    double phase[UNISON_MAX_VOICES];
    double phase_delta[UNISON_MAX_VOICES];
    double phase_delta_scale[UNISON_MAX_VOICES];
    double noise_sample[UNISON_MAX_VOICES];

    double prev_pitch_expression;
    bool has_prev_pitch_expression;
} noise_voice_s;

typedef struct noise_inst {
    bpbxsyn_synth_s base;

    uint8_t noise_type;
    uint8_t unison_type;

    bool is_noise_channel;
    const noise_wavetable_s *wavetable;
    noise_voice_s voices[BPBXSYN_SYNTH_MAX_VOICES];

    prng_state_s prng_state;
} noise_inst_s;

static void noise_init(bpbxsyn_context_s *ctx, bpbxsyn_synth_s *p_inst) {
    noise_inst_s *inst = (noise_inst_s*)p_inst;
    *inst = (noise_inst_s){0};
    bbsyn_inst_init(ctx, p_inst, BPBXSYN_SYNTH_NOISE);

    inst->noise_type = BPBXSYN_NOISE_RETRO;
    inst->prng_state = bbsyn_random_seeded_state((uint64_t)clock());
}

static bpbxsyn_voice_id noise_note_on(bpbxsyn_synth_s *p_inst, int key,
                                      double velocity, int32_t length) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_NOISE);
    noise_inst_s *inst = (noise_inst_s*)p_inst;

    bool continuation;
    bpbxsyn_voice_id id =
        bbsyn_trigger_voice(p_inst, GENERIC_LIST(inst->voices), key, velocity,
                            length, &continuation);

    if (!continuation) {
        noise_voice_s *voice = &inst->voices[id];
        *voice = (noise_voice_s) {
            .base = voice->base
        };
    }

    return id;
}

static void noise_note_off(bpbxsyn_synth_s *p_inst, bpbxsyn_voice_id id) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_NOISE);
    noise_inst_s *inst = (noise_inst_s*)p_inst;

    bbsyn_release_voice(p_inst, GENERIC_LIST(inst->voices), id);
}

static void noise_note_all_off(bpbxsyn_synth_s *p_inst) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_NOISE);
    noise_inst_s *inst = (noise_inst_s*)p_inst;

    bbsyn_release_all_voices(p_inst, GENERIC_LIST(inst->voices));
}

static void compute_voice(const bpbxsyn_synth_s *const base_inst,
                          inst_base_voice_s *base_voice,
                          voice_compute_s *compute_data) {
    const noise_inst_s *const inst = (noise_inst_s*)base_inst;
    noise_voice_s *voice = (noise_voice_s*)base_voice;
    
    const double sample_len = compute_data->varying.sample_len;
    const double rounded_samples_per_tick =
        compute_data->varying.rounded_samples_per_tick;

    voice_compute_varying_s *const varying = &compute_data->varying;
    const unison_desc_s unison = bbsyn_unison_info[inst->unison_type];

    int base_pitch = inst->wavetable->base_pitch;
    if (!inst->is_noise_channel)
        base_pitch -= 12;

    const double pitch_damping = inst->wavetable->is_soft ? 24.0 : 60.0;

    const double interval_start = compute_data->varying.interval_start;
    const double interval_end = compute_data->varying.interval_end;
    const double start_pitch = voice->base.current_key + interval_start + base_pitch;
    const double end_pitch = voice->base.current_key + interval_end + base_pitch;

    // pitch expression
    double pitch_expression_start;
    if (voice->has_prev_pitch_expression) {
        pitch_expression_start = voice->prev_pitch_expression;
    } else {
        pitch_expression_start =
            calc_pitch_expression_ex(start_pitch, base_pitch, pitch_damping);
    }
    const double pitch_expression_end =
        calc_pitch_expression_ex(end_pitch, base_pitch, pitch_damping);
    voice->has_prev_pitch_expression = true;
    voice->prev_pitch_expression = pitch_expression_end;

    const double expr_mult = inst->wavetable->expression * unison.expression *
                             unison.voices / 2.0;

    const double expr_start = BASE_EXPRESSION * varying->expr_start *
                              pitch_expression_start * expr_mult;
    const double expr_end = BASE_EXPRESSION * varying->expr_end *
                            pitch_expression_end * expr_mult;

    const double unison_env_start =
        voice->base.env_computer.envelope_starts[BPBXSYN_ENV_INDEX_UNISON];
    const double unison_env_end =
        voice->base.env_computer.envelope_ends[BPBXSYN_ENV_INDEX_UNISON];

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

    for (int i = 0; i < UNISON_MAX_VOICES; ++i) {
        voice->phase_delta[i] = start_freq * sample_len * unison_starts[i];
        voice->phase_delta_scale[i] = base_phase_delta_scale *
            pow(unison_ends[i] / unison_starts[i], 1.0 / rounded_samples_per_tick);
    }
    
    voice->base.expression = expr_start;
    voice->base.expression_delta =
        (expr_end - expr_start) / rounded_samples_per_tick;
}

static void noise_tick(bpbxsyn_synth_s *p_inst,
                       const bpbxsyn_tick_ctx_s *tick_ctx) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_NOISE);
    noise_inst_s *inst = (noise_inst_s*)p_inst;

    assert(inst->noise_type >= 0 && inst->noise_type < BPBXSYN_NOISE_COUNT);
    inst->wavetable =
        &inst->base.ctx->wavetables.noise_wavetables[inst->noise_type];

    bbsyn_inst_tick(p_inst, tick_ctx, &(audio_compute_s) {
        .voice_list = inst->voices,
        .sizeof_voice = sizeof(*inst->voices),
        .compute_voice = compute_voice,
        .userdata = NULL
    });
}

void noise_run(bpbxsyn_synth_s *p_inst, float *samples, size_t frame_count) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_NOISE);
    noise_inst_s *inst = (noise_inst_s*)p_inst;
    assert(inst->wavetable);

    memset(samples, 0, frame_count * sizeof(float));

    const float *wave = inst->wavetable->samples;
    const double pitch_filter_mult = inst->wavetable->pitch_filter_mult;
    const unison_desc_s unison = bbsyn_unison_info[inst->unison_type];

    for (int i = 0; i < BPBXSYN_SYNTH_MAX_VOICES; ++i) {
        noise_voice_s *voice = &inst->voices[i];
        if (!voice_is_computing(&voice->base)) continue;

        double phase[UNISON_MAX_VOICES];
        double phase_delta[UNISON_MAX_VOICES];
        double phase_delta_scale[UNISON_MAX_VOICES];
        double noise_sample[UNISON_MAX_VOICES];
        double pitch_relfilter[UNISON_MAX_VOICES];

        double expression = voice->base.expression;
        const double expression_delta = voice->base.expression_delta;

        const bool phase_sync = unison.voices == 1 && unison.spread == 0; /*TODO: && customInterval*/

        // compiler pls unroll this loop?
        for (int v = 0; v < UNISON_MAX_VOICES; ++v) {
            phase_delta[v] = voice->phase_delta[v];
            phase_delta_scale[v] = voice->phase_delta_scale[v];
            phase[v] = fmod(voice->phase[v], 1.0) * NOISE_WAVETABLE_LENGTH;

            if (v > 0 && phase_sync) {
                phase[v] = phase[0];
            } else if (phase[v] == 0.0) {
                // Zero phase means the tone was reset, just give noise a random
                // start phase instead.
                phase[v] = bbsyn_frandom(&inst->prng_state) * NOISE_WAVETABLE_LENGTH;
            }

            noise_sample[v] = voice->noise_sample[v];

            // This is for a "legacy" style simplified 1st order lowpass filter
            // with a cutoff frequency that is relative to the tone's
            // fundamental frequency.
            pitch_relfilter[v] = min(1.0, phase_delta[v] * pitch_filter_mult);
        }
        
        const int phase_mask = NOISE_WAVETABLE_LENGTH - 1;
        double x0 = voice->base.note_filter_input[0];
        double x1 = voice->base.note_filter_input[1];

        for (size_t smp = 0; smp < frame_count; ++smp) {
            // compiler pls unroll this loop?
            for (int v = 0; v < UNISON_MAX_VOICES; ++v) {
                const double wave_sample = wave[(int)phase[v] & phase_mask];
                noise_sample[v] += (wave_sample - noise_sample[v]) *
                                   pitch_relfilter[v];
            }

            const double input_sample = noise_sample[0] +
                                        noise_sample[1] * unison.sign;
            const double sample = bbsyn_apply_filters(
                input_sample, x0, x1,
                voice->base.note_filters);
            x1 = x0;
            x0 = input_sample;

            // compiler pls unroll this loop?
            for (int v = 0; v < UNISON_MAX_VOICES; ++v) {
                phase[v] += phase_delta[v];
                phase_delta[v] *= phase_delta_scale[v];
            }

            const double output = sample * expression;
            expression += expression_delta;

            samples[smp] += (float)output;
        }

        // compiler pls unroll this loop?
        for (int v = 0; v < UNISON_MAX_VOICES; ++v) {
            voice->phase[v] = phase[v] / NOISE_WAVETABLE_LENGTH;
            voice->phase_delta[v] = phase_delta[v];
            voice->noise_sample[v] = noise_sample[v];
        }

        voice->base.expression_delta = expression_delta;

        bbsyn_sanitize_filters(voice->base.note_filters, FILTER_GROUP_COUNT);
        voice->base.note_filter_input[0] = x0;
        voice->base.note_filter_input[1] = x1;
    }
}









//////////
// DATA //
//////////
static const char *noise_enum_values[] = {
    "retro",
    "white",
    "clang",
    "buzz",
    "hollow",
    "shine",
    "deep",
    "cutter",
    "metallic",
};

static const bpbxsyn_param_info_s noise_param_info[BPBXSYN_NOISE_PARAM_COUNT] = {
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "noiseChn",
        .group = "Basic Noise",
        .name = "Is Noise Channel?",
        .min_value = 0,
        .max_value = 1,
        .default_value = 0,
        .enum_values = bbsyn_yes_no_enum_values
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "noiseTyp",
        .group = "Basic Noise",
        .name = "Noise Type",
        .min_value = 0,
        .max_value = BPBXSYN_NOISE_COUNT - 1,
        .default_value = BPBXSYN_NOISE_RETRO,
        .enum_values = noise_enum_values
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "noiseUni",
        .group = "Basic Noise",
        .name = "Unison",
        .min_value = 0,
        .max_value = BPBXSYN_UNISON_COUNT - 1,
        .default_value = BPBXSYN_UNISON_NONE,
        .enum_values = bbsyn_unison_enum_values
    }
};

static const size_t noise_param_addresses[BPBXSYN_NOISE_PARAM_COUNT] = {
    offsetof(noise_inst_s, is_noise_channel),
    offsetof(noise_inst_s, noise_type),
    offsetof(noise_inst_s, unison_type),
};

const inst_vtable_s bbsyn_inst_noise_vtable = {
    .struct_size = sizeof(noise_inst_s),

    .param_count = BPBXSYN_NOISE_PARAM_COUNT,
    .param_info = noise_param_info,
    .param_addresses = noise_param_addresses,

    .envelope_target_count = 0,

    .inst_init = noise_init,
    .inst_note_on = noise_note_on,
    .inst_note_off = noise_note_off,
    .inst_note_all_off = noise_note_all_off,

    .inst_tick = noise_tick,
    .inst_run = noise_run
};