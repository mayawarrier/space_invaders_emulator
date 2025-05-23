// 
// References
// Intel manual: https://altairclone.com/downloads/manuals/8080%20Programmers%20Manual.pdf
// Tandy manual: https://archive.org/details/8080-8085_Assembly_Language_Programming_1977_Intel
// 8080 Data sheet: https://deramp.com/downloads/intel/8080%20Data%20Sheet.pdf
// opcode table: http://pastraiser.com/cpu/i8080/i8080_opcodes.html
// 

#include "i8080.hpp"
#include "i8080_opcodes.hpp"

#include <cstring>

#ifdef __has_cpp_attribute
#if __has_cpp_attribute(unlikely)
#define IF_UNLIKELY(x) if (x) [[unlikely]] 
#endif
#endif

#ifndef IF_UNLIKELY
#if defined(__has_builtin)
    #if __has_builtin(__builtin_expect)
    #define HAS_BUILTIN_EXPECT
    #endif
#elif __GNUC__ >= 3
#define HAS_BUILTIN_EXPECT
#endif

#ifdef HAS_BUILTIN_EXPECT
    #define IF_UNLIKELY(x) if (__builtin_expect(!!(x), 0))
#else
    #define IF_UNLIKELY(x) if (x)
#endif
#endif

#define min2(a, b) (((a) < (b)) ? (a) : (b))

#define CARRY_BIT     0
#define PARITY_BIT    2
#define AUX_CARRY_BIT 4
#define ZERO_BIT      6
#define SIGN_BIT      7

#define WORD_MAX 0xff

#define word_lo(word) ((word) & 0x0f)
#define word_hi(word) ((word) >> 4)

#define dword_lo(dword) ((i8080_word_t)((dword) & WORD_MAX))
#define dword_hi(dword) ((i8080_word_t)((dword) >> 8))

#define get_bit(buf, bit) (((buf) >> (bit)) & 0x1)
#define set_bit(ptr, bit, val) (*(ptr) = (*(ptr) & ~(0x1 << (bit))) | ((val) << (bit)))

#define concatenate(word1, word2) (((i8080_dword_t)(word1) << 8) | (word2))


/* Intel manual, pg 77-79 */
/* For conditional RETs and CALLs, add 6 if condition is true. */
static const uint8_t CYCLES[] = {
/*  0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
    4,  10, 7,  5,  5,  5,  7,  4,  4,  10, 7,  5,  5,  5,  7,  4,  /* 0 */
    4,  10, 7,  5,  5,  5,  7,  4,  4,  10, 7,  5,  5,  5,  7,  4,  /* 1 */
    4,  10, 16, 5,  5,  5,  7,  4,  4,  10, 16, 5,  5,  5,  7,  4,  /* 2 */
    4,  10, 13, 5,  10, 10, 10, 4,  4,  10, 13, 5,  5,  5,  7,  4,  /* 3 */
    5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5,  /* 4 */
    5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5,  /* 5 */
    5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5,  /* 6 */
    7,  7,  7,  7,  7,  7,  7,  7,  5,  5,  5,  5,  5,  5,  7,  5,  /* 7 */
    4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,  /* 8 */
    4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,  /* 9 */
    4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,  /* A */
    4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,  /* B */
    5,  10, 10, 10, 11, 11, 7,  11, 5,  10, 10, 10, 11, 17, 7,  11, /* C */
    5,  10, 10, 10, 11, 11, 7,  11, 5,  10, 10, 10, 11, 17, 7,  11, /* D */
    5,  10, 10, 18, 11, 11, 7,  11, 5,  5,  10, 4,  11, 17, 7,  11, /* E */
    5,  10, 10, 4,  11, 11, 7,  11, 5,  5,  10, 4,  11, 17, 7,  11  /* F */
};

/* Get carry out of bit 3. */
static inline i8080_word_t aux_carry(i8080_word_t w1, i8080_word_t w2, i8080_word_t cy) {
    return get_bit(word_lo(w1) + word_lo(w2) + cy, 4);
}

static inline i8080_word_t parity(i8080_word_t w) {
    /* XNOR all bits (even parity) */
    w ^= (w >> 4);
    w ^= (w >> 2);
    w ^= (w >> 1);
    w &= 0x1;
    return !w;
}

/* Update z, s, p flags. */
static inline void update_zsp(i8080* cpu, i8080_word_t word) {
    cpu->z = (word == 0);
    cpu->s = get_bit(word, 7);
    cpu->p = parity(word);
}

static inline i8080_word_t get_flags(i8080* cpu) {
    /* Bit 1 is always 1, see opcode table */
    i8080_word_t flags = 0x02;
    flags |= (
        (cpu->cy << CARRY_BIT) |
        (cpu->p << PARITY_BIT) |
        (cpu->ac << AUX_CARRY_BIT) |
        (cpu->z << ZERO_BIT) |
        (cpu->s << SIGN_BIT));
    return flags;
}

static inline void set_flags(i8080* cpu, i8080_word_t flags) {
    cpu->cy = get_bit(flags, CARRY_BIT);
    cpu->p = get_bit(flags, PARITY_BIT);
    cpu->ac = get_bit(flags, AUX_CARRY_BIT);
    cpu->z = get_bit(flags, ZERO_BIT);
    cpu->s = get_bit(flags, SIGN_BIT);
}

#define get_bc(cpu) concatenate(cpu->b, cpu->c)
#define get_de(cpu) concatenate(cpu->d, cpu->e)
#define get_hl(cpu) concatenate(cpu->h, cpu->l)

/* Get program status (A, flags). */
#define get_psw(cpu) concatenate(cpu->a, get_flags(cpu))

