#include <assert.h>
#include "../synth/fm.h"

typedef enum x64_reg {
    X64_REG_EAX,
    X64_REG_ECX,
    X64_REG_EDX,
    X64_REG_EBX,
    X64_REG_ESP,
    X64_REG_EBP,
    X64_REG_ESI,
    X64_REG_EDI,
    X64_REG_R8D,
    X64_REG_R9D,
    X64_REG_R10D,
    X64_REG_R11D,
    X64_REG_R12D,
    X64_REG_R13D,
    X64_REG_R14D,
    X64_REG_R15D,

    X64_REG_RAX,
    X64_REG_RCX,
    X64_REG_RDX,
    X64_REG_RBX,
    X64_REG_RSP,
    X64_REG_RBP,
    X64_REG_RSI,
    X64_REG_RDI,
    X64_REG_R8,
    X64_REG_R9,
    X64_REG_R10,
    X64_REG_R11,
    X64_REG_R12,
    X64_REG_R13,
    X64_REG_R14,
    X64_REG_R15,

    X64_REG_XMM0,
    X64_REG_XMM1,
    X64_REG_XMM2,
    X64_REG_XMM3,
    X64_REG_XMM4,
    X64_REG_XMM5,
    X64_REG_XMM6,
    X64_REG_XMM7,
    X64_REG_XMM8,
    X64_REG_XMM9,
    X64_REG_XMM10,
    X64_REG_XMM11,
    X64_REG_XMM12,
    X64_REG_XMM13,
    X64_REG_XMM14,
    X64_REG_XMM15,
} x64_reg_e;

typedef enum x64_optype {
    X64_OPTYPE_IMM32,
    X64_OPTYPE_REG,
    X64_OPTYPE_INDIRECT
} x64_optype_e;

typedef struct x64_operand {
    uint8_t type;
    union {
        uint32_t imm32;
        uint32_t reg;
        struct {
            uint32_t disp;

            uint32_t base;
            uint32_t index;
            uint8_t shift; // lsh minus 1; 0 = don't use index
        } indirect;
    };
} x64_operand_s;

#define X64_IMM(imm) \
    ((x64_operand_s) { .type = X64_OPTYPE_IMM32, .imm32 = (imm) })
