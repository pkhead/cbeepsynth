#include "alloc.h"
#include <assert.h>
#include <stdlib.h>


static void *default_alloc(size_t size, void *userdata) {
    (void)userdata;
    return malloc(size);
}

static void default_free(void *ptr, void *userdata) {
    (void)userdata;
    free(ptr);
}

static bpbxsyn_malloc_f alloc_new = default_alloc;
static bpbxsyn_mfree_f alloc_free = default_free;
static void *alloc_userdata = NULL;

void *bpbxsyn_malloc(size_t size) {
    assert(alloc_new);
    return alloc_new(size, alloc_userdata);
}

void bpbxsyn_free(void *ptr) {
    assert(alloc_free);
    if (ptr)
        alloc_free(ptr, alloc_userdata);
}

void bpbxsyn_set_allocator(bpbxsyn_malloc_f alloc, bpbxsyn_mfree_f free,
                           void *userdata) {
    alloc_new = alloc;
    alloc_free = free;
    alloc_userdata = userdata;
}