static inline void set_bc(i8080* cpu, i8080_dword_t dword) {
    cpu->b = dword_hi(dword);
    cpu->c = dword_lo(dword);
}
static inline void set_de(i8080* cpu, i8080_dword_t dword) {
    cpu->d = dword_hi(dword);
    cpu->e = dword_lo(dword);
}
static inline void set_hl(i8080* cpu, i8080_dword_t dword) {
    cpu->h = dword_hi(dword);
    cpu->l = dword_lo(dword);
}

/* Set program status (A, flags). */
static inline void set_psw(i8080* cpu, i8080_dword_t dword) {
    cpu->a = dword_hi(dword);
    set_flags(cpu, dword_lo(dword));
}

/* Read word at [HL] */
#define read_mem_hl(cpu) (cpu->mem_read(cpu, get_hl(cpu)))

/* Write word to [HL] */
#define write_mem_hl(cpu, word) (cpu->mem_write(cpu, get_hl(cpu), word))

/* Read word, advance PC by 1. */
static inline i8080_word_t read_word_adv(i8080* cpu) {
    i8080_word_t word = cpu->mem_read(cpu, cpu->pc);
    cpu->pc += 1;
    return word;
}

/* Read address, advance PC by 2. */
static inline i8080_addr_t read_addr_adv(i8080* cpu) {
    i8080_word_t lo = read_word_adv(cpu);
    i8080_word_t hi = read_word_adv(cpu);
    return concatenate(hi, lo);
}

static void i8080_add(i8080* cpu, i8080_word_t word, i8080_word_t cy) {
    i8080_dword_t res = (i8080_dword_t)cpu->a + word + cy;
    cpu->ac = aux_carry(cpu->a, word, cy);
    cpu->cy = get_bit(res, 8);
    cpu->a = dword_lo(res);
    update_zsp(cpu, cpu->a);
}

static void i8080_sub(i8080* cpu, i8080_word_t word, i8080_word_t cy) {
    i8080_dword_t res = (i8080_dword_t)cpu->a + (word ^ WORD_MAX) + !cy;
    cpu->ac = aux_carry(cpu->a, word ^ 0x0f, !cy);
    /* carry is the borrow flag for SUB, SBB etc */
    cpu->cy = !get_bit(res, 8);
    cpu->a = dword_lo(res);
    update_zsp(cpu, cpu->a);
}

static void i8080_ana(i8080* cpu, i8080_word_t word) {
    /* Tandy manual, pg 24 */
    cpu->ac = get_bit(cpu->a, 3) | get_bit(word, 3);
    /* Tandy manual, pg 63 */
    cpu->cy = 0;
    cpu->a &= word;
    update_zsp(cpu, cpu->a);
}

static void i8080_xra(i8080* cpu, i8080_word_t word) {
    cpu->a ^= word;
    update_zsp(cpu, cpu->a);
    /* Tandy manual, pg 122 */
    cpu->ac = 0;
    cpu->cy = 0;
}

static void i8080_ora(i8080* cpu, i8080_word_t word) {
    cpu->a |= word;
    update_zsp(cpu, cpu->a);
    /* Tandy manual, pg 122 */
    cpu->ac = 0;
    cpu->cy = 0;
}

static void i8080_cmp(i8080* cpu, i8080_word_t word) {
    i8080_dword_t res = (i8080_dword_t)cpu->a + (word ^ WORD_MAX) + 1;
    cpu->ac = aux_carry(cpu->a, word ^ 0x0f, 1);
    cpu->cy = !get_bit(res, 8);
    update_zsp(cpu, dword_lo(res));
}

static i8080_word_t i8080_inr(i8080* cpu, i8080_word_t word) {
    cpu->ac = aux_carry(word, 1, 0);
    word++;
    update_zsp(cpu, word);
    return word;
}

static i8080_word_t i8080_dcr(i8080* cpu, i8080_word_t word) {
    cpu->ac = aux_carry(word, 0xf /* (1 ^ 0x0f) + 1 */, 0);
    word--;
    update_zsp(cpu, word);
    return word;
}

static void i8080_dad(i8080* cpu, i8080_dword_t dword) {
    i8080_dword_t old_hl = get_hl(cpu);
    i8080_dword_t new_hl = old_hl + dword;
    set_hl(cpu, new_hl);
    /* check for unsigned overflow */
    cpu->cy = (new_hl < min2(old_hl, dword)) ? 1 : 0;
}

static inline void i8080_shld(i8080* cpu) {
    i8080_addr_t addr = read_addr_adv(cpu);
    cpu->mem_write(cpu, addr, cpu->l);
    addr += 1;
    cpu->mem_write(cpu, addr, cpu->h);
}

static inline void i8080_lhld(i8080* cpu) {
    i8080_addr_t addr = read_addr_adv(cpu);
    cpu->l = cpu->mem_read(cpu, addr);
    addr += 1;
    cpu->h = cpu->mem_read(cpu, addr);
}

/* Circular shift accumulator left, set carry to old MSB. */
static inline void i8080_rlc(i8080* cpu) {
    i8080_word_t msb = get_bit(cpu->a, 7);
    cpu->a <<= 1;
    set_bit(&cpu->a, 0, msb);
    cpu->cy = msb;
}

/* Circular shift accumulator right, set carry to old LSB. */
static inline void i8080_rrc(i8080* cpu) {
    i8080_word_t lsb = get_bit(cpu->a, 0);
    cpu->a >>= 1;
    set_bit(&cpu->a, 7, lsb);
    cpu->cy = lsb;
}

/* Circular shift accumulator left through carry. */
static inline void i8080_ral(i8080* cpu) {
    i8080_word_t old_cy = cpu->cy;
    i8080_word_t msb = get_bit(cpu->a, 7);
    cpu->a <<= 1;
    cpu->cy = msb;
    set_bit(&cpu->a, 0, old_cy);
}

