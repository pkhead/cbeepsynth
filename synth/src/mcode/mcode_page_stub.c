#include "mcode_page.h"

bpbxsyn_mcalloc_status_e bbsyn_mcode_page_alloc(size_t size, void **handle,
                                                void **rw, const void **exec)
{
    (void)size, (void)handle, (void)rw, (void)exec;
    return BPBXSYN_MCALLOC_ERR_PLATFORM;
}

void bbsyn_mcode_page_free(void *page) {
    (void)page;
}

void bbsyn_mcode_page_mlock(void *page) {
    (void)page;
}

void bbsyn_mcode_page_munlock(void *page) {
    (void)page;
}

void bbsyn_mcode_flush_icache(const void *page, size_t page_size) {
    (void)page;
    (void)page_size;
}