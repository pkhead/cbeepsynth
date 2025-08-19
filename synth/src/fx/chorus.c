#include "chorus.h"

#include <assert.h>
#include <string.h>
#include "audio.h"
#include "alloc.h"
#include "util.h"
#include "log.h"

#define CHORUS_PERIOD_SECONDS 2.0
#define CHORUS_DELAY_RANGE 0.0034

#define CHORUS_CHANNEL_COUNT 2
#define CHORUS_VOICE_COUNT 3

static const double chorus_delay_offsets[CHORUS_CHANNEL_COUNT][CHORUS_VOICE_COUNT] = {
    {1.51, 2.10, 3.35},
    {1.47, 2.15, 3.25}
};

static const double chorus_phase_offsets[CHORUS_CHANNEL_COUNT][CHORUS_VOICE_COUNT] = {
    {0.0, 2.1, 4.2},
    {3.2, 5.3, 1.0}
};

static double chorus_max_delay(void) {
    // Config.chorusDelayRange * (1.0 + Config.chorusDelayOffsets[0].concat(Config.chorusDelayOffsets[1]).reduce((x, y) => Math.max(x, y)));
    double max = chorus_delay_offsets[0][0];

    for (int i = 0; i < CHORUS_CHANNEL_COUNT; ++i) {
        for (int j = 0; j < CHORUS_VOICE_COUNT; ++j) {
            double v = chorus_delay_offsets[i][j];
            if (v > max) max = v;
        }
    }

    return CHORUS_DELAY_RANGE * max;
}

void bpbxsyn_effect_init_chorus(bpbxsyn_context_s *ctx, chorus_effect_s *inst) {
    *inst = (chorus_effect_s){
        .base.ctx = ctx,
        .base.type = BPBXSYN_EFFECT_CHORUS
    };
}

void chorus_stop(bpbxsyn_effect_s *p_inst) {
    chorus_effect_s *const inst = (chorus_effect_s*)p_inst;
    
    if (inst->delay_line[0])
        memset(inst->delay_line[0], 0, inst->delay_line_size * sizeof(float));

    if (inst->delay_line[1])
        memset(inst->delay_line[1], 0, inst->delay_line_size * sizeof(float));

    inst->delay_pos = 0;
}

void chorus_destroy(bpbxsyn_effect_s *p_inst) {
    chorus_effect_s *const inst = (chorus_effect_s *)p_inst;
    const bpbxsyn_context_s *ctx = inst->base.ctx;

    bpbxsyn_free(inst->base.ctx, inst->delay_line_alloc);
    inst->delay_line_alloc = NULL;
    inst->delay_line[0] = NULL;
    inst->delay_line[1] = NULL;
}

void chorus_sample_rate_changed(bpbxsyn_effect_s *p_inst,
                                 double old_sr, double new_sr) {
    chorus_effect_s *const inst = (chorus_effect_s *)p_inst;
    const bpbxsyn_context_s *ctx = inst->base.ctx;
    if (old_sr == new_sr) return;

    // reallocate/resize delay line
    bpbxsyn_free(ctx, inst->delay_line_alloc);

    inst->delay_line_size = fitting_power_of_two(new_sr * chorus_max_delay());
    inst->delay_line_mask = inst->delay_line_size - 1;

    inst->delay_line_alloc_size = (size_t)inst->delay_line_size * 2;
    inst->delay_line_alloc = bpbxsyn_malloc(ctx, inst->delay_line_alloc_size * sizeof(float));

    assert(inst->delay_line_alloc);
    if (!inst->delay_line_alloc) {
        inst->delay_line[0] = NULL;
        inst->delay_line[1] = NULL;
        logmsgf(ctx, BPBXSYN_LOG_ERROR, "Could not allocate chorus delay line");
        return;
    }

    memset(inst->delay_line_alloc, 0, inst->delay_line_alloc_size);
    inst->delay_line[0] = inst->delay_line_alloc;
    inst->delay_line[1] = inst->delay_line_alloc + inst->delay_line_size;
}

