/*
 * Module: src/ppu/ppu.c
 * Responsibility: PPU rendering pipeline and register handling.
 * Key invariants:
 *  - Background fetches every 8 dots (NT → AT → pattern lo → pattern hi)
 *  - Sprite evaluation at dot 257, sprite fetches distributed over 257-320
 *  - v/t scroll registers updated: inc_x at end of tile, inc_y at dot 256, copy_x at 257
 * Notes:
 *  - nes_palette[] is ARGB8888 for direct framebuffer use
 *  - Palette mirroring: $3F10/$3F14/$3F18/$3F1C mirror $3F00/$3F04/$3F08/$3F0C
 */
#include "ppu.h"
#include "../cartridge/cartridge.h"
#include "../mapper/mapper.h"
#include <string.h>
#include <stdio.h>

static const uint32_t nes_palette[64] = {
    0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00,
    0x333500, 0x0B4800, 0x005200, 0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000,
    0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00,
    0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000,
    0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22,
    0xBCBE00, 0x88D800, 0x5CE430, 0x45E082, 0x48CDDE, 0x4F4F4F, 0x000000, 0x000000,
    0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5, 0xF7D8A5,
    0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000
};

void ppu_init(PPU* ppu) {
    memset(ppu, 0, sizeof(*ppu));
}

void ppu_reset(PPU* ppu) {
    ppu->scanline = 0;
    ppu->dot = 0;
    ppu->frame = 0;
    ppu->ctrl = 0;
    ppu->mask = 0;
    ppu->status = 0;
    ppu->oam_addr = 0;
    ppu->v = 0;
    ppu->t = 0;
    ppu->fine_x = 0;
    ppu->w = false;
    ppu->data_buffer = 0;
    ppu->frame_ready = false;
    ppu->nmi_occurred = false;
    ppu->nmi_output = false;
    ppu->nmi_pending = false;
    ppu->odd_frame = false;
}

static uint16_t ppu_mirror_vram_addr(PPU* ppu, uint16_t addr) {
    addr = (addr - 0x2000) & 0x0FFF;
    
    uint8_t mirroring = ppu->cart ? ppu->cart->mirroring : 0;
    
    uint16_t nametable = addr / 0x0400;
    uint16_t offset = addr % 0x0400;
    
    if (mirroring == 0) {
        nametable = (nametable & 0x02) >> 1;
    } else {
        nametable = nametable & 0x01;
    }
    
    return nametable * 0x0400 + offset;
}

static uint8_t ppu_read(PPU* ppu, uint16_t addr) {
    addr &= 0x3FFF;
    
    if (ppu->cart && ppu->cart->mapper && ppu->cart->mapper->a12_latch) {
        uint32_t current_cycle = ppu->scanline * 341 + ppu->dot;
        ppu->cart->mapper->a12_latch(ppu->cart->mapper, addr, current_cycle);
    }
    
    if (addr < 0x2000) {
        if (ppu->cart) {
            return cartridge_ppu_read(ppu->cart, addr);
        }
        return 0;
    } else if (addr < 0x3F00) {
        return ppu->vram[ppu_mirror_vram_addr(ppu, addr)];
    } else {
        addr &= 0x1F;
        if (addr == 0x10 || addr == 0x14 || addr == 0x18 || addr == 0x1C) {
            addr -= 0x10;
        }
        return ppu->palette[addr];
    }
}

static void ppu_write(PPU* ppu, uint16_t addr, uint8_t val) {
    addr &= 0x3FFF;
    
    if (ppu->cart && ppu->cart->mapper && ppu->cart->mapper->a12_latch) {
        uint32_t current_cycle = ppu->scanline * 341 + ppu->dot;
        ppu->cart->mapper->a12_latch(ppu->cart->mapper, addr, current_cycle);
    }
    
    if (addr < 0x2000) {
        if (ppu->cart) {
            cartridge_ppu_write(ppu->cart, addr, val);
        }
    } else if (addr < 0x3F00) {
        ppu->vram[ppu_mirror_vram_addr(ppu, addr)] = val;
    } else {
        addr &= 0x1F;
        if (addr == 0x10 || addr == 0x14 || addr == 0x18 || addr == 0x1C) {
            addr -= 0x10;
        }
        ppu->palette[addr] = val;
    }
}