/* Circular shift accumulator right through carry. */
static inline void i8080_rar(i8080* cpu) {
    i8080_word_t old_cy = cpu->cy;
    i8080_word_t lsb = get_bit(cpu->a, 0);
    cpu->a >>= 1;
    cpu->cy = lsb;
    set_bit(&cpu->a, 7, old_cy);
}

/* Decimal adjust accumulator (convert to 4-bit BCD). */
static inline void i8080_daa(i8080* cpu) {
    i8080_word_t lo = word_lo(cpu->a);
    i8080_word_t hi = word_hi(cpu->a);
    /* units */
    if (cpu->ac || lo > 9) {
        cpu->ac = aux_carry(cpu->a, 0x06, 0);
        cpu->a += 0x06;
    }
    /* tens, hundreds */
    if (cpu->cy || hi > 9 || (hi == 9 && lo > 9)) {
        cpu->cy = 1;
        cpu->a += 0x60;
    }
    update_zsp(cpu, cpu->a);
}

static void i8080_push(i8080* cpu, i8080_dword_t dword) {
    i8080_word_t hi = dword_hi(dword);
    i8080_word_t lo = dword_lo(dword);
    cpu->sp -= 1;
    cpu->mem_write(cpu, cpu->sp, hi);
    cpu->sp -= 1;
    cpu->mem_write(cpu, cpu->sp, lo);
}

static i8080_dword_t i8080_pop(i8080* cpu) {
    i8080_word_t lo = cpu->mem_read(cpu, cpu->sp);
    cpu->sp += 1;
    i8080_word_t hi = cpu->mem_read(cpu, cpu->sp);
    cpu->sp += 1;
    return concatenate(hi, lo);
}

static inline void i8080_call_addr(i8080* cpu, i8080_addr_t addr) {
    i8080_push(cpu, cpu->pc);
    cpu->pc = addr;
}

/* Jump to immediate address. */
#define i8080_jmp(cpu) (cpu->pc = read_addr_adv(cpu))

/* Call immediate address. */
#define i8080_call(cpu) i8080_call_addr(cpu, read_addr_adv(cpu))

/* Return from called subroutine. */
#define i8080_ret(cpu) (cpu->pc = i8080_pop(cpu))

static void i8080_cond_jmp(i8080* cpu, i8080_word_t cond) {
    if (cond) { i8080_jmp(cpu); }
    else { cpu->pc += 2; }
}

static void i8080_cond_call(i8080* cpu, i8080_word_t cond) {
    if (cond) {
        i8080_call(cpu);
        cpu->cycles += 6;
    }
    else cpu->pc += 2;
}

static void i8080_cond_ret(i8080* cpu, i8080_word_t cond) {
    if (cond) {
        i8080_ret(cpu);
        cpu->cycles += 6;
    }
}

/* Exchange HL with top two words on the stack. */
static inline void i8080_xthl(i8080* cpu) {
    i8080_word_t lo = cpu->mem_read(cpu, cpu->sp);
    cpu->sp += 1;
    i8080_word_t hi = cpu->mem_read(cpu, cpu->sp);
    cpu->mem_write(cpu, cpu->sp, cpu->h);
    cpu->sp -= 1;
    cpu->mem_write(cpu, cpu->sp, cpu->l);
    cpu->h = hi;
    cpu->l = lo;
}

/* Exchange DE and HL. */
static inline void i8080_xchg(i8080* cpu) {
    i8080_word_t old_h = cpu->h;
    i8080_word_t old_l = cpu->l;
    cpu->h = cpu->d;
    cpu->l = cpu->e;
    cpu->d = old_h;
    cpu->e = old_l;
}

