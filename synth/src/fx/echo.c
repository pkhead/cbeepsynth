#include "echo.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#include "../audio.h"
#include "../alloc.h"
#include "../util.h"
#include "../filtering.h"
#include "../log.h"

#define ECHO_SHELF_HZ 4000.0 // The cutoff freq of the shelf filter that is used
                             // to decay echoes.
#define ECHO_SHELF_GAIN (pow(2.0, -0.5))

#define ECHO_DELAY_STEP_TICKS 4

/*
There is an implementation detail that i must have differ from the original
BeepBox implementation.

It seems that BeepBox reallocates buffers while the effect is running in
accordance to tempo/delay changes. Since I have the restriction that I cannot
perform memory allocations or deallocations while the effect is running, I must
instead have this buffer allocated up-front with a certain length that will fit
a reasonable maximum. This length will have to depend only on the sample rate.
*/
#define ECHO_MAX_DELAY_SECS 5.0

#define ECHO_SUSTAIN_DEFAULT 3
#define ECHO_DELAY_DEFAULT 11

void bpbxsyn_effect_init_echo(bpbxsyn_context_s *ctx, echo_effect_s *inst) {
    *inst = (echo_effect_s){
        .base.type = BPBXSYN_EFFECT_ECHO,
        .base.ctx = ctx,

        .sustain[0] = ECHO_SUSTAIN_DEFAULT,
        .sustain[1] = ECHO_SUSTAIN_DEFAULT,
        .delay[0] = ECHO_DELAY_DEFAULT,
        .delay[1] = ECHO_DELAY_DEFAULT,
    };
}

void echo_stop(bpbxsyn_effect_s *p_inst) {
    echo_effect_s *const inst = (echo_effect_s *)p_inst;
    
    for (int i = 0; i < 2; ++i) {
        if (inst->delay_lines[i][0])
            memset(inst->delay_lines[i][0], 0, inst->delay_line_size * sizeof(float));
    
        if (inst->delay_lines[i][1])
            memset(inst->delay_lines[i][1], 0, inst->delay_line_size * sizeof(float));
    }

    inst->echo_shelf_sample[0] = 0.0;
    inst->echo_shelf_sample[1] = 0.0;
    inst->echo_shelf_prev_input[0] = 0.0;
    inst->echo_shelf_prev_input[1] = 0.0;
}

void echo_destroy(bpbxsyn_effect_s *p_inst) {
    echo_effect_s *const inst = (echo_effect_s *)p_inst;
    const bpbxsyn_context_s *ctx = inst->base.ctx;

    for (int i = 0; i < 2; ++i) {
        bpbxsyn_free(ctx, inst->delay_lines[i][0]);
        bpbxsyn_free(ctx, inst->delay_lines[i][1]);
        inst->delay_lines[i][0] = NULL;
        inst->delay_lines[i][1] = NULL;
    }
}

void echo_sample_rate_changed(bpbxsyn_effect_s *p_inst,
                                 double old_sr, double new_sr)
{
    (void)old_sr;

    echo_effect_s *const inst = (echo_effect_s *)p_inst;
    const bpbxsyn_context_s *ctx = inst->base.ctx;
    
    int delay_line_capacity = (int)ceil(new_sr * ECHO_MAX_DELAY_SECS);
    delay_line_capacity = fitting_power_of_two(delay_line_capacity);
    inst->delay_line_capacity = delay_line_capacity;

    for (int i = 0; i < 2; ++i) {
        bpbxsyn_free(ctx, inst->delay_lines[i][0]);
        bpbxsyn_free(ctx, inst->delay_lines[i][1]);

        size_t alloc_size = (size_t)delay_line_capacity * sizeof(float);
        inst->delay_lines[i][0] = bpbxsyn_malloc(ctx, alloc_size);
        inst->delay_lines[i][1] = bpbxsyn_malloc(ctx, alloc_size);
        inst->delay_line_dirty = true;

        if (!inst->delay_lines[i][0])
            logmsgf(ctx, BPBXSYN_LOG_ERROR, "could not allocate echo delay_lines[%i].L", i);
    
        if (!inst->delay_lines[i][1])
            logmsgf(ctx, BPBXSYN_LOG_ERROR, "could not allocate echo delay_lines[%i].R", i);
    
        if (!inst->delay_lines[i][0] || !inst->delay_lines[i][1]) {
            bpbxsyn_free(ctx, inst->delay_lines[i][0]);
            bpbxsyn_free(ctx, inst->delay_lines[i][1]);
            inst->delay_lines[i][0] = NULL;
            inst->delay_lines[i][1] = NULL;
        }
    }

}

