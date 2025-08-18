#include "distortion.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>

#include "../util.h"
#include "../audio.h"


#define DISTORTION_BASE_VOLUME 0.011

void bpbxsyn_effect_init_distortion(distortion_effect_s *inst) {
    *inst = (distortion_effect_s){
        .base.type = BPBXSYN_EFFECT_DISTORTION
    };
}

void distortion_destroy(bpbxsyn_effect_s *inst) {
    (void)inst;
}

void distortion_tick(bpbxsyn_effect_s *p_inst, const bpbxsyn_tick_ctx_s *ctx) {
    distortion_effect_s *const inst = (distortion_effect_s*)p_inst;

    double rounded_samples_per_tick = 
        ceil(calc_samples_per_tick(ctx->bpm, inst->base.sample_rate));
    
    double use_distortion_start = inst->param[0];
    double use_distortion_end = inst->param[1];

    const double distortion_slider_start =
        min(1.0, use_distortion_start / BPBXSYN_DISTORTION_MAX);
    const double distortion_slider_end =
        min(1.0, use_distortion_end / BPBXSYN_DISTORTION_MAX);

    const double distortion_start =
        pow(1.0 - 0.895 * (pow(20.0, distortion_slider_start) - 1.0) / 19.0, 2.0);
    const double distortion_end =
        pow(1.0 - 0.895 * (pow(20.0, distortion_slider_end) - 1.0) / 19.0, 2.0);
    
    const double distortion_drive_start
        = (1.0 + 2.0 * distortion_slider_start) / DISTORTION_BASE_VOLUME;
    const double distortion_drive_end =
        (1.0 + 2.0 * distortion_slider_end) / DISTORTION_BASE_VOLUME;

    inst->distortion = distortion_start;
    inst->distortion_delta =
        (distortion_end - distortion_start) / rounded_samples_per_tick;
    
    inst->drive = distortion_drive_start;
    inst->drive_delta =
        (distortion_drive_end - distortion_drive_start) / rounded_samples_per_tick;

    inst->param[0] = inst->param[1];
}

void distortion_run(bpbxsyn_effect_s *p_inst, float **p_buffer,
                 size_t frame_count)
{
    distortion_effect_s *const inst = (distortion_effect_s*)p_inst;

    float *buffer = p_buffer[0];
    assert(buffer);
    
    double distortion = inst->distortion;
    const double distortion_delta = inst->distortion_delta;
    double drive = inst->drive;
    const double drive_delta = inst->drive_delta;
    const double fractional_resolution = 4.0;
    const double oversample_compensation = DISTORTION_BASE_VOLUME / fractional_resolution;
    const double fractional_delay1 = 1.0 / fractional_resolution;
    const double fractional_delay2 = 2.0 / fractional_resolution;
    const double fractional_delay3 = 3.0 / fractional_resolution;
    const double fractional_delay_g1 = (1.0 - fractional_delay1) / (1.0 + fractional_delay1); // Inlined version of FilterCoefficients.prototype.allPass1stOrderFractionalDelay
    const double fractional_delay_g2 = (1.0 - fractional_delay2) / (1.0 + fractional_delay2); // Inlined version of FilterCoefficients.prototype.allPass1stOrderFractionalDelay
    const double fractional_delay_g3 = (1.0 - fractional_delay3) / (1.0 + fractional_delay3); // Inlined version of FilterCoefficients.prototype.allPass1stOrderFractionalDelay
    const double next_output_weight1 = cos(PI * fractional_delay1) * 0.5 + 0.5;
    const double next_output_weight2 = cos(PI * fractional_delay2) * 0.5 + 0.5;
    const double next_output_weight3 = cos(PI * fractional_delay3) * 0.5 + 0.5;
    const double prev_output_weight1 = 1.0 - next_output_weight1;
    const double prev_output_weight2 = 1.0 - next_output_weight2;
    const double prev_output_weight3 = 1.0 - next_output_weight3;
    
    double fractional_input1 = inst->fractional_input[0];
    double fractional_input2 = inst->fractional_input[1];
    double fractional_input3 = inst->fractional_input[2];
    double prev_input = inst->prev_input;
    double next_output = inst->next_output;

    for (size_t frame = 0; frame < frame_count; ++frame) {
        double sample = (double)buffer[frame];
        
        const double reverse = 1.0 - distortion;
        const double next_input = sample * drive;
        sample = next_output;
        next_output = next_input / (reverse * fabs(next_input) + distortion);
        fractional_input1 = fractional_delay_g1 * next_input + prev_input - fractional_delay_g1 * fractional_input1;
        fractional_input2 = fractional_delay_g2 * next_input + prev_input - fractional_delay_g2 * fractional_input2;
        fractional_input3 = fractional_delay_g3 * next_input + prev_input - fractional_delay_g3 * fractional_input3;
        const double output1 = fractional_input1 / (reverse * fabs(fractional_input1) + distortion);
        const double output2 = fractional_input2 / (reverse * fabs(fractional_input2) + distortion);
        const double output3 = fractional_input3 / (reverse * fabs(fractional_input3) + distortion);
        next_output += output1 * next_output_weight1 + output2 * next_output_weight2 + output3 * next_output_weight3;
        sample += output1 * prev_output_weight1 + output2 * prev_output_weight2 + output3 * prev_output_weight3;
        sample *= oversample_compensation;
        prev_input = next_input;
        distortion += distortion_delta;
        drive += drive_delta;

        buffer[frame] = (float)sample;
    }
    
    inst->distortion = distortion;
    inst->drive = drive;
    
    // if (!Number.isFinite(fractional_input1) || Math.abs(fractional_input1) < epsilon) fractional_input1 = 0.0;
    // if (!Number.isFinite(fractional_input2) || Math.abs(fractional_input2) < epsilon) fractional_input2 = 0.0;
    // if (!Number.isFinite(fractional_input3) || Math.abs(fractional_input3) < epsilon) fractional_input3 = 0.0;
    // if (!Number.isFinite(prev_input) || Math.abs(prev_input) < epsilon) prev_input = 0.0;
    // if (!Number.isFinite(next_output) || Math.abs(next_output) < epsilon) next_output = 0.0;
    
    inst->fractional_input[0] = fractional_input1;
    inst->fractional_input[1] = fractional_input2;
    inst->fractional_input[2] = fractional_input3;
    inst->prev_input = prev_input;
    inst->next_output = next_output;
}






static const bpbxsyn_param_info_s param_info[BPBXSYN_PANNING_PARAM_COUNT] = {
    {
        .group = "Effects/Distortion",
        .name = "Distortion Drive",
        .id = "distort\0",
        .type = BPBXSYN_PARAM_DOUBLE,
        
        .min_value = 0.0,
        .max_value = BPBXSYN_DISTORTION_MAX,
        .default_value = 0.0,
    },
};

static const size_t param_addresses[BPBXSYN_PANNING_PARAM_COUNT] = {
    offsetof(distortion_effect_s, param[1]),
};

const effect_vtable_s effect_distortion_vtable = {
    .struct_size = sizeof(distortion_effect_s),
    .effect_init = (effect_init_f)bpbxsyn_effect_init_distortion,
    .effect_destroy = distortion_destroy,

    .input_channel_count = 1,
    .output_channel_count = 1,

    .param_count = BPBXSYN_DISTORTION_PARAM_COUNT,
    .param_info = param_info,
    .param_addresses = param_addresses,

    .effect_tick = distortion_tick,
    .effect_run = distortion_run
};