#ifndef _panning_h_
#define _panning_h_

#include "../../include/beepbox_synth.h"
#include "effect.h"

typedef struct {
    bpbxsyn_effect_s base;

    // for old and new value
    double pan[2];
    double pan_delay[2];

    // for left and right channels
    double volume[2];
    double volume_delta[2];
    double offset[2];
    double offset_delta[2];

    float *delay_line;
    int delay_line_size;
    int delay_pos;
    int delay_buffer_mask;
} panning_effect_s;

void bpbxsyn_effect_init_panning(bpbxsyn_context_s *ctx, panning_effect_s *inst);
void bbsyn_panning_destroy(bpbxsyn_effect_s *inst);
void bbsyn_panning_sample_rate_changed(bpbxsyn_effect_s *inst,
                                       double old_sr, double new_sr);
void bbsyn_panning_tick(bpbxsyn_effect_s *inst, const bpbxsyn_tick_ctx_s *ctx);
void bbsyn_panning_run(bpbxsyn_effect_s *inst, float **buffer,
                       size_t frame_count);

extern const effect_vtable_s bbsyn_effect_panning_vtable;

#endif