/*
 * Module: src/cartridge/cartridge.c
 * Responsibility: iNES ROM loading implementation.
 * Key invariants:
 *  - Validates "NES\x1A" magic before parsing
 *  - PRG-RAM always 8KB (standard, some mappers vary)
 *  - cartridge_free nulls all pointers to prevent use-after-free
 */
#include "cartridge.h"
#include "../mapper/mapper.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

bool cartridge_load(Cartridge* cart, const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return false;

    uint8_t header[16];
    if (fread(header, 1, 16, f) != 16) {
        fclose(f);
        return false;
    }

    if (header[0] != 'N' || header[1] != 'E' || header[2] != 'S' || header[3] != 0x1A) {
        fclose(f);
        return false;
    }

    uint8_t prg_banks = header[4];
    uint8_t chr_banks = header[5];
    uint8_t flags6 = header[6];
    uint8_t flags7 = header[7];

    cart->mapper_id = (flags7 & 0xF0) | (flags6 >> 4);
    cart->mirroring = (flags6 & 0x01) ? 1 : 0;
    cart->has_battery = (flags6 & 0x02) != 0;

    bool has_trainer = (flags6 & 0x04) != 0;
    if (has_trainer) {
        fseek(f, 512, SEEK_CUR);
    }

    cart->prg_rom_size = prg_banks * 16384;
    cart->prg_rom = malloc(cart->prg_rom_size);
    if (fread(cart->prg_rom, 1, cart->prg_rom_size, f) != cart->prg_rom_size) {
        free(cart->prg_rom);
        fclose(f);
        return false;
    }

    cart->chr_rom_size = chr_banks * 8192;
    if (cart->chr_rom_size > 0) {
        cart->chr_rom = malloc(cart->chr_rom_size);
        if (fread(cart->chr_rom, 1, cart->chr_rom_size, f) != cart->chr_rom_size) {
            free(cart->prg_rom);
            free(cart->chr_rom);
            fclose(f);
            return false;
        }
        cart->chr_ram = NULL;
    } else {
        cart->chr_rom = NULL;
        cart->chr_ram = calloc(8192, 1);
    }

    cart->prg_ram_size = 8192;
    cart->prg_ram = calloc(cart->prg_ram_size, 1);

    fclose(f);

    cart->mapper = mapper_create(cart, cart->mapper_id);
    return cart->mapper != NULL;
}

void cartridge_free(Cartridge* cart) {
    if (cart->prg_rom) free(cart->prg_rom);
    if (cart->chr_rom) free(cart->chr_rom);
    if (cart->chr_ram) free(cart->chr_ram);
    if (cart->prg_ram) free(cart->prg_ram);
    if (cart->mapper) mapper_destroy(cart->mapper);
    memset(cart, 0, sizeof(*cart));
}

uint8_t cartridge_cpu_read(Cartridge* cart, uint16_t addr) {
    if (cart->mapper && cart->mapper->cpu_read) {
        return cart->mapper->cpu_read(cart->mapper, addr);
    }
    return 0;
}

void cartridge_cpu_write(Cartridge* cart, uint16_t addr, uint8_t val) {
    if (cart->mapper && cart->mapper->cpu_write) {
        cart->mapper->cpu_write(cart->mapper, addr, val);
    }
}

uint8_t cartridge_ppu_read(Cartridge* cart, uint16_t addr) {
    if (cart->mapper && cart->mapper->ppu_read) {
        return cart->mapper->ppu_read(cart->mapper, addr);
    }
    return 0;
}

void cartridge_ppu_write(Cartridge* cart, uint16_t addr, uint8_t val) {
    if (cart->mapper && cart->mapper->ppu_write) {
        cart->mapper->ppu_write(cart->mapper, addr, val);
    }
}
