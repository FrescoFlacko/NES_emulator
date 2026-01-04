#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "ppu/ppu.h"
#include "cartridge/cartridge.h"
#include "test_helpers.h"

static void test_ppu_init(void) {
    PPU ppu;
    memset(&ppu, 0xFF, sizeof(ppu));
    ppu_init(&ppu);

    ASSERT_EQ_INT(0, ppu.scanline);
    ASSERT_EQ_INT(0, ppu.dot);
    ASSERT_EQ_U8(0, ppu.ctrl);
    ASSERT_EQ_U8(0, ppu.mask);
    ASSERT_EQ_U8(0, ppu.status);
    ASSERT_EQ_U16(0, ppu.v);
    ASSERT_EQ_U16(0, ppu.t);
    ASSERT_FALSE(ppu.w);
    ASSERT_FALSE(ppu.nmi_pending);
    ASSERT_FALSE(ppu.frame_ready);

    printf("test_ppu_init: PASS\n");
}

static void test_ppu_reset(void) {
    PPU ppu;
    ppu_init(&ppu);
    
    ppu.scanline = 100;
    ppu.dot = 200;
    ppu.ctrl = 0xFF;
    ppu.mask = 0xFF;
    ppu.status = 0xFF;
    ppu.w = true;
    ppu.nmi_pending = true;

    ppu_reset(&ppu);

    ASSERT_EQ_INT(0, ppu.scanline);
    ASSERT_EQ_INT(0, ppu.dot);
    ASSERT_EQ_U8(0, ppu.ctrl);
    ASSERT_EQ_U8(0, ppu.mask);
    ASSERT_EQ_U8(0, ppu.status);
    ASSERT_FALSE(ppu.w);
    ASSERT_FALSE(ppu.nmi_pending);

    printf("test_ppu_reset: PASS\n");
}

static void test_palette_mirroring(void) {
    PPU ppu;
    ppu_init(&ppu);

    ppu_write_register(&ppu, 0x2006, 0x3F);
    ppu_write_register(&ppu, 0x2006, 0x00);
    ppu_write_register(&ppu, 0x2007, 0x11);

    ppu.v = 0x3F10;
    uint8_t val = ppu_read_register(&ppu, 0x2007);
    ASSERT_EQ_U8(0x11, val);

    ppu_write_register(&ppu, 0x2006, 0x3F);
    ppu_write_register(&ppu, 0x2006, 0x14);
    ppu_write_register(&ppu, 0x2007, 0x22);

    ppu.v = 0x3F04;
    val = ppu_read_register(&ppu, 0x2007);
    ASSERT_EQ_U8(0x22, val);

    printf("test_palette_mirroring: PASS\n");
}

static void test_status_read_clears_vblank(void) {
    PPU ppu;
    ppu_init(&ppu);

    ppu.status = PPU_STATUS_VBLANK | PPU_STATUS_SPRITE0_HIT;
    ppu.w = true;
    ppu.data_buffer = 0x1F;

    uint8_t val = ppu_read_register(&ppu, 0x2002);
    
    ASSERT_TRUE(val & PPU_STATUS_VBLANK);
    ASSERT_TRUE(val & PPU_STATUS_SPRITE0_HIT);
    ASSERT_EQ_U8(0x1F, val & 0x1F);

    ASSERT_FALSE(ppu.status & PPU_STATUS_VBLANK);
    ASSERT_FALSE(ppu.w);

    printf("test_status_read_clears_vblank: PASS\n");
}

static void test_ctrl_write_updates_t(void) {
    PPU ppu;
    ppu_init(&ppu);

    ppu.t = 0;
    ppu_write_register(&ppu, 0x2000, 0x03);

    ASSERT_EQ_U8(0x03, ppu.ctrl);
    ASSERT_EQ_U16(0x0C00, ppu.t & 0x0C00);

    printf("test_ctrl_write_updates_t: PASS\n");
}

static void test_ctrl_nmi_enable_during_vblank(void) {
    PPU ppu;
    ppu_init(&ppu);

    ppu.status = PPU_STATUS_VBLANK;
    ppu.nmi_output = false;
    ppu.nmi_pending = false;

    ppu_write_register(&ppu, 0x2000, PPU_CTRL_NMI_ENABLE);

    ASSERT_TRUE(ppu.nmi_pending);

    printf("test_ctrl_nmi_enable_during_vblank: PASS\n");
}

static void test_scroll_first_write(void) {
    PPU ppu;
    ppu_init(&ppu);

    ppu.t = 0;
    ppu.fine_x = 0;
    ppu.w = false;

    ppu_write_register(&ppu, 0x2005, 0b11111111);

    ASSERT_EQ_U16(0b11111, ppu.t & 0x001F);
    ASSERT_EQ_U8(0b111, ppu.fine_x);
    ASSERT_TRUE(ppu.w);

    printf("test_scroll_first_write: PASS\n");
}

static void test_scroll_second_write(void) {
    PPU ppu;
    ppu_init(&ppu);

    ppu.t = 0;
    ppu.w = true;

    ppu_write_register(&ppu, 0x2005, 0b11111111);

    ASSERT_EQ_U16(0b111, (ppu.t >> 12) & 0x07);
    ASSERT_EQ_U16(0b11111, (ppu.t >> 5) & 0x1F);
    ASSERT_FALSE(ppu.w);

    printf("test_scroll_second_write: PASS\n");
}

static void test_addr_first_write(void) {
    PPU ppu;
    ppu_init(&ppu);

    ppu.t = 0xFFFF;
    ppu.w = false;

    ppu_write_register(&ppu, 0x2006, 0x21);

    ASSERT_EQ_U16(0x2100 | (ppu.t & 0x00FF), ppu.t);
    ASSERT_TRUE(ppu.w);

    printf("test_addr_first_write: PASS\n");
}

