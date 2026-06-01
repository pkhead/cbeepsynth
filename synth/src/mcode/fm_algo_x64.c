/*
** This file has been pre-processed with DynASM.
** https://luajit.org/dynasm.html
** DynASM version 1.5.0, DynASM x64 version 1.5.0
** DO NOT EDIT! The original file is in "C:/Users/nabca/Documents/src/misc/beepboxvst/src/cbeepsynth/synth/src/mcode/fm_algo_x64.dasc".
*/

#line 1 "C:/Users/nabca/Documents/src/misc/beepboxvst/src/cbeepsynth/synth/src/mcode/fm_algo_x64.dasc"
#include <assert.h>
#include <stdint.h>

#include "../synth/fm.h"
#include "../alloc.h"
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
//|.arch x64
#if DASM_VERSION != 10500
#error "Version mismatch between DynASM and included encoding engine"
#endif
#line 30 "C:/Users/nabca/Documents/src/misc/beepboxvst/src/cbeepsynth/synth/src/mcode/fm_algo_x64.dasc"
//|.globals lbl_
enum {
  lbl__MAX
};
#line 31 "C:/Users/nabca/Documents/src/misc/beepboxvst/src/cbeepsynth/synth/src/mcode/fm_algo_x64.dasc"
//|.actionlist fm_actions
static const unsigned char fm_actions[471] = {
  102,15,252,239,210,102,15,252,239,192,102,15,252,239,219,255,72,131,252,236,
  8,252,242,68,15,17,4,36,72,131,252,236,8,252,242,68,15,17,12,36,72,131,252,
  236,8,252,242,68,15,17,20,36,72,131,252,236,8,252,242,68,15,17,28,36,73,184,
  237,237,73,137,201,252,242,15,16,252,233,252,242,65,15,16,137,233,252,242,
  15,44,193,15,182,208,252,242,15,42,216,252,242,15,92,203,252,243,65,15,90,
  20,144,252,243,65,15,90,132,253,144,233,252,242,15,92,194,252,242,15,89,193,
  252,242,15,88,194,252,242,65,15,17,129,233,252,242,65,15,16,137,233,252,242,
  15,89,193,252,242,68,15,16,192,252,242,65,15,16,137,233,252,242,15,44,193,
  15,182,208,252,242,15,42,216,252,242,15,92,203,252,243,65,15,90,20,144,252,
  243,65,15,90,132,253,144,233,252,242,15,92,194,252,242,15,89,193,252,242,
  15,88,194,252,242,65,15,17,129,233,252,242,65,15,16,137,233,252,242,15,89,
  193,252,242,68,15,16,200,252,242,65,15,16,137,233,252,242,15,44,193,15,182,
  208,252,242,15,42,216,252,242,15,92,203,252,243,65,15,90,20,144,252,243,65,
  15,90,132,253,144,233,252,242,15,92,194,252,242,15,89,193,252,242,15,88,194,
  252,242,65,15,17,129,233,252,242,65,15,16,137,233,252,242,15,89,193,252,242,
  68,15,16,208,252,242,65,15,16,137,233,252,242,15,44,193,15,182,208,252,242,
  15,42,216,252,242,15,92,203,252,243,65,15,90,20,144,252,243,65,15,90,132,
  253,144,233,252,242,15,92,194,252,242,15,89,193,252,242,15,88,194,252,242,
  65,15,17,129,233,252,242,65,15,16,137,233,252,242,15,89,193,252,242,68,15,
  16,216,252,242,65,15,16,192,252,242,65,15,88,193,252,242,65,15,88,194,252,
  242,65,15,88,195,252,242,68,15,16,4,36,72,131,196,8,252,242,68,15,16,12,36,
  72,131,196,8,252,242,68,15,16,20,36,72,131,196,8,252,242,68,15,16,28,36,72,
  131,196,8,195,255
};

#line 32 "C:/Users/nabca/Documents/src/misc/beepboxvst/src/cbeepsynth/synth/src/mcode/fm_algo_x64.dasc"

struct fm_algoc {
    dasm_State *state;
    const bpbxsyn_context_s *ctx;
    void *labels[lbl__MAX == 0 ? 1 : 0];

#ifdef _MSC_VER
    __declspec(align(8))
#else
    __attribute__((aligned(8)))
#endif
    uint8_t dasm_mempool[DASM_MEMPOOL_SIZE];
    umm_heap dasm_heap;
};

#include <dynasm/dasm_x86.h>

#include <stdio.h>
#include <windows.h>

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

