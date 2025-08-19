#ifndef _volume_h_
#define _volume_h_

#include "../../include/beepbox_synth.h"
#include "effect.h"

typedef struct {
    bpbxsyn_effect_s base;

    // for old and new value
    double param[2];

    double linear_gain;
    double linear_gain_delta;
} volume_effect_s;

void bpbxsyn_effect_init_volume(bpbxsyn_context_s *ctx, volume_effect_s *inst);
void volume_destroy(bpbxsyn_effect_s *inst);
void volume_tick(bpbxsyn_effect_s *inst, const bpbxsyn_tick_ctx_s *ctx);
void volume_run(bpbxsyn_effect_s *inst, float **buffer, size_t frame_count);

extern const effect_vtable_s effect_volume_vtable;

#endif