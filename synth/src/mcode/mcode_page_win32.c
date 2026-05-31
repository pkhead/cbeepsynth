#include "mcode_page.h"

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// TODO: use mutexes for tlock and tunlock

void *bbsyn_mcode_page_alloc(void **rw, const void **exec) {
    uint8_t *page = VirtualAlloc(NULL, BPBXSYN_MCODE_PAGE_SIZE, MEM_COMMIT,
                                 PAGE_EXECUTE_READWRITE);
    if (!page) return NULL;

    *rw = page;
    *exec = page;
    return page;
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