fm_algo2_f bbsyn_fm_algoc_compile(fm_algoc_s *algoc, const fm_desc_s *desc,
                                  const float *sine_wave, void *code_rw,
                                  const void *code_x)
{
    dasm_init(algoc, 1);
    dasm_setupglobal(algoc, algoc->labels, lbl__MAX);
    dasm_setup(algoc, fm_actions);

    #define opst fm_voice_opstate_s
    #define Dst algoc
    //| .macro sample_sine_wave
    //|     // [in]  xmm1: register where phase is stored
    //|     // [out] xmm0
    //|     // destroys rax, rdx, and xmm0-3
    //|     // r8 is where the sine wave ptr is stored
    //|     cvttsd2si eax, xmm1
    //|     movzx edx, al
    //|     cvtsi2sd xmm3, eax
    //|     subsd xmm1, xmm3
    //|     cvtss2sd xmm2, dword [r8 + rdx * 4]
    //|     cvtss2sd xmm0, dword [r8 + rdx * 4 + 4]
    //|     subsd xmm0, xmm2
    //|     mulsd xmm0, xmm1
    //|     addsd xmm0, xmm2
    //| .endmacro
    //|
    //| .macro calc_fm_op, opidx, out
    //|     // [out] out
    //|     // destroys rax, rdx, and xmm0-3
    //|     movsd xmm1, qword [r9 + sizeof(opst) * opidx + offsetof(opst, phase)]
    //|     sample_sine_wave
    //|     movsd qword [r9 + sizeof(opst) * opidx + offsetof(opst, output)], xmm0
    //|     movsd xmm1, qword [r9 + sizeof(opst) * opidx + offsetof(opst, expression)]
    //|     mulsd xmm0, xmm1
    //|     movsd out, xmm0
    //| .endmacro
    //|
    //| // function parameters:
    //| // rcx: fm_opstate_s *opstates
    //| // xmm1: double feedback amp
    //| pxor xmm2, xmm2
    //| pxor xmm0, xmm0
    //| pxor xmm3, xmm3
    dasm_put(Dst, 0);
#line 107 "C:/Users/nabca/Documents/src/misc/beepboxvst/src/cbeepsynth/synth/src/mcode/fm_algo_x64.dasc"
    // |
    // | sub rsp, 8
    // | mov64 r8, ((size_t)sine_wave)
    // | movsd xmm1, qword [rcx + sizeof(opst) * 0 + offsetof(opst, phase)]
    // | sample_sine_wave
    // | movsd xmm1, qword [rcx + sizeof(opst) * 0 + offsetof(opst, expression)]
    // | mulsd xmm0, xmm1
    //| sub rsp, 8
    //| movsd qword [rsp], xmm8
    //| sub rsp, 8
    //| movsd qword [rsp], xmm9
    //| sub rsp, 8
    //| movsd qword [rsp], xmm10
    //| sub rsp, 8
    //| movsd qword [rsp], xmm11
    //|
    //| // r8: float *sine_wave
    //| // r9: fm_opstate_s *opstates;
    //| // xmm5: double feedback_amp
    //| mov64 r8, ((size_t)sine_wave)
    //| mov r9, rcx
    //| movsd xmm5, xmm1
    //|
    //| // op3 (xmm8 = op3_scaled)
    //| calc_fm_op 3, xmm8
    //| // op2 (xmm9 = op2_scaled)
    //| calc_fm_op 2, xmm9
    //| // op1 (xmm10 = op1_scaled)
    //| calc_fm_op 1, xmm10
    //| // op0 (xmm11 = op0_scaled)
    //| calc_fm_op 0, xmm11
    //|
    //| // return the sum of all the operator outputs
    //| movsd xmm0, xmm8
    //| addsd xmm0, xmm9
    //| addsd xmm0, xmm10
    //| addsd xmm0, xmm11
    //|
    //| // epilogue
    //| movsd xmm8, qword [rsp]
    //| add rsp, 8
    //| movsd xmm9, qword [rsp]
    //| add rsp, 8
    //| movsd xmm10, qword [rsp]
    //| add rsp, 8
    //| movsd xmm11, qword [rsp]
    //| add rsp, 8
    //| ret
    dasm_put(Dst, 16, (unsigned int)(((size_t)sine_wave)), (unsigned int)((((size_t)sine_wave))>>32), sizeof(opst) * 3 + offsetof(opst, phase), 4, sizeof(opst) * 3 + offsetof(opst, output), sizeof(opst) * 3 + offsetof(opst, expression), sizeof(opst) * 2 + offsetof(opst, phase), 4, sizeof(opst) * 2 + offsetof(opst, output), sizeof(opst) * 2 + offsetof(opst, expression), sizeof(opst) * 1 + offsetof(opst, phase), 4, sizeof(opst) * 1 + offsetof(opst, output), sizeof(opst) * 1 + offsetof(opst, expression), sizeof(opst) * 0 + offsetof(opst, phase), 4, sizeof(opst) * 0 + offsetof(opst, output), sizeof(opst) * 0 + offsetof(opst, expression));
#line 155 "C:/Users/nabca/Documents/src/misc/beepboxvst/src/cbeepsynth/synth/src/mcode/fm_algo_x64.dasc"
    #undef opst
    #undef Dst

    size_t szp;
    dasm_link(algoc, &szp);
    assert(szp < 512);

    dasm_encode(algoc, code_rw);

    // double const (*func)(double a, double b) = code_x;
    // double val = func(1.0, 2.0);
    // assert(val == 3.0);

    dasm_free(algoc);
    return code_x;
}
