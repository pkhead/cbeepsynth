#include "limiter.h"

#include <assert.h>
#include <math.h>

void bpbxsyn_effect_init_limiter(bpbxsyn_context_s *ctx,
                                 limiter_effect_s *inst)
{
    *inst = (limiter_effect_s) {
        .base.type = BPBXSYN_EFFECT_LIMITER,
        .base.ctx = ctx,

        .limit = 1.0,

        .compression_ratio = BPBXSYN_LIMITER_BOOST_THRESHOLD_DEFAULT,
        .compression_threshold = BPBXSYN_LIMITER_BOOST_THRESHOLD_DEFAULT,
        .limit_ratio = BPBXSYN_LIMITER_CUTOFF_RATIO_DEFAULT,
        .limit_threshold = BPBXSYN_LIMITER_CUTOFF_THRESHOLD_DEFAULT,
        .master_gain = BPBXSYN_LIMITER_MASTER_GAIN_DEFAULT,
        .rise = BPBXSYN_LIMITER_LIMIT_RISE_DEFAULT,
        .decay = BPBXSYN_LIMITER_LIMIT_DECAY_DEFAULT,
    };
}

void limiter_destroy(bpbxsyn_effect_s *inst) {
    (void)inst;
}

void limiter_tick(bpbxsyn_effect_s *inst, const bpbxsyn_tick_ctx_s *ctx) {
    (void)inst;
    (void)ctx;
}

void limiter_run(bpbxsyn_effect_s *p_inst, float **buffer, size_t frame_count) {
    limiter_effect_s *const inst = (limiter_effect_s*)p_inst;

    const double decay = 1.0 - pow(0.5, inst->decay / inst->base.sample_rate);
    const double rise = 1.0 - pow(0.5, inst->rise / inst->base.sample_rate);

    float *left = buffer[0];
    float *right = buffer[1];
    assert(left && right);

    const double master_gain = inst->master_gain;
    const double compression_ratio = inst->compression_ratio;
    const double compression_threshold = inst->compression_threshold;

    const double limit_ratio = (inst->limit_ratio < 10 ? inst->limit_ratio / 10 : (inst->limit_ratio - 9));
    const double limit_threshold = (inst->compression_ratio < 10 ? inst->compression_ratio / 10 : (1 + (inst->compression_ratio - 10) / 60));
    double limit = inst->limit;

    for (size_t i = 0; i < frame_count; ++i) {
        const double sampleL = left[i] * master_gain * master_gain;
        const double sampleR = right[i] * master_gain * master_gain;
        const double absL = sampleL < 0.0 ? -sampleL : sampleL;
        const double absR = sampleR < 0.0 ? -sampleR : sampleR;
        const double abs = absL > absR ? absL : absR;
        // this.song.inVolumeCap = (this.song.inVolumeCap > abs ? this.song.inVolumeCap : abs); // Analytics, spit out raw input volume

        // Determines which formula to use. 0 when volume is between [0, compressionThreshold], 1 when between (compressionThreshold, limitThreshold], 2 above
        const double limitRange = (+(abs > compression_threshold)) + (+(abs > limit_threshold));
        // Determine the target amplification based on the range of the curve
        const double limitTarget =
            (+(limitRange == 0)) * (((abs + 1 - compression_threshold) * 0.8 + 0.25) * compression_ratio + 1.05 * (1 - compression_ratio))
            + (+(limitRange == 1)) * (1.05)
            + (+(limitRange == 2)) * (1.05 * ((abs + 1 - limit_threshold) * limit_ratio + (1 - limit_threshold)));
        // Move the limit towards the target
        limit += ((limitTarget - limit) * (limit < limitTarget ? rise : decay));

        const double limitedVolume = 1.0 / (limit >= 1 ? limit * 1.05 : limit * 0.8 + 0.25);
        left[i] = (float)(sampleL * limitedVolume);
        right[i] = (float)(sampleR * limitedVolume);

        // this.song.outVolumeCap = (this.song.outVolumeCap > abs * limitedVolume ? this.song.outVolumeCap : abs * limitedVolume); // Analytics, spit out limited output volume
    }

    inst->limit = limit;
}