void chorus_tick(bpbxsyn_effect_s *p_inst, const bpbxsyn_tick_ctx_s *ctx) {
    chorus_effect_s *const inst = (chorus_effect_s *)p_inst;

    double rounded_samples_per_tick =
        ceil(calc_samples_per_tick(ctx->bpm, inst->base.sample_rate));
    
    double use_chorus_start = inst->param[0];
    double use_chorus_end = inst->param[1];

    double chorus_start = min(1.0, use_chorus_start / BPBXSYN_CHORUS_MAX);
    double chorus_end = min(1.0, use_chorus_end / BPBXSYN_CHORUS_MAX);
    chorus_start = chorus_start * 0.6 + (pow(chorus_start, 6.0)) * 0.4;
    chorus_end = chorus_end * 0.6 + (pow(chorus_end, 6.0)) * 0.4;
    const double chorus_combined_mult_start = 1.0 / sqrt(3.0 * chorus_start * chorus_start + 1.0);
    const double chorus_combined_mult_end = 1.0 / sqrt(3.0 * chorus_end * chorus_end + 1.0);

    inst->voice_mult = chorus_start;
    inst->voice_mult_delta = (chorus_end - chorus_start) / rounded_samples_per_tick;
    inst->combined_mult = chorus_combined_mult_start;
    inst->combined_mult_delta = (chorus_combined_mult_end - chorus_combined_mult_start) / rounded_samples_per_tick;

    inst->param[0] = inst->param[1];
}

