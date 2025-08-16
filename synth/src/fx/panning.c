#include "panning.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>


#include "../audio.h"
#include "../alloc.h"
#include "../util.h"

#define PAN_DELAY_SECS_MAX 0.001

// count leading zeroes
static int32_t clz32(int32_t x) {
    // TODO: intrinstic?
    int tz = 0;
    for (; x != 0; x >>= 1)
        ++tz;
    return 32 - tz;
}

static int fitting_power_of_two(int x) { return 1 << (32 - clz32(x - 1)); }

void bpbxsyn_effect_init_panning(panning_effect_s *inst) {
    *inst = (panning_effect_s){0};
    inst->base.type = BPBXSYN_EFFECT_PANNING;

    for (int i = 0; i < 2; ++i) {
        inst->pan[i] = PAN_VALUE_CENTER;
        inst->pan_delay[i] = (double)PAN_DELAY_MAX / 2;
    }
}

void panning_stop(bpbxsyn_effect_s *p_inst) {
    panning_effect_s *const inst = (panning_effect_s *)p_inst;
    
    if (inst->delay_line) {
        memset(inst->delay_line, 0, inst->delay_line_size * sizeof(float));
    }
    inst->delay_pos = 0;
}

void panning_destroy(bpbxsyn_effect_s *p_inst) {
    panning_effect_s *const inst = (panning_effect_s *)p_inst;
    bpbxsyn_free(inst->delay_line);
}

void panning_sample_rate_changed(bpbxsyn_effect_s *p_inst,
                                 double old_sr, double new_sr) {
    panning_effect_s *const inst = (panning_effect_s *)p_inst;
    if (old_sr == new_sr) return;

    bpbxsyn_free(inst->delay_line);

    inst->delay_line_size =
        fitting_power_of_two(ceil(new_sr * PAN_DELAY_SECS_MAX));
    inst->delay_line = bpbxsyn_malloc(inst->delay_line_size);
    inst->delay_buffer_mask = inst->delay_line_size - 1;
}

void panning_tick(bpbxsyn_effect_s *p_inst, const bpbxsyn_tick_ctx_s *ctx) {
    panning_effect_s *const inst = (panning_effect_s *)p_inst;
    (void)inst;
    (void)ctx;

    double rounded_samples_per_tick =
        ceil(calc_samples_per_tick(ctx->bpm, inst->base.sample_rate));
    
    double pan_start = (inst->pan[1] - PAN_VALUE_CENTER) / PAN_VALUE_CENTER;
    double pan_end = (inst->pan[0] - PAN_VALUE_CENTER) / PAN_VALUE_CENTER;
    pan_start = clampd(pan_start, -1.0, 1.0);
    pan_end = clampd(pan_end, -1.0, 1.0);

    double volume_start_l = cos((1.0 + pan_start) * PI2 * 0.25) * 1.414;
    double volume_start_r = cos((1.0 - pan_start) * PI2 * 0.25) * 1.414;
    double volume_end_l = cos((1.0 + pan_end) * PI2 * 0.25) * 1.414;
    double volume_end_r = cos((1.0 - pan_end) * PI2 * 0.25) * 1.414;
    double max_delay_samples = inst->base.sample_rate * PAN_DELAY_SECS_MAX;
    
    double use_pan_delay_start = inst->pan_delay[1];
    double use_pan_delay_end = inst->pan_delay[0];

    double delay_start =
        pan_start * use_pan_delay_start * max_delay_samples / 10;
    double delay_end =
        pan_end * use_pan_delay_end * max_delay_samples / 10;
    double delay_start_l = max(0.0, delay_start);
    double delay_start_r = max(0.0, -delay_start);
    double delay_end_l = max(0.0, delay_end);
    double delay_end_r = max(0.0, -delay_end);

    inst->volume[0] = volume_start_l;
    inst->volume[1] = volume_start_r;

    inst->volume_delta[0] =
        (volume_end_l - volume_start_l) / rounded_samples_per_tick;
    inst->volume_delta[1] =
        (volume_end_r - volume_start_r) / rounded_samples_per_tick;
    
    inst->offset[0] =
        inst->delay_pos - delay_start_l + inst->delay_line_size;
    inst->offset[1] =
        inst->delay_buffer_mask - delay_start_r + inst->delay_line_size;
    
    inst->offset_delta[0] =
        (delay_end_l - delay_start_l) / rounded_samples_per_tick;
    inst->offset_delta[1] =
        (delay_end_r - delay_start_r) / rounded_samples_per_tick;

    inst->pan[0] = inst->pan[1];
    inst->pan_delay[0] = inst->pan_delay[1];
}