uint8_t ppu_read_register(PPU* ppu, uint16_t addr) {
    uint8_t result = 0;
    
    switch (addr & 0x07) {
        case 0:
            break;
        case 1:
            break;
        case 2:
            result = (ppu->status & 0xE0) | (ppu->data_buffer & 0x1F);
            ppu->status &= ~PPU_STATUS_VBLANK;
            ppu->nmi_occurred = false;
            ppu->w = false;
            break;
        case 3:
            break;
        case 4:
            result = ppu->oam[ppu->oam_addr];
            break;
        case 5:
            break;
        case 6:
            break;
        case 7: {
            if ((ppu->v & 0x3FFF) < 0x3F00) {
                result = ppu->data_buffer;
                ppu->data_buffer = ppu_read(ppu, ppu->v);
            } else {
                result = ppu_read(ppu, ppu->v);
                ppu->data_buffer = ppu_read(ppu, ppu->v - 0x1000);
            }
            ppu->v += (ppu->ctrl & PPU_CTRL_INCREMENT) ? 32 : 1;
            ppu->v &= 0x7FFF;
            break;
        }
    }
    
    return result;
}

void ppu_write_register(PPU* ppu, uint16_t addr, uint8_t val) {
    switch (addr & 0x07) {
        case 0: {
            uint8_t old_nmi_output = ppu->nmi_output;
            ppu->ctrl = val;
            ppu->nmi_output = (val & PPU_CTRL_NMI_ENABLE) != 0;
            ppu->t = (ppu->t & 0xF3FF) | ((val & 0x03) << 10);
            if (!old_nmi_output && ppu->nmi_output && (ppu->status & PPU_STATUS_VBLANK)) {
                ppu->nmi_pending = true;
            }
            break;
        }
        case 1:
            ppu->mask = val;
            break;
        case 2:
            break;
        case 3:
            ppu->oam_addr = val;
            break;
        case 4:
            ppu->oam[ppu->oam_addr++] = val;
            break;
        case 5:
            if (!ppu->w) {
                ppu->t = (ppu->t & 0xFFE0) | (val >> 3);
                ppu->fine_x = val & 0x07;
                ppu->w = true;
            } else {
                ppu->t = (ppu->t & 0x8C1F) | ((val & 0x07) << 12) | ((val & 0xF8) << 2);
                ppu->w = false;
            }
            break;
        case 6:
            if (!ppu->w) {
                ppu->t = (ppu->t & 0x00FF) | ((val & 0x3F) << 8);
                ppu->w = true;
            } else {
                ppu->t = (ppu->t & 0xFF00) | val;
                ppu->v = ppu->t;
                ppu->w = false;
            }
            break;
        case 7:
            ppu_write(ppu, ppu->v, val);
            ppu->v += (ppu->ctrl & PPU_CTRL_INCREMENT) ? 32 : 1;
            ppu->v &= 0x7FFF;
            break;
    }
}

void ppu_oam_dma(PPU* ppu, uint8_t* page) {
    memcpy(ppu->oam, page, 256);
}

static bool ppu_rendering_enabled(PPU* ppu) {
    return (ppu->mask & (PPU_MASK_BG_ENABLE | PPU_MASK_SPRITE_ENABLE)) != 0;
}

static void ppu_increment_x(PPU* ppu) {
    if ((ppu->v & 0x001F) == 31) {
        ppu->v &= ~0x001F;
        ppu->v ^= 0x0400;
    } else {
        ppu->v++;
    }
}

static void ppu_increment_y(PPU* ppu) {
    if ((ppu->v & 0x7000) != 0x7000) {
        ppu->v += 0x1000;
    } else {
        ppu->v &= ~0x7000;
        int y = (ppu->v & 0x03E0) >> 5;
        if (y == 29) {
            y = 0;
            ppu->v ^= 0x0800;
        } else if (y == 31) {
            y = 0;
        } else {
            y++;
        }
        ppu->v = (ppu->v & ~0x03E0) | (y << 5);
    }
}

static void ppu_copy_x(PPU* ppu) {
    ppu->v = (ppu->v & 0xFBE0) | (ppu->t & 0x041F);
}

static void ppu_copy_y(PPU* ppu) {
    ppu->v = (ppu->v & 0x841F) | (ppu->t & 0x7BE0);
}

static void ppu_load_shifters(PPU* ppu) {
    ppu->bg_shift_lo = (ppu->bg_shift_lo & 0xFF00) | ppu->bg_lo;
    ppu->bg_shift_hi = (ppu->bg_shift_hi & 0xFF00) | ppu->bg_hi;
    ppu->at_latch_lo = (ppu->at_byte & 0x01) ? 0xFF : 0x00;
    ppu->at_latch_hi = (ppu->at_byte & 0x02) ? 0xFF : 0x00;
}

