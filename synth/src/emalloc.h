// embedded memory allocaor. do not confuse with alloc.h, which provides an
// interface to a user-given allocator passed to bpbsyn_context_new.

// actually, this seems to be somewhat buggy. i honestly do not feel like
// debugging this i am already going insane. i am going insane. i'm just going
// to use this fucking allocator library i found on the internet.

#ifndef BBSYN_EMALLOC_H
#define BBSYN_EMALLOC_H

#include <stdbool.h>

#define NATIVE_ALIGN sizeof(void*)
#define ALIGN(ptr, alignment) \
    (((uintptr_t)(ptr) + (alignment) - 1) / (alignment) * (alignment))

typedef struct emalloc_heap emalloc_heap_s;

// given heap pointer must be aligned to native integer size. returns false
// if the heap was too small.
bool bbsyn_emalloc_init(emalloc_heap_s *heap, size_t heap_size);

void *bbsyn_emalloc_malloc(emalloc_heap_s *heap, size_t size);
void bbsyn_emalloc_free(emalloc_heap_s *heap, void *ptr);
// TODO: realloc

#endif