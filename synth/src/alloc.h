#ifndef _alloc_h_
#define _alloc_h_

#include "../include/beepbox_synth.h"

void *bbxs__malloc(const bbxs_context_s *ctx, size_t size);
void bbxs__free(const bbxs_context_s *ctx, void *ptr);

#endif