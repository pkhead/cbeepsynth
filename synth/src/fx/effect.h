#ifndef _effect_h_
#define _effect_h_

#include <stddef.h>

#include "../../include/beepbox_synth.h"

struct bbxs_effect {
    bbxs_effect_type_e type;
    const bbxs_context_s *ctx;
    double sample_rate;

    void *userdata;
};

typedef void (*effect_init_f)(bbxs_context_s *ctx, bbxs_effect_s *inst);
typedef void (*effect_destroy_f)(bbxs_effect_s *inst);

typedef struct {
    const size_t struct_size;
    const effect_init_f effect_init;
    const effect_destroy_f effect_destroy;
    const uint8_t input_channel_count;
    const uint8_t output_channel_count;
    const uint32_t param_count;
    const bbxs_param_info_s *param_info;
    const size_t *param_addresses;

    void (*const effect_stop)(bbxs_effect_s *inst);
    void (*const effect_sample_rate_changed)(bbxs_effect_s *inst, double old,
                                             double new);
    void (*const effect_tick)(bbxs_effect_s *inst,
                              const bbxs_tick_ctx_s *tick_ctx);
    void (*const effect_run)(bbxs_effect_s *inst, float **buffer,
                             size_t frame_count);
} effect_vtable_s;

extern const effect_vtable_s bbsyn_effect_bitcrusher_vtable;
extern const effect_vtable_s bbsyn_effect_chorus_vtable;
extern const effect_vtable_s bbsyn_effect_distortion_vtable;
extern const effect_vtable_s bbsyn_effect_echo_vtable;
extern const effect_vtable_s bbsyn_effect_eq_vtable;
extern const effect_vtable_s bbsyn_effect_limiter_vtable;
extern const effect_vtable_s bbsyn_effect_panning_vtable;
extern const effect_vtable_s bbsyn_effect_reverb_vtable;
extern const effect_vtable_s bbsyn_effect_volume_vtable;

#endif