static void ppu_shift_shifters(PPU* ppu) {
    if (ppu->mask & PPU_MASK_BG_ENABLE) {
        ppu->bg_shift_lo <<= 1;
        ppu->bg_shift_hi <<= 1;
        ppu->at_shift_lo = (ppu->at_shift_lo << 1) | (ppu->at_latch_lo & 1);
        ppu->at_shift_hi = (ppu->at_shift_hi << 1) | (ppu->at_latch_hi & 1);
    }
}

static void ppu_fetch_nt_byte(PPU* ppu) {
    ppu->nt_byte = ppu_read(ppu, 0x2000 | (ppu->v & 0x0FFF));
}

static void ppu_fetch_at_byte(PPU* ppu) {
    uint16_t addr = 0x23C0 | (ppu->v & 0x0C00) | ((ppu->v >> 4) & 0x38) | ((ppu->v >> 2) & 0x07);
    uint8_t shift = ((ppu->v >> 4) & 4) | (ppu->v & 2);
    ppu->at_byte = (ppu_read(ppu, addr) >> shift) & 0x03;
}

static void ppu_fetch_bg_lo(PPU* ppu) {
    /* 
     * Force dynamic address calculation for every fetch to support mid-scanline 
     * bank switching (e.g., MMC3 IRQ effects in SMB3).
     * Do NOT cache the pattern table base address.
     */
    uint16_t bg_pattern_base = (ppu->ctrl & PPU_CTRL_BG_PATTERN) ? 0x1000 : 0x0000;
    uint16_t tile_offset = (ppu->nt_byte << 4) + ((ppu->v >> 12) & 0x07);
    uint16_t pattern_addr = bg_pattern_base + tile_offset;
    
    ppu->bg_lo = ppu_read(ppu, pattern_addr);
}



static void ppu_fetch_bg_hi(PPU* ppu) {
    uint16_t bg_pattern_base = (ppu->ctrl & PPU_CTRL_BG_PATTERN) ? 0x1000 : 0x0000;
    uint16_t tile_offset = (ppu->nt_byte << 4) + ((ppu->v >> 12) & 0x07);
    uint16_t pattern_addr = bg_pattern_base + tile_offset + 8;
    
    ppu->bg_hi = ppu_read(ppu, pattern_addr);
}

static void ppu_evaluate_sprites(PPU* ppu) {
    int sprite_height = (ppu->ctrl & PPU_CTRL_SPRITE_SIZE) ? 16 : 8;
    ppu->sprite_count = 0;
    memset(ppu->secondary_oam, 0xFF, 32);
    
    for (int i = 0; i < 64; i++) {
        uint8_t y = ppu->oam[i * 4 + 0];
        
        int row = ppu->scanline - y;
        if (row >= 0 && row < sprite_height) {
            if (ppu->sprite_count < 8) {
                ppu->secondary_oam[ppu->sprite_count * 4 + 0] = ppu->oam[i * 4 + 0];
                ppu->secondary_oam[ppu->sprite_count * 4 + 1] = ppu->oam[i * 4 + 1];
                ppu->secondary_oam[ppu->sprite_count * 4 + 2] = ppu->oam[i * 4 + 2];
                ppu->secondary_oam[ppu->sprite_count * 4 + 3] = ppu->oam[i * 4 + 3];
                ppu->sprite_indices[ppu->sprite_count] = i;
                ppu->sprite_count++;
            }
        }
    }
}

