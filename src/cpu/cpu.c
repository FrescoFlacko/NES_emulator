/*
 * Module: src/cpu/cpu.c
 * Responsibility: 6502 CPU implementation - instruction decode, execute, cycle counting.
 * Key invariants:
 *  - Opcode table covers all 256 entries (official + illegal for nestest)
 *  - Page-cross penalties applied per-instruction where documented
 *  - Stack operations: push decrements S, pop increments S
 * Notes:
 *  - Indirect JMP bug ($xxFF wraps within page) emulated via read16_jmp_bug
 *  - SBC implemented as ADC with inverted operand
 */
#include "cpu.h"
#include "../bus/bus.h"
#include "../ppu/ppu.h"
#include <stdio.h>

static inline void set_flag(CPU* cpu, uint8_t flag, bool val) {
    if (val)
        cpu->P |= flag;
    else
        cpu->P &= ~flag;
}

static inline bool get_flag(CPU* cpu, uint8_t flag) {
    return (cpu->P & flag) != 0;
}

static inline void set_ZN(CPU* cpu, uint8_t val) {
    set_flag(cpu, FLAG_Z, val == 0);
    set_flag(cpu, FLAG_N, val & 0x80);
}

static inline void push8(CPU* cpu, uint8_t val) {
    bus_write(cpu->bus, 0x0100 | cpu->S, val);
    cpu->S--;
}

static inline uint8_t pop8(CPU* cpu) {
    cpu->S++;
    return bus_read(cpu->bus, 0x0100 | cpu->S);
}

static inline void push16(CPU* cpu, uint16_t val) {
    push8(cpu, (val >> 8) & 0xFF);
    push8(cpu, val & 0xFF);
}

static inline uint16_t pop16(CPU* cpu) {
    uint16_t lo = pop8(cpu);
    uint16_t hi = pop8(cpu);
    return (hi << 8) | lo;
}

static inline uint16_t read16(Bus* bus, uint16_t addr) {
    uint8_t lo = bus_read(bus, addr);
    uint8_t hi = bus_read(bus, addr + 1);
    return (hi << 8) | lo;
}

static inline uint16_t read16_zp(Bus* bus, uint8_t addr) {
    uint8_t lo = bus_read(bus, addr);
    uint8_t hi = bus_read(bus, (addr + 1) & 0xFF);
    return (hi << 8) | lo;
}

static inline uint16_t read16_jmp_bug(Bus* bus, uint16_t addr) {
    uint8_t lo = bus_read(bus, addr);
    uint16_t hi_addr = (addr & 0xFF00) | ((addr + 1) & 0x00FF);
    uint8_t hi = bus_read(bus, hi_addr);
    return (hi << 8) | lo;
}

typedef enum {
    ADDR_IMP,
    ADDR_ACC,
    ADDR_IMM,
    ADDR_ZP,
    ADDR_ZPX,
    ADDR_ZPY,
    ADDR_ABS,
    ADDR_ABX,
    ADDR_ABY,
    ADDR_IND,
    ADDR_IZX,
    ADDR_IZY,
    ADDR_REL,
} AddrMode;

typedef struct {
    uint16_t addr;
    bool page_crossed;
} AddrResult;

static AddrResult addr_imp(CPU* cpu) {
    (void)cpu;
    return (AddrResult){ 0, false };
}

static AddrResult addr_acc(CPU* cpu) {
    (void)cpu;
    return (AddrResult){ 0, false };
}

static AddrResult addr_imm(CPU* cpu) {
    uint16_t addr = cpu->PC++;
    return (AddrResult){ addr, false };
}

static AddrResult addr_zp(CPU* cpu) {
    uint16_t addr = bus_read(cpu->bus, cpu->PC++);
    return (AddrResult){ addr, false };
}

static AddrResult addr_zpx(CPU* cpu) {
    uint8_t base = bus_read(cpu->bus, cpu->PC++);
    uint16_t addr = (base + cpu->X) & 0xFF;
    return (AddrResult){ addr, false };
}

static AddrResult addr_zpy(CPU* cpu) {
    uint8_t base = bus_read(cpu->bus, cpu->PC++);
    uint16_t addr = (base + cpu->Y) & 0xFF;
    return (AddrResult){ addr, false };
}

static AddrResult addr_abs(CPU* cpu) {
    uint16_t addr = read16(cpu->bus, cpu->PC);
    cpu->PC += 2;
    return (AddrResult){ addr, false };
}

static AddrResult addr_abx(CPU* cpu) {
    uint16_t base = read16(cpu->bus, cpu->PC);
    cpu->PC += 2;
    uint16_t addr = base + cpu->X;
    bool crossed = (base & 0xFF00) != (addr & 0xFF00);
    return (AddrResult){ addr, crossed };
}

static AddrResult addr_aby(CPU* cpu) {
    uint16_t base = read16(cpu->bus, cpu->PC);
    cpu->PC += 2;
    uint16_t addr = base + cpu->Y;
    bool crossed = (base & 0xFF00) != (addr & 0xFF00);
    return (AddrResult){ addr, crossed };
}

static AddrResult addr_ind(CPU* cpu) {
    uint16_t ptr = read16(cpu->bus, cpu->PC);
    cpu->PC += 2;
    uint16_t addr = read16_jmp_bug(cpu->bus, ptr);
    return (AddrResult){ addr, false };
}

static AddrResult addr_izx(CPU* cpu) {
    uint8_t base = bus_read(cpu->bus, cpu->PC++);
    uint8_t ptr = (base + cpu->X) & 0xFF;
    uint16_t addr = read16_zp(cpu->bus, ptr);
    return (AddrResult){ addr, false };
}

static AddrResult addr_izy(CPU* cpu) {
    uint8_t ptr = bus_read(cpu->bus, cpu->PC++);
    uint16_t base = read16_zp(cpu->bus, ptr);
    uint16_t addr = base + cpu->Y;
    bool crossed = (base & 0xFF00) != (addr & 0xFF00);
    return (AddrResult){ addr, crossed };
}

static AddrResult addr_rel(CPU* cpu) {
    int8_t offset = (int8_t)bus_read(cpu->bus, cpu->PC++);
    uint16_t addr = cpu->PC + offset;
    bool crossed = (cpu->PC & 0xFF00) != (addr & 0xFF00);
    return (AddrResult){ addr, crossed };
}

// =============================================================================
// Instruction Implementations
// =============================================================================

// Load/Store
static void op_lda(CPU* cpu, uint16_t addr) {
    cpu->A = bus_read(cpu->bus, addr);
    set_ZN(cpu, cpu->A);
}

static void op_ldx(CPU* cpu, uint16_t addr) {
    cpu->X = bus_read(cpu->bus, addr);
    set_ZN(cpu, cpu->X);
}

