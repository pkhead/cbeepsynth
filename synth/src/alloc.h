#ifndef _alloc_h_
#define _alloc_h_

#include "../include/beepbox_synth.h"

void *bbxs_malloc(const bbxs_context_s *ctx, size_t size);
void bbxs_free(const bbxs_context_s *ctx, void *ptr);

#endif