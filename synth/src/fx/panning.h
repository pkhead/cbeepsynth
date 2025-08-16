#ifndef _panning_h_
#define _panning_h_

#include "../../include/beepbox_synth.h"
#include "effect.h"

#define PAN_VALUE_CENTER 50
#define PAN_VALUE_MAX ((PAN_VALUE_CENTER * 2))
#define PAN_DELAY_MAX 20

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
    size_t delay_line_size;
    int delay_pos;
    int delay_buffer_mask;
} panning_effect_s;

void bpbxsyn_effect_init_panning(panning_effect_s *inst);
void panning_destroy(bpbxsyn_effect_s *inst);
void panning_sample_rate_changed(bpbxsyn_effect_s *inst,
                                 double old_sr, double new_sr);
void panning_tick(bpbxsyn_effect_s *inst, const bpbxsyn_tick_ctx_s *ctx);
void panning_run(bpbxsyn_effect_s *inst, float **input, float **output,
                 size_t frame_count);

extern const effect_vtable_s effect_panning_vtable;

#endif