static const bpbxsyn_param_info_s param_info[BPBXSYN_LIMITER_PARAM_COUNT] = {
    {
        .name = "Master Gain",
        .id = "mtrGain\0",
        .type = BPBXSYN_PARAM_DOUBLE,
        
        .min_value =     BPBXSYN_LIMITER_MASTER_GAIN_MIN,
        .max_value =     BPBXSYN_LIMITER_MASTER_GAIN_MAX,
        .default_value = BPBXSYN_LIMITER_MASTER_GAIN_DEFAULT,
    },
    {
        .name = "Limit Decay",
        .id = "decay\0\0\0",
        .type = BPBXSYN_PARAM_DOUBLE,
        
        .min_value =     BPBXSYN_LIMITER_LIMIT_DECAY_MIN,
        .max_value =     BPBXSYN_LIMITER_LIMIT_DECAY_MAX,
        .default_value = BPBXSYN_LIMITER_LIMIT_DECAY_DEFAULT,
    },
    {
        .name = "Limit Rise",
        .id = "rise\0\0\0\0",
        .type = BPBXSYN_PARAM_DOUBLE,
        
        .min_value =     BPBXSYN_LIMITER_LIMIT_RISE_MIN,
        .max_value =     BPBXSYN_LIMITER_LIMIT_RISE_MAX,
        .default_value = BPBXSYN_LIMITER_LIMIT_RISE_DEFAULT,
    },
    {
        .name = "Boost Threshold",
        .id = "bstThres",
        .type = BPBXSYN_PARAM_DOUBLE,
        
        .min_value =     BPBXSYN_LIMITER_BOOST_THRESHOLD_MIN,
        .max_value =     BPBXSYN_LIMITER_BOOST_THRESHOLD_MAX,
        .default_value = BPBXSYN_LIMITER_BOOST_THRESHOLD_DEFAULT,
    },
    {
        .name = "Boost Ratio",
        .id = "bstRatio",
        .type = BPBXSYN_PARAM_DOUBLE,
        
        .min_value =     BPBXSYN_LIMITER_BOOST_RATIO_MIN,
        .max_value =     BPBXSYN_LIMITER_BOOST_RATIO_MAX,
        .default_value = BPBXSYN_LIMITER_BOOST_RATIO_DEFAULT,
    },
    {
        .name = "Cutoff Threshold",
        .id = "ctfThres",
        .type = BPBXSYN_PARAM_DOUBLE,
        
        .min_value =     BPBXSYN_LIMITER_CUTOFF_THRESHOLD_MIN,
        .max_value =     BPBXSYN_LIMITER_CUTOFF_THRESHOLD_MAX,
        .default_value = BPBXSYN_LIMITER_CUTOFF_THRESHOLD_DEFAULT,
    },
    {
        .name = "Cutoff Ratio",
        .id = "ctfRatio",
        .type = BPBXSYN_PARAM_DOUBLE,
        
        .min_value =     BPBXSYN_LIMITER_CUTOFF_THRESHOLD_MIN,
        .max_value =     BPBXSYN_LIMITER_CUTOFF_THRESHOLD_MAX,
        .default_value = BPBXSYN_LIMITER_CUTOFF_THRESHOLD_DEFAULT,
    },
};

static const size_t param_addresses[BPBXSYN_LIMITER_PARAM_COUNT] = {
    offsetof(limiter_effect_s, master_gain),
    offsetof(limiter_effect_s, decay),
    offsetof(limiter_effect_s, rise),
    offsetof(limiter_effect_s, compression_threshold),
    offsetof(limiter_effect_s, compression_ratio),
    offsetof(limiter_effect_s, limit_threshold),
    offsetof(limiter_effect_s, limit_ratio),
};

const effect_vtable_s effect_limiter_vtable = {
    .struct_size = sizeof(limiter_effect_s),
    .effect_init = (effect_init_f)bpbxsyn_effect_init_limiter,
    .effect_destroy = limiter_destroy,

    .input_channel_count = 2,
    .output_channel_count = 2,

    .param_count = BPBXSYN_LIMITER_PARAM_COUNT,
    .param_info = param_info,
    .param_addresses = param_addresses,

    .effect_tick = limiter_tick,
    .effect_run = limiter_run
};