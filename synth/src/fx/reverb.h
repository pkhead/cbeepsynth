#ifndef _reverb_h_
#define _reverb_h_

#include <stddef.h>

#include "../../include/beepbox_synth.h"
#include "effect.h"

typedef struct reverb_effect {
    bpbxsyn_effect_s base;

    double param[2];

    float *delay_line;
    int delay_line_size;
    int delay_line_mask;
    int delay_pos;

    int delay_offsets[3];

    double mult;
    double mult_delta;

    double shelf_a1;
    double shelf_b0;
    double shelf_b1;
    double shelf_sample[4];
    double shelf_prev_input[4];
} reverb_effect_s;

void bpbxsyn_effect_init_reverb(bpbxsyn_context_s *ctx, reverb_effect_s *inst);
void bbsyn_reverb_destroy(bpbxsyn_effect_s *inst);
void bbsyn_reverb_sample_rate_changed(bpbxsyn_effect_s *inst,
                                      double old_sr, double new_sr);
void bbsyn_reverb_tick(bpbxsyn_effect_s *inst, const bpbxsyn_tick_ctx_s *ctx);
void bbsyn_reverb_run(bpbxsyn_effect_s *inst, float **buffer,
                      size_t frame_count);

extern const effect_vtable_s bbsyn_effect_reverb_vtable;

#endif