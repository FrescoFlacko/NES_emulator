/*
 * Module: src/cartridge/cartridge.h
 * Responsibility: ROM loading and cartridge abstraction - iNES parsing, memory access routing.
 * Key invariants:
 *  - iNES header: "NES\x1A" magic, PRG size = header[4] * 16KB, CHR size = header[5] * 8KB
 *  - If chr_rom_size == 0, cartridge uses CHR-RAM (8KB allocated)
 *  - All cartridge memory access delegated to mapper via function pointers
 * Notes:
 *  - Trainer (512B before PRG) is skipped if present
 *  - cartridge_load returns false if mapper is unsupported
 */
#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct Mapper Mapper;

typedef struct Cartridge {
    uint8_t* prg_rom;
    uint8_t* chr_rom;
    uint8_t* chr_ram;
    uint8_t* prg_ram;

    uint32_t prg_rom_size;
    uint32_t chr_rom_size;
    uint32_t prg_ram_size;

    uint8_t mapper_id;
    uint8_t mirroring;
    bool    has_battery;

    Mapper* mapper;
} Cartridge;

/* Load ROM from file. Parses iNES header, allocates PRG/CHR, creates mapper. */
bool cartridge_load(Cartridge* cart, const char* filename);

/* Free all cartridge memory (PRG, CHR, PRG-RAM, mapper). */
void cartridge_free(Cartridge* cart);

/* CPU-side read ($6000-$FFFF). Delegated to mapper. */
uint8_t cartridge_cpu_read(Cartridge* cart, uint16_t addr);

/* CPU-side write ($6000-$FFFF). Delegated to mapper. */
void    cartridge_cpu_write(Cartridge* cart, uint16_t addr, uint8_t val);

/* PPU-side read ($0000-$1FFF pattern tables). Delegated to mapper. */
uint8_t cartridge_ppu_read(Cartridge* cart, uint16_t addr);

/* PPU-side write ($0000-$1FFF for CHR-RAM). Delegated to mapper. */
void    cartridge_ppu_write(Cartridge* cart, uint16_t addr, uint8_t val);

#endif
