/*
 * Module: src/ppu/ppu.h
 * Responsibility: Picture Processing Unit - background/sprite rendering, VRAM access, timing.
 * Key invariants:
 *  - 341 dots per scanline, 262 scanlines per frame (NTSC)
 *  - v/t registers: 15-bit VRAM address with fine scroll (loopy scrolling)
 *  - Rendering: scanlines 0-239 visible, 241 starts VBlank, 261 is pre-render
 *  - nmi_pending set when VBlank starts AND NMI output enabled
 * Notes:
 *  - Odd frames skip dot 0 of pre-render line when rendering enabled
 *  - Sprite 0 hit won't trigger at x=255 or when clipping left 8px
 */
#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <stdbool.h>

typedef struct Cartridge Cartridge;

#define PPU_CTRL_NAMETABLE_X     0x01
#define PPU_CTRL_NAMETABLE_Y     0x02
#define PPU_CTRL_INCREMENT       0x04
#define PPU_CTRL_SPRITE_PATTERN  0x08
#define PPU_CTRL_BG_PATTERN      0x10
#define PPU_CTRL_SPRITE_SIZE     0x20
#define PPU_CTRL_MASTER_SLAVE    0x40
#define PPU_CTRL_NMI_ENABLE      0x80

#define PPU_MASK_GREYSCALE       0x01
#define PPU_MASK_BG_LEFT         0x02
#define PPU_MASK_SPRITE_LEFT     0x04
#define PPU_MASK_BG_ENABLE       0x08
#define PPU_MASK_SPRITE_ENABLE   0x10
#define PPU_MASK_EMPHASIZE_R     0x20
#define PPU_MASK_EMPHASIZE_G     0x40
#define PPU_MASK_EMPHASIZE_B     0x80

#define PPU_STATUS_OVERFLOW      0x20
#define PPU_STATUS_SPRITE0_HIT   0x40
#define PPU_STATUS_VBLANK        0x80

typedef struct PPU {
    int scanline;
    int dot;
    uint64_t frame;

    uint8_t ctrl;
    uint8_t mask;
    uint8_t status;
    uint8_t oam_addr;

    uint8_t vram[2048];
    uint8_t palette[32];
    uint8_t oam[256];
    uint8_t secondary_oam[32];

    uint16_t v;
    uint16_t t;
    uint8_t fine_x;
    bool w;
    uint8_t data_buffer;

    uint8_t nt_byte;
    uint8_t at_byte;
    uint8_t bg_lo;
    uint8_t bg_hi;
    uint16_t bg_shift_lo;
    uint16_t bg_shift_hi;
    uint8_t at_latch_lo;
    uint8_t at_latch_hi;
    uint16_t at_shift_lo;
    uint16_t at_shift_hi;

    uint8_t sprite_count;
    uint8_t sprite_patterns_lo[8];
    uint8_t sprite_patterns_hi[8];
    uint8_t sprite_positions[8];
    uint8_t sprite_attributes[8];
    uint8_t sprite_indices[8];

    uint32_t framebuffer[256 * 240];
    bool frame_ready;

    bool nmi_occurred;
    bool nmi_output;
    bool nmi_pending;

    bool odd_frame;

    Cartridge* cart;
} PPU;

/* Initialize PPU state (zeroes everything). */
void ppu_init(PPU* ppu);

/* Reset PPU to power-on state (clears registers, timing). */
void ppu_reset(PPU* ppu);

/* Advance PPU by one dot. Updates rendering, scroll, NMI. */
void ppu_tick(PPU* ppu);

/* Read from PPU register ($2000-$2007 mirrored). */
uint8_t ppu_read_register(PPU* ppu, uint16_t addr);

/* Write to PPU register ($2000-$2007 mirrored). */
void ppu_write_register(PPU* ppu, uint16_t addr, uint8_t val);

/* Copy 256 bytes to OAM (triggered by $4014 write). */
void ppu_oam_dma(PPU* ppu, uint8_t* page);

#endif
