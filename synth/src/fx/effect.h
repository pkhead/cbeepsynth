#ifndef _effect_h_
#define _effect_h_

#include "../../include/beepbox_synth.h"
#include <stddef.h>

struct bpbxsyn_effect {
    bpbxsyn_effect_type_e type;
    const bpbxsyn_context_s *ctx;
    double sample_rate;

    void *userdata;
};

typedef void (*effect_init_f)(bpbxsyn_context_s *ctx, bpbxsyn_effect_s *inst);
typedef void (*effect_destroy_f)(bpbxsyn_effect_s *inst);

typedef struct {
    const size_t                struct_size;
    const effect_init_f         effect_init;
    const effect_destroy_f      effect_destroy;
    const uint8_t               input_channel_count;
    const uint8_t               output_channel_count;
    const uint32_t              param_count;
    const bpbxsyn_param_info_s  *param_info;
    const size_t                *param_addresses;

    void (*const effect_stop)(bpbxsyn_effect_s *inst);
    void (*const effect_sample_rate_changed)(bpbxsyn_effect_s *inst,
                                             double old, double new);
    void (*const effect_tick)(bpbxsyn_effect_s *inst,
                              const bpbxsyn_tick_ctx_s *tick_ctx);
    void (*const effect_run)(bpbxsyn_effect_s *inst, float **buffer,
                             size_t frame_count);
} effect_vtable_s;

#endif