#define X64_REG(rname) \
    ((x64_operand_s) { .type = X64_OPTYPE_REG, .reg = X64_REG_##rname })
#define X64_INDIRECT_FULL(rbase, ridx, vshift, vconst) \
    ((x64_operand_s) { .type = X64_OPTYPE_INDIRECT, .indirect = { .disp = vconst, .base = X64_REG_##rbase, .index = ridx, .shift = vshift }})
#define X64_INDIRECT_ISC(rbase, ridx, vshift, vconst) \
    X64_INDIRECT_FULL(rbase, X64_REG_##ridx, (vshift) + 1, vconst)
#define X64_INDIRECT_IC(rbase, ridx, vconst) X64_INDIRECT_ISC(rbase, ridx, 1, vconst)
#define X64_INDIRECT_C(rbase, vconst) X64_INDIRECT_FULL(rbase, 0, 0, vconst)
#define X64_INDIRECT_I(rbase, ridx) X64_INDIRECT_ISC(rbase, ridx, 1, 0)
#define X64_INDIRECT_IS(rbase, ridx, vshift) X64_INDIRECT_ISC(rbase, ridx, (vshift) + 1, 0)
#define X64_INDIRECT(rbase) X64_INDIRECT_FULL(rbase, 0, 0, 0)

// addressing mode of register
static inline int x64_regadr(uint32_t reg_name) {
    if (reg_name >= 32) return 2; // SIMD register
    if (reg_name >= 16) return 1; // 64-bit register
    return 0;                     // 32-bit register
}

static bool encode_operands(uint8_t **buf, uint8_t opcode[3], int opcode_size,
                            x64_operand_s opa, x64_operand_s opb, bool p_64)
{
    // opa must be a direct register
    if (opa.type != X64_OPTYPE_REG)
        return true;

    // no generic support for immediate values
    if (opb.type == X64_OPTYPE_IMM32)
        return true;

    uint8_t modrm_mod;
    uint8_t modrm_reg = opa.reg & 15;
    uint8_t modrm_rm;

    bool use_sib = false;
    uint8_t sib_scale = 0;
    uint8_t sib_idx = 0;
    uint8_t sib_base = 0;

    bool use_disp32 = false;
    uint32_t disp32 = 0;

    if (opb.type == X64_OPTYPE_REG) {
        modrm_rm = opb.reg & 15;
        modrm_mod = 3; // direct addressing
    } else {
        assert(opb.type == X64_OPTYPE_INDIRECT);

        if (x64_regadr(opb.indirect.base) != 1)
            return true;

        // TODO: investigate shorter encodings
        if (opb.indirect.shift == 0 && opb.indirect.disp == 0) {
            if (opb.indirect.base < 4 || (opb.indirect.base >= 6 && opb.indirect.base <= 11) || (opb.indirect.base >= 14)) {
                modrm_mod = 0;
                modrm_rm = opb.indirect.base & 15;
            } else {
                assert(false);
                return true;
            }
        } else if (opb.indirect.shift == 0) {
            modrm_mod = 2;
            modrm_rm = 4;

            use_sib = true;
            sib_idx = 4;
            sib_base = opb.indirect.base & 15;
        } else {
            if (x64_regadr(opb.indirect.index) != 1)
                return true;

            if (opb.indirect.index == 4) {
                assert(false);
                return true;
            }

            modrm_mod = 2;
            modrm_rm = 4;

            use_sib = true;
            sib_scale = opb.indirect.shift - 1;
            sib_idx = opb.indirect.index & 15;
            sib_base = opb.indirect.base & 15;

            use_disp32 = true;
            disp32 = opb.indirect.disp;
        }
    }

    // need to use either an extended register, or 64-bit addressing mode; add
    // REX prefix
    if (modrm_reg > 7 || modrm_rm > 7 || p_64) {
        // modrm.reg = dst
        // modrm.rm  = src
        *((*buf)++) = 0x40
                      | (p_64 << 3) // W bit
                      | ((modrm_reg & 8) >> 1) // R bit
                      | ((sib_idx & 8) >> 2)   // X bit
                      | (((use_sib ? sib_base : modrm_rm) & 8) >> 3); // B bit
    }

    // write opcode
    for (int i = 0; i < opcode_size; ++i) {
        *((*buf)++) = opcode[i];
    }

    // modr/m byte
    *((*buf)++) = (modrm_rm & 7) | ((modrm_reg & 7) << 3)
                  | ((modrm_mod & 3) << 6) | ((modrm_reg & 7) << 2);
    
    // sib byte
    if (use_sib) {
        *((*buf)++) = (sib_base & 7) | ((sib_idx & 7) << 3) 
                      | ((sib_scale & 3) << 6);
    }

    // displacement bytes
    if (use_disp32) {
        *((*buf)++) = disp32 & 0xFF;
        *((*buf)++) = (disp32 >> 8) & 0xFF;
        *((*buf)++) = (disp32 >> 16) & 0xFF;
        *((*buf)++) = (disp32 >> 24) & 0xFF;
    }

    return false;
}

static bool x64_movsd(uint8_t **buf, x64_operand_s dst, x64_operand_s src) {
    if (dst.type == X64_OPTYPE_REG) {
        if (x64_regadr(dst.reg) != 2) return true;

        if (src.type == X64_OPTYPE_REG) {
            if (x64_regadr(src.reg) != 2) return true;
            
            *((*buf)++) = 0xf2; // SIMD prefix
            uint8_t opcode[2] = { 0x0f, 0x10 };
            return encode_operands(buf, opcode, 2, dst, src, false);
        } else if (src.type == X64_OPTYPE_INDIRECT) {
            *((*buf)++) = 0xf2; // SIMD prefix
            uint8_t opcode[2] = { 0x0f, 0x10 };
            return encode_operands(buf, opcode, 2, dst, src, false);
        }
    }
    // TODO: write to indirect

    return true;
}

static bool x64_addsd(uint8_t **buf, x64_operand_s dst, x64_operand_s src) {
    if (dst.type == X64_OPTYPE_REG) {
        if (x64_regadr(dst.reg) != 2) return true;

        if (src.type == X64_OPTYPE_REG) {
            if (x64_regadr(src.reg) != 2) return true;
            
            *((*buf)++) = 0xf2; // SIMD prefix
            uint8_t opcode[2] = { 0x0f, 0x58 };
            return encode_operands(buf, opcode, 2, dst, src, false);
        } else if (src.type == X64_OPTYPE_INDIRECT) {
            *((*buf)++) = 0xf2; // SIMD prefix
            uint8_t opcode[2] = { 0x0f, 0x58 };
            return encode_operands(buf, opcode, 2, dst, src, false);
        }
    }

    return true;
}

inline static bool x64_ret(uint8_t **buf) { *((*buf)++) = 0xc3; return false; }

fm_algo2_f bbsyn_calc_fm_algo(const fm_desc_s *desc, const float *sine_wave,
                              void *p_code_rw, const void **code_x)
{
    uint8_t *code_rw = p_code_rw;
    bool fail = false;

    #define ASM(mnemonic, ...) fail |= x64_##mnemonic(&code_rw, ## __VA_ARGS__)

    ASM( addsd, X64_REG(XMM0), X64_REG(XMM1) );
    ASM( ret );

    // ASM(movsd, X64_REG(XMM0), X64_REG(XMM1));
    // ASM(movsd, X64_REG(XMM0), X64_INDIRECT_ISC(RAX, RDX, 0, 64));

    #undef ASM

    if (fail) return NULL;

    double (*testfunc)(double a, double b) = (void *)code_x;
    double val = testfunc(1.0, 3.0);
    assert(val == 4.0);

    return (fm_algo2_f) code_x;

    /*
    const int phase_int = (int) phase_mix;
    const int index = phase_int & (SINE_WAVE_LENGTH - 1);
    const double sample = sine_wave[index];
    return sample + (sine_wave[index+1] - sample) * (phase_mix - phase_int);
    */
}