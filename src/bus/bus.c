/*
 * Module: src/bus/bus.c
 * Responsibility: Memory bus implementation - address decoding and peripheral routing.
 * Key invariants:
 *  - RAM mirroring: addr & 0x07FF for $0000-$1FFF
 *  - PPU register mirroring: addr & 0x07 for $2000-$3FFF
 *  - Controller shift: strobe=1 keeps bit0; strobe 1->0 latches state
 */
#include "bus.h"
#include "../cartridge/cartridge.h"
#include "../ppu/ppu.h"
#include "../apu/apu.h"
#include "../cpu/cpu.h"
#include "../mapper/mapper.h"
#include <string.h>
#include <stdio.h>

void bus_init(Bus* bus) {
    memset(bus->ram, 0, sizeof(bus->ram));
    memset(bus->controller, 0, sizeof(bus->controller));
    memset(bus->controller_state, 0, sizeof(bus->controller_state));
    bus->controller_strobe = 0;
    bus->open_bus = 0xFF;
    bus->cpu = NULL;
    bus->ppu = NULL;
    bus->apu = NULL;
    bus->cart = NULL;
    bus->dma_pending = false;
    bus->dma_page = 0;
    bus->dma_cycles = 0;
}

uint8_t bus_read(Bus* bus, uint16_t addr) {
    if (addr < 0x2000) {
        return bus->ram[addr & 0x07FF];
    } else if (addr < 0x4000) {
        if (bus->ppu) {
            return ppu_read_register(bus->ppu, addr);
        }
        return bus->open_bus;
    } else if (addr < 0x4020) {
        if (addr == 0x4016) {
            uint8_t bit;
            if (bus->controller_strobe) {
                bit = bus->controller[0] & 1;
            } else {
                bit = bus->controller_state[0] & 1;
                bus->controller_state[0] = (bus->controller_state[0] >> 1) | 0x80;
            }

            uint8_t result = (bit & 1) | 0x40;
            return result;
        } else if (addr == 0x4017) {
            uint8_t bit;
            if (bus->controller_strobe) {
                bit = bus->controller[1] & 1;
            } else {
                bit = bus->controller_state[1] & 1;
                bus->controller_state[1] = (bus->controller_state[1] >> 1) | 0x80;
            }
            return (bit & 1) | 0x40;
        } else if (addr == 0x4015) {
            if (bus->apu) {
                return apu_read(bus->apu, addr);
            }
        }
        return bus->open_bus;
    } else {
        if (bus->cart) {
            return cartridge_cpu_read(bus->cart, addr);
        }
        return bus->open_bus;
    }
}

void bus_write(Bus* bus, uint16_t addr, uint8_t val) {
    if (addr < 0x2000) {
        bus->ram[addr & 0x07FF] = val;
    } else if (addr < 0x4000) {
        if (bus->ppu) {
            ppu_write_register(bus->ppu, addr, val);
        }
    } else if (addr < 0x4020) {
        if (addr == 0x4014) {
            bus->dma_page = val;
            bus->dma_pending = true;
        } else if (addr == 0x4016) {
            if (bus->controller_strobe && !(val & 1)) {
                bus->controller_state[0] = bus->controller[0];
                bus->controller_state[1] = bus->controller[1];
            }
            bus->controller_strobe = val & 1;
        } else if (addr >= 0x4000 && addr <= 0x4017 && addr != 0x4014 && addr != 0x4016) {
            if (bus->apu) {
                apu_write(bus->apu, addr, val);
            }
        }
    } else {
        if (bus->cart) {
            cartridge_cpu_write(bus->cart, addr, val);
        }
    }
}

void bus_tick(Bus* bus, int cpu_cycles) {
    if (bus->ppu) {
        for (int i = 0; i < cpu_cycles * 3; i++) {
            ppu_tick(bus->ppu);
        }
    }
    if (bus->apu) {
        for (int i = 0; i < cpu_cycles; i++) {
            apu_tick(bus->apu);
        }
    }
    
    if (bus->cart && bus->cart->mapper) {
        Mapper* mapper = bus->cart->mapper;
        if (mapper->irq_pending && mapper->irq_pending(mapper)) {
            if (bus->cpu) {
                cpu_irq(bus->cpu);
            }
            if (mapper->irq_clear) {
                mapper->irq_clear(mapper);
            }
        }
    }
}
