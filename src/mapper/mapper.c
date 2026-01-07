/*
 * Module: src/mapper/mapper.c
 * Responsibility: Mapper factory and NROM (Mapper 0) implementation.
 * Key invariants:
 *  - NROM: PRG at $8000-$FFFF mirrors if < 32KB, PRG-RAM at $6000-$7FFF
 *  - NROM: CHR-ROM at $0000-$1FFF, or CHR-RAM if no CHR-ROM
 */
#include "mapper.h"
#include "../cartridge/cartridge.h"
#include <stdlib.h>
#include <string.h>

/*
 * MMC3 A12 Filter Configuration
 * -----------------------------
 * The MMC3 IRQ counter is clocked by A12 rising edges (0->1).
 * To filter out noise (short toggles during sprite fetches), the hardware
 * requires A12 to remain LOW for a minimum duration (M2 cycles) before
 * a new rising edge triggers the counter.
 *
 * Standard value: 12 cycles.
 * 
 * Reason: Sprite fetches occur every 8 dots (257, 265, ..., 321).
 * If A12 is HIGH during sprite fetches (Sprites at $1000), we see
 * rising edges every 8 dots. We MUST ignore these 8-cycle gaps.
 * A filter > 8 is required. 12 is the standard hardware value.
 * This ensures we only clock once per scanline (at the first fetch),
 * and reset the filter only during the long HBlank/Draw period.
 */
#define MMC3_A12_FILTER_DELAY 12

Mapper* mapper_create(Cartridge* cart, uint8_t mapper_id) {
    switch (mapper_id) {
        case 0: return mapper000_create(cart);
        case 4: return mapper004_create(cart);
        default: return NULL;
    }
}

void mapper_destroy(Mapper* mapper) {
    if (mapper) {
        if (mapper->state) free(mapper->state);
        free(mapper);
    }
}

static uint8_t nrom_cpu_read(Mapper* m, uint16_t addr) {
    Cartridge* cart = m->cart;
    if (addr >= 0x8000) {
        uint32_t offset = (addr - 0x8000) % cart->prg_rom_size;
        return cart->prg_rom[offset];
    }
    if (addr >= 0x6000) {
        return cart->prg_ram[addr - 0x6000];
    }
    return 0;
}

static void nrom_cpu_write(Mapper* m, uint16_t addr, uint8_t val) {
    Cartridge* cart = m->cart;
    if (addr >= 0x6000 && addr < 0x8000) {
        cart->prg_ram[addr - 0x6000] = val;
    }
}

static uint8_t nrom_ppu_read(Mapper* m, uint16_t addr) {
    Cartridge* cart = m->cart;
    if (addr < 0x2000) {
        if (cart->chr_rom) {
            return cart->chr_rom[addr];
        } else if (cart->chr_ram) {
            return cart->chr_ram[addr];
        }
    }
    return 0;
}

static void nrom_ppu_write(Mapper* m, uint16_t addr, uint8_t val) {
    Cartridge* cart = m->cart;
    if (addr < 0x2000 && cart->chr_ram) {
        cart->chr_ram[addr] = val;
    }
}

Mapper* mapper000_create(Cartridge* cart) {
    Mapper* m = calloc(1, sizeof(Mapper));
    if (!m) return NULL;

    m->cart = cart;
    m->cpu_read = nrom_cpu_read;
    m->cpu_write = nrom_cpu_write;
    m->ppu_read = nrom_ppu_read;
    m->ppu_write = nrom_ppu_write;
    m->save_state = NULL;
    m->load_state = NULL;
    m->state = NULL;

    return m;
}

typedef struct {
    uint8_t bank_select;
    uint8_t bank_data[8];
    uint8_t prg_mode;
    uint8_t chr_mode;
    uint8_t irq_latch;
    uint8_t irq_counter;
    bool irq_enabled;
    bool irq_pending;
    bool irq_reload;
    uint8_t mirroring;
    uint8_t prg_ram_protect;
    bool prev_a12_high;
    uint32_t last_a12_rise_cycle;
    uint32_t last_a12_high_cycle; /* Tracks the last time A12 was HIGH */
} MMC3State;

