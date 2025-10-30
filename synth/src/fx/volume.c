#include <math.h>
#include "effect.h"
#include "audio.h"


#define VOLUME_LOG_SCALE 0.1428

typedef struct {
    bpbxsyn_effect_s base;

    // for old and new value
    double param[2];

    double linear_gain;
    double linear_gain_delta;
} volume_effect_s;

static double inst_volume_to_mult(double inst_volume) {
    if (inst_volume <= BPBXSYN_INSTRUMENT_VOLUME_MIN) return 0.0;
    return pow(2.0, VOLUME_LOG_SCALE * inst_volume);
}

static void volume_init(bpbxsyn_context_s *ctx, bpbxsyn_effect_s *p_inst) {
    volume_effect_s *inst = (volume_effect_s*)p_inst;
    *inst = (volume_effect_s){
        .base.ctx = ctx,
        .base.type = BPBXSYN_EFFECT_VOLUME
    };
}

static void volume_destroy(bpbxsyn_effect_s *inst) {
    (void)inst;
}

static void volume_tick(bpbxsyn_effect_s *p_inst,
                        const bpbxsyn_tick_ctx_s *ctx) {
    volume_effect_s *const inst = (volume_effect_s*)p_inst;
    const double rounded_samples_per_tick =
        ceil(bbsyn_calc_samples_per_tick(ctx->bpm, inst->base.sample_rate));
    
    const double volume_start = inst_volume_to_mult(inst->param[0]);
    const double volume_end = inst_volume_to_mult(inst->param[1]);
    
    inst->linear_gain = volume_start;
    inst->linear_gain_delta = (volume_end - volume_start) / rounded_samples_per_tick;

    inst->param[0] = inst->param[1];
}

static void volume_run(bpbxsyn_effect_s *p_inst, float **buffer,
                       size_t frame_count) {
    volume_effect_s *const inst = (volume_effect_s*)p_inst;

    double linear_gain = inst->linear_gain;
    double linear_gain_delta = inst->linear_gain_delta;

    float *left = buffer[0];
    float *right = buffer[1];
    for (size_t i = 0; i < frame_count; ++i) {
        float linear_gain_f = (float)linear_gain;
        left[i] *= linear_gain_f;
        right[i] *= linear_gain_f;

        linear_gain += linear_gain_delta;
    }

    inst->linear_gain = linear_gain;
}





static const bpbxsyn_param_info_s param_info[BPBXSYN_PANNING_PARAM_COUNT] = {
    {
        .group = "General",
        .name = "Volume",
        .id = "volume\0\0",
        .type = BPBXSYN_PARAM_DOUBLE,
        
        .min_value = BPBXSYN_INSTRUMENT_VOLUME_MIN,
        .max_value = BPBXSYN_INSTRUMENT_VOLUME_MAX,
        .default_value = 0.0,
    },
};

static const size_t param_addresses[BPBXSYN_PANNING_PARAM_COUNT] = {
    offsetof(volume_effect_s, param[1]),
};

const effect_vtable_s bbsyn_effect_volume_vtable = {
    .struct_size = sizeof(volume_effect_s),
    .effect_init = volume_init,
    .effect_destroy = volume_destroy,

    .input_channel_count = 2,
    .output_channel_count = 2,

    .param_count = BPBXSYN_VOLUME_PARAM_COUNT,
    .param_info = param_info,
    .param_addresses = param_addresses,

    .effect_tick = volume_tick,
    .effect_run = volume_run
};