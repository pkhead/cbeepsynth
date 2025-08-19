#ifndef _chorus_h_
#define _chorus_h_

#include <stddef.h>

#include "../../include/beepbox_synth.h"
#include "effect.h"

typedef struct chorus_effect {
    bpbxsyn_effect_s base;

    double param[2];

    float *delay_line_alloc;
    size_t delay_line_alloc_size;

    // left/right delay line
    float *delay_line[2];
    int delay_line_size;
    int delay_line_mask;

    int delay_pos;
    double phase;
    double voice_mult;
    double voice_mult_delta;
    double combined_mult;
    double combined_mult_delta;
} chorus_effect_s;

void bpbxsyn_effect_init_chorus(bpbxsyn_context_s *ctx, chorus_effect_s *inst);
void chorus_destroy(bpbxsyn_effect_s *inst);
void chorus_sample_rate_changed(bpbxsyn_effect_s *inst,
                                 double old_sr, double new_sr);
void chorus_tick(bpbxsyn_effect_s *inst, const bpbxsyn_tick_ctx_s *ctx);
void chorus_run(bpbxsyn_effect_s *inst, float **buffer,
                 size_t frame_count);

extern const effect_vtable_s effect_chorus_vtable;

#endif