#include <stdio.h>
#include "cpu/cpu.h"
#include "bus/bus.h"
#include "ppu/ppu.h"
#include "cartridge/cartridge.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    Bus bus;
    CPU cpu;
    PPU ppu;
    Cartridge cart;

    bus_init(&bus);
    ppu_init(&ppu);
    bus.ppu = &ppu;

    if (!cartridge_load(&cart, "roms/test/nestest.nes")) {
        fprintf(stderr, "Failed to load nestest.nes\n");
        return 1;
    }
    bus.cart = &cart;

    cpu_init(&cpu, &bus);
    cpu.PC = 0xC000;
    cpu.P = 0x24;
    cpu.S = 0xFD;
    cpu.cycles = 7;

    ppu.scanline = 0;
    ppu.dot = 21;

    char trace_buf[256];
    int line_count = 0;
    const int MAX_LINES = 8991;  // nestest.log has exactly 8991 lines

    while (line_count < MAX_LINES) {
        cpu_trace(&cpu, trace_buf, sizeof(trace_buf));
        printf("%s\n", trace_buf);

        int cycles = cpu_step(&cpu);
        bus_tick(&bus, cycles);
        line_count++;
    }

    cartridge_free(&cart);
    return 0;
}
