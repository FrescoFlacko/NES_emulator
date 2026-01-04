/*
 * Module: src/cpu/cpu.h
 * Responsibility: 6502 CPU emulation - instruction execution, registers, interrupts.
 * Key invariants:
 *  - Stack pointer (S) is 8-bit; stack lives at $0100-$01FF
 *  - P register bits 4 (B) and 5 (U) are not real flags; B is set only on stack pushes
 *  - All memory access goes through Bus (cpu->bus)
 * Notes:
 *  - Passes nestest (8991/8991 lines) - trace-perfect official + illegal opcodes
 *  - See docs/technical-design.md for architecture overview
 */
#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_U 0x20
#define FLAG_V 0x40
#define FLAG_N 0x80

typedef struct Bus Bus;

typedef struct CPU {
    uint8_t  A;
    uint8_t  X;
    uint8_t  Y;
    uint8_t  P;
    uint8_t  S;
    uint16_t PC;

    uint64_t cycles;

    bool nmi_pending;
    bool irq_pending;

    Bus* bus;
} CPU;

/* Initialize CPU state. Does not set PC (caller must set or call cpu_reset). */
void cpu_init(CPU* cpu, Bus* bus);

/* Reset CPU: sets PC from reset vector ($FFFC), initializes registers. */
void cpu_reset(CPU* cpu);

/* Execute one instruction. Returns cycles consumed. Updates cpu->cycles. */
int  cpu_step(CPU* cpu);

/* Trigger non-maskable interrupt (used by PPU VBlank). */
void cpu_nmi(CPU* cpu);

/* Trigger IRQ if I flag is clear. */
void cpu_irq(CPU* cpu);

/* Write nestest-format trace line to buffer. Requires bus->ppu for PPU timing. */
void cpu_trace(CPU* cpu, char* buffer, size_t size);

#ifdef CPU_TEST_HELPERS
void cpu_push8(CPU* cpu, uint8_t val);
uint8_t cpu_pop8(CPU* cpu);
void cpu_push16(CPU* cpu, uint16_t val);
uint16_t cpu_pop16(CPU* cpu);
void cpu_set_flag(CPU* cpu, uint8_t flag, bool val);
bool cpu_get_flag(CPU* cpu, uint8_t flag);
uint16_t cpu_read16(Bus* bus, uint16_t addr);
uint16_t cpu_read16_zp(Bus* bus, uint8_t addr);
uint16_t cpu_read16_jmp_bug(Bus* bus, uint16_t addr);
#endif

#endif
