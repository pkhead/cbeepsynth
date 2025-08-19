#include "bitcrusher.h"

#include <assert.h>
#include <math.h>

#include "../util.h"
#include "../audio.h"


#define BITCRUSHER_OCTAVE_STEP 0.5
#define BITCRUSHER_BASE_VOLUME 0.010
#define ZERO_EPSILON 1e-16

void bpbxsyn_effect_init_bitcrusher(bpbxsyn_context_s *ctx, bitcrusher_effect_s *inst) {
    *inst = (bitcrusher_effect_s){
        .base.type = BPBXSYN_EFFECT_BITCRUSHER,
        .base.ctx = ctx
    };
}

void bitcrusher_destroy(bpbxsyn_effect_s *inst) {
    (void)inst;
}

void bitcrusher_tick(bpbxsyn_effect_s *p_inst, const bpbxsyn_tick_ctx_s *ctx) {
    bitcrusher_effect_s *const inst = (bitcrusher_effect_s*)p_inst;
    
    const double samples_per_second = inst->base.sample_rate;
    const double rounded_samples_per_tick =
        ceil(calc_samples_per_tick(ctx->bpm, inst->base.sample_rate));
    
    double freq_setting_start = inst->freq_crush[0];
    double freq_setting_end = inst->freq_crush[1];

    double quantization_setting_start = inst->bit_crush[0];
    double quantization_setting_end = inst->bit_crush[1];

    // const base_pitch: number = Config.keys[synth.song!.key].base_pitch; // TODO: What if there's a key change mid-song?
    int base_pitch = ctx->base_pitch;
    if (base_pitch == 0)
        base_pitch = 12;

    const double freq_start = key_to_hz_d(base_pitch + 60) * pow(2.0, (BPBXSYN_BITCRUSHER_FREQ_MAX - freq_setting_start) * BITCRUSHER_OCTAVE_STEP);
    const double freq_end = key_to_hz_d(base_pitch + 60) * pow(2.0, (BPBXSYN_BITCRUSHER_FREQ_MAX - freq_setting_end) * BITCRUSHER_OCTAVE_STEP);
    const double phase_delta_start = min(1.0, freq_start / samples_per_second);
    const double phase_delta_end = min(1.0, freq_end / samples_per_second);

    inst->phase_delta = phase_delta_start;
    inst->phase_delta_scale = pow(phase_delta_end / phase_delta_start, 1.0 / rounded_samples_per_tick);

    const double scale_start = 2.0 * BITCRUSHER_BASE_VOLUME * pow(2.0, 1.0 - pow(2.0, (BPBXSYN_BITCRUSHER_QUANTIZATION_MAX - quantization_setting_start) * 0.5));
    const double scale_end = 2.0 * BITCRUSHER_BASE_VOLUME * pow(2.0, 1.0 - pow(2.0, (BPBXSYN_BITCRUSHER_QUANTIZATION_MAX - quantization_setting_end) * 0.5));
    inst->scale = scale_start;
    inst->scale_scale = pow(scale_end / scale_start, 1.0 / rounded_samples_per_tick);

    const double fold_level_start = 2.0 * BITCRUSHER_BASE_VOLUME * pow(1.5, BPBXSYN_BITCRUSHER_QUANTIZATION_MAX - quantization_setting_start);
    const double fold_level_end = 2.0 * BITCRUSHER_BASE_VOLUME * pow(1.5, BPBXSYN_BITCRUSHER_QUANTIZATION_MAX - quantization_setting_end);
    inst->fold_level = fold_level_start;
    inst->fold_level_scale = pow(fold_level_end / fold_level_start, 1.0 / rounded_samples_per_tick);

    inst->bit_crush[0] = inst->bit_crush[1];
    inst->freq_crush[0] = inst->freq_crush[1];
}

