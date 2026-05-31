#ifndef BPBXSYN_MCALLOC_H
#define BPBXSYN_MCALLOC_H

#include "../include/beepbox_synth.h"

void *bbsyn_init_mcalloc(void);
void bbsyn_destroy_mcalloc(void *allocator);

bpbxsyn_mcalloc_id bbsyn_default_mcalloc(size_t size, void *userdata,
                                         void **write, const void **exec);
void bbsyn_default_mcfree(bpbxsyn_mcalloc_id id, void *userdata);

// default implementation of machine-code allocator
// currently windows-only
#ifdef _WIN32
#define BPBXSYN_HAS_DEFAULT_MCALLOC
#endif

#endif