static uint8_t mmc3_cpu_read(Mapper* m, uint16_t addr) {
    Cartridge* cart = m->cart;
    MMC3State* s = (MMC3State*)m->state;
    
    if (addr >= 0x8000) {
        uint32_t bank;
        uint32_t prg_banks = cart->prg_rom_size / 8192;
        
        if (addr < 0xA000) {
            bank = s->prg_mode ? (prg_banks - 2) : s->bank_data[6];
        } else if (addr < 0xC000) {
            bank = s->bank_data[7];
        } else if (addr < 0xE000) {
            bank = s->prg_mode ? s->bank_data[6] : (prg_banks - 2);
        } else {
            bank = prg_banks - 1;
        }
        
        bank %= prg_banks;
        uint32_t offset = (bank * 8192) + (addr & 0x1FFF);
        return cart->prg_rom[offset];
    }
    
    if (addr >= 0x6000) {
        if (cart->prg_ram) {
            return cart->prg_ram[addr - 0x6000];
        }
    }
    
    return 0;
}

static void mmc3_cpu_write(Mapper* m, uint16_t addr, uint8_t val) {
    Cartridge* cart = m->cart;
    MMC3State* s = (MMC3State*)m->state;
    
    if (addr >= 0x6000 && addr < 0x8000) {
        if (cart->prg_ram) {
            cart->prg_ram[addr - 0x6000] = val;
        }
        return;
    }
    
    if (addr >= 0x8000) {
        switch (addr & 0xE001) {
            case 0x8000:
                s->bank_select = val & 0x07;
                s->prg_mode = (val >> 6) & 1;
                s->chr_mode = (val >> 7) & 1;
                break;
            case 0x8001:
                s->bank_data[s->bank_select] = val;
                break;
            case 0xA000:
                s->mirroring = val & 1;
                /* MMC3 has opposite mirroring bit meaning compared to iNES/our internal representation */
                cart->mirroring = s->mirroring ^ 1;
                break;
            case 0xA001:
                s->prg_ram_protect = val;
                break;
            case 0xC000:
                s->irq_latch = val;
                break;
            case 0xC001:
                s->irq_counter = 0;
                s->irq_reload = true;
                break;
            case 0xE000:
                s->irq_enabled = false;
                s->irq_pending = false;
                break;
            case 0xE001:
                s->irq_enabled = true;
                break;
        }
    }
}

static uint8_t mmc3_ppu_read(Mapper* m, uint16_t addr) {
    Cartridge* cart = m->cart;
    MMC3State* s = (MMC3State*)m->state;
    
    if (addr < 0x2000) {
        uint32_t bank;
        uint32_t chr_banks = cart->chr_rom_size / 1024;
        if (chr_banks == 0) chr_banks = 8;
        
        if (s->chr_mode == 0) {
            if (addr < 0x0800) {
                bank = s->bank_data[0] & 0xFE;
                bank += (addr >> 10) & 1;
            } else if (addr < 0x1000) {
                bank = s->bank_data[1] & 0xFE;
                bank += ((addr - 0x0800) >> 10) & 1;
            } else if (addr < 0x1400) {
                bank = s->bank_data[2];
            } else if (addr < 0x1800) {
                bank = s->bank_data[3];
            } else if (addr < 0x1C00) {
                bank = s->bank_data[4];
            } else {
                bank = s->bank_data[5];
            }
        } else {
            if (addr < 0x0400) {
                bank = s->bank_data[2];
            } else if (addr < 0x0800) {
                bank = s->bank_data[3];
            } else if (addr < 0x0C00) {
                bank = s->bank_data[4];
            } else if (addr < 0x1000) {
                bank = s->bank_data[5];
            } else if (addr < 0x1800) {
                bank = s->bank_data[0] & 0xFE;
                bank += ((addr - 0x1000) >> 10) & 1;
            } else {
                bank = s->bank_data[1] & 0xFE;
                bank += ((addr - 0x1800) >> 10) & 1;
            }
        }
        
        bank %= chr_banks;
        uint32_t offset = (bank * 1024) + (addr & 0x03FF);
        
        if (cart->chr_rom) {
            return cart->chr_rom[offset];
        } else if (cart->chr_ram) {
            return cart->chr_ram[offset];
        }
    }
    
    return 0;
}

