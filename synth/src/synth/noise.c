#include "noise.h"

#include <assert.h>
#include <string.h>
#include <time.h>
#include "../context.h"


// i feel like noise volume should be halved for pitch channels to be consistent
// with spectrum and the comment "drums tend to be loud but brief" for some
// drumset config thing. but several mods have implemented noise in pitch
// channels and the volume level is the same regardless of what type of channel
// the instrument is in. i should probably value compatibility than what i feel
// is shaktool's extrapolated intentions.
#define BASE_EXPRESSION 0.19

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

    const double expr_mult = inst->wavetable->expression;

    const double expr_start =
        BASE_EXPRESSION * varying->expr_start * pitch_expression_start * expr_mult;
    const double expr_end =
        BASE_EXPRESSION * varying->expr_end * pitch_expression_end * expr_mult;

    const double start_freq = key_to_hz_d(start_pitch);
    const double end_freq = key_to_hz_d(end_pitch);

    voice->phase_delta = start_freq * sample_len;
    voice->phase_delta_scale =
        pow(end_freq / start_freq, 1.0 / rounded_samples_per_tick);
    
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

    for (int i = 0; i < BPBXSYN_SYNTH_MAX_VOICES; ++i) {
        noise_voice_s *voice = &inst->voices[i];
        if (!voice_is_computing(&voice->base)) continue;
        
        double phase_delta = voice->phase_delta;
        const double phase_delta_scale = voice->phase_delta_scale;
        double expression = voice->base.expression;
        const double expression_delta = voice->base.expression_delta;
        double phase = fmod(voice->phase, 1.0) * NOISE_WAVETABLE_LENGTH;
        if (phase == 0.0) {
            // Zero phase means the tone was reset, just give noise a random
            // start phase instead.
            phase = bbsyn_frandom(&inst->prng_state) * NOISE_WAVETABLE_LENGTH;
        }
        const int phase_mask = NOISE_WAVETABLE_LENGTH - 1;
        double noise_sample = voice->noise_sample;

        double x0 = voice->base.note_filter_input[0];
        double x1 = voice->base.note_filter_input[1];

        // This is for a "legacy" style simplified 1st order lowpass filter with
        // a cutoff frequency that is relative to the tone's fundamental
        // frequency.
        const double pitch_relfilter =
            min(1.0, phase_delta * pitch_filter_mult);

        for (size_t smp = 0; smp < frame_count; ++smp) {
            const double wave_sample = wave[(int)phase & phase_mask];

            noise_sample += (wave_sample - noise_sample) * pitch_relfilter;

            const double input_sample = noise_sample;
            const double sample = bbsyn_apply_filters(
                input_sample, x0, x1,
                voice->base.note_filters);
            x1 = x0;
            x0 = input_sample;

            phase += phase_delta;
            phase_delta *= phase_delta_scale;

            const double output = sample * expression;
            expression += expression_delta;

            samples[smp] += (float)output;
        }

        voice->phase = phase / NOISE_WAVETABLE_LENGTH;
        voice->phase_delta = phase_delta;
        voice->base.expression_delta = expression_delta;
        voice->noise_sample = noise_sample;

        bbsyn_sanitize_filters(voice->base.note_filters, FILTER_GROUP_COUNT);
        voice->base.note_filter_input[0] = x0;
        voice->base.note_filter_input[1] = x1;
    }
}









//////////
// DATA //
//////////
static const char *noise_enum_values[] = {
    "Retro",
    "White",
    "Clang",
    "Buzz",
    "Hollow",
    "Shine",
    "Deep",
    "Cutter",
    "Metallic",
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
    }
};

static const size_t noise_param_addresses[BPBXSYN_PULSE_WIDTH_PARAM_COUNT] = {
    offsetof(noise_inst_s, is_noise_channel),
    offsetof(noise_inst_s, noise_type),
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