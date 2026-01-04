#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "bus/bus.h"
#include "ppu/ppu.h"
#include "apu/apu.h"
#include "test_helpers.h"

static void test_ram_mirroring(void) {
    Bus bus;
    bus_init(&bus);

    bus_write(&bus, 0x0000, 0x42);
    ASSERT_EQ_U8(0x42, bus_read(&bus, 0x0000));
    ASSERT_EQ_U8(0x42, bus_read(&bus, 0x0800));
    ASSERT_EQ_U8(0x42, bus_read(&bus, 0x1000));
    ASSERT_EQ_U8(0x42, bus_read(&bus, 0x1800));

    bus_write(&bus, 0x07FF, 0xAB);
    ASSERT_EQ_U8(0xAB, bus_read(&bus, 0x07FF));
    ASSERT_EQ_U8(0xAB, bus_read(&bus, 0x0FFF));
    ASSERT_EQ_U8(0xAB, bus_read(&bus, 0x17FF));
    ASSERT_EQ_U8(0xAB, bus_read(&bus, 0x1FFF));

    bus_write(&bus, 0x1234, 0xCD);
    ASSERT_EQ_U8(0xCD, bus_read(&bus, 0x0234));

    printf("test_ram_mirroring: PASS\n");
}

static void test_ppu_register_routing(void) {
    Bus bus;
    PPU ppu;
    bus_init(&bus);
    ppu_init(&ppu);
    bus.ppu = &ppu;

    bus_write(&bus, 0x2000, 0x88);
    ASSERT_EQ_U8(0x88, ppu.ctrl);

    bus_write(&bus, 0x2001, 0x1E);
    ASSERT_EQ_U8(0x1E, ppu.mask);

    ppu.status = PPU_STATUS_VBLANK | PPU_STATUS_SPRITE0_HIT;
    ppu.data_buffer = 0x12;
    ppu.w = true;

    uint8_t status = bus_read(&bus, 0x2002);
    ASSERT_EQ_U8((PPU_STATUS_VBLANK | PPU_STATUS_SPRITE0_HIT) | 0x12, status);
    ASSERT_FALSE(ppu.w);
    ASSERT_EQ_U8(ppu.status & PPU_STATUS_VBLANK, 0);

    bus_write(&bus, 0x2008, 0x99);
    ASSERT_EQ_U8(0x99, ppu.ctrl);

    printf("test_ppu_register_routing: PASS\n");
}

static void test_controller_strobe_high(void) {
    Bus bus;
    bus_init(&bus);

    bus.controller[0] = 0b10101010;
    bus_write(&bus, 0x4016, 1);

    for (int i = 0; i < 10; i++) {
        uint8_t val = bus_read(&bus, 0x4016);
        ASSERT_EQ_U8(0x40, val);
    }

    bus.controller[0] = 0b10101011;
    for (int i = 0; i < 10; i++) {
        uint8_t val = bus_read(&bus, 0x4016);
        ASSERT_EQ_U8(0x41, val);
    }

    printf("test_controller_strobe_high: PASS\n");
}

static void test_controller_strobe_latch(void) {
    Bus bus;
    bus_init(&bus);

    bus.controller[0] = 0b11001010;
    bus_write(&bus, 0x4016, 1);
    bus_write(&bus, 0x4016, 0);

    ASSERT_EQ_U8(0b11001010, bus.controller_state[0]);

    printf("test_controller_strobe_latch: PASS\n");
}

static void test_controller_shift_behavior(void) {
    Bus bus;
    bus_init(&bus);

    bus.controller[0] = 0b10110100;
    bus_write(&bus, 0x4016, 1);
    bus_write(&bus, 0x4016, 0);

    uint8_t expected_bits[] = {0, 0, 1, 0, 1, 1, 0, 1};
    for (int i = 0; i < 8; i++) {
        uint8_t val = bus_read(&bus, 0x4016);
        ASSERT_EQ_U8(expected_bits[i] | 0x40, val);
    }

    for (int i = 0; i < 4; i++) {
        uint8_t val = bus_read(&bus, 0x4016);
        ASSERT_EQ_U8(0x41, val);
    }

    printf("test_controller_shift_behavior: PASS\n");
}