static void mmc3_ppu_write(Mapper* m, uint16_t addr, uint8_t val) {
    Cartridge* cart = m->cart;
    MMC3State* s = (MMC3State*)m->state;
    
    if (addr < 0x2000 && cart->chr_ram) {
        uint32_t bank;
        uint32_t chr_banks = cart->chr_rom_size / 1024;
        if (chr_banks == 0) chr_banks = 8;
        
        if (s->chr_mode == 0) {
            if (addr < 0x0800) {
                bank = s->bank_data[0] & 0xFE;
                bank += (addr >> 10) & 1;
            } else if (addr < 0x1000) {
                bank = s->bank_data[1] & 0xFE;
                bank += ((addr - 0x0800) >> 10) & 1;
            } else if (addr < 0x1400) {
                bank = s->bank_data[2];
            } else if (addr < 0x1800) {
                bank = s->bank_data[3];
            } else if (addr < 0x1C00) {
                bank = s->bank_data[4];
            } else {
                bank = s->bank_data[5];
            }
        } else {
            if (addr < 0x0400) {
                bank = s->bank_data[2];
            } else if (addr < 0x0800) {
                bank = s->bank_data[3];
            } else if (addr < 0x0C00) {
                bank = s->bank_data[4];
            } else if (addr < 0x1000) {
                bank = s->bank_data[5];
            } else if (addr < 0x1800) {
                bank = s->bank_data[0] & 0xFE;
                bank += ((addr - 0x1000) >> 10) & 1;
            } else {
                bank = s->bank_data[1] & 0xFE;
                bank += ((addr - 0x1800) >> 10) & 1;
            }
        }
        
        bank %= chr_banks;
        uint32_t offset = (bank * 1024) + (addr & 0x03FF);
        cart->chr_ram[offset] = val;
    }
}

static void mmc3_scanline(Mapper* m) {
    MMC3State* s = (MMC3State*)m->state;
    
    if (s->irq_counter == 0 || s->irq_reload) {
        s->irq_counter = s->irq_latch;
        s->irq_reload = false;
    } else {
        s->irq_counter--;
    }
    
    if (s->irq_counter == 0 && s->irq_enabled) {
        s->irq_pending = true;
    }
}

static void mmc3_a12_latch(Mapper* m, uint16_t addr, uint32_t cycle) {
    MMC3State* s = (MMC3State*)m->state;
    bool a12_high = (addr & 0x1000) != 0;

    if (a12_high) {
        if (!s->prev_a12_high) {
            if (cycle - s->last_a12_high_cycle > MMC3_A12_FILTER_DELAY) {
                mmc3_scanline(m);
            }
        }
        s->last_a12_high_cycle = cycle;
        s->prev_a12_high = true;
    } else {
        s->prev_a12_high = false;
    }
}

static bool mmc3_irq_pending(Mapper* m) {
    MMC3State* s = (MMC3State*)m->state;
    return s->irq_pending;
}

static void mmc3_irq_clear(Mapper* m) {
    MMC3State* s = (MMC3State*)m->state;
    s->irq_pending = false;
}

static void mmc3_reset(Mapper* m) {
    MMC3State* s = (MMC3State*)m->state;
    memset(s, 0, sizeof(MMC3State));
    s->bank_data[0] = 0;
    s->bank_data[1] = 2;
    s->bank_data[2] = 4;
    s->bank_data[3] = 5;
    s->bank_data[4] = 6;
    s->bank_data[5] = 7;
    s->bank_data[6] = 0;
    s->bank_data[7] = 1;
}

static bool mmc3_save_state(Mapper* m, FILE* f) {
    MMC3State* s = (MMC3State*)m->state;
    return fwrite(s, sizeof(MMC3State), 1, f) == 1;
}

static bool mmc3_load_state(Mapper* m, FILE* f) {
    MMC3State* s = (MMC3State*)m->state;
    return fread(s, sizeof(MMC3State), 1, f) == 1;
}

Mapper* mapper004_create(Cartridge* cart) {
    Mapper* m = calloc(1, sizeof(Mapper));
    if (!m) return NULL;
    
    MMC3State* s = calloc(1, sizeof(MMC3State));
    if (!s) {
        free(m);
        return NULL;
    }
    
    m->cart = cart;
    m->state = s;
    m->cpu_read = mmc3_cpu_read;
    m->cpu_write = mmc3_cpu_write;
    m->ppu_read = mmc3_ppu_read;
    m->ppu_write = mmc3_ppu_write;
    m->a12_latch = mmc3_a12_latch;
    m->irq_pending = mmc3_irq_pending;
    m->irq_clear = mmc3_irq_clear;
    m->reset = mmc3_reset;
    m->save_state = mmc3_save_state;
    m->load_state = mmc3_load_state;
    
    mmc3_reset(m);
    
    return m;
}