static void echo_realloc_buffers_if_necessary(echo_effect_s *inst,
                                              double samples_per_tick)
{
    // The delay may be very short now, but if it increases later make sure we
    // have enough sample history.
    int safe_echo_delay_steps =
        max((double)(BPBXSYN_ECHO_DELAY_RANGE >> 1), (inst->delay[1] + 1));
    
    int base_echo_delay_buffer_size =
        fitting_power_of_two((int)(safe_echo_delay_steps * ECHO_DELAY_STEP_TICKS * samples_per_tick));
    
    // If the tempo or delay changes and we suddenly need a longer delay, make 
    // sure that we have enough sample history to accomodate the longer delay.
    int safe_echo_delay_buffer_size = base_echo_delay_buffer_size * 2;

    if (safe_echo_delay_buffer_size > inst->delay_line_capacity)
        safe_echo_delay_buffer_size = inst->delay_line_capacity;

    if (inst->delay_line_size < safe_echo_delay_buffer_size) {
        // determine old and new buffers
        float *old_delay_line[2];
        old_delay_line[0] = inst->delay_lines[inst->delay_line_buffer_idx][0];
        old_delay_line[1] = inst->delay_lines[inst->delay_line_buffer_idx][1];

        int new_buffer_idx = (inst->delay_line_buffer_idx + 1) % 2;
        float *new_delay_line[2];
        new_delay_line[0] = inst->delay_lines[new_buffer_idx][0];
        new_delay_line[1] = inst->delay_lines[new_buffer_idx][1];

        // clear new buffer
        memset(new_delay_line[0], 0, safe_echo_delay_buffer_size * sizeof(float));
        memset(new_delay_line[1], 0, safe_echo_delay_buffer_size * sizeof(float));

        // The echo delay length may change whlie the song is playing if tempo
        // changes, so buffers may need to be reallocated, but we don't want to
        // lose any echoes so we need to copy the contents of the old buffer to
        // the new one.
        int old_mask = inst->delay_line_size - 1;
        const int pos = inst->delay_line_pos;
        for (int i = 0; i < inst->delay_line_size; ++i) {
            new_delay_line[0][i] = old_delay_line[0][(pos + i) & old_mask];
            new_delay_line[1][i] = old_delay_line[1][(pos + i) & old_mask];
        }
        
        inst->delay_line_pos = inst->delay_line_size;

        // buffer swap
        inst->delay_line_buffer_idx = new_buffer_idx;
        inst->delay_line_size = safe_echo_delay_buffer_size;
    }
}

