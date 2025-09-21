#include "reverb.h"

#include <assert.h>
#include <math.h>
#include <string.h>
#include "../alloc.h"
#include "../log.h"
#include "../util.h"
#include "../audio.h"
#include "../filtering.h"


// TODO: think of better/more efficient way of making reverb sample
// rate-agnostic. i just "scale" the buffer operations and this requires me to
// use modulo rather than and. Not as performant then, i guess.
#define REVERB_DELAY_BUFFER_SIZE 16384
#define REVERB_SHELF_HZ 8000.0 // The cutoff freq of the shelf filter that is
                               // used to decay reverb.
#define REVERB_SHELF_GAIN (pow(2.0, -1.5))

void bpbxsyn_effect_init_reverb(bpbxsyn_context_s *ctx, reverb_effect_s *inst) {
    *inst = (reverb_effect_s) {
        .base.type = BPBXSYN_EFFECT_REVERB,
        .base.ctx = ctx
    };
}

void reverb_destroy(bpbxsyn_effect_s *p_inst) {
    reverb_effect_s *const inst = (reverb_effect_s*)p_inst;
    const bpbxsyn_context_s *ctx = inst->base.ctx;

    bpbxsyn_free(ctx, inst->delay_line);
}

void reverb_stop(bpbxsyn_effect_s *p_inst) {
    reverb_effect_s *const inst = (reverb_effect_s*)p_inst;

    if (inst->delay_line) {
        memset(inst->delay_line, 0, inst->delay_line_size * sizeof(float));
    }
}

void reverb_sample_rate_changed(bpbxsyn_effect_s *p_inst, double old_sr,
                                double new_sr)
{
    if (new_sr == old_sr)
        return;

    reverb_effect_s *const inst = (reverb_effect_s*)p_inst;
    const bpbxsyn_context_s *ctx = inst->base.ctx;

    bpbxsyn_free(ctx, inst->delay_line);

    // good ratios:    0.555235 + 0.618033 + 0.818 +   1.0 = 2.991268
    // Delay lengths:  3041     + 3385     + 4481  +  5477 = 16384 = 2^14
    // Buffer offsets: 3041    -> 6426   -> 10907 -> 16384
    const int size =
        (int)((double)REVERB_DELAY_BUFFER_SIZE / 48000.0 * new_sr);
    inst->delay_offsets[0] = (int)((3041.0  / 48000.0) * new_sr);
    inst->delay_offsets[1] = (int)((6426.0  / 48000.0) * new_sr);
    inst->delay_offsets[2] = (int)((10907.0 / 48000.0) * new_sr);

    inst->delay_line_size = size;
    size_t alloc_size = (size_t)inst->delay_line_size * sizeof(float);
    inst->delay_line = bpbxsyn_malloc(ctx, alloc_size);
    memset(inst->delay_line, 0, alloc_size);
    
    if (!inst->delay_line) {
        logmsgf(ctx, BPBXSYN_LOG_ERROR, "Could not allocate reverb delay line");
    }
}

void reverb_tick(bpbxsyn_effect_s *p_inst, const bpbxsyn_tick_ctx_s *ctx) {
    reverb_effect_s *const inst = (reverb_effect_s*)p_inst;

    const double rounded_samples_per_tick =
        ceil(calc_samples_per_tick(ctx->bpm, inst->base.sample_rate));

    double use_reverb_start = inst->param[0];
    double use_reverb_end = inst->param[1];

    double reverb_start = min(1.0, pow(use_reverb_start / BPBXSYN_REVERB_RANGE, 0.667)) * 0.425;
    double reverb_end = min(1.0, pow(use_reverb_end / BPBXSYN_REVERB_RANGE, 0.667)) * 0.425;

    inst->mult = reverb_start;
    inst->mult_delta = (reverb_end - reverb_start) / rounded_samples_per_tick;
    // maxReverbMult = Math.max(reverb_start, reverb_end);

    double shelf_radians = 2.0 * PI * REVERB_SHELF_HZ / inst->base.sample_rate;
    filter_coefs_s coefs;
    filter_hshelf1(&coefs, shelf_radians, REVERB_SHELF_GAIN);

    inst->shelf_a1 = coefs.a[1];
    inst->shelf_b0 = coefs.b[0];
    inst->shelf_b1 = coefs.b[1];

    inst->param[0] = inst->param[1];
}