static void op_ldy(CPU* cpu, uint16_t addr) {
    cpu->Y = bus_read(cpu->bus, addr);
    set_ZN(cpu, cpu->Y);
}

static void op_sta(CPU* cpu, uint16_t addr) {
    bus_write(cpu->bus, addr, cpu->A);
}

static void op_stx(CPU* cpu, uint16_t addr) {
    bus_write(cpu->bus, addr, cpu->X);
}

static void op_sty(CPU* cpu, uint16_t addr) {
    bus_write(cpu->bus, addr, cpu->Y);
}

// Transfer
static void op_tax(CPU* cpu) { cpu->X = cpu->A; set_ZN(cpu, cpu->X); }
static void op_tay(CPU* cpu) { cpu->Y = cpu->A; set_ZN(cpu, cpu->Y); }
static void op_txa(CPU* cpu) { cpu->A = cpu->X; set_ZN(cpu, cpu->A); }
static void op_tya(CPU* cpu) { cpu->A = cpu->Y; set_ZN(cpu, cpu->A); }
static void op_tsx(CPU* cpu) { cpu->X = cpu->S; set_ZN(cpu, cpu->X); }
static void op_txs(CPU* cpu) { cpu->S = cpu->X; }

// Stack
static void op_pha(CPU* cpu) { push8(cpu, cpu->A); }
static void op_php(CPU* cpu) { push8(cpu, cpu->P | FLAG_B | FLAG_U); }
static void op_pla(CPU* cpu) { cpu->A = pop8(cpu); set_ZN(cpu, cpu->A); }
static void op_plp(CPU* cpu) { cpu->P = (pop8(cpu) & ~FLAG_B) | FLAG_U; }

// Arithmetic
static void op_adc(CPU* cpu, uint8_t val) {
    uint16_t sum = cpu->A + val + (get_flag(cpu, FLAG_C) ? 1 : 0);
    set_flag(cpu, FLAG_C, sum > 0xFF);
    set_flag(cpu, FLAG_V, (~(cpu->A ^ val) & (cpu->A ^ sum)) & 0x80);
    cpu->A = sum & 0xFF;
    set_ZN(cpu, cpu->A);
}

static void op_sbc(CPU* cpu, uint8_t val) {
    op_adc(cpu, ~val);
}

// Logic
static void op_and(CPU* cpu, uint8_t val) {
    cpu->A &= val;
    set_ZN(cpu, cpu->A);
}

static void op_ora(CPU* cpu, uint8_t val) {
    cpu->A |= val;
    set_ZN(cpu, cpu->A);
}

static void op_eor(CPU* cpu, uint8_t val) {
    cpu->A ^= val;
    set_ZN(cpu, cpu->A);
}

// Shift/Rotate (returns result for memory operations)
static uint8_t op_asl(CPU* cpu, uint8_t val) {
    set_flag(cpu, FLAG_C, val & 0x80);
    val <<= 1;
    set_ZN(cpu, val);
    return val;
}

static uint8_t op_lsr(CPU* cpu, uint8_t val) {
    set_flag(cpu, FLAG_C, val & 0x01);
    val >>= 1;
    set_ZN(cpu, val);
    return val;
}

static uint8_t op_rol(CPU* cpu, uint8_t val) {
    uint8_t carry = get_flag(cpu, FLAG_C) ? 1 : 0;
    set_flag(cpu, FLAG_C, val & 0x80);
    val = (val << 1) | carry;
    set_ZN(cpu, val);
    return val;
}

static uint8_t op_ror(CPU* cpu, uint8_t val) {
    uint8_t carry = get_flag(cpu, FLAG_C) ? 0x80 : 0;
    set_flag(cpu, FLAG_C, val & 0x01);
    val = (val >> 1) | carry;
    set_ZN(cpu, val);
    return val;
}

// Inc/Dec
static uint8_t op_inc(CPU* cpu, uint8_t val) {
    val++;
    set_ZN(cpu, val);
    return val;
}

static uint8_t op_dec(CPU* cpu, uint8_t val) {
    val--;
    set_ZN(cpu, val);
    return val;
}

static void op_inx(CPU* cpu) { cpu->X++; set_ZN(cpu, cpu->X); }
static void op_iny(CPU* cpu) { cpu->Y++; set_ZN(cpu, cpu->Y); }
static void op_dex(CPU* cpu) { cpu->X--; set_ZN(cpu, cpu->X); }
static void op_dey(CPU* cpu) { cpu->Y--; set_ZN(cpu, cpu->Y); }

// Compare
static void op_cmp(CPU* cpu, uint8_t val) {
    uint8_t diff = cpu->A - val;
    set_flag(cpu, FLAG_C, cpu->A >= val);
    set_ZN(cpu, diff);
}

static void op_cpx(CPU* cpu, uint8_t val) {
    uint8_t diff = cpu->X - val;
    set_flag(cpu, FLAG_C, cpu->X >= val);
    set_ZN(cpu, diff);
}

static void op_cpy(CPU* cpu, uint8_t val) {
    uint8_t diff = cpu->Y - val;
    set_flag(cpu, FLAG_C, cpu->Y >= val);
    set_ZN(cpu, diff);
}

static void op_bit(CPU* cpu, uint8_t val) {
    set_flag(cpu, FLAG_Z, (cpu->A & val) == 0);
    set_flag(cpu, FLAG_V, val & 0x40);
    set_flag(cpu, FLAG_N, val & 0x80);
}

// Branch helper - returns extra cycles
static int op_branch(CPU* cpu, bool condition, uint16_t addr, bool page_crossed) {
    if (condition) {
        cpu->PC = addr;
        return page_crossed ? 2 : 1;
    }
    return 0;
}

// Flag operations
static void op_clc(CPU* cpu) { set_flag(cpu, FLAG_C, false); }
static void op_cld(CPU* cpu) { set_flag(cpu, FLAG_D, false); }
static void op_cli(CPU* cpu) { set_flag(cpu, FLAG_I, false); }
static void op_clv(CPU* cpu) { set_flag(cpu, FLAG_V, false); }
static void op_sec(CPU* cpu) { set_flag(cpu, FLAG_C, true); }
static void op_sed(CPU* cpu) { set_flag(cpu, FLAG_D, true); }
static void op_sei(CPU* cpu) { set_flag(cpu, FLAG_I, true); }

// Jump/Return
static void op_jmp(CPU* cpu, uint16_t addr) {
    cpu->PC = addr;
}

static void op_jsr(CPU* cpu, uint16_t addr) {
    push16(cpu, cpu->PC - 1);
    cpu->PC = addr;
}

static void op_rts(CPU* cpu) {
    cpu->PC = pop16(cpu) + 1;
}

