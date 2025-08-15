#ifndef _alloc_h_
#define _alloc_h_

#include "../include/beepbox_synth.h"

void* bpbxsyn_malloc(size_t size);
void bpbxsyn_free(void *ptr);

#endif