void reverb_run(bpbxsyn_effect_s *p_inst, float **buffer, size_t frame_count)
{
    reverb_effect_s *const inst = (reverb_effect_s*)p_inst;

    // const int mask = inst->delay_line_mask;
    const int wrap = inst->delay_line_size;
    float *const delay_line = inst->delay_line;
    // instrumentState.reverbDelayLineDirty = true;
    int delay_pos = inst->delay_pos % wrap;
    
    double reverb = inst->mult;
    const double delta = inst->mult_delta;
    
    double shelf_sample[4];
    double shelf_prev_input[4];
    int offset[3];

    const double shelf_a1 = inst->shelf_a1;
    const double shelf_b0 = inst->shelf_b0;
    const double shelf_b1 = inst->shelf_b1;
    
    shelf_sample[0] = inst->shelf_sample[0];
    shelf_sample[1] = inst->shelf_sample[1];
    shelf_sample[2] = inst->shelf_sample[2];
    shelf_sample[3] = inst->shelf_sample[3];
    shelf_prev_input[0] = inst->shelf_prev_input[0];
    shelf_prev_input[1] = inst->shelf_prev_input[1];
    shelf_prev_input[2] = inst->shelf_prev_input[2];
    shelf_prev_input[3] = inst->shelf_prev_input[3];
    offset[0] = inst->delay_offsets[0];
    offset[1] = inst->delay_offsets[1];
    offset[2] = inst->delay_offsets[2];

    float *const left = buffer[0];
    float *const right = buffer[1];

    assert(left && right);

    // TODO: how is delayInputMult calculated?
    const double delayInputMult = 1.0;

    for (size_t frame = 0; frame < frame_count; ++frame) {
        double sample_l = (double)left[frame];
        double sample_r = (double)right[frame];

        // Reverb, implemented using a feedback delay network with a Hadamard matrix and lowpass filters.
        const int delay_pos1 = (delay_pos + offset[0]) % wrap;
        const int delay_pos2 = (delay_pos + offset[1]) % wrap;
        const int delay_pos3 = (delay_pos + offset[2]) % wrap;
        const double sample0 = (delay_line[delay_pos]);
        const double sample1 = delay_line[delay_pos1];
        const double sample2 = delay_line[delay_pos2];
        const double sample3 = delay_line[delay_pos3];
        const double temp0 = -(sample0 + sample_l) + sample1;
        const double temp1 = -(sample0 + sample_r) - sample1;
        const double temp2 = -sample2 + sample3;
        const double temp3 = -sample2 - sample3;
        const double shelf_input0 = (temp0 + temp2) * reverb;
        const double shelf_input1 = (temp1 + temp3) * reverb;
        const double shelf_input2 = (temp0 - temp2) * reverb;
        const double shelf_input3 = (temp1 - temp3) * reverb;
        shelf_sample[0] = shelf_b0 * shelf_input0 + shelf_b1 * shelf_prev_input[0] - shelf_a1 * shelf_sample[0];
        shelf_sample[1] = shelf_b0 * shelf_input1 + shelf_b1 * shelf_prev_input[1] - shelf_a1 * shelf_sample[1];
        shelf_sample[2] = shelf_b0 * shelf_input2 + shelf_b1 * shelf_prev_input[2] - shelf_a1 * shelf_sample[2];
        shelf_sample[3] = shelf_b0 * shelf_input3 + shelf_b1 * shelf_prev_input[3] - shelf_a1 * shelf_sample[3];
        shelf_prev_input[0] = shelf_input0;
        shelf_prev_input[1] = shelf_input1;
        shelf_prev_input[2] = shelf_input2;
        shelf_prev_input[3] = shelf_input3;
        delay_line[delay_pos1] = (float)(shelf_sample[0] * delayInputMult);
        delay_line[delay_pos2] = (float)(shelf_sample[1] * delayInputMult);
        delay_line[delay_pos3] = (float)(shelf_sample[2] * delayInputMult);
        delay_line[delay_pos ] = (float)(shelf_sample[3] * delayInputMult);
        delay_pos = (delay_pos + 1) % wrap;
        sample_l += sample1 + sample2 + sample3;
        sample_r += sample0 + sample2 - sample3;
        reverb += delta;

        left[frame] = (float)sample_l;
        right[frame] = (float)sample_r;
    }

    sanitize_delay_line_mod(delay_line, delay_pos            , wrap);
    sanitize_delay_line_mod(delay_line, delay_pos + offset[0], wrap);
    sanitize_delay_line_mod(delay_line, delay_pos + offset[1], wrap);
    sanitize_delay_line_mod(delay_line, delay_pos + offset[2], wrap);
    inst->delay_pos = delay_pos;
    inst->mult = reverb;
    
    if (!isfinite(shelf_sample[0]) || fabs(shelf_sample[0]) < FLUSH_ZERO_EPSILON) shelf_sample[0] = 0.0;
    if (!isfinite(shelf_sample[1]) || fabs(shelf_sample[1]) < FLUSH_ZERO_EPSILON) shelf_sample[1] = 0.0;
    if (!isfinite(shelf_sample[2]) || fabs(shelf_sample[2]) < FLUSH_ZERO_EPSILON) shelf_sample[2] = 0.0;
    if (!isfinite(shelf_sample[3]) || fabs(shelf_sample[3]) < FLUSH_ZERO_EPSILON) shelf_sample[3] = 0.0;
    if (!isfinite(shelf_prev_input[0]) || fabs(shelf_prev_input[0]) < FLUSH_ZERO_EPSILON) shelf_prev_input[0] = 0.0;
    if (!isfinite(shelf_prev_input[1]) || fabs(shelf_prev_input[1]) < FLUSH_ZERO_EPSILON) shelf_prev_input[1] = 0.0;
    if (!isfinite(shelf_prev_input[2]) || fabs(shelf_prev_input[2]) < FLUSH_ZERO_EPSILON) shelf_prev_input[2] = 0.0;
    if (!isfinite(shelf_prev_input[3]) || fabs(shelf_prev_input[3]) < FLUSH_ZERO_EPSILON) shelf_prev_input[3] = 0.0;

    inst->shelf_sample[0] = shelf_sample[0];
    inst->shelf_sample[1] = shelf_sample[1];
    inst->shelf_sample[2] = shelf_sample[2];
    inst->shelf_sample[3] = shelf_sample[3];
    inst->shelf_prev_input[0] = shelf_prev_input[0];
    inst->shelf_prev_input[1] = shelf_prev_input[1];
    inst->shelf_prev_input[2] = shelf_prev_input[2];
    inst->shelf_prev_input[3] = shelf_prev_input[3];
}


static const bpbxsyn_param_info_s param_info[BPBXSYN_REVERB_PARAM_COUNT] = {
    {
        .group = "Effects/Reverb",
        .name = "Reverb",
        .id = "reverb\0\0",
        .type = BPBXSYN_PARAM_DOUBLE,
        
        .min_value = 0.0,
        .max_value = BPBXSYN_REVERB_MAX,
        .default_value = 0.0,
    },
};

static const size_t param_addresses[BPBXSYN_REVERB_PARAM_COUNT] = {
    offsetof(reverb_effect_s, param[1]),
};

const effect_vtable_s effect_reverb_vtable = {
    .struct_size = sizeof(reverb_effect_s),
    .effect_init = (effect_init_f)bpbxsyn_effect_init_reverb,
    .effect_destroy = reverb_destroy,

    .input_channel_count = 2,
    .output_channel_count = 2,

    .param_count = BPBXSYN_REVERB_PARAM_COUNT,
    .param_info = param_info,
    .param_addresses = param_addresses,

    .effect_stop = reverb_stop,
    .effect_sample_rate_changed = reverb_sample_rate_changed,
    .effect_tick = reverb_tick,
    .effect_run = reverb_run
};