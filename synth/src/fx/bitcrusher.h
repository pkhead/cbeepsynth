#ifndef _bitcrusher_h_
#define _bitcrusher_h_

#include "../../include/beepbox_synth.h"
#include "effect.h"

typedef struct {
    bpbxsyn_effect_s base;

    // for old and new value
    double bit_crush[2];
    double freq_crush[2];

    double prev_input;
    double current_output;
    double phase;
    double phase_delta;
    double phase_delta_scale;
    double scale;
    double scale_scale;
    double fold_level;
    double fold_level_scale;
} bitcrusher_effect_s;

void bpbxsyn_effect_init_bitcrusher(bpbxsyn_context_s *ctx, bitcrusher_effect_s *inst);
void bbsyn_bitcrusher_destroy(bpbxsyn_effect_s *inst);
void bbsyn_bitcrusher_tick(bpbxsyn_effect_s *inst,
                           const bpbxsyn_tick_ctx_s *ctx);
void bbsyn_bitcrusher_run(bpbxsyn_effect_s *inst, float **buffer,
                          size_t frame_count);

extern const effect_vtable_s bbsyn_effect_bitcrusher_vtable;

#endif