static int i8080_exec(i8080* cpu, i8080_word_t opcode) 
{
    switch (opcode)
    {
    /* NOPs. Do nothing. */
    case i8080_NOP: case i8080_UD_NOP1: case i8080_UD_NOP2: case i8080_UD_NOP3:
    case i8080_UD_NOP4: case i8080_UD_NOP5: case i8080_UD_NOP6: case i8080_UD_NOP7:
        break;

    /* Move between registers */
    case i8080_MOV_B_C: cpu->b = cpu->c; break; case i8080_MOV_B_D: cpu->b = cpu->d; break; case i8080_MOV_B_E: cpu->b = cpu->e; break;
    case i8080_MOV_B_H: cpu->b = cpu->h; break; case i8080_MOV_B_L: cpu->b = cpu->l; break; case i8080_MOV_B_A: cpu->b = cpu->a; break;
    case i8080_MOV_C_B: cpu->c = cpu->b; break; case i8080_MOV_C_D: cpu->c = cpu->d; break; case i8080_MOV_C_E: cpu->c = cpu->e; break;
    case i8080_MOV_C_H: cpu->c = cpu->h; break; case i8080_MOV_C_L: cpu->c = cpu->l; break; case i8080_MOV_C_A: cpu->c = cpu->a; break;
    case i8080_MOV_D_C: cpu->d = cpu->c; break; case i8080_MOV_D_B: cpu->d = cpu->b; break; case i8080_MOV_D_E: cpu->d = cpu->e; break;
    case i8080_MOV_D_H: cpu->d = cpu->h; break; case i8080_MOV_D_L: cpu->d = cpu->l; break; case i8080_MOV_D_A: cpu->d = cpu->a; break;
    case i8080_MOV_E_C: cpu->e = cpu->c; break; case i8080_MOV_E_D: cpu->e = cpu->d; break; case i8080_MOV_E_B: cpu->e = cpu->b; break;
    case i8080_MOV_E_H: cpu->e = cpu->h; break; case i8080_MOV_E_L: cpu->e = cpu->l; break; case i8080_MOV_E_A: cpu->e = cpu->a; break;
    case i8080_MOV_H_C: cpu->h = cpu->c; break; case i8080_MOV_H_D: cpu->h = cpu->d; break; case i8080_MOV_H_E: cpu->h = cpu->e; break;
    case i8080_MOV_H_B: cpu->h = cpu->b; break; case i8080_MOV_H_L: cpu->h = cpu->l; break; case i8080_MOV_H_A: cpu->h = cpu->a; break;
    case i8080_MOV_L_C: cpu->l = cpu->c; break; case i8080_MOV_L_D: cpu->l = cpu->d; break; case i8080_MOV_L_E: cpu->l = cpu->e; break;
    case i8080_MOV_L_H: cpu->l = cpu->h; break; case i8080_MOV_L_B: cpu->l = cpu->b; break; case i8080_MOV_L_A: cpu->l = cpu->a; break;
    case i8080_MOV_A_C: cpu->a = cpu->c; break; case i8080_MOV_A_D: cpu->a = cpu->d; break; case i8080_MOV_A_E: cpu->a = cpu->e; break;
    case i8080_MOV_A_H: cpu->a = cpu->h; break; case i8080_MOV_A_L: cpu->a = cpu->l; break; case i8080_MOV_A_B: cpu->a = cpu->b; break;
    case i8080_MOV_A_A: case i8080_MOV_B_B: case i8080_MOV_C_C: case i8080_MOV_D_D:
    case i8080_MOV_E_E: case i8080_MOV_H_H: case i8080_MOV_L_L: break;

    /* Move memory to register */
    case i8080_MOV_B_M: cpu->b = read_mem_hl(cpu); break;
    case i8080_MOV_C_M: cpu->c = read_mem_hl(cpu); break;
    case i8080_MOV_D_M: cpu->d = read_mem_hl(cpu); break;
    case i8080_MOV_E_M: cpu->e = read_mem_hl(cpu); break;
    case i8080_MOV_H_M: cpu->h = read_mem_hl(cpu); break;
    case i8080_MOV_L_M: cpu->l = read_mem_hl(cpu); break;
    case i8080_MOV_A_M: cpu->a = read_mem_hl(cpu); break;

    /* Move register to memory */
    case i8080_MOV_M_B: write_mem_hl(cpu, cpu->b); break;
    case i8080_MOV_M_C: write_mem_hl(cpu, cpu->c); break;
    case i8080_MOV_M_D: write_mem_hl(cpu, cpu->d); break;
    case i8080_MOV_M_E: write_mem_hl(cpu, cpu->e); break;
    case i8080_MOV_M_H: write_mem_hl(cpu, cpu->h); break;
    case i8080_MOV_M_L: write_mem_hl(cpu, cpu->l); break;
    case i8080_MOV_M_A: write_mem_hl(cpu, cpu->a); break;

    /* Move immediate */
    case i8080_MVI_B: cpu->b = read_word_adv(cpu); break;
    case i8080_MVI_C: cpu->c = read_word_adv(cpu); break;
    case i8080_MVI_D: cpu->d = read_word_adv(cpu); break;
    case i8080_MVI_E: cpu->e = read_word_adv(cpu); break;
    case i8080_MVI_H: cpu->h = read_word_adv(cpu); break;
    case i8080_MVI_L: cpu->l = read_word_adv(cpu); break;
    case i8080_MVI_M: write_mem_hl(cpu, read_word_adv(cpu)); break;
    case i8080_MVI_A: cpu->a = read_word_adv(cpu); break;

    /* Add */
    case i8080_ADD_B: i8080_add(cpu, cpu->b, 0); break;
    case i8080_ADD_C: i8080_add(cpu, cpu->c, 0); break;
    case i8080_ADD_D: i8080_add(cpu, cpu->d, 0); break;
    case i8080_ADD_E: i8080_add(cpu, cpu->e, 0); break;
    case i8080_ADD_H: i8080_add(cpu, cpu->h, 0); break;
    case i8080_ADD_L: i8080_add(cpu, cpu->l, 0); break;
    case i8080_ADD_M: i8080_add(cpu, read_mem_hl(cpu), 0); break;
    case i8080_ADD_A: i8080_add(cpu, cpu->a, 0); break;

    /* Add with carry */
    case i8080_ADC_B: i8080_add(cpu, cpu->b, cpu->cy); break;
    case i8080_ADC_C: i8080_add(cpu, cpu->c, cpu->cy); break;
    case i8080_ADC_D: i8080_add(cpu, cpu->d, cpu->cy); break;
    case i8080_ADC_E: i8080_add(cpu, cpu->e, cpu->cy); break;
    case i8080_ADC_H: i8080_add(cpu, cpu->h, cpu->cy); break;
    case i8080_ADC_L: i8080_add(cpu, cpu->l, cpu->cy); break;
    case i8080_ADC_M: i8080_add(cpu, read_mem_hl(cpu), cpu->cy); break;
    case i8080_ADC_A: i8080_add(cpu, cpu->a, cpu->cy); break;

    /* Subtract */
    case i8080_SUB_B: i8080_sub(cpu, cpu->b, 0); break;
    case i8080_SUB_C: i8080_sub(cpu, cpu->c, 0); break;
    case i8080_SUB_D: i8080_sub(cpu, cpu->d, 0); break;
    case i8080_SUB_E: i8080_sub(cpu, cpu->e, 0); break;
    case i8080_SUB_H: i8080_sub(cpu, cpu->h, 0); break;
    case i8080_SUB_L: i8080_sub(cpu, cpu->l, 0); break;
    case i8080_SUB_M: i8080_sub(cpu, read_mem_hl(cpu), 0); break;
    case i8080_SUB_A: i8080_sub(cpu, cpu->a, 0); break;

    /* Subtract with borrow */
    case i8080_SBB_B: i8080_sub(cpu, cpu->b, cpu->cy); break;
    case i8080_SBB_C: i8080_sub(cpu, cpu->c, cpu->cy); break;
    case i8080_SBB_D: i8080_sub(cpu, cpu->d, cpu->cy); break;
    case i8080_SBB_E: i8080_sub(cpu, cpu->e, cpu->cy); break;
    case i8080_SBB_H: i8080_sub(cpu, cpu->h, cpu->cy); break;
    case i8080_SBB_L: i8080_sub(cpu, cpu->l, cpu->cy); break;
    case i8080_SBB_M: i8080_sub(cpu, read_mem_hl(cpu), cpu->cy); break;
    case i8080_SBB_A: i8080_sub(cpu, cpu->a, cpu->cy); break;

    /* Logical AND */
    case i8080_ANA_B: i8080_ana(cpu, cpu->b); break;
    case i8080_ANA_C: i8080_ana(cpu, cpu->c); break;
    case i8080_ANA_D: i8080_ana(cpu, cpu->d); break;
    case i8080_ANA_E: i8080_ana(cpu, cpu->e); break;
    case i8080_ANA_H: i8080_ana(cpu, cpu->h); break;
    case i8080_ANA_L: i8080_ana(cpu, cpu->l); break;
    case i8080_ANA_M: i8080_ana(cpu, read_mem_hl(cpu)); break;
    case i8080_ANA_A: i8080_ana(cpu, cpu->a); break;

    /* Exclusive logical OR */
    case i8080_XRA_B: i8080_xra(cpu, cpu->b); break;
    case i8080_XRA_C: i8080_xra(cpu, cpu->c); break;
    case i8080_XRA_D: i8080_xra(cpu, cpu->d); break;
    case i8080_XRA_E: i8080_xra(cpu, cpu->e); break;
    case i8080_XRA_H: i8080_xra(cpu, cpu->h); break;
    case i8080_XRA_L: i8080_xra(cpu, cpu->l); break;
    case i8080_XRA_M: i8080_xra(cpu, read_mem_hl(cpu)); break;
    case i8080_XRA_A: i8080_xra(cpu, cpu->a); break;

    /* Inclusive logical OR */
    case i8080_ORA_B: i8080_ora(cpu, cpu->b); break;
    case i8080_ORA_C: i8080_ora(cpu, cpu->c); break;
    case i8080_ORA_D: i8080_ora(cpu, cpu->d); break;
    case i8080_ORA_E: i8080_ora(cpu, cpu->e); break;
    case i8080_ORA_H: i8080_ora(cpu, cpu->h); break;
    case i8080_ORA_L: i8080_ora(cpu, cpu->l); break;
    case i8080_ORA_M: i8080_ora(cpu, read_mem_hl(cpu)); break;
    case i8080_ORA_A: i8080_ora(cpu, cpu->a); break;

    /* Compare */
    case i8080_CMP_B: i8080_cmp(cpu, cpu->b); break;
    case i8080_CMP_C: i8080_cmp(cpu, cpu->c); break;
    case i8080_CMP_D: i8080_cmp(cpu, cpu->d); break;
    case i8080_CMP_E: i8080_cmp(cpu, cpu->e); break;
    case i8080_CMP_H: i8080_cmp(cpu, cpu->h); break;
    case i8080_CMP_L: i8080_cmp(cpu, cpu->l); break;
    case i8080_CMP_M: i8080_cmp(cpu, read_mem_hl(cpu)); break;
    case i8080_CMP_A: i8080_cmp(cpu, cpu->a); break;

    /* Increment */
    case i8080_INR_B: cpu->b = i8080_inr(cpu, cpu->b); break;
    case i8080_INR_C: cpu->c = i8080_inr(cpu, cpu->c); break;
    case i8080_INR_D: cpu->d = i8080_inr(cpu, cpu->d); break;
    case i8080_INR_E: cpu->e = i8080_inr(cpu, cpu->e); break;
    case i8080_INR_H: cpu->h = i8080_inr(cpu, cpu->h); break;
    case i8080_INR_L: cpu->l = i8080_inr(cpu, cpu->l); break;
    case i8080_INR_M: write_mem_hl(cpu, i8080_inr(cpu, read_mem_hl(cpu))); break;
    case i8080_INR_A: cpu->a = i8080_inr(cpu, cpu->a); break;

    /* Decrement */
    case i8080_DCR_B: cpu->b = i8080_dcr(cpu, cpu->b); break;
    case i8080_DCR_C: cpu->c = i8080_dcr(cpu, cpu->c); break;
    case i8080_DCR_D: cpu->d = i8080_dcr(cpu, cpu->d); break;
    case i8080_DCR_E: cpu->e = i8080_dcr(cpu, cpu->e); break;
    case i8080_DCR_H: cpu->h = i8080_dcr(cpu, cpu->h); break;
    case i8080_DCR_L: cpu->l = i8080_dcr(cpu, cpu->l); break;
    case i8080_DCR_M: write_mem_hl(cpu, i8080_dcr(cpu, read_mem_hl(cpu))); break;
    case i8080_DCR_A: cpu->a = i8080_dcr(cpu, cpu->a); break;

    /* Increment or decrement register pair */
    case i8080_INX_B: set_bc(cpu, get_bc(cpu) + 1); break;
    case i8080_INX_D: set_de(cpu, get_de(cpu) + 1); break;
    case i8080_INX_H: set_hl(cpu, get_hl(cpu) + 1); break;
    case i8080_DCX_B: set_bc(cpu, get_bc(cpu) - 1); break;
    case i8080_DCX_D: set_de(cpu, get_de(cpu) - 1); break;
    case i8080_DCX_H: set_hl(cpu, get_hl(cpu) - 1); break;
    case i8080_INX_SP: cpu->sp += 1; break;
    case i8080_DCX_SP: cpu->sp -= 1; break;

    /* Add to register pair (16-bit addition) */
    case i8080_DAD_B: i8080_dad(cpu, get_bc(cpu)); break;
    case i8080_DAD_D: i8080_dad(cpu, get_de(cpu)); break;
    case i8080_DAD_H: i8080_dad(cpu, get_hl(cpu)); break;
    case i8080_DAD_SP: i8080_dad(cpu, cpu->sp); break;

    /* Load register pair from immediate */
    case i8080_LXI_B: cpu->c = read_word_adv(cpu); cpu->b = read_word_adv(cpu); break;
    case i8080_LXI_D: cpu->e = read_word_adv(cpu); cpu->d = read_word_adv(cpu); break;
    case i8080_LXI_H: cpu->l = read_word_adv(cpu); cpu->h = read_word_adv(cpu); break;
    case i8080_LXI_SP: cpu->sp = read_addr_adv(cpu); break;

    /* Indirect load/store accumulator from immediate */
    case i8080_STA: cpu->mem_write(cpu, read_addr_adv(cpu), cpu->a); break;
    case i8080_LDA: cpu->a = cpu->mem_read(cpu, read_addr_adv(cpu)); break;

    /* Indirect load/store accumulator from register pair */
    case i8080_LDAX_B: cpu->a = cpu->mem_read(cpu, get_bc(cpu)); break;
    case i8080_LDAX_D: cpu->a = cpu->mem_read(cpu, get_de(cpu)); break;
    case i8080_STAX_B: cpu->mem_write(cpu, get_bc(cpu), cpu->a); break;
    case i8080_STAX_D: cpu->mem_write(cpu, get_de(cpu), cpu->a); break;

    /* Indirect load/store register pair from immediate */
    case i8080_SHLD: i8080_shld(cpu); break;
    case i8080_LHLD: i8080_lhld(cpu); break;

    /* Rotate (circular shift) */
    case i8080_RLC: i8080_rlc(cpu); break;
    case i8080_RRC: i8080_rrc(cpu); break;
    case i8080_RAL: i8080_ral(cpu); break;
    case i8080_RAR: i8080_rar(cpu); break;

    /* Arithmetic/logical from immediate */
    case i8080_ADI: i8080_add(cpu, read_word_adv(cpu), 0); break;
    case i8080_ACI: i8080_add(cpu, read_word_adv(cpu), cpu->cy); break;
    case i8080_SUI: i8080_sub(cpu, read_word_adv(cpu), 0); break;
    case i8080_SBI: i8080_sub(cpu, read_word_adv(cpu), cpu->cy); break;
    case i8080_ANI: i8080_ana(cpu, read_word_adv(cpu)); break;
    case i8080_XRI: i8080_xra(cpu, read_word_adv(cpu)); break;
    case i8080_ORI: i8080_ora(cpu, read_word_adv(cpu)); break;
    case i8080_CPI: i8080_cmp(cpu, read_word_adv(cpu)); break;

    /* Stack push / pop */
    case i8080_PUSH_B: i8080_push(cpu, get_bc(cpu)); break;
    case i8080_PUSH_D: i8080_push(cpu, get_de(cpu)); break;
    case i8080_PUSH_H: i8080_push(cpu, get_hl(cpu)); break;
    case i8080_PUSH_PSW: i8080_push(cpu, get_psw(cpu)); break;
    case i8080_POP_B: set_bc(cpu, i8080_pop(cpu)); break;
    case i8080_POP_D: set_de(cpu, i8080_pop(cpu)); break;
    case i8080_POP_H: set_hl(cpu, i8080_pop(cpu)); break;
    case i8080_POP_PSW: set_psw(cpu, i8080_pop(cpu)); break;

    /* Call subroutine */
    case i8080_CALL: case i8080_UD_CALL1:
    case i8080_UD_CALL2: case i8080_UD_CALL3:
        i8080_call(cpu); 
        break;
    case i8080_CNZ: i8080_cond_call(cpu, !cpu->z); break;
    case i8080_CZ: i8080_cond_call(cpu, cpu->z); break;
    case i8080_CNC: i8080_cond_call(cpu, !cpu->cy); break;
    case i8080_CC: i8080_cond_call(cpu, cpu->cy); break;
    case i8080_CPO: i8080_cond_call(cpu, !cpu->p); break;
    case i8080_CPE: i8080_cond_call(cpu, cpu->p); break;
    case i8080_CP:  i8080_cond_call(cpu, !cpu->s); break;
    case i8080_CM: i8080_cond_call(cpu, cpu->s); break;

    /* Return from subroutine */
    case i8080_RET: case i8080_UD_RET:
        i8080_ret(cpu);
        break;
    case i8080_RNZ: i8080_cond_ret(cpu, !cpu->z); break;
    case i8080_RZ: i8080_cond_ret(cpu, cpu->z); break;
    case i8080_RNC: i8080_cond_ret(cpu, !cpu->cy); break;
    case i8080_RC: i8080_cond_ret(cpu, cpu->cy); break;
    case i8080_RPO: i8080_cond_ret(cpu, !cpu->p); break;
    case i8080_RPE: i8080_cond_ret(cpu, cpu->p); break;
    case i8080_RP: i8080_cond_ret(cpu, !cpu->s); break;
    case i8080_RM: i8080_cond_ret(cpu, cpu->s); break;

    /* Jump immediate */
    case i8080_JMP: case i8080_UD_JMP:
        i8080_jmp(cpu); 
        break;
    case i8080_JNZ: i8080_cond_jmp(cpu, !cpu->z); break;
    case i8080_JZ: i8080_cond_jmp(cpu, cpu->z); break;
    case i8080_JNC: i8080_cond_jmp(cpu, !cpu->cy); break;
    case i8080_JC: i8080_cond_jmp(cpu, cpu->cy); break;
    case i8080_JPO: i8080_cond_jmp(cpu, !cpu->p); break;
    case i8080_JPE: i8080_cond_jmp(cpu, cpu->p); break;
    case i8080_JP: i8080_cond_jmp(cpu, !cpu->s); break;
    case i8080_JM: i8080_cond_jmp(cpu, cpu->s); break;

    /* Special instructions */
    case i8080_CMA: cpu->a = ~cpu->a; break;         // Complement accumulator
    case i8080_STC: cpu->cy = 1; break;              // Set carry
    case i8080_CMC: cpu->cy = !cpu->cy; break;       // Complement carry
    case i8080_PCHL: cpu->pc = get_hl(cpu); break;   // Move HL into PC
    case i8080_SPHL: cpu->sp = get_hl(cpu); break;   // Move HL into SP
    case i8080_DAA: i8080_daa(cpu); break;
    case i8080_XTHL: i8080_xthl(cpu); break;
    case i8080_XCHG: i8080_xchg(cpu); break;

     /* Read input port into accumulator. */
    case i8080_IN:
        IF_UNLIKELY(!cpu->io_read) { return -1; }
        cpu->a = cpu->io_read(cpu, read_word_adv(cpu));
        break;
    
    /* Write accumulator to output port. */
    case i8080_OUT:
        IF_UNLIKELY(!cpu->io_write) { return -1; }
        cpu->io_write(cpu, read_word_adv(cpu), cpu->a);
        break;

    /* Soft interrupt */
    case i8080_RST_0: i8080_call_addr(cpu, 0x0000); break;
    case i8080_RST_1: i8080_call_addr(cpu, 0x0008); break;
    case i8080_RST_2: i8080_call_addr(cpu, 0x0010); break;
    case i8080_RST_3: i8080_call_addr(cpu, 0x0018); break;
    case i8080_RST_4: i8080_call_addr(cpu, 0x0020); break;
    case i8080_RST_5: i8080_call_addr(cpu, 0x0028); break;
    case i8080_RST_6: i8080_call_addr(cpu, 0x0030); break;
    case i8080_RST_7: i8080_call_addr(cpu, 0x0038); break;

    /* Enable / disable interrupts */
    case i8080_EI: cpu->int_en = 1; break;
    case i8080_DI: cpu->int_en = 0; break;

    /* Halt */
    case i8080_HLT: cpu->halt = 1; break;
    }
    
    cpu->cycles += CYCLES[opcode];
    return 0;
}

