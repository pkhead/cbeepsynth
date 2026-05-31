#ifndef BPBXSYN_MCODE_INTERNAL_H
#define BPBXSYN_MCODE_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include "../../include/beepbox_synth.h"

bpbxsyn_mcalloc_status_e bbsyn_mcode_page_alloc(size_t size, void **handle,
                                                void **rw, const void **exec);
void bbsyn_mcode_page_free(void *page);

// mutexes
void bbsyn_mcode_page_mlock(void *page);
void bbsyn_mcode_page_munlock(void *page);

#endif