static void ppu_fetch_sprite_info(PPU* ppu, int slot_index) {
    int sprite_height = (ppu->ctrl & PPU_CTRL_SPRITE_SIZE) ? 16 : 8;
    
    uint8_t y, tile, attr, x;
    bool valid_sprite = (slot_index < ppu->sprite_count);
    
    if (valid_sprite) {
        y = ppu->secondary_oam[slot_index * 4 + 0];
        tile = ppu->secondary_oam[slot_index * 4 + 1];
        attr = ppu->secondary_oam[slot_index * 4 + 2];
        x = ppu->secondary_oam[slot_index * 4 + 3];
        
        ppu->sprite_attributes[slot_index] = attr;
        ppu->sprite_positions[slot_index] = x;
    } else {
        tile = 0xFF;
        attr = 0xFF;
    }
    
    uint16_t pattern_addr;
    if (valid_sprite) {
        int row = ppu->scanline - y;
        if (attr & 0x80) {
            row = sprite_height - 1 - row;
        }
        
        if (sprite_height == 16) {
            uint16_t table = (tile & 0x01) ? 0x1000 : 0x0000;
            tile &= 0xFE;
            if (row >= 8) {
                tile++;
                row -= 8;
            }
            pattern_addr = table + (tile << 4) + row;
        } else {
            uint16_t table = (ppu->ctrl & PPU_CTRL_SPRITE_PATTERN) ? 0x1000 : 0x0000;
            pattern_addr = table + (tile << 4) + row;
        }
    } else {
        uint16_t table = (ppu->ctrl & PPU_CTRL_SPRITE_PATTERN) ? 0x1000 : 0x0000;
        if (sprite_height == 16) {
            table = 0x1000;
        }
        pattern_addr = table + (0xFF << 4);
    }
    
    uint8_t lo = ppu_read(ppu, pattern_addr);
    uint8_t hi = ppu_read(ppu, pattern_addr + 8);
    
    if (valid_sprite) {
        if (attr & 0x40) {
            lo = ((lo & 0x55) << 1) | ((lo & 0xAA) >> 1);
            lo = ((lo & 0x33) << 2) | ((lo & 0xCC) >> 2);
            lo = ((lo & 0x0F) << 4) | ((lo & 0xF0) >> 4);
            hi = ((hi & 0x55) << 1) | ((hi & 0xAA) >> 1);
            hi = ((hi & 0x33) << 2) | ((hi & 0xCC) >> 2);
            hi = ((hi & 0x0F) << 4) | ((hi & 0xF0) >> 4);
        }
        
        ppu->sprite_patterns_lo[slot_index] = lo;
        ppu->sprite_patterns_hi[slot_index] = hi;
    }
}

static void ppu_render_pixel(PPU* ppu) {
    int x = ppu->dot - 1;
    int y = ppu->scanline;
    
    if (x < 0 || x >= 256 || y < 0 || y >= 240) return;
    
    uint8_t bg_pixel = 0;
    uint8_t bg_palette = 0;
    
    if (ppu->mask & PPU_MASK_BG_ENABLE) {
        if ((ppu->mask & PPU_MASK_BG_LEFT) || x >= 8) {
            uint16_t mux = 0x8000 >> ppu->fine_x;
            uint8_t p0 = (ppu->bg_shift_lo & mux) ? 1 : 0;
            uint8_t p1 = (ppu->bg_shift_hi & mux) ? 1 : 0;
            bg_pixel = (p1 << 1) | p0;
            
            uint8_t a0 = (ppu->at_shift_lo & (0x80 >> ppu->fine_x)) ? 1 : 0;
            uint8_t a1 = (ppu->at_shift_hi & (0x80 >> ppu->fine_x)) ? 1 : 0;
            bg_palette = (a1 << 1) | a0;
        }
    }
    
    uint8_t sprite_pixel = 0;
    uint8_t sprite_palette = 0;
    uint8_t sprite_priority = 0;
    bool sprite_zero = false;
    
    if (ppu->mask & PPU_MASK_SPRITE_ENABLE) {
        if ((ppu->mask & PPU_MASK_SPRITE_LEFT) || x >= 8) {
            for (int i = 0; i < ppu->sprite_count; i++) {
                int offset = x - ppu->sprite_positions[i];
                if (offset >= 0 && offset < 8) {
                    uint8_t p0 = (ppu->sprite_patterns_lo[i] >> (7 - offset)) & 1;
                    uint8_t p1 = (ppu->sprite_patterns_hi[i] >> (7 - offset)) & 1;
                    uint8_t pixel = (p1 << 1) | p0;
                    
                    if (pixel != 0) {
                        if (ppu->sprite_indices[i] == 0) {
                            sprite_zero = true;
                        }
                        sprite_pixel = pixel;
                        sprite_palette = (ppu->sprite_attributes[i] & 0x03) + 4;
                        sprite_priority = (ppu->sprite_attributes[i] >> 5) & 1;
                        break;
                    }
                }
            }
        }
    }
    
    uint8_t pixel = 0;
    uint8_t palette = 0;
    
    if (bg_pixel == 0 && sprite_pixel == 0) {
        pixel = 0;
        palette = 0;
    } else if (bg_pixel == 0 && sprite_pixel != 0) {
        pixel = sprite_pixel;
        palette = sprite_palette;
    } else if (bg_pixel != 0 && sprite_pixel == 0) {
        pixel = bg_pixel;
        palette = bg_palette;
    } else {
        if (sprite_zero && x < 255) {
            if ((ppu->mask & PPU_MASK_BG_ENABLE) && (ppu->mask & PPU_MASK_SPRITE_ENABLE)) {
                if (!((ppu->mask & PPU_MASK_BG_LEFT) == 0 && x < 8)) {
                    ppu->status |= PPU_STATUS_SPRITE0_HIT;
                }
            }
        }
        
        if (sprite_priority == 0) {
            pixel = sprite_pixel;
            palette = sprite_palette;
        } else {
            pixel = bg_pixel;
            palette = bg_palette;
        }
    }
    
    uint8_t color_index = ppu_read(ppu, 0x3F00 + (palette << 2) + pixel) & 0x3F;
    ppu->framebuffer[y * 256 + x] = nes_palette[color_index] | 0xFF000000;
}

