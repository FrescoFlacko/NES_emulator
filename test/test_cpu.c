#include <stdio.h>
#include <assert.h>
#include <string.h>
#ifndef CPU_TEST_HELPERS
#define CPU_TEST_HELPERS
#endif
#include "cpu/cpu.h"
#include "bus/bus.h"
#include "cartridge/cartridge.h"

static void test_cpu_init(void) {
    Bus bus;
    CPU cpu;

    bus_init(&bus);
    cpu_init(&cpu, &bus);

    assert(cpu.S == 0xFD);
    assert((cpu.P & FLAG_U) != 0);
    assert((cpu.P & FLAG_I) != 0);
    assert(cpu.A == 0);
    assert(cpu.X == 0);
    assert(cpu.Y == 0);
    printf("test_cpu_init: PASS\n");
}

static void test_ram_mirroring(void) {
    Bus bus;
    bus_init(&bus);

    bus_write(&bus, 0x0000, 0x42);
    assert(bus_read(&bus, 0x0000) == 0x42);
    assert(bus_read(&bus, 0x0800) == 0x42);
    assert(bus_read(&bus, 0x1000) == 0x42);
    assert(bus_read(&bus, 0x1800) == 0x42);

    bus_write(&bus, 0x07FF, 0xAB);
    assert(bus_read(&bus, 0x07FF) == 0xAB);
    assert(bus_read(&bus, 0x0FFF) == 0xAB);
    assert(bus_read(&bus, 0x17FF) == 0xAB);
    assert(bus_read(&bus, 0x1FFF) == 0xAB);

    bus_write(&bus, 0x1234, 0xCD);
    assert(bus_read(&bus, 0x0234) == 0xCD);

    printf("test_ram_mirroring: PASS\n");
}

static void test_cartridge_load(void) {
    Cartridge cart;
    memset(&cart, 0, sizeof(cart));

    bool loaded = cartridge_load(&cart, "roms/test/nestest.nes");
    if (!loaded) {
        printf("test_cartridge_load: SKIP (nestest.nes not found, run 'make fetch-nestest')\n");
        return;
    }

    assert(cart.prg_rom_size == 16384);
    assert(cart.mapper_id == 0);
    assert(cart.mapper != NULL);

    uint8_t first_byte = cartridge_cpu_read(&cart, 0xC000);
    assert(first_byte == 0x4C);

    cartridge_free(&cart);
    printf("test_cartridge_load: PASS\n");
}

static void test_stack_operations(void) {
    Bus bus;
    CPU cpu;
    bus_init(&bus);
    cpu_init(&cpu, &bus);

    assert(cpu.S == 0xFD);

    cpu_push8(&cpu, 0x42);
    assert(cpu.S == 0xFC);
    assert(bus_read(&bus, 0x01FD) == 0x42);

    cpu_push8(&cpu, 0xAB);
    assert(cpu.S == 0xFB);
    assert(bus_read(&bus, 0x01FC) == 0xAB);

    uint8_t val = cpu_pop8(&cpu);
    assert(val == 0xAB);
    assert(cpu.S == 0xFC);

    val = cpu_pop8(&cpu);
    assert(val == 0x42);
    assert(cpu.S == 0xFD);

    cpu.S = 0xFD;
    cpu_push16(&cpu, 0xBEEF);
    assert(cpu.S == 0xFB);
    assert(bus_read(&bus, 0x01FD) == 0xBE);
    assert(bus_read(&bus, 0x01FC) == 0xEF);

    uint16_t val16 = cpu_pop16(&cpu);
    assert(val16 == 0xBEEF);
    assert(cpu.S == 0xFD);

    printf("test_stack_operations: PASS\n");
}

static void test_flag_operations(void) {
    Bus bus;
    CPU cpu;
    bus_init(&bus);
    cpu_init(&cpu, &bus);

    cpu.P = 0;
    cpu_set_flag(&cpu, FLAG_C, true);
    assert(cpu.P == FLAG_C);
    assert(cpu_get_flag(&cpu, FLAG_C) == true);

    cpu_set_flag(&cpu, FLAG_Z, true);
    assert(cpu.P == (FLAG_C | FLAG_Z));

    cpu_set_flag(&cpu, FLAG_C, false);
    assert(cpu.P == FLAG_Z);
    assert(cpu_get_flag(&cpu, FLAG_C) == false);

    cpu.P = 0;
    cpu_set_flag(&cpu, FLAG_N, true);
    cpu_set_flag(&cpu, FLAG_V, true);
    assert(cpu.P == (FLAG_N | FLAG_V));

    printf("test_flag_operations: PASS\n");
}

static void test_read16_variants(void) {
    Bus bus;
    bus_init(&bus);

    bus_write(&bus, 0x0010, 0x34);
    bus_write(&bus, 0x0011, 0x12);
    assert(cpu_read16(&bus, 0x0010) == 0x1234);

    bus_write(&bus, 0x00FF, 0xCD);
    bus_write(&bus, 0x0000, 0xAB);
    assert(cpu_read16_zp(&bus, 0xFF) == 0xABCD);

    bus_write(&bus, 0x02FF, 0xEF);
    bus_write(&bus, 0x0200, 0xBE);
    assert(cpu_read16_jmp_bug(&bus, 0x02FF) == 0xBEEF);

    printf("test_read16_variants: PASS\n");
}

int main(void) {
    test_cpu_init();
    test_ram_mirroring();
    test_cartridge_load();
    test_stack_operations();
    test_flag_operations();
    test_read16_variants();

    printf("\nAll tests passed.\n");
    return 0;
}