void bitcrusher_run(bpbxsyn_effect_s *p_inst, float **p_buffer,
                 size_t frame_count)
{
    bitcrusher_effect_s *const inst = (bitcrusher_effect_s*)p_inst;

    float *buffer = p_buffer[0];
    assert(buffer);

    double prev_input = inst->prev_input;
    double current_output = inst->current_output;
    double phase = inst->phase;
    double phase_delta = inst->phase_delta;
    const double phase_delta_scale = inst->phase_delta_scale;
    double scale = inst->scale;
    const double scale_scale = inst->scale_scale;
    double fold_level = inst->fold_level;
    const double fold_level_scale = inst->fold_level_scale;

    for (size_t frame = 0; frame < frame_count; ++frame) {
        double sample = (double)buffer[frame];
        
        // it is necessary that this effect not compute when the input is
        // silent, because of two reasons:
        //  1. when silent, it generates a (very small) DC offset.
        //  2. when silent, clicks occur when changing the quantization
        //     parameter (probably because the DC offset changes).
        if (fabs(sample) < ZERO_EPSILON) continue;

        phase += phase_delta;
        if (phase < 1.0) {
            prev_input = sample;
            sample = current_output;
        } else {
            phase = fmod(phase, 1.0);
            const double ratio = phase / phase_delta;
            
            const double lerped_input = sample + (prev_input - sample) * ratio;
            prev_input = sample;
            
            const double wrap_level = fold_level * 4.0;
            const double wrapped_sample = fmod((fmod(lerped_input + fold_level, wrap_level) + wrap_level), wrap_level);
            const double folded_sample = fold_level - fabs(fold_level * 2.0 - wrapped_sample);
            const double scaled_sample = folded_sample / scale;
            const double old_value = current_output;
            const double new_value = (((int)(scaled_sample > 0 ? scaled_sample + 1 : scaled_sample))-.5) * scale;
            
            sample = old_value + (new_value - old_value) * ratio;
            current_output = new_value;
        }

        phase_delta *= phase_delta_scale;
        scale *= scale_scale;
        fold_level *= fold_level_scale;

        buffer[frame] = (float)sample;
    }

    if (fabs(prev_input) < FLUSH_ZERO_EPSILON) prev_input = 0.0;
    if (fabs(current_output) < FLUSH_ZERO_EPSILON) current_output = 0.0;

    inst->prev_input = prev_input;
    inst->current_output = current_output;
    inst->phase = phase;
    inst->phase_delta = phase_delta;
    inst->scale = scale;
    inst->fold_level = fold_level;
}



static const bpbxsyn_param_info_s param_info[BPBXSYN_PANNING_PARAM_COUNT] = {
    {
        .group = "Effects/Bitcrusher",
        .name = "Bitcrusher Quantization",
        .id = "bitQuant",
        .type = BPBXSYN_PARAM_DOUBLE,
        
        .min_value = 0.0,
        .max_value = BPBXSYN_BITCRUSHER_QUANTIZATION_MAX,
        .default_value = 0.0,
    },
    {
        .group = "Effects/Bitcrusher",
        .name = "Frequency Quantization",
        .id = "bitFreqQ",
        .type = BPBXSYN_PARAM_DOUBLE,

        .min_value = 0.0,
        .max_value = BPBXSYN_BITCRUSHER_FREQ_MAX,
        .default_value = 0.0
    }
};

static const size_t param_addresses[BPBXSYN_PANNING_PARAM_COUNT] = {
    offsetof(bitcrusher_effect_s, bit_crush[1]),
    offsetof(bitcrusher_effect_s, freq_crush[1]),
};

const effect_vtable_s effect_bitcrusher_vtable = {
    .struct_size = sizeof(bitcrusher_effect_s),
    .effect_init = (effect_init_f)bpbxsyn_effect_init_bitcrusher,
    .effect_destroy = bitcrusher_destroy,

    .input_channel_count = 1,
    .output_channel_count = 1,

    .param_count = BPBXSYN_BITCRUSHER_PARAM_COUNT,
    .param_info = param_info,
    .param_addresses = param_addresses,

    .effect_tick = bitcrusher_tick,
    .effect_run = bitcrusher_run
};