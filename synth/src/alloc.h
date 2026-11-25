#ifndef _alloc_h_
#define _alloc_h_

#include "../include/beepbox_synth.h"

void *bbxs__malloc(const bbxs_context_s *ctx, size_t size);
void bbxs__free(const bbxs_context_s *ctx, void *ptr);

__attribute__((deprecated))
static inline void *bbxs_malloc(const bbxs_context_s *ctx, size_t size) {
    return bbxs__malloc(ctx, size);
}

__attribute__((deprecated))
static inline void bbxs_free(const bbxs_context_s *ctx, void *ptr) {
    bbxs__free(ctx, ptr);
}

#endif