void i8080::reset() 
{
    pc = 0;
    int_en = 0;
    int_rq = 0;
    halt = 0;
    cycles = 0;
}

void i8080::interrupt() { int_rq = 1; }

// Follows the state transitions as closely as possible.
// (datasheet pg 7)
int i8080::step() 
{
    // handle interrupt
    if (int_rq) {
        IF_UNLIKELY(!intr_read) { 
            return -1; 
        }
        int_en = 0;
        int_rq = 0;
        halt = 0;
        return i8080_exec(this, intr_read(this));
    }

    IF_UNLIKELY(halt) { 
        return 0;
    }
    // execute next instruction
    return i8080_exec(this, read_word_adv(this));
}

// '?' indicates that the instruction is undocumented
static const char* OP_TO_STR[] = {
    "nop",  "lxi", "stax", "inx", "inr", "dcr", "mvi", "rlc",
    "?nop", "dad", "ldax", "dcx", "inr", "dcr", "mvi", "rrc",
    "?nop", "lxi", "stax", "inx", "inr", "dcr", "mvi", "ral",
    "?nop", "dad", "ldax", "dcx", "inr", "dcr", "mvi", "rar",
    "?nop", "lxi", "shld", "inx", "inr", "dcr", "mvi", "daa",
    "?nop", "dad", "lhld", "dcx", "inr", "dcr", "mvi", "cma",
    "?nop", "lxi", "sta",  "inx", "inr", "dcr", "mvi", "stc",
    "?nop", "dad", "lda",  "dcx", "inr", "dcr", "mvi", "cmc",

    "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov",
    "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov",
    "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov",
    "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov",
    "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov",
    "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov",
    "mov", "mov", "mov", "mov", "mov", "mov", "hlt", "mov",
    "mov", "mov", "mov", "mov", "mov", "mov", "mov", "mov",

    "add", "add", "add", "add", "add", "add", "add", "add",
    "adc", "adc", "adc", "adc", "adc", "adc", "adc", "adc",
    "sub", "sub", "sub", "sub", "sub", "sub", "sub", "sub",
    "sbb", "sbb", "sbb", "sbb", "sbb", "sbb", "sbb", "sbb",

    "ana", "ana", "ana", "ana", "ana", "ana", "ana", "ana",
    "xra", "xra", "xra", "xra", "xra", "xra", "xra", "xra",
    "ora", "ora", "ora", "ora", "ora", "ora", "ora", "ora",
    "cmp", "cmp", "cmp", "cmp", "cmp", "cmp", "cmp", "cmp",

    "rnz", "pop",  "jnz", "jmp",  "cnz", "push",  "adi", "rst",
    "rz",  "ret",  "jz",  "?jmp", "cz",  "call",  "aci", "rst",
    "rnc", "pop",  "jnc", "out",  "cnc", "push",  "sui", "rst",
    "rc",  "?ret", "jc",  "in",   "cc",  "?call", "sbi", "rst",
    "rpo", "pop",  "jpo", "xthl", "cpo", "push",  "ani", "rst",
    "rpe", "pchl", "jpe", "xchg", "cpe", "?call", "xri", "rst",
    "rp",  "pop",  "jp",  "di",   "cp",  "push",  "ori", "rst",
    "rm",  "sphl", "jm",  "ei",   "cm",  "?call", "cpi", "rst"
};

