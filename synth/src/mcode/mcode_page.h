#ifndef BPBXSYN_MCODE_INTERNAL_H
#define BPBXSYN_MCODE_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#define BPBXSYN_MCODE_PAGE_SIZE 4096

void *bbsyn_mcode_page_alloc(void **rw, const void **exec);
void bbsyn_mcode_page_free(void *page);

// mutexes
void bbsyn_mcode_page_mlock(void *page);
void bbsyn_mcode_page_munlock(void *page);

#endif