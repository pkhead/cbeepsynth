#ifndef _alloc_h_
#define _alloc_h_

#include "../include/beepbox_synth.h"

void* bpbxsyn_malloc(const bpbxsyn_context_s *ctx, size_t size);
void bpbxsyn_free(const bpbxsyn_context_s *ctx, void *ptr);
bpbxsyn_mcalloc_id bpbxsyn_mc_alloc(const bpbxsyn_context_s *ctx, size_t size,
                                    void **rw, const void **exec);
void bpbxsyn_mc_free(const bpbxsyn_context_s *ctx, bpbxsyn_mcalloc_id handle);

#endif