static void test_addr_second_write(void) {
    PPU ppu;
    ppu_init(&ppu);

    ppu.t = 0x2100;
    ppu.v = 0;
    ppu.w = true;

    ppu_write_register(&ppu, 0x2006, 0x34);

    ASSERT_EQ_U16(0x2134, ppu.t);
    ASSERT_EQ_U16(0x2134, ppu.v);
    ASSERT_FALSE(ppu.w);

    printf("test_addr_second_write: PASS\n");
}

static void test_data_read_increment(void) {
    PPU ppu;
    ppu_init(&ppu);

    ppu.v = 0x2000;
    ppu.ctrl = 0;
    
    ppu.vram[0] = 0xAA;
    ppu.vram[1] = 0xBB;

    (void)ppu_read_register(&ppu, 0x2007);
    ASSERT_EQ_U16(0x2001, ppu.v);

    ppu.ctrl = PPU_CTRL_INCREMENT;
    ppu.v = 0x2000;
    (void)ppu_read_register(&ppu, 0x2007);
    ASSERT_EQ_U16(0x2020, ppu.v);

    printf("test_data_read_increment: PASS\n");
}

static void test_data_read_buffered(void) {
    PPU ppu;
    ppu_init(&ppu);

    ppu.vram[0] = 0xAA;
    ppu.vram[1] = 0xBB;
    ppu.v = 0x2000;
    ppu.data_buffer = 0x00;

    uint8_t val1 = ppu_read_register(&ppu, 0x2007);
    ASSERT_EQ_U8(0x00, val1);
    ASSERT_EQ_U8(0xAA, ppu.data_buffer);

    uint8_t val2 = ppu_read_register(&ppu, 0x2007);
    ASSERT_EQ_U8(0xAA, val2);

    printf("test_data_read_buffered: PASS\n");
}

static void test_data_read_palette_immediate(void) {
    PPU ppu;
    ppu_init(&ppu);

    ppu.palette[0] = 0x0F;
    ppu.v = 0x3F00;

    uint8_t val = ppu_read_register(&ppu, 0x2007);
    ASSERT_EQ_U8(0x0F, val);

    printf("test_data_read_palette_immediate: PASS\n");
}

static void test_vblank_timing(void) {
    PPU ppu;
    ppu_init(&ppu);

    ppu.scanline = 241;
    ppu.dot = 1;
    ppu.nmi_output = true;
    ppu.status = 0;

    ppu_tick(&ppu);
    
    ASSERT_TRUE(ppu.status & PPU_STATUS_VBLANK);
    ASSERT_TRUE(ppu.nmi_pending);

    printf("test_vblank_timing: PASS\n");
}

static void test_prerender_clears_flags(void) {
    PPU ppu;
    ppu_init(&ppu);

    ppu.scanline = 261;
    ppu.dot = 1;
    ppu.status = PPU_STATUS_VBLANK | PPU_STATUS_SPRITE0_HIT | PPU_STATUS_OVERFLOW;

    ppu_tick(&ppu);

    ASSERT_FALSE(ppu.status & PPU_STATUS_VBLANK);
    ASSERT_FALSE(ppu.status & PPU_STATUS_SPRITE0_HIT);
    ASSERT_FALSE(ppu.status & PPU_STATUS_OVERFLOW);

    printf("test_prerender_clears_flags: PASS\n");
}

static void test_oam_dma(void) {
    PPU ppu;
    ppu_init(&ppu);

    uint8_t page[256];
    for (int i = 0; i < 256; i++) {
        page[i] = (uint8_t)i;
    }

    ppu_oam_dma(&ppu, page);

    for (int i = 0; i < 256; i++) {
        ASSERT_EQ_U8((uint8_t)i, ppu.oam[i]);
    }

    printf("test_oam_dma: PASS\n");
}

static void test_oam_write(void) {
    PPU ppu;
    ppu_init(&ppu);

    ppu.oam_addr = 0;
    ppu_write_register(&ppu, 0x2003, 0x10);
    ASSERT_EQ_U8(0x10, ppu.oam_addr);

    ppu_write_register(&ppu, 0x2004, 0xAB);
    ASSERT_EQ_U8(0xAB, ppu.oam[0x10]);
    ASSERT_EQ_U8(0x11, ppu.oam_addr);

    printf("test_oam_write: PASS\n");
}

static void test_frame_completion(void) {
    PPU ppu;
    ppu_init(&ppu);

    ppu.scanline = 261;
    ppu.dot = 340;
    ppu.frame = 0;
    ppu.frame_ready = false;

    ppu_tick(&ppu);

    ASSERT_EQ_INT(0, ppu.scanline);
    ASSERT_EQ_INT(0, ppu.dot);
    ASSERT_EQ_INT(1, (int)ppu.frame);
    ASSERT_TRUE(ppu.frame_ready);

    printf("test_frame_completion: PASS\n");
}

int main(void) {
    test_ppu_init();
    test_ppu_reset();
    test_palette_mirroring();
    test_status_read_clears_vblank();
    test_ctrl_write_updates_t();
    test_ctrl_nmi_enable_during_vblank();
    test_scroll_first_write();
    test_scroll_second_write();
    test_addr_first_write();
    test_addr_second_write();
    test_data_read_increment();
    test_data_read_buffered();
    test_data_read_palette_immediate();
    test_vblank_timing();
    test_prerender_clears_flags();
    test_oam_dma();
    test_oam_write();
    test_frame_completion();

    printf("\nAll PPU tests passed.\n");
    return 0;
}
