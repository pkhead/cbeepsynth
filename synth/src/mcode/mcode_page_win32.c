#include "mcode_page.h"

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// TODO: use mutexes for tlock and tunlock

bpbxsyn_mcalloc_status_e bbsyn_mcode_page_alloc(size_t size, void **handle,
                                                void **rw, const void **exec) {
    uint8_t *page = VirtualAlloc(NULL, size, MEM_COMMIT,
                                 PAGE_EXECUTE_READWRITE);
    if (!page) return BPBXSYN_MCALLOC_ERR_IO;

    *rw = page;
    *exec = page;
    *handle = page;
    return BPBXSYN_MCALLOC_OK;
}

void bbsyn_mcode_page_free(void *page) {
    VirtualFree(page, 0, MEM_RELEASE);
}

void bbsyn_mcode_page_mlock(void *page) {
    (void)page;
}

void bbsyn_mcode_page_munlock(void *page) {
    (void)page;
}