#ifndef _context_h_
#define _context_h_

#include <stdbool.h>
#include "../include/beepbox_synth.h"
#include "wavetables.h"

typedef struct bpbxsyn_context {
    bpbxsyn_allocator_s alloc;

    bpbxsyn_log_f log_func;
    void *log_userdata;

    wavetables_s wavetables;
} bpbxsyn_context_s;

#endif