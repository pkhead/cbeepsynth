#include <assert.h>

#include <umm_malloc/umm_malloc.h>
#include "mcode_page.h"
#include "../util.h"

// TODO: maybe add a way to allocate more pages? idk how that would work though.
#define NATIVE_ALIGN sizeof(void*)

typedef struct page_header {
    void *page_handle;
    const void *exec;
    umm_heap heap;
} page_header_s;

static bpbxsyn_mcalloc_id bbsyn_default_mcalloc(size_t size, void *userdata,
                                         void **write, const void **exec);
static void bbsyn_default_mcfree(bpbxsyn_mcalloc_id id, void *userdata);

bpbxsyn_mcalloc_status_e bpbxsyn_mcode_allocator_new(
    size_t arena_size, bpbxsyn_mcode_allocator_s *alloc)
{
    uint8_t *rw;
    const void *exec;
    void *page_handle;

    size_t arena_size_bytes = arena_size * 1024;

    bpbxsyn_mcalloc_status_e status =
        bbsyn_mcode_page_alloc(arena_size_bytes, &page_handle, (void **)&rw,
                               &exec);
    
    if (status != BPBXSYN_MCALLOC_OK)
        return status;

    page_header_s *page_header = (void *)rw;
    page_header->exec = exec;
    page_header->page_handle = page_handle;

    size_t heap_start_offset = ALIGN(sizeof(page_header_s), NATIVE_ALIGN);
    void *heap_start = (void *)((uintptr_t)page_header + heap_start_offset);

    umm_multi_init_heap(&page_header->heap, heap_start,
                        arena_size_bytes - heap_start_offset);

    *alloc = (bpbxsyn_mcode_allocator_s) {
        .alloc = bbsyn_default_mcalloc,
        .free = bbsyn_default_mcfree,
        .userdata = page_header
    };

    return BPBXSYN_MCALLOC_OK;
}

void bpbxsyn_mcode_allocator_destroy(void *alloc) {
    if (!alloc) return;
    
    page_header_s *page = (void *)alloc;
    bbsyn_mcode_page_free(page->page_handle);
}

bpbxsyn_mcalloc_id bbsyn_default_mcalloc(size_t size, void *userdata,
                                         void **write, const void **exec)
{
    page_header_s *page_hdr = (void *)userdata;
    umm_heap *heap = &page_hdr->heap;

    bbsyn_mcode_page_mlock(page_hdr->page_handle);
    void *ptr = umm_multi_malloc(heap, size);
    bbsyn_mcode_page_munlock(page_hdr->page_handle);

    if (!ptr) {
        *write = NULL;
        *exec = NULL;
        return BPBXSYN_MCALLOC_INVALID_ID;
    }

    *write = ptr;
    *exec = (const void *)( (uintptr_t)page_hdr->exec
                            + ((uintptr_t)ptr - (uintptr_t)page_hdr) );
    return (bpbxsyn_mcalloc_id) ptr;
}

void bbsyn_default_mcfree(bpbxsyn_mcalloc_id id, void *userdata) {
    if (id == BPBXSYN_MCALLOC_INVALID_ID) return;

    page_header_s *page_hdr = (void *)userdata;
    umm_heap *heap = &page_hdr->heap;

    bbsyn_mcode_page_mlock(page_hdr->page_handle);
    umm_multi_free(heap, (void *)id);
    bbsyn_mcode_page_munlock(page_hdr->page_handle);
}