static void op_rti(CPU* cpu) {
    cpu->P = (pop8(cpu) & ~FLAG_B) | FLAG_U;
    cpu->PC = pop16(cpu);
}

static void op_brk(CPU* cpu) {
    cpu->PC++;
    push16(cpu, cpu->PC);
    push8(cpu, cpu->P | FLAG_B | FLAG_U);
    set_flag(cpu, FLAG_I, true);
    cpu->PC = read16(cpu->bus, 0xFFFE);
}

// =============================================================================
// Opcode Table
// =============================================================================

typedef struct {
    const char* name;
    AddrMode mode;
    uint8_t cycles;
    bool page_penalty;  // Add cycle on page cross
} OpcodeInfo;

static const OpcodeInfo opcode_table[256] = {
    [0x00] = {"BRK", ADDR_IMP, 7, false},
    [0x01] = {"ORA", ADDR_IZX, 6, false},
    [0x05] = {"ORA", ADDR_ZP,  3, false},
    [0x06] = {"ASL", ADDR_ZP,  5, false},
    [0x08] = {"PHP", ADDR_IMP, 3, false},
    [0x09] = {"ORA", ADDR_IMM, 2, false},
    [0x0A] = {"ASL", ADDR_ACC, 2, false},
    [0x0D] = {"ORA", ADDR_ABS, 4, false},
    [0x0E] = {"ASL", ADDR_ABS, 6, false},
    [0x10] = {"BPL", ADDR_REL, 2, true},
    [0x11] = {"ORA", ADDR_IZY, 5, true},
    [0x15] = {"ORA", ADDR_ZPX, 4, false},
    [0x16] = {"ASL", ADDR_ZPX, 6, false},
    [0x18] = {"CLC", ADDR_IMP, 2, false},
    [0x19] = {"ORA", ADDR_ABY, 4, true},
    [0x1D] = {"ORA", ADDR_ABX, 4, true},
    [0x1E] = {"ASL", ADDR_ABX, 7, false},
    [0x20] = {"JSR", ADDR_ABS, 6, false},
    [0x21] = {"AND", ADDR_IZX, 6, false},
    [0x24] = {"BIT", ADDR_ZP,  3, false},
    [0x25] = {"AND", ADDR_ZP,  3, false},
    [0x26] = {"ROL", ADDR_ZP,  5, false},
    [0x28] = {"PLP", ADDR_IMP, 4, false},
    [0x29] = {"AND", ADDR_IMM, 2, false},
    [0x2A] = {"ROL", ADDR_ACC, 2, false},
    [0x2C] = {"BIT", ADDR_ABS, 4, false},
    [0x2D] = {"AND", ADDR_ABS, 4, false},
    [0x2E] = {"ROL", ADDR_ABS, 6, false},
    [0x30] = {"BMI", ADDR_REL, 2, true},
    [0x31] = {"AND", ADDR_IZY, 5, true},
    [0x35] = {"AND", ADDR_ZPX, 4, false},
    [0x36] = {"ROL", ADDR_ZPX, 6, false},
    [0x38] = {"SEC", ADDR_IMP, 2, false},
    [0x39] = {"AND", ADDR_ABY, 4, true},
    [0x3D] = {"AND", ADDR_ABX, 4, true},
    [0x3E] = {"ROL", ADDR_ABX, 7, false},
    [0x40] = {"RTI", ADDR_IMP, 6, false},
    [0x41] = {"EOR", ADDR_IZX, 6, false},
    [0x45] = {"EOR", ADDR_ZP,  3, false},
    [0x46] = {"LSR", ADDR_ZP,  5, false},
    [0x48] = {"PHA", ADDR_IMP, 3, false},
    [0x49] = {"EOR", ADDR_IMM, 2, false},
    [0x4A] = {"LSR", ADDR_ACC, 2, false},
    [0x4C] = {"JMP", ADDR_ABS, 3, false},
    [0x4D] = {"EOR", ADDR_ABS, 4, false},
    [0x4E] = {"LSR", ADDR_ABS, 6, false},
    [0x50] = {"BVC", ADDR_REL, 2, true},
    [0x51] = {"EOR", ADDR_IZY, 5, true},
    [0x55] = {"EOR", ADDR_ZPX, 4, false},
    [0x56] = {"LSR", ADDR_ZPX, 6, false},
    [0x58] = {"CLI", ADDR_IMP, 2, false},
    [0x59] = {"EOR", ADDR_ABY, 4, true},
    [0x5D] = {"EOR", ADDR_ABX, 4, true},
    [0x5E] = {"LSR", ADDR_ABX, 7, false},
    [0x60] = {"RTS", ADDR_IMP, 6, false},
    [0x61] = {"ADC", ADDR_IZX, 6, false},
    [0x65] = {"ADC", ADDR_ZP,  3, false},
    [0x66] = {"ROR", ADDR_ZP,  5, false},
    [0x68] = {"PLA", ADDR_IMP, 4, false},
    [0x69] = {"ADC", ADDR_IMM, 2, false},
    [0x6A] = {"ROR", ADDR_ACC, 2, false},
    [0x6C] = {"JMP", ADDR_IND, 5, false},
    [0x6D] = {"ADC", ADDR_ABS, 4, false},
    [0x6E] = {"ROR", ADDR_ABS, 6, false},
    [0x70] = {"BVS", ADDR_REL, 2, true},
    [0x71] = {"ADC", ADDR_IZY, 5, true},
    [0x75] = {"ADC", ADDR_ZPX, 4, false},
    [0x76] = {"ROR", ADDR_ZPX, 6, false},
    [0x78] = {"SEI", ADDR_IMP, 2, false},
    [0x79] = {"ADC", ADDR_ABY, 4, true},
    [0x7D] = {"ADC", ADDR_ABX, 4, true},
    [0x7E] = {"ROR", ADDR_ABX, 7, false},
    [0x81] = {"STA", ADDR_IZX, 6, false},
    [0x84] = {"STY", ADDR_ZP,  3, false},
    [0x85] = {"STA", ADDR_ZP,  3, false},
    [0x86] = {"STX", ADDR_ZP,  3, false},
    [0x88] = {"DEY", ADDR_IMP, 2, false},
    [0x8A] = {"TXA", ADDR_IMP, 2, false},
    [0x8C] = {"STY", ADDR_ABS, 4, false},
    [0x8D] = {"STA", ADDR_ABS, 4, false},
    [0x8E] = {"STX", ADDR_ABS, 4, false},
    [0x90] = {"BCC", ADDR_REL, 2, true},
    [0x91] = {"STA", ADDR_IZY, 6, false},
    [0x94] = {"STY", ADDR_ZPX, 4, false},
    [0x95] = {"STA", ADDR_ZPX, 4, false},
    [0x96] = {"STX", ADDR_ZPY, 4, false},
    [0x98] = {"TYA", ADDR_IMP, 2, false},
    [0x99] = {"STA", ADDR_ABY, 5, false},
    [0x9A] = {"TXS", ADDR_IMP, 2, false},
    [0x9D] = {"STA", ADDR_ABX, 5, false},
    [0xA0] = {"LDY", ADDR_IMM, 2, false},
    [0xA1] = {"LDA", ADDR_IZX, 6, false},
    [0xA2] = {"LDX", ADDR_IMM, 2, false},
    [0xA4] = {"LDY", ADDR_ZP,  3, false},
    [0xA5] = {"LDA", ADDR_ZP,  3, false},
    [0xA6] = {"LDX", ADDR_ZP,  3, false},
    [0xA8] = {"TAY", ADDR_IMP, 2, false},
    [0xA9] = {"LDA", ADDR_IMM, 2, false},
    [0xAA] = {"TAX", ADDR_IMP, 2, false},
    [0xAC] = {"LDY", ADDR_ABS, 4, false},
    [0xAD] = {"LDA", ADDR_ABS, 4, false},
    [0xAE] = {"LDX", ADDR_ABS, 4, false},
    [0xB0] = {"BCS", ADDR_REL, 2, true},
    [0xB1] = {"LDA", ADDR_IZY, 5, true},
    [0xB4] = {"LDY", ADDR_ZPX, 4, false},
    [0xB5] = {"LDA", ADDR_ZPX, 4, false},
    [0xB6] = {"LDX", ADDR_ZPY, 4, false},
    [0xB8] = {"CLV", ADDR_IMP, 2, false},
    [0xB9] = {"LDA", ADDR_ABY, 4, true},
    [0xBA] = {"TSX", ADDR_IMP, 2, false},
    [0xBC] = {"LDY", ADDR_ABX, 4, true},
    [0xBD] = {"LDA", ADDR_ABX, 4, true},
    [0xBE] = {"LDX", ADDR_ABY, 4, true},
    [0xC0] = {"CPY", ADDR_IMM, 2, false},
    [0xC1] = {"CMP", ADDR_IZX, 6, false},
    [0xC4] = {"CPY", ADDR_ZP,  3, false},
    [0xC5] = {"CMP", ADDR_ZP,  3, false},
    [0xC6] = {"DEC", ADDR_ZP,  5, false},
    [0xC8] = {"INY", ADDR_IMP, 2, false},
    [0xC9] = {"CMP", ADDR_IMM, 2, false},
    [0xCA] = {"DEX", ADDR_IMP, 2, false},
    [0xCC] = {"CPY", ADDR_ABS, 4, false},
    [0xCD] = {"CMP", ADDR_ABS, 4, false},
    [0xCE] = {"DEC", ADDR_ABS, 6, false},
    [0xD0] = {"BNE", ADDR_REL, 2, true},
    [0xD1] = {"CMP", ADDR_IZY, 5, true},
    [0xD5] = {"CMP", ADDR_ZPX, 4, false},
    [0xD6] = {"DEC", ADDR_ZPX, 6, false},
    [0xD8] = {"CLD", ADDR_IMP, 2, false},
    [0xD9] = {"CMP", ADDR_ABY, 4, true},
    [0xDD] = {"CMP", ADDR_ABX, 4, true},
    [0xDE] = {"DEC", ADDR_ABX, 7, false},
    [0xE0] = {"CPX", ADDR_IMM, 2, false},
    [0xE1] = {"SBC", ADDR_IZX, 6, false},
    [0xE4] = {"CPX", ADDR_ZP,  3, false},
    [0xE5] = {"SBC", ADDR_ZP,  3, false},
    [0xE6] = {"INC", ADDR_ZP,  5, false},
    [0xE8] = {"INX", ADDR_IMP, 2, false},
    [0xE9] = {"SBC", ADDR_IMM, 2, false},
    [0xEA] = {"NOP", ADDR_IMP, 2, false},
    [0xEC] = {"CPX", ADDR_ABS, 4, false},
    [0xED] = {"SBC", ADDR_ABS, 4, false},
    [0xEE] = {"INC", ADDR_ABS, 6, false},
    [0xF0] = {"BEQ", ADDR_REL, 2, true},
    [0xF1] = {"SBC", ADDR_IZY, 5, true},
    [0xF5] = {"SBC", ADDR_ZPX, 4, false},
    [0xF6] = {"INC", ADDR_ZPX, 6, false},
    [0xF8] = {"SED", ADDR_IMP, 2, false},
    [0xF9] = {"SBC", ADDR_ABY, 4, true},
    [0xFD] = {"SBC", ADDR_ABX, 4, true},
    [0xFE] = {"INC", ADDR_ABX, 7, false},
    // Illegal opcodes needed for nestest
    [0x04] = {"*NOP", ADDR_ZP,  3, false},
    [0x0C] = {"*NOP", ADDR_ABS, 4, false},
    [0x14] = {"*NOP", ADDR_ZPX, 4, false},
    [0x1A] = {"*NOP", ADDR_IMP, 2, false},
    [0x1C] = {"*NOP", ADDR_ABX, 4, true},
    [0x34] = {"*NOP", ADDR_ZPX, 4, false},
    [0x3A] = {"*NOP", ADDR_IMP, 2, false},
    [0x3C] = {"*NOP", ADDR_ABX, 4, true},
    [0x44] = {"*NOP", ADDR_ZP,  3, false},
    [0x54] = {"*NOP", ADDR_ZPX, 4, false},
    [0x5A] = {"*NOP", ADDR_IMP, 2, false},
    [0x5C] = {"*NOP", ADDR_ABX, 4, true},
    [0x64] = {"*NOP", ADDR_ZP,  3, false},
    [0x74] = {"*NOP", ADDR_ZPX, 4, false},
    [0x7A] = {"*NOP", ADDR_IMP, 2, false},
    [0x7C] = {"*NOP", ADDR_ABX, 4, true},
    [0x80] = {"*NOP", ADDR_IMM, 2, false},
    [0x82] = {"*NOP", ADDR_IMM, 2, false},
    [0x89] = {"*NOP", ADDR_IMM, 2, false},
    [0xC2] = {"*NOP", ADDR_IMM, 2, false},
    [0xD4] = {"*NOP", ADDR_ZPX, 4, false},
    [0xDA] = {"*NOP", ADDR_IMP, 2, false},
    [0xDC] = {"*NOP", ADDR_ABX, 4, true},
    [0xE2] = {"*NOP", ADDR_IMM, 2, false},
    [0xF4] = {"*NOP", ADDR_ZPX, 4, false},
    [0xFA] = {"*NOP", ADDR_IMP, 2, false},
    [0xFC] = {"*NOP", ADDR_ABX, 4, true},
    // LAX
    [0xA3] = {"*LAX", ADDR_IZX, 6, false},
    [0xA7] = {"*LAX", ADDR_ZP,  3, false},
    [0xAF] = {"*LAX", ADDR_ABS, 4, false},
    [0xB3] = {"*LAX", ADDR_IZY, 5, true},
    [0xB7] = {"*LAX", ADDR_ZPY, 4, false},
    [0xBF] = {"*LAX", ADDR_ABY, 4, true},
    // SAX
    [0x83] = {"*SAX", ADDR_IZX, 6, false},
    [0x87] = {"*SAX", ADDR_ZP,  3, false},
    [0x8F] = {"*SAX", ADDR_ABS, 4, false},
    [0x97] = {"*SAX", ADDR_ZPY, 4, false},
    // DCP
    [0xC3] = {"*DCP", ADDR_IZX, 8, false},
    [0xC7] = {"*DCP", ADDR_ZP,  5, false},
    [0xCF] = {"*DCP", ADDR_ABS, 6, false},
    [0xD3] = {"*DCP", ADDR_IZY, 8, false},
    [0xD7] = {"*DCP", ADDR_ZPX, 6, false},
    [0xDB] = {"*DCP", ADDR_ABY, 7, false},
    [0xDF] = {"*DCP", ADDR_ABX, 7, false},
    // ISC (ISB)
    [0xE3] = {"*ISB", ADDR_IZX, 8, false},
    [0xE7] = {"*ISB", ADDR_ZP,  5, false},
    [0xEF] = {"*ISB", ADDR_ABS, 6, false},
    [0xF3] = {"*ISB", ADDR_IZY, 8, false},
    [0xF7] = {"*ISB", ADDR_ZPX, 6, false},
    [0xFB] = {"*ISB", ADDR_ABY, 7, false},
    [0xFF] = {"*ISB", ADDR_ABX, 7, false},
    // SLO
    [0x03] = {"*SLO", ADDR_IZX, 8, false},
    [0x07] = {"*SLO", ADDR_ZP,  5, false},
    [0x0F] = {"*SLO", ADDR_ABS, 6, false},
    [0x13] = {"*SLO", ADDR_IZY, 8, false},
    [0x17] = {"*SLO", ADDR_ZPX, 6, false},
    [0x1B] = {"*SLO", ADDR_ABY, 7, false},
    [0x1F] = {"*SLO", ADDR_ABX, 7, false},
    // RLA
    [0x23] = {"*RLA", ADDR_IZX, 8, false},
    [0x27] = {"*RLA", ADDR_ZP,  5, false},
    [0x2F] = {"*RLA", ADDR_ABS, 6, false},
    [0x33] = {"*RLA", ADDR_IZY, 8, false},
    [0x37] = {"*RLA", ADDR_ZPX, 6, false},
    [0x3B] = {"*RLA", ADDR_ABY, 7, false},
    [0x3F] = {"*RLA", ADDR_ABX, 7, false},
    // SRE
    [0x43] = {"*SRE", ADDR_IZX, 8, false},
    [0x47] = {"*SRE", ADDR_ZP,  5, false},
    [0x4F] = {"*SRE", ADDR_ABS, 6, false},
    [0x53] = {"*SRE", ADDR_IZY, 8, false},
    [0x57] = {"*SRE", ADDR_ZPX, 6, false},
    [0x5B] = {"*SRE", ADDR_ABY, 7, false},
    [0x5F] = {"*SRE", ADDR_ABX, 7, false},
    // RRA
    [0x63] = {"*RRA", ADDR_IZX, 8, false},
    [0x67] = {"*RRA", ADDR_ZP,  5, false},
    [0x6F] = {"*RRA", ADDR_ABS, 6, false},
    [0x73] = {"*RRA", ADDR_IZY, 8, false},
    [0x77] = {"*RRA", ADDR_ZPX, 6, false},
    [0x7B] = {"*RRA", ADDR_ABY, 7, false},
    [0x7F] = {"*RRA", ADDR_ABX, 7, false},
    // ANC
    [0x0B] = {"*ANC", ADDR_IMM, 2, false},
    [0x2B] = {"*ANC", ADDR_IMM, 2, false},
    // ALR
    [0x4B] = {"*ALR", ADDR_IMM, 2, false},
    // ARR
    [0x6B] = {"*ARR", ADDR_IMM, 2, false},
    // AXS
    [0xCB] = {"*AXS", ADDR_IMM, 2, false},
    // SBC illegal
    [0xEB] = {"*SBC", ADDR_IMM, 2, false},
};