void ppu_tick(PPU* ppu) {
    bool rendering = ppu_rendering_enabled(ppu);
    bool pre_line = (ppu->scanline == 261);
    bool visible_line = (ppu->scanline < 240);
    bool render_line = pre_line || visible_line;
    bool prefetch_cycle = (ppu->dot >= 321 && ppu->dot <= 336);
    bool visible_cycle = (ppu->dot >= 1 && ppu->dot <= 256);
    bool fetch_cycle = prefetch_cycle || visible_cycle;
    
    if (rendering) {
        if (visible_line && visible_cycle) {
            ppu_render_pixel(ppu);
        }

        if (render_line && fetch_cycle) {
            ppu_shift_shifters(ppu);
            
            switch ((ppu->dot - 1) % 8) {
                case 0:
                    ppu_load_shifters(ppu);
                    ppu_fetch_nt_byte(ppu);
                    break;
                case 2:
                    ppu_fetch_at_byte(ppu);
                    break;
                case 4:
                    ppu_fetch_bg_lo(ppu);
                    break;
                case 6:
                    ppu_fetch_bg_hi(ppu);
                    break;
                case 7:
                    ppu_increment_x(ppu);
                    break;
            }
        }
        
        if (render_line) {
            if (ppu->dot == 256) {
                ppu_increment_y(ppu);
            }
            if (ppu->dot == 257) {
                ppu_load_shifters(ppu);
                ppu_copy_x(ppu);
            }
            if (ppu->dot == 337 || ppu->dot == 339) {
                ppu_fetch_nt_byte(ppu);
            }
        }
        
        if (pre_line && ppu->dot >= 280 && ppu->dot <= 304) {
            ppu_copy_y(ppu);
        }
        
        if (visible_line) {
            if (ppu->dot == 257) {
                ppu_evaluate_sprites(ppu);
            }
            
            /*
             * Distribute sprite fetches across dots 257-320 (64 dots).
             * 8 sprites * 8 dots per sprite.
             * We initiate the fetch at the start of each 8-dot window (257, 265, ...).
             */
            if (ppu->dot >= 257 && ppu->dot <= 320) {
                if ((ppu->dot - 257) % 8 == 0) {
                    int slot = (ppu->dot - 257) / 8;
                    ppu_fetch_sprite_info(ppu, slot);
                }
            }
        }
    }
    
    if (ppu->scanline == 241 && ppu->dot == 1) {
        ppu->status |= PPU_STATUS_VBLANK;
        ppu->nmi_occurred = true;
        if (ppu->nmi_output) {
            ppu->nmi_pending = true;
        }
    }
    
    if (pre_line && ppu->dot == 1) {
        ppu->status &= ~(PPU_STATUS_VBLANK | PPU_STATUS_SPRITE0_HIT | PPU_STATUS_OVERFLOW);
        ppu->nmi_occurred = false;
    }
    
    if (pre_line && ppu->dot == 340 && rendering && ppu->odd_frame) {
        ppu->dot++;
    }
    
    ppu->dot++;
    if (ppu->dot > 340) {
        ppu->dot = 0;
        
        ppu->scanline++;
        if (ppu->scanline > 261) {
            ppu->scanline = 0;
            ppu->frame++;
            ppu->frame_ready = true;
            ppu->odd_frame = !ppu->odd_frame;
        }
    }
}
