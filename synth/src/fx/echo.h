#ifndef _echo_h_
#define _echo_h_

#include <stdbool.h>

#include "../../include/beepbox_synth.h"
#include "effect.h"


typedef struct echo_effect {
    bpbxsyn_effect_s base;

    // for old and new value
    double sustain[2];
    double delay[2];

    // left/right delay lines
    // there are two buffers in order for the code to handle tempo changes
    // correctly. it needs to copy the data from the old buffer to the new
    // buffer, so i use double-buffering for this.
    int delay_line_buffer_idx;
    float *delay_lines[2][2];

    int delay_line_capacity;
    int delay_line_size;
    
    bool delay_line_dirty;
    int delay_line_pos;
    
    double delay_offset_start;
    double delay_offset_end;
    double delay_offset_ratio;
    double delay_offset_ratio_delta;
    double echo_mult;
    double echo_mult_delta;
    double echo_shelf_a1;
    double echo_shelf_b0;
    double echo_shelf_b1;

    // left/right
    double echo_shelf_sample[2];
    double echo_shelf_prev_input[2];

} echo_effect_s;

void bpbxsyn_effect_init_echo(echo_effect_s *inst);
void echo_destroy(bpbxsyn_effect_s *inst);
void echo_sample_rate_changed(bpbxsyn_effect_s *inst, double old_sr,
                              double new_sr);
void echo_tick(bpbxsyn_effect_s *inst, const bpbxsyn_tick_ctx_s *ctx);
void echo_run(bpbxsyn_effect_s *inst, float **buffer, size_t frame_count);

extern const effect_vtable_s effect_echo_vtable;

#endif