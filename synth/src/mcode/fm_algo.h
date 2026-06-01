#ifndef BBSYN_MCODE_FM_ALGO_H
#define BBSYN_MCODE_FM_ALGO_H

#include <assert.h>
#include <stdint.h>

#include "../context.h"
#include "../synth/fm.h"
#include <umm_malloc/umm_malloc.h>

#define DASM_MEMPOOL_SIZE 1024

#define Dst_DECL	fm_algoc_s *Dst
#define Dst_REF		((Dst)->state)

#define DASM_M_GROW(CTX, t, p, sz, need) \
  do { \
    size_t _sz = (sz), _need = (need); \
    if (_sz < _need) { \
      if (_sz < 16) _sz = 16; \
      while (_sz < _need) _sz += _sz; \
      (p) = (t *)umm_multi_realloc(&((CTX)->dasm_heap), (p), _sz); \
      if ((p) == NULL) exit(1); \
      (sz) = _sz; \
    } \
  } while(0)

#define DASM_M_FREE(CTX, p, sz) \
    umm_multi_free(&((CTX)->dasm_heap), (p))

#include <dynasm/dasm_proto.h>

struct fm_algoc {
    dasm_State *state;
    const bpbxsyn_context_s *ctx;

    // use custom heap with embedded memory allocator, because the compilation
    // code will run in the audio thread, which ought not to use OS facilities.
#ifdef _MSC_VER
    __declspec(align(8))
#else
    __attribute__((aligned(8)))
#endif
    uint8_t dasm_mempool[DASM_MEMPOOL_SIZE];
    umm_heap dasm_heap;
};

#endif