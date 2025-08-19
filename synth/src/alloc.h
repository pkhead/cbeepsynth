#ifndef _alloc_h_
#define _alloc_h_

#include "../include/beepbox_synth.h"

void* bpbxsyn_malloc(const bpbxsyn_context_s *ctx, size_t size);
void bpbxsyn_free(const bpbxsyn_context_s *ctx, void *ptr);

#endif