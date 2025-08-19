#ifndef _distortion_h_
#define _distortion_h_

#include "../../include/beepbox_synth.h"
#include "effect.h"

typedef struct {
    bpbxsyn_effect_s base;

    // for old and new value
    double param[2];

    double distortion;
    double distortion_delta;
    double drive;
    double drive_delta;

    double fractional_input[3];
    double prev_input;
    double next_output;
} distortion_effect_s;

void bpbxsyn_effect_init_distortion(bpbxsyn_context_s *ctx, distortion_effect_s *inst);
void distortion_destroy(bpbxsyn_effect_s *inst);
void distortion_tick(bpbxsyn_effect_s *inst, const bpbxsyn_tick_ctx_s *ctx);
void distortion_run(bpbxsyn_effect_s *inst, float **buffer,
                 size_t frame_count);

extern const effect_vtable_s effect_distortion_vtable;

#endif