void echo_tick(bpbxsyn_effect_s *p_inst, const bpbxsyn_tick_ctx_s *ctx) {
    echo_effect_s *const inst = (echo_effect_s *)p_inst;

    if (!inst->delay_lines[inst->delay_line_buffer_idx][0])
        return;

    double samples_per_tick = calc_samples_per_tick(ctx->bpm, inst->base.sample_rate);
    double rounded_samples_per_tick =
        ceil(samples_per_tick);
    
    echo_realloc_buffers_if_necessary(inst, samples_per_tick);

    double use_echo_sustain_start = inst->sustain[0];
    double use_echo_sustain_end = inst->sustain[1];
    
    // TODO: but range is max - 1 ?
    const double echo_mult_start = min(1.0, pow(use_echo_sustain_start / BPBXSYN_ECHO_SUSTAIN_RANGE, 1.1)) * 0.9;
    const double echo_mult_end = min(1.0, pow(use_echo_sustain_end / BPBXSYN_ECHO_SUSTAIN_RANGE, 1.1)) * 0.9;

    inst->echo_mult = echo_mult_start;
    inst->echo_mult_delta = max(0.0, (echo_mult_end - echo_mult_start) / rounded_samples_per_tick);

    // double max_echo_mult = max(echo_mult_start, echo_mult_end);

    // TODO: After computing a tick's settings once for multiple run lengths (which is
    // good for audio worklet threads), compute the echo delay envelopes at tick (or
    // part) boundaries to interpolate between two delay taps.
    double use_echo_delay_start = inst->delay[0];
    double use_echo_delay_end = inst->delay[1];
    // bool ignore_ticks = false; // TODO: what to do with ignoreTicks?

    const double tmp_echo_delay_offset_start = round((use_echo_delay_start + 1) * ECHO_DELAY_STEP_TICKS * samples_per_tick);
    const double tmp_echo_delay_offset_end = round((use_echo_delay_end + 1) * ECHO_DELAY_STEP_TICKS * samples_per_tick);

    inst->delay_offset_start = tmp_echo_delay_offset_start;
    // if (this.echoDelayOffsetEnd != null && !ignoreTicks) {
    //     this.echoDelayOffsetStart = this.echoDelayOffsetEnd;
    // } else {
    //     this.echoDelayOffsetStart = tmpEchoDelayOffsetStart;
    // }

    inst->delay_offset_end = tmp_echo_delay_offset_end;
    // double avg_echo_delay_secs = (inst->delay_offset_start + inst->delay_offset_end) * 0.5 / inst->base.sample_rate;

    inst->delay_offset_ratio = 0.0;
    inst->delay_offset_ratio_delta = 1.0 / rounded_samples_per_tick;

    filter_coefs_s coefs;
    const double shelf_radins = PI2 * ECHO_SHELF_HZ / inst->base.sample_rate;
    filter_hshelf1(&coefs, shelf_radins, ECHO_SHELF_GAIN);

    inst->echo_shelf_a1 = coefs.a[1];
    inst->echo_shelf_b0 = coefs.b[0];
    inst->echo_shelf_b1 = coefs.b[1];

    inst->sustain[0] = inst->sustain[1];
    inst->delay[0] = inst->delay[1];
}