void cpu_init(CPU* cpu, Bus* bus) {
    cpu->A = 0;
    cpu->X = 0;
    cpu->Y = 0;
    cpu->P = FLAG_U | FLAG_I;
    cpu->S = 0xFD;
    cpu->PC = 0;
    cpu->cycles = 0;
    cpu->nmi_pending = false;
    cpu->irq_pending = false;
    cpu->bus = bus;
}

void cpu_reset(CPU* cpu) {
    cpu->A = 0;
    cpu->X = 0;
    cpu->Y = 0;
    cpu->P = FLAG_U | FLAG_I;
    cpu->S = 0xFD;
    cpu->PC = read16(cpu->bus, 0xFFFC);
    cpu->cycles = 7;
}

static AddrResult resolve_addr(CPU* cpu, AddrMode mode) {
    switch (mode) {
        case ADDR_IMP: return addr_imp(cpu);
        case ADDR_ACC: return addr_acc(cpu);
        case ADDR_IMM: return addr_imm(cpu);
        case ADDR_ZP:  return addr_zp(cpu);
        case ADDR_ZPX: return addr_zpx(cpu);
        case ADDR_ZPY: return addr_zpy(cpu);
        case ADDR_ABS: return addr_abs(cpu);
        case ADDR_ABX: return addr_abx(cpu);
        case ADDR_ABY: return addr_aby(cpu);
        case ADDR_IND: return addr_ind(cpu);
        case ADDR_IZX: return addr_izx(cpu);
        case ADDR_IZY: return addr_izy(cpu);
        case ADDR_REL: return addr_rel(cpu);
        default: return (AddrResult){0, false};
    }
}