static const char* OPARGS_TO_STR[] = {
    NULL, "b, %04xh",  "b",     "b",  "b", "b", "b, %02xh", NULL,
    NULL, "b",         "b",     "b",  "c", "c", "c, %02xh", NULL,
    NULL, "d, %04xh",  "d",     "d",  "d", "d", "d, %02xh", NULL,
    NULL, "d",         "d",     "d",  "e", "e", "e, %02xh", NULL,
    NULL, "h, %04xh",  "%04xh", "h",  "h", "h", "h, %02xh", NULL,
    NULL, "h",         "%04xh", "h",  "l", "l", "l, %02xh", NULL,
    NULL, "sp, %04xh", "%04xh", "sp", "m", "m", "m, %02xh", NULL,
    NULL, "sp",        "%04xh", "sp", "a", "a", "a, %02xh", NULL,

    "b, b", "b, c", "b, d", "b, e", "b, h", "b, l", "b, m", "b, a",
    "c, b", "c, c", "c, d", "c, e", "c, h", "c, l", "c, m", "c, a",
    "d, b", "d, c", "d, d", "d, e", "d, h", "d, l", "d, m", "d, a",
    "e, b", "e, c", "e, d", "e, e", "e, h", "e, l", "e, m", "e, a",
    "h, b", "h, c", "h, d", "h, e", "h, h", "h, l", "h, m", "h, a",
    "l, b", "l, c", "l, d", "l, e", "l, h", "l, l", "l, m", "l, a",
    "m, b", "m, c", "m, d", "m, e", "m, h", "m, l", NULL,   "m, a",
    "a, b", "a, c", "a, d", "a, e", "a, h", "a, l", "a, m", "a, a",

    "b", "c", "d", "e", "h", "l", "m", "a",
    "b", "c", "d", "e", "h", "l", "m", "a",
    "b", "c", "d", "e", "h", "l", "m", "a",
    "b", "c", "d", "e", "h", "l", "m", "a",

    "b", "c", "d", "e", "h", "l", "m", "a",
    "b", "c", "d", "e", "h", "l", "m", "a",
    "b", "c", "d", "e", "h", "l", "m", "a",
    "b", "c", "d", "e", "h", "l", "m", "a",

    NULL, "b",   "%04xh", "%04xh", "%04xh", "b",     "%02xh", "0",
    NULL, NULL,  "%04xh", "%04xh", "%04xh", "%04xh", "%02xh", "1",
    NULL, "d",   "%04xh", "%02xh", "%04xh", "d",     "%02xh", "2",
    NULL, NULL,  "%04xh", "%02xh", "%04xh", "%04xh", "%02xh", "3",
    NULL, "h",   "%04xh", NULL,    "%04xh", "h",     "%02xh", "4",
    NULL, NULL,  "%04xh", NULL,    "%04xh", "%04xh", "%02xh", "5",
    NULL, "psw", "%04xh", NULL,    "%04xh", "psw",   "%02xh", "6",
    NULL, NULL,  "%04xh", NULL,    "%04xh", "%04xh", "%02xh", "7"
};