void echo_run(bpbxsyn_effect_s *p_inst, float **buffer,
                 size_t frame_count)
{
    echo_effect_s *const inst = (echo_effect_s *)p_inst;

    assert(inst->delay_lines[inst->delay_line_buffer_idx][0] && inst->delay_lines[inst->delay_line_buffer_idx][1]);
    if (!inst->delay_lines[inst->delay_line_buffer_idx][0] ||
        !inst->delay_lines[inst->delay_line_buffer_idx][1])
        return;

    float *delay_line[2];
    delay_line[0] = inst->delay_lines[inst->delay_line_buffer_idx][0];
    delay_line[1] = inst->delay_lines[inst->delay_line_buffer_idx][1];

    float *left = buffer[0];
    float *right = buffer[1];

    double mult = inst->echo_mult;
    const double mult_delta = inst->echo_mult_delta;
    
    const int mask = inst->delay_line_size - 1;
    inst->delay_line_dirty = true;
    
    int delay_pos = inst->delay_line_pos & mask;
    const int delay_offset_start = (int)(inst->delay_line_size - inst->delay_offset_start) & mask;
    const int delay_offset_end   = (int)(inst->delay_line_size - inst->delay_offset_end) & mask;
    double delay_offset_ratio = inst->delay_offset_ratio;
    const double delay_offset_ratio_delta = inst->delay_offset_ratio_delta;
    
    const double shelf_a1 = inst->echo_shelf_a1;
    const double shelf_b0 = inst->echo_shelf_b0;
    const double shelf_b1 = inst->echo_shelf_b1;

    double shelf_sample[2];
    double shelf_prev_input[2];
    shelf_sample[0] = inst->echo_shelf_sample[0];
    shelf_sample[1] = inst->echo_shelf_sample[1];
    shelf_prev_input[0] = inst->echo_shelf_prev_input[0];
    shelf_prev_input[1] = inst->echo_shelf_prev_input[1];
    
    // TODO: how is delayInputMult calculated?
    const double delay_input_mult = 1.0;

    for (size_t frame = 0; frame < frame_count; ++frame) {
        double sample_l = (double)left[frame];
        double sample_r = (double)right[frame];

        const int tap_start_index = (delay_pos + delay_offset_start) & mask;
        const int tap_end_index   = (delay_pos + delay_offset_end  ) & mask;
        const double tap_start_l  = (double)delay_line[0][tap_start_index];
        const double tap_end_l    = (double)delay_line[0][tap_end_index];
        const double tap_start_r  = (double)delay_line[1][tap_start_index];
        const double tap_end_r    = (double)delay_line[1][tap_end_index];
        const double tap_l = (tap_start_l + (tap_end_l - tap_start_l) * delay_offset_ratio) * mult;
        const double tap_r = (tap_start_r + (tap_end_r - tap_start_r) * delay_offset_ratio) * mult;
        
        shelf_sample[0] =
            shelf_b0 * tap_l + shelf_b1 * shelf_prev_input[0]
                - shelf_a1 * shelf_sample[0];
        shelf_sample[1] =
            shelf_b0 * tap_r + shelf_b1 * shelf_prev_input[1]
                - shelf_a1 * shelf_sample[1];
        
        shelf_prev_input[0] = tap_l;
        shelf_prev_input[1] = tap_r;
        sample_l += shelf_sample[0];
        sample_r += shelf_sample[1];
        
        delay_line[0][delay_pos] = sample_l * delay_input_mult;
        delay_line[1][delay_pos] = sample_r * delay_input_mult;
        delay_pos = (delay_pos + 1) & mask;
        delay_offset_ratio += delay_offset_ratio_delta;
        mult += mult_delta;
        
        left[frame] = sample_l;
        right[frame] = sample_r;
    }

    sanitize_delay_line(delay_line[0], delay_pos, mask);
    sanitize_delay_line(delay_line[1], delay_pos, mask);

    inst->delay_line_pos = delay_pos;
    inst->echo_mult = mult;
    inst->delay_offset_ratio = delay_offset_ratio;
    
    if (!isfinite(shelf_sample[0]) || fabs(shelf_sample[0]) < FLUSH_ZERO_EPSILON)
        shelf_sample[0] = 0.0;
    if (!isfinite(shelf_sample[1]) || fabs(shelf_sample[1]) < FLUSH_ZERO_EPSILON)
        shelf_sample[1] = 0.0;
    if (!isfinite(shelf_prev_input[0]) || fabs(shelf_prev_input[0]) < FLUSH_ZERO_EPSILON)
        shelf_prev_input[0] = 0.0;
    if (!isfinite(shelf_prev_input[1]) || fabs(shelf_prev_input[1]) < FLUSH_ZERO_EPSILON)
        shelf_prev_input[1] = 0.0;

    inst->echo_shelf_sample[0] = shelf_sample[0];
    inst->echo_shelf_sample[1] = shelf_sample[1];
    inst->echo_shelf_prev_input[0] = shelf_prev_input[0];
    inst->echo_shelf_prev_input[1] = shelf_prev_input[1];
}







static const bpbxsyn_param_info_s param_info[BPBXSYN_PANNING_PARAM_COUNT] = {
    {
        .group = "Effects/Echo",
        .name = "Echo",
        .id = "echoSus\0",
        .type = BPBXSYN_PARAM_DOUBLE,
        
        .min_value = 0.0,
        .max_value = BPBXSYN_ECHO_SUSTAIN_MAX,
        .default_value = ECHO_SUSTAIN_DEFAULT,
    },
    {
        .group = "Effects/Echo",
        .name = "Echo Delay",
        .id = "echoDly\0",
        .type = BPBXSYN_PARAM_DOUBLE,
        
        .min_value = 0.0,
        .max_value = BPBXSYN_ECHO_DELAY_MAX,
        .default_value = ECHO_DELAY_DEFAULT,

        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION
    },
};

static const size_t param_addresses[BPBXSYN_PANNING_PARAM_COUNT] = {
    offsetof(echo_effect_s, sustain[1]),
    offsetof(echo_effect_s, delay[1]),
};

const effect_vtable_s effect_echo_vtable = {
    .struct_size = sizeof(echo_effect_s),
    .effect_init = (effect_init_f)bpbxsyn_effect_init_echo,
    .effect_destroy = echo_destroy,

    .input_channel_count = 2,
    .output_channel_count = 2,

    .param_count = BPBXSYN_ECHO_PARAM_COUNT,
    .param_info = param_info,
    .param_addresses = param_addresses,

    .effect_stop = echo_stop,
    .effect_sample_rate_changed = echo_sample_rate_changed,
    .effect_tick = echo_tick,
    .effect_run = echo_run
};