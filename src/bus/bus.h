/*
 * Module: src/bus/bus.h
 * Responsibility: Memory bus - routes CPU reads/writes to RAM, PPU, APU, cartridge.
 * Key invariants:
 *  - RAM ($0000-$07FF) mirrored through $1FFF
 *  - PPU registers ($2000-$2007) mirrored through $3FFF
 *  - bus_tick advances PPU 3× and APU 1× per CPU cycle
 * Notes:
 *  - Controller I/O at $4016/$4017 uses shift register with strobe
 *  - OAM DMA ($4014 write) sets dma_pending; caller handles transfer
 */
#ifndef BUS_H
#define BUS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct CPU CPU;
typedef struct PPU PPU;
typedef struct APU APU;
typedef struct Cartridge Cartridge;

typedef struct Bus {
    CPU* cpu;
    PPU* ppu;
    APU* apu;
    Cartridge* cart;

    uint8_t ram[2048];
    uint8_t controller[2];
    uint8_t controller_state[2];
    uint8_t controller_strobe;

    uint8_t open_bus;
    
    bool dma_pending;
    uint8_t dma_page;
    int dma_cycles;
} Bus;

/* Initialize bus state (zeroes RAM, clears pointers). */
void    bus_init(Bus* bus);

/* Read byte from address. Routes to RAM/PPU/APU/cartridge. */
uint8_t bus_read(Bus* bus, uint16_t addr);

/* Write byte to address. Routes to RAM/PPU/APU/cartridge. */
void    bus_write(Bus* bus, uint16_t addr, uint8_t val);

/* Advance PPU by cpu_cycles*3 dots, APU by cpu_cycles ticks. */
void    bus_tick(Bus* bus, int cpu_cycles);

#endif
