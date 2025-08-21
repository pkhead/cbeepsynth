#ifndef _limiter_h_
#define _limiter_h_

#include "../../include/beepbox_synth.h"
#include "effect.h"

typedef struct {
    bpbxsyn_effect_s base;
    
    double master_gain;
    double compression_threshold;
    double compression_ratio;
    double limit_threshold;
    double limit_ratio;
    double rise;
    double decay;

    double limit;
} limiter_effect_s;

void bpbxsyn_effect_init_limiter(bpbxsyn_context_s *ctx,
                                 limiter_effect_s *inst);
void limiter_destroy(bpbxsyn_effect_s *inst);
void limiter_sample_rate_changed(bpbxsyn_effect_s *inst,
                                 double old_sr, double new_sr);
void limiter_tick(bpbxsyn_effect_s *inst, const bpbxsyn_tick_ctx_s *ctx);
void limiter_run(bpbxsyn_effect_s *inst, float **buffer, size_t frame_count);

extern const effect_vtable_s effect_limiter_vtable;

#endif