#ifndef _context_h_
#define _context_h_

#include <stdbool.h>

#include "../include/beepbox_synth.h"
#include "wavetables.h"
#include "util.h"

struct bbxs_context {
    bbxs_allocator_s alloc;
    prng_state_s prng;

    bbxs_log_f log_func;
    void *log_userdata;

    wavetables_s wavetables;
};

#endif