void i8080::disassemble(std::FILE* os)
{
    i8080_word_t opcode = read_word_adv(this);

    const char* opname = OP_TO_STR[opcode];
    const char* opargs = OPARGS_TO_STR[opcode];

    std::fprintf(os, "0x%04x\t", pc);

    if (opargs == NULL) {
        fputs(opname, os);
    }
    else { 
        // requires special formatting
        char fmtbuf[64];
        std::strcpy(fmtbuf, "%-6s");
        std::strcat(fmtbuf, opargs);

        switch (opcode)
        {
            // 3 byte instructions
        case i8080_LXI_B: case i8080_LXI_D:
        case i8080_LXI_H: case i8080_LXI_SP:
        case i8080_SHLD: case i8080_LHLD:
        case i8080_STA: case i8080_LDA:
        case i8080_JNZ: case i8080_JZ:
        case i8080_JNC: case i8080_JC:
        case i8080_JPO: case i8080_JPE:
        case i8080_JP:  case i8080_JM:
        case i8080_JMP: case i8080_UD_JMP:
        case i8080_CNZ: case i8080_CZ:
        case i8080_CNC: case i8080_CC:
        case i8080_CPO: case i8080_CPE:
        case i8080_CP:  case i8080_CM:
        case i8080_CALL:
        case i8080_UD_CALL1:
        case i8080_UD_CALL2:
        case i8080_UD_CALL3:
            std::fprintf(os, fmtbuf, opname, read_addr_adv(this));
            break;

            // 2 byte instructions
        case i8080_MVI_B: case i8080_MVI_C:
        case i8080_MVI_D: case i8080_MVI_E:
        case i8080_MVI_H: case i8080_MVI_L:
        case i8080_MVI_M: case i8080_MVI_A:
        case i8080_OUT: case i8080_IN:
        case i8080_ADI: case i8080_ACI:
        case i8080_SUI: case i8080_SBI:
        case i8080_ANI: case i8080_XRI:
        case i8080_ORI: case i8080_CPI:
            std::fprintf(os, fmtbuf, opname, read_word_adv(this));
            break;

            // 1 byte instructions
        default:
            std::fprintf(os, fmtbuf, opname);
            break;
        }
    }
}
