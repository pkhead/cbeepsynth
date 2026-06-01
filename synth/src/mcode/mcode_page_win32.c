#include "mcode_page.h"

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdlib.h>
#include <assert.h>

typedef struct page_meta {
    void *page;
    HANDLE mutex;
} page_meta_s;

bpbxsyn_mcalloc_status_e bbsyn_mcode_page_alloc(size_t size, void **handle,
                                                void **rw, const void **exec) {
    uint8_t *page = NULL;
    page_meta_s *meta = NULL;
    HANDLE mutex = NULL;

    page = VirtualAlloc(NULL, size, MEM_COMMIT,
                                 PAGE_EXECUTE_READWRITE);
    if (!page) goto error;

    meta = malloc(sizeof(page_meta_s));
    if (!meta) goto error;

    mutex = CreateMutex(NULL, false, NULL);
    if (!mutex) goto error;

    *meta = (page_meta_s) {
        .page = page,
        .mutex = mutex
    };

    *rw = page;
    *exec = page;
    *handle = meta;
    return BPBXSYN_MCALLOC_OK;

    error:
    if (page) VirtualFree(page, 0, MEM_RELEASE);
    if (meta) free(meta);
    if (mutex) CloseHandle(mutex);
    return BPBXSYN_MCALLOC_ERR_IO;
}

void bbsyn_mcode_page_free(void *handle) {
    page_meta_s *meta = handle;

    VirtualFree(meta->page, 0, MEM_RELEASE);
    CloseHandle(meta->mutex);
    free(meta);
}

void bbsyn_mcode_page_mlock(void *handle) {
    page_meta_s *meta = handle;

    DWORD wait_result = WaitForSingleObject(meta->mutex, INFINITE);
    assert(wait_result != WAIT_FAILED);
}

void bbsyn_mcode_page_munlock(void *handle) {
    page_meta_s *meta = handle;

    bool s = ReleaseMutex(meta->mutex);
    assert(s);
}

void bbsyn_mcode_flush_icache(const void *page, size_t page_size) {
    FlushInstructionCache(GetCurrentProcess(), page, page_size);
}