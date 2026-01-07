/*
 * Module: src/mapper/mapper.h
 * Responsibility: Mapper abstraction - bank switching, address translation for cartridge variants.
 * Key invariants:
 *  - Mapper struct uses function pointers for polymorphism (cpu_read/write, ppu_read/write)
 *  - mapper_create returns NULL for unsupported mapper IDs
 *  - Each mapper implementation owns its state pointer (freed by mapper_destroy)
 * Notes:
 *  - Only Mapper 0 (NROM) implemented - no bank switching, simplest mapper
 *  - scanline/irq hooks reserved for mappers like MMC3 that use scanline counters
 */
#ifndef MAPPER_H
#define MAPPER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct Cartridge Cartridge;

typedef struct Mapper {
    Cartridge* cart;

    uint8_t (*cpu_read)(struct Mapper* m, uint16_t addr);
    void    (*cpu_write)(struct Mapper* m, uint16_t addr, uint8_t val);
    uint8_t (*ppu_read)(struct Mapper* m, uint16_t addr);
    void    (*ppu_write)(struct Mapper* m, uint16_t addr, uint8_t val);

    void    (*reset)(struct Mapper* m);
    void    (*a12_latch)(struct Mapper* m, uint16_t addr, uint32_t cycle);
    bool    (*irq_pending)(struct Mapper* m);
    void    (*irq_clear)(struct Mapper* m);

    bool    (*save_state)(struct Mapper* m, FILE* f);
    bool    (*load_state)(struct Mapper* m, FILE* f);

    void* state;
} Mapper;

/* Create mapper by ID. Returns NULL if unsupported. Caller owns result. */
Mapper* mapper_create(Cartridge* cart, uint8_t mapper_id);

/* Free mapper and its state. Safe to call with NULL. */
void    mapper_destroy(Mapper* mapper);

/* Mapper 0 (NROM) constructor. */
Mapper* mapper000_create(Cartridge* cart);

/* Mapper 4 (MMC3) constructor. */
Mapper* mapper004_create(Cartridge* cart);

#endif