static void test_controller2(void) {
    Bus bus;
    bus_init(&bus);

    bus.controller[1] = 0b11110000;
    bus_write(&bus, 0x4016, 1);
    bus_write(&bus, 0x4016, 0);

    uint8_t expected_bits[] = {0, 0, 0, 0, 1, 1, 1, 1};
    for (int i = 0; i < 8; i++) {
        uint8_t val = bus_read(&bus, 0x4017);
        ASSERT_EQ_U8(expected_bits[i] | 0x40, val);
    }

    printf("test_controller2: PASS\n");
}

static void test_apu_routing(void) {
    Bus bus;
    APU apu;
    bus_init(&bus);
    apu_init(&apu);
    bus.apu = &apu;

    bus_write(&bus, 0x4015, 0x0F);
    ASSERT_TRUE(apu.pulse1.enabled);
    ASSERT_TRUE(apu.pulse2.enabled);
    ASSERT_TRUE(apu.triangle.enabled);
    ASSERT_TRUE(apu.noise.enabled);

    bus_write(&bus, 0x4015, 0x00);
    ASSERT_FALSE(apu.pulse1.enabled);
    ASSERT_FALSE(apu.pulse2.enabled);

    apu.pulse1.length_counter = 5;
    apu.pulse2.length_counter = 0;
    apu.triangle.length_counter = 3;
    apu.noise.length_counter = 0;
    apu.dmc.bytes_remaining = 10;

    uint8_t status = bus_read(&bus, 0x4015);
    ASSERT_TRUE(status & 0x01);
    ASSERT_FALSE(status & 0x02);
    ASSERT_TRUE(status & 0x04);
    ASSERT_FALSE(status & 0x08);
    ASSERT_TRUE(status & 0x10);

    printf("test_apu_routing: PASS\n");
}

static void test_oam_dma_trigger(void) {
    Bus bus;
    bus_init(&bus);

    ASSERT_FALSE(bus.dma_pending);
    bus_write(&bus, 0x4014, 0x02);
    ASSERT_TRUE(bus.dma_pending);
    ASSERT_EQ_U8(0x02, bus.dma_page);

    printf("test_oam_dma_trigger: PASS\n");
}

static void test_bus_tick_ppu_advancement(void) {
    Bus bus;
    PPU ppu;
    bus_init(&bus);
    ppu_init(&ppu);
    bus.ppu = &ppu;

    ppu.scanline = 0;
    ppu.dot = 0;

    bus_tick(&bus, 1);
    ASSERT_EQ_INT(3, ppu.dot);

    ppu.dot = 0;
    bus_tick(&bus, 10);
    ASSERT_EQ_INT(30, ppu.dot);

    printf("test_bus_tick_ppu_advancement: PASS\n");
}

static void test_bus_tick_apu_advancement(void) {
    Bus bus;
    APU apu;
    bus_init(&bus);
    apu_init(&apu);
    bus.apu = &apu;

    uint64_t initial = apu.frame_count;
    bus_tick(&bus, 1);
    ASSERT_EQ_INT(initial + 1, (int)apu.frame_count);

    initial = apu.frame_count;
    bus_tick(&bus, 10);
    ASSERT_EQ_INT(initial + 10, (int)apu.frame_count);

    printf("test_bus_tick_apu_advancement: PASS\n");
}

static void test_open_bus_behavior(void) {
    Bus bus;
    bus_init(&bus);

    uint8_t val = bus_read(&bus, 0x4018);
    ASSERT_EQ_U8(0xFF, val);

    val = bus_read(&bus, 0x2000);
    ASSERT_EQ_U8(0xFF, val);

    printf("test_open_bus_behavior: PASS\n");
}

int main(void) {
    test_ram_mirroring();
    test_ppu_register_routing();
    test_controller_strobe_high();
    test_controller_strobe_latch();
    test_controller_shift_behavior();
    test_controller2();
    test_apu_routing();
    test_oam_dma_trigger();
    test_bus_tick_ppu_advancement();
    test_bus_tick_apu_advancement();
    test_open_bus_behavior();

    printf("\nAll bus tests passed.\n");
    return 0;
}