void panning_run(bpbxsyn_effect_s *p_inst, float **input, float **output,
                 size_t frame_count) {
    panning_effect_s *const inst = (panning_effect_s *)p_inst;

    assert(inst->delay_line);

    double volume[2];
    double volume_delta[2];
    double offset[2];
    double offset_delta[2];

    const int mask = inst->delay_buffer_mask;
    const float *delay_line = inst->delay_line;
    int delay_pos = inst->delay_pos & mask;

    for (int i = 0; i < 2; ++i) {
        volume[i] = inst->volume[i];
        volume_delta[i] = inst->volume[i];
        offset[i] = inst->offset[i];
        offset_delta[i] = inst->offset_delta[i];
    }

    float *lin = input[0];
    float *rin = input[1];
    float *lout = output[0];
    float *rout = output[1];
    
    for (size_t frame = 0; frame < frame_count; ++frame) {
        // oh. audio before panning processing is in mono.
        // so the delay line is mono. dangit. uhhhh
        double sample = (float)((*lin + *rin) / 2.f);

        // is offset an integer..?
        double ratio_l = fmod(offset[0], 1.0);
        double ratio_r = fmod(offset[1], 1.0);
        double tap_la = delay_line[(int)(offset[0])     & mask];
        double tap_lb = delay_line[(int)(offset[0] + 1) & mask];
        double tap_ra = delay_line[(int)(offset[1])     & mask];
        double tap_rb = delay_line[(int)(offset[1] + 1) & mask];
        double tap_l = tap_la + (tap_lb - tap_la) * ratio_l;
        double tap_r = tap_ra + (tap_rb - tap_ra) * ratio_r;
        double sample_l = tap_l * volume[0];
        double sample_r = tap_r * volume[1];

        delay_pos = (delay_pos + 1) & mask;

        volume[0] += volume_delta[0];
        volume[1] += volume_delta[1];
        offset[0] += offset_delta[0];
        offset[1] += offset_delta[1];

        ++lin;
        ++rin;
        *(lout++) = sample_l;
        *(rout++) = sample_r;
    }

    for (int i = 0; i < 2; ++i) {
        inst->volume[i] = volume[i];
        inst->volume[i] = volume_delta[i];
        inst->offset[i] = offset[i];
        inst->offset_delta[i] = offset_delta[i];
    }
}







const bpbxsyn_param_info_s param_info[BPBXSYN_PANNING_PARAM_COUNT] = {
    {
        .group = "",
        .name = "Pan",
        .id = "panning\0",
        .type = BPBXSYN_PARAM_DOUBLE,
        
        .min_value = 0.0,
        .max_value = PAN_VALUE_MAX,
        .default_value = PAN_VALUE_CENTER,
    },
    {
        .group = "",
        .name = "Pan",
        .id = "panDelay",
        .type = BPBXSYN_PARAM_DOUBLE,
        
        .min_value = 0.0,
        .max_value = PAN_VALUE_MAX,
        .default_value = PAN_VALUE_CENTER,

        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION
    },
};

const size_t param_addresses[BPBXSYN_PANNING_PARAM_COUNT] = {
    offsetof(panning_effect_s, pan[1]),
    offsetof(panning_effect_s, pan_delay[1]),
};

const effect_vtable_s effect_panning_vtable = {
    .struct_size = sizeof(panning_effect_s),
    .effect_init = (effect_init_f)bpbxsyn_effect_init_panning,
    .effect_destroy = panning_destroy,

    .param_count = BPBXSYN_PANNING_PARAM_COUNT,
    .param_info = param_info,
    .param_addresses = param_addresses,

    .effect_stop = panning_stop,
    .effect_sample_rate_changed = panning_sample_rate_changed,
    .effect_tick = panning_tick,
    .effect_run = panning_run
};