void chorus_run(bpbxsyn_effect_s *p_inst, float **buffer,
                 size_t frame_count)
{
    chorus_effect_s *const inst = (chorus_effect_s*)p_inst;

    assert(inst->delay_line[0]);
    assert(inst->delay_line[1]);
    if (!inst->delay_line[0] || !inst->delay_line[1])
        return;

    const int mask = inst->delay_line_mask;
    float *const delay_line_l = inst->delay_line[0];
    float *const delay_line_r = inst->delay_line[1];
    // instrumentState.chorusDelayLineDirty = true;
    int delay_pos = inst->delay_pos & mask;
    
    double voice_mult = inst->voice_mult;
    const double voice_mult_delta = inst->voice_mult_delta;
    double combined_mult = inst->combined_mult;
    const double combined_mult_delta = inst->combined_mult_delta;

    const double sample_rate = inst->base.sample_rate;
    const double delay_input_mult = 1.0; // TODO: how is delay_input_mult calculated?
    
    const double duration = CHORUS_PERIOD_SECONDS;
    const double angle = PI * 2.0 / (duration * sample_rate);
    const double range = sample_rate * CHORUS_DELAY_RANGE;
    const double offset0 = inst->delay_line_size - chorus_delay_offsets[0][0] * range;
    const double offset1 = inst->delay_line_size - chorus_delay_offsets[0][1] * range;
    const double offset2 = inst->delay_line_size - chorus_delay_offsets[0][2] * range;
    const double offset3 = inst->delay_line_size - chorus_delay_offsets[1][0] * range;
    const double offset4 = inst->delay_line_size - chorus_delay_offsets[1][1] * range;
    const double offset5 = inst->delay_line_size - chorus_delay_offsets[1][2] * range;
    double phase = fmod(inst->phase, PI * 2.0);
    double tap0_index = delay_pos + offset0 - range * sin(phase + chorus_phase_offsets[0][0]);
    double tap1_index = delay_pos + offset1 - range * sin(phase + chorus_phase_offsets[0][1]);
    double tap2_index = delay_pos + offset2 - range * sin(phase + chorus_phase_offsets[0][2]);
    double tap3_index = delay_pos + offset3 - range * sin(phase + chorus_phase_offsets[1][0]);
    double tap4_index = delay_pos + offset4 - range * sin(phase + chorus_phase_offsets[1][1]);
    double tap5_index = delay_pos + offset5 - range * sin(phase + chorus_phase_offsets[1][2]);
    phase += angle * frame_count;
    const double tap0_end = delay_pos + offset0 - range * sin(phase + chorus_phase_offsets[0][0]) + frame_count;
    const double tap1_end = delay_pos + offset1 - range * sin(phase + chorus_phase_offsets[0][1]) + frame_count;
    const double tap2_end = delay_pos + offset2 - range * sin(phase + chorus_phase_offsets[0][2]) + frame_count;
    const double tap3_end = delay_pos + offset3 - range * sin(phase + chorus_phase_offsets[1][0]) + frame_count;
    const double tap4_end = delay_pos + offset4 - range * sin(phase + chorus_phase_offsets[1][1]) + frame_count;
    const double tap5_end = delay_pos + offset5 - range * sin(phase + chorus_phase_offsets[1][2]) + frame_count;
    const double tap0_delta = (tap0_end - tap0_index) / frame_count;
    const double tap1_delta = (tap1_end - tap1_index) / frame_count;
    const double tap2_delta = (tap2_end - tap2_index) / frame_count;
    const double tap3_delta = (tap3_end - tap3_index) / frame_count;
    const double tap4_delta = (tap4_end - tap4_index) / frame_count;
    const double tap5_delta = (tap5_end - tap5_index) / frame_count;

    float *left = buffer[0];
    float *right = buffer[1];
    for (size_t frame = 0; frame < frame_count; ++frame) {
        double sample_l = (double)left[frame];
        double sample_r = (double)right[frame];

        const double tap0_ratio = fmod(tap0_index, 1);
        const double tap1_ratio = fmod(tap1_index, 1);
        const double tap2_ratio = fmod(tap2_index, 1);
        const double tap3_ratio = fmod(tap3_index, 1);
        const double tap4_ratio = fmod(tap4_index, 1);
        const double tap5_ratio = fmod(tap5_index, 1);
        const double tap0a = delay_line_l[(int)(tap0_index) & mask];
        const double tap0b = delay_line_l[(int)(tap0_index + 1) & mask];
        const double tap1a = delay_line_l[(int)(tap1_index) & mask];
        const double tap1b = delay_line_l[(int)(tap1_index + 1) & mask];
        const double tap2a = delay_line_l[(int)(tap2_index) & mask];
        const double tap2b = delay_line_l[(int)(tap2_index + 1) & mask];
        const double tap3a = delay_line_r[(int)(tap3_index) & mask];
        const double tap3b = delay_line_r[(int)(tap3_index + 1) & mask];
        const double tap4a = delay_line_r[(int)(tap4_index) & mask];
        const double tap4b = delay_line_r[(int)(tap4_index + 1) & mask];
        const double tap5a = delay_line_r[(int)(tap5_index) & mask];
        const double tap5b = delay_line_r[(int)(tap5_index + 1) & mask];
        const double tap0 = tap0a + (tap0b - tap0a) * tap0_ratio;
        const double tap1 = tap1a + (tap1b - tap1a) * tap1_ratio;
        const double tap2 = tap2a + (tap2b - tap2a) * tap2_ratio;
        const double tap3 = tap3a + (tap3b - tap3a) * tap3_ratio;
        const double tap4 = tap4a + (tap4b - tap4a) * tap4_ratio;
        const double tap5 = tap5a + (tap5b - tap5a) * tap5_ratio;
        delay_line_l[delay_pos] = sample_l * delay_input_mult;
        delay_line_r[delay_pos] = sample_r * delay_input_mult;
        sample_l = combined_mult * (sample_l + voice_mult * (tap1 - tap0 - tap2));
        sample_r = combined_mult * (sample_r + voice_mult * (tap4 - tap3 - tap5));
        delay_pos = (delay_pos + 1) & mask;
        tap0_index += tap0_delta;
        tap1_index += tap1_delta;
        tap2_index += tap2_delta;
        tap3_index += tap3_delta;
        tap4_index += tap4_delta;
        tap5_index += tap5_delta;
        voice_mult += voice_mult_delta;
        combined_mult += combined_mult_delta;

        left[frame] = sample_l;
        right[frame] = sample_r;
    }

    sanitize_delay_line(delay_line_l, delay_pos, mask);
    sanitize_delay_line(delay_line_r, delay_pos, mask);
    inst->phase = phase;
    inst->delay_pos = delay_pos;
    inst->voice_mult = voice_mult;
    inst->combined_mult = combined_mult;
}





static const bpbxsyn_param_info_s param_info[BPBXSYN_PANNING_PARAM_COUNT] = {
    {
        .group = "Effects/Chorus",
        .name = "Chorus",
        .id = "chorus\0\0",
        .type = BPBXSYN_PARAM_DOUBLE,
        
        .min_value = 0.0,
        .max_value = BPBXSYN_CHORUS_MAX,
        .default_value = 0.0,
    },
};

static const size_t param_addresses[BPBXSYN_PANNING_PARAM_COUNT] = {
    offsetof(chorus_effect_s, param[1]),
};

const effect_vtable_s effect_chorus_vtable = {
    .struct_size = sizeof(chorus_effect_s),
    .effect_init = (effect_init_f)bpbxsyn_effect_init_chorus,
    .effect_destroy = chorus_destroy,

    .input_channel_count = 2,
    .output_channel_count = 2,

    .param_count = BPBXSYN_CHORUS_PARAM_COUNT,
    .param_info = param_info,
    .param_addresses = param_addresses,

    .effect_stop = chorus_stop,
    .effect_sample_rate_changed = chorus_sample_rate_changed,
    .effect_tick = chorus_tick,
    .effect_run = chorus_run
};