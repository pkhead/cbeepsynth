#ifndef _eq_h_
#define _eq_h_

#include "../../include/beepbox_synth.h"
#include "effect.h"
#include "filtering.h"

typedef struct {
    bpbxsyn_effect_s base;
    
    // current state of the note filter
    filter_group_s params;

    // previous state of the note filter
    filter_group_s old_params;

    dyn_biquad_s filters[FILTER_GROUP_COUNT];
    double filter_volume;
    double filter_input[2];
} eq_effect_s;

void bpbxsyn_effect_init_eq(bpbxsyn_context_s *ctx, eq_effect_s *inst);
void bbsyn_eq_destroy(bpbxsyn_effect_s *inst);
// void bbsyn_panning_sample_rate_changed(bpbxsyn_effect_s *inst,
//                                  double old_sr, double new_sr);
void bbsyn_eq_tick(bpbxsyn_effect_s *inst, const bpbxsyn_tick_ctx_s *ctx);
void bbsyn_eq_run(bpbxsyn_effect_s *inst, float **buffer, size_t frame_count);

extern const effect_vtable_s bbsyn_effect_eq_vtable;

#endif