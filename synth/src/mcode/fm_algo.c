#include "fm_algo.h"
#ifdef BBSYN_SUPPORT_FMGEN

#include "../alloc.h"

fm_algoc_s *bbsyn_fm_algoc_new(const bpbxsyn_context_s *ctx) {
    fm_algoc_s *self = bpbxsyn_malloc(ctx, sizeof(fm_algoc_s));
    *self = (fm_algoc_s) { .ctx = ctx };
    umm_multi_init_heap(&self->dasm_heap, self->dasm_mempool,
                        DASM_MEMPOOL_SIZE);
    return self;
}

void bbsyn_fm_algoc_destroy(fm_algoc_s *algoc) {
    if (!algoc) return;
}

// since, apparently, CMake is unable to reliably detect the target
// architecture, i will just use the C preprocessor to evaluate the correct
// source file.
#ifdef BBSYN_ARCH_X64
#   include "../dasm/fm_algo_x64.c"
#else
#   error FM dynamic code generator is not supported on this platform!
#endif

#endif