int cpu_step(CPU* cpu) {
    uint8_t opcode = bus_read(cpu->bus, cpu->PC++);
    const OpcodeInfo* info = &opcode_table[opcode];
    AddrResult ar = resolve_addr(cpu, info->mode);
    int extra_cycles = 0;

    switch (opcode) {
        case 0x00: op_brk(cpu); break;
        case 0x01: case 0x05: case 0x09: case 0x0D:
        case 0x11: case 0x15: case 0x19: case 0x1D:
            op_ora(cpu, bus_read(cpu->bus, ar.addr));
            if (info->page_penalty && ar.page_crossed) extra_cycles = 1;
            break;
        case 0x06: case 0x0E: case 0x16: case 0x1E:
            bus_write(cpu->bus, ar.addr, op_asl(cpu, bus_read(cpu->bus, ar.addr)));
            break;
        case 0x0A:
            cpu->A = op_asl(cpu, cpu->A);
            break;
        case 0x08: op_php(cpu); break;
        case 0x10: extra_cycles = op_branch(cpu, !get_flag(cpu, FLAG_N), ar.addr, ar.page_crossed); break;
        case 0x18: op_clc(cpu); break;
        case 0x20: op_jsr(cpu, ar.addr); break;
        case 0x21: case 0x25: case 0x29: case 0x2D:
        case 0x31: case 0x35: case 0x39: case 0x3D:
            op_and(cpu, bus_read(cpu->bus, ar.addr));
            if (info->page_penalty && ar.page_crossed) extra_cycles = 1;
            break;
        case 0x24: case 0x2C:
            op_bit(cpu, bus_read(cpu->bus, ar.addr));
            break;
        case 0x26: case 0x2E: case 0x36: case 0x3E:
            bus_write(cpu->bus, ar.addr, op_rol(cpu, bus_read(cpu->bus, ar.addr)));
            break;
        case 0x2A:
            cpu->A = op_rol(cpu, cpu->A);
            break;
        case 0x28: op_plp(cpu); break;
        case 0x30: extra_cycles = op_branch(cpu, get_flag(cpu, FLAG_N), ar.addr, ar.page_crossed); break;
        case 0x38: op_sec(cpu); break;
        case 0x40: op_rti(cpu); break;
        case 0x41: case 0x45: case 0x49: case 0x4D:
        case 0x51: case 0x55: case 0x59: case 0x5D:
            op_eor(cpu, bus_read(cpu->bus, ar.addr));
            if (info->page_penalty && ar.page_crossed) extra_cycles = 1;
            break;
        case 0x46: case 0x4E: case 0x56: case 0x5E:
            bus_write(cpu->bus, ar.addr, op_lsr(cpu, bus_read(cpu->bus, ar.addr)));
            break;
        case 0x4A:
            cpu->A = op_lsr(cpu, cpu->A);
            break;
        case 0x48: op_pha(cpu); break;
        case 0x4C: case 0x6C: op_jmp(cpu, ar.addr); break;
        case 0x50: extra_cycles = op_branch(cpu, !get_flag(cpu, FLAG_V), ar.addr, ar.page_crossed); break;
        case 0x58: op_cli(cpu); break;
        case 0x60: op_rts(cpu); break;
        case 0x61: case 0x65: case 0x69: case 0x6D:
        case 0x71: case 0x75: case 0x79: case 0x7D:
            op_adc(cpu, bus_read(cpu->bus, ar.addr));
            if (info->page_penalty && ar.page_crossed) extra_cycles = 1;
            break;
        case 0x66: case 0x6E: case 0x76: case 0x7E:
            bus_write(cpu->bus, ar.addr, op_ror(cpu, bus_read(cpu->bus, ar.addr)));
            break;
        case 0x6A:
            cpu->A = op_ror(cpu, cpu->A);
            break;
        case 0x68: op_pla(cpu); break;
        case 0x70: extra_cycles = op_branch(cpu, get_flag(cpu, FLAG_V), ar.addr, ar.page_crossed); break;
        case 0x78: op_sei(cpu); break;
        case 0x81: case 0x85: case 0x8D: case 0x91:
        case 0x95: case 0x99: case 0x9D:
            op_sta(cpu, ar.addr);
            break;
        case 0x84: case 0x8C: case 0x94:
            op_sty(cpu, ar.addr);
            break;
        case 0x86: case 0x8E: case 0x96:
            op_stx(cpu, ar.addr);
            break;
        case 0x88: op_dey(cpu); break;
        case 0x8A: op_txa(cpu); break;
        case 0x90: extra_cycles = op_branch(cpu, !get_flag(cpu, FLAG_C), ar.addr, ar.page_crossed); break;
        case 0x98: op_tya(cpu); break;
        case 0x9A: op_txs(cpu); break;
        case 0xA0: case 0xA4: case 0xAC: case 0xB4: case 0xBC:
            op_ldy(cpu, ar.addr);
            if (info->page_penalty && ar.page_crossed) extra_cycles = 1;
            break;
        case 0xA1: case 0xA5: case 0xA9: case 0xAD:
        case 0xB1: case 0xB5: case 0xB9: case 0xBD:
            op_lda(cpu, ar.addr);
            if (info->page_penalty && ar.page_crossed) extra_cycles = 1;
            break;
        case 0xA2: case 0xA6: case 0xAE: case 0xB6: case 0xBE:
            op_ldx(cpu, ar.addr);
            if (info->page_penalty && ar.page_crossed) extra_cycles = 1;
            break;
        case 0xA8: op_tay(cpu); break;
        case 0xAA: op_tax(cpu); break;
        case 0xB0: extra_cycles = op_branch(cpu, get_flag(cpu, FLAG_C), ar.addr, ar.page_crossed); break;
        case 0xB8: op_clv(cpu); break;
        case 0xBA: op_tsx(cpu); break;
        case 0xC0: case 0xC4: case 0xCC:
            op_cpy(cpu, bus_read(cpu->bus, ar.addr));
            break;
        case 0xC1: case 0xC5: case 0xC9: case 0xCD:
        case 0xD1: case 0xD5: case 0xD9: case 0xDD:
            op_cmp(cpu, bus_read(cpu->bus, ar.addr));
            if (info->page_penalty && ar.page_crossed) extra_cycles = 1;
            break;
        case 0xC6: case 0xCE: case 0xD6: case 0xDE:
            bus_write(cpu->bus, ar.addr, op_dec(cpu, bus_read(cpu->bus, ar.addr)));
            break;
        case 0xC8: op_iny(cpu); break;
        case 0xCA: op_dex(cpu); break;
        case 0xD0: extra_cycles = op_branch(cpu, !get_flag(cpu, FLAG_Z), ar.addr, ar.page_crossed); break;
        case 0xD8: op_cld(cpu); break;
        case 0xE0: case 0xE4: case 0xEC:
            op_cpx(cpu, bus_read(cpu->bus, ar.addr));
            break;
        case 0xE1: case 0xE5: case 0xE9: case 0xED:
        case 0xF1: case 0xF5: case 0xF9: case 0xFD:
        case 0xEB:
            op_sbc(cpu, bus_read(cpu->bus, ar.addr));
            if (info->page_penalty && ar.page_crossed) extra_cycles = 1;
            break;
        case 0xE6: case 0xEE: case 0xF6: case 0xFE:
            bus_write(cpu->bus, ar.addr, op_inc(cpu, bus_read(cpu->bus, ar.addr)));
            break;
        case 0xE8: op_inx(cpu); break;
        case 0xEA: break;
        case 0xF0: extra_cycles = op_branch(cpu, get_flag(cpu, FLAG_Z), ar.addr, ar.page_crossed); break;
        case 0xF8: op_sed(cpu); break;
        case 0x04: case 0x0C: case 0x14: case 0x1A: case 0x1C:
        case 0x34: case 0x3A: case 0x3C: case 0x44: case 0x54:
        case 0x5A: case 0x5C: case 0x64: case 0x74: case 0x7A:
        case 0x7C: case 0x80: case 0x82: case 0x89: case 0xC2:
        case 0xD4: case 0xDA: case 0xDC: case 0xE2: case 0xF4:
        case 0xFA: case 0xFC:
            if (info->page_penalty && ar.page_crossed) extra_cycles = 1;
            break;
        case 0xA3: case 0xA7: case 0xAF: case 0xB3: case 0xB7: case 0xBF:
            cpu->A = cpu->X = bus_read(cpu->bus, ar.addr);
            set_ZN(cpu, cpu->A);
            if (info->page_penalty && ar.page_crossed) extra_cycles = 1;
            break;
        case 0x83: case 0x87: case 0x8F: case 0x97:
            bus_write(cpu->bus, ar.addr, cpu->A & cpu->X);
            break;
        case 0xC3: case 0xC7: case 0xCF: case 0xD3: case 0xD7: case 0xDB: case 0xDF: {
            uint8_t val = op_dec(cpu, bus_read(cpu->bus, ar.addr));
            bus_write(cpu->bus, ar.addr, val);
            op_cmp(cpu, val);
            break;
        }
        case 0xE3: case 0xE7: case 0xEF: case 0xF3: case 0xF7: case 0xFB: case 0xFF: {
            uint8_t val = op_inc(cpu, bus_read(cpu->bus, ar.addr));
            bus_write(cpu->bus, ar.addr, val);
            op_sbc(cpu, val);
            break;
        }
        case 0x03: case 0x07: case 0x0F: case 0x13: case 0x17: case 0x1B: case 0x1F: {
            uint8_t val = op_asl(cpu, bus_read(cpu->bus, ar.addr));
            bus_write(cpu->bus, ar.addr, val);
            op_ora(cpu, val);
            break;
        }
        case 0x23: case 0x27: case 0x2F: case 0x33: case 0x37: case 0x3B: case 0x3F: {
            uint8_t val = op_rol(cpu, bus_read(cpu->bus, ar.addr));
            bus_write(cpu->bus, ar.addr, val);
            op_and(cpu, val);
            break;
        }
        case 0x43: case 0x47: case 0x4F: case 0x53: case 0x57: case 0x5B: case 0x5F: {
            uint8_t val = op_lsr(cpu, bus_read(cpu->bus, ar.addr));
            bus_write(cpu->bus, ar.addr, val);
            op_eor(cpu, val);
            break;
        }
        case 0x63: case 0x67: case 0x6F: case 0x73: case 0x77: case 0x7B: case 0x7F: {
            uint8_t val = op_ror(cpu, bus_read(cpu->bus, ar.addr));
            bus_write(cpu->bus, ar.addr, val);
            op_adc(cpu, val);
            break;
        }
        case 0x0B: case 0x2B:
            op_and(cpu, bus_read(cpu->bus, ar.addr));
            set_flag(cpu, FLAG_C, cpu->A & 0x80);
            break;
        case 0x4B:
            op_and(cpu, bus_read(cpu->bus, ar.addr));
            cpu->A = op_lsr(cpu, cpu->A);
            break;
        case 0x6B: {
            op_and(cpu, bus_read(cpu->bus, ar.addr));
            cpu->A = op_ror(cpu, cpu->A);
            set_flag(cpu, FLAG_C, cpu->A & 0x40);
            set_flag(cpu, FLAG_V, ((cpu->A & 0x40) ^ ((cpu->A & 0x20) << 1)));
            break;
        }
        case 0xCB: {
            uint8_t val = bus_read(cpu->bus, ar.addr);
            uint8_t result = (cpu->A & cpu->X) - val;
            set_flag(cpu, FLAG_C, (cpu->A & cpu->X) >= val);
            cpu->X = result;
            set_ZN(cpu, cpu->X);
            break;
        }
        default:
            break;
    }

    int cycles = info->cycles + extra_cycles;
    cpu->cycles += cycles;
    return cycles;
}

void cpu_nmi(CPU* cpu) {
    push16(cpu, cpu->PC);
    push8(cpu, (cpu->P | FLAG_U) & ~FLAG_B);
    set_flag(cpu, FLAG_I, true);
    cpu->PC = read16(cpu->bus, 0xFFFA);
    cpu->cycles += 7;
}

void cpu_irq(CPU* cpu) {
    if (get_flag(cpu, FLAG_I)) return;
    push16(cpu, cpu->PC);
    push8(cpu, (cpu->P | FLAG_U) & ~FLAG_B);
    set_flag(cpu, FLAG_I, true);
    cpu->PC = read16(cpu->bus, 0xFFFE);
    cpu->cycles += 7;
}

void cpu_trace(CPU* cpu, char* buffer, size_t size) {
    uint16_t pc = cpu->PC;
    uint8_t opcode = bus_read(cpu->bus, pc);
    const OpcodeInfo* info = &opcode_table[opcode];
    
    char opcode_bytes[10];
    char disasm[32];
    
    uint8_t b1 = bus_read(cpu->bus, pc + 1);
    uint8_t b2 = bus_read(cpu->bus, pc + 2);
    
    switch (info->mode) {
        case ADDR_IMP:
        case ADDR_ACC:
            snprintf(opcode_bytes, sizeof(opcode_bytes), "%02X", opcode);
            if (info->mode == ADDR_ACC)
                snprintf(disasm, sizeof(disasm), "%s A", info->name);
            else
                snprintf(disasm, sizeof(disasm), "%s", info->name);
            break;
        case ADDR_IMM:
            snprintf(opcode_bytes, sizeof(opcode_bytes), "%02X %02X", opcode, b1);
            snprintf(disasm, sizeof(disasm), "%s #$%02X", info->name, b1);
            break;
        case ADDR_ZP: {
            uint8_t val = bus_read(cpu->bus, b1);
            snprintf(opcode_bytes, sizeof(opcode_bytes), "%02X %02X", opcode, b1);
            snprintf(disasm, sizeof(disasm), "%s $%02X = %02X", info->name, b1, val);
            break;
        }
        case ADDR_ZPX: {
            uint8_t addr = (b1 + cpu->X) & 0xFF;
            uint8_t val = bus_read(cpu->bus, addr);
            snprintf(opcode_bytes, sizeof(opcode_bytes), "%02X %02X", opcode, b1);
            snprintf(disasm, sizeof(disasm), "%s $%02X,X @ %02X = %02X", info->name, b1, addr, val);
            break;
        }
        case ADDR_ZPY: {
            uint8_t addr = (b1 + cpu->Y) & 0xFF;
            uint8_t val = bus_read(cpu->bus, addr);
            snprintf(opcode_bytes, sizeof(opcode_bytes), "%02X %02X", opcode, b1);
            snprintf(disasm, sizeof(disasm), "%s $%02X,Y @ %02X = %02X", info->name, b1, addr, val);
            break;
        }
        case ADDR_ABS: {
            uint16_t addr = (b2 << 8) | b1;
            snprintf(opcode_bytes, sizeof(opcode_bytes), "%02X %02X %02X", opcode, b1, b2);
            if (opcode == 0x4C || opcode == 0x20) {
                snprintf(disasm, sizeof(disasm), "%s $%04X", info->name, addr);
            } else {
                uint8_t val = bus_read(cpu->bus, addr);
                snprintf(disasm, sizeof(disasm), "%s $%04X = %02X", info->name, addr, val);
            }
            break;
        }
        case ADDR_ABX: {
            uint16_t base = (b2 << 8) | b1;
            uint16_t addr = base + cpu->X;
            uint8_t val = bus_read(cpu->bus, addr);
            snprintf(opcode_bytes, sizeof(opcode_bytes), "%02X %02X %02X", opcode, b1, b2);
            snprintf(disasm, sizeof(disasm), "%s $%04X,X @ %04X = %02X", info->name, base, addr, val);
            break;
        }
        case ADDR_ABY: {
            uint16_t base = (b2 << 8) | b1;
            uint16_t addr = base + cpu->Y;
            uint8_t val = bus_read(cpu->bus, addr);
            snprintf(opcode_bytes, sizeof(opcode_bytes), "%02X %02X %02X", opcode, b1, b2);
            snprintf(disasm, sizeof(disasm), "%s $%04X,Y @ %04X = %02X", info->name, base, addr, val);
            break;
        }
        case ADDR_IND: {
            uint16_t ptr = (b2 << 8) | b1;
            uint16_t lo_addr = ptr;
            uint16_t hi_addr = (ptr & 0xFF00) | ((ptr + 1) & 0x00FF);
            uint16_t addr = bus_read(cpu->bus, lo_addr) | (bus_read(cpu->bus, hi_addr) << 8);
            snprintf(opcode_bytes, sizeof(opcode_bytes), "%02X %02X %02X", opcode, b1, b2);
            snprintf(disasm, sizeof(disasm), "%s ($%04X) = %04X", info->name, ptr, addr);
            break;
        }
        case ADDR_IZX: {
            uint8_t ptr = (b1 + cpu->X) & 0xFF;
            uint16_t addr = bus_read(cpu->bus, ptr) | (bus_read(cpu->bus, (ptr + 1) & 0xFF) << 8);
            uint8_t val = bus_read(cpu->bus, addr);
            snprintf(opcode_bytes, sizeof(opcode_bytes), "%02X %02X", opcode, b1);
            snprintf(disasm, sizeof(disasm), "%s ($%02X,X) @ %02X = %04X = %02X", info->name, b1, ptr, addr, val);
            break;
        }
        case ADDR_IZY: {
            uint16_t base = bus_read(cpu->bus, b1) | (bus_read(cpu->bus, (b1 + 1) & 0xFF) << 8);
            uint16_t addr = base + cpu->Y;
            uint8_t val = bus_read(cpu->bus, addr);
            snprintf(opcode_bytes, sizeof(opcode_bytes), "%02X %02X", opcode, b1);
            snprintf(disasm, sizeof(disasm), "%s ($%02X),Y = %04X @ %04X = %02X", info->name, b1, base, addr, val);
            break;
        }
        case ADDR_REL: {
            int8_t offset = (int8_t)b1;
            uint16_t addr = pc + 2 + offset;
            snprintf(opcode_bytes, sizeof(opcode_bytes), "%02X %02X", opcode, b1);
            snprintf(disasm, sizeof(disasm), "%s $%04X", info->name, addr);
            break;
        }
        default:
            snprintf(opcode_bytes, sizeof(opcode_bytes), "%02X", opcode);
            snprintf(disasm, sizeof(disasm), "???");
            break;
    }
    
    int scanline = 0, dot = 0;
    if (cpu->bus && cpu->bus->ppu) {
        scanline = cpu->bus->ppu->scanline;
        dot = cpu->bus->ppu->dot;
    }
    
    bool is_illegal = info->name && info->name[0] == '*';
    if (is_illegal) {
        snprintf(buffer, size, "%04X  %-9s%-33sA:%02X X:%02X Y:%02X P:%02X SP:%02X PPU:%3d,%3d CYC:%llu",
            pc, opcode_bytes, disasm,
            cpu->A, cpu->X, cpu->Y, cpu->P, cpu->S,
            scanline, dot, (unsigned long long)cpu->cycles);
    } else {
        snprintf(buffer, size, "%04X  %-10s%-32sA:%02X X:%02X Y:%02X P:%02X SP:%02X PPU:%3d,%3d CYC:%llu",
            pc, opcode_bytes, disasm,
            cpu->A, cpu->X, cpu->Y, cpu->P, cpu->S,
            scanline, dot, (unsigned long long)cpu->cycles);
    }
}

#ifdef CPU_TEST_HELPERS
void cpu_push8(CPU* cpu, uint8_t val) { push8(cpu, val); }
uint8_t cpu_pop8(CPU* cpu) { return pop8(cpu); }
void cpu_push16(CPU* cpu, uint16_t val) { push16(cpu, val); }
uint16_t cpu_pop16(CPU* cpu) { return pop16(cpu); }
void cpu_set_flag(CPU* cpu, uint8_t flag, bool val) { set_flag(cpu, flag, val); }
bool cpu_get_flag(CPU* cpu, uint8_t flag) { return get_flag(cpu, flag); }
uint16_t cpu_read16(Bus* bus, uint16_t addr) { return read16(bus, addr); }
uint16_t cpu_read16_zp(Bus* bus, uint8_t addr) { return read16_zp(bus, addr); }
uint16_t cpu_read16_jmp_bug(Bus* bus, uint16_t addr) { return read16_jmp_bug(bus, addr); }
#endif
