#include "savestate.h"
#include "../cpu/cpu.h"
#include "../ppu/ppu.h"
#include "../apu/apu.h"
#include "../bus/bus.h"
#include "../cartridge/cartridge.h"
#include "../mapper/mapper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAVESTATE_MAGIC "NESSAVE1"
#define SAVESTATE_VERSION 1

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t flags;
} SaveHeader;

static bool write_header(FILE* f) {
    SaveHeader header;
    memcpy(header.magic, SAVESTATE_MAGIC, 8);
    header.version = SAVESTATE_VERSION;
    header.flags = 0;
    return fwrite(&header, sizeof(SaveHeader), 1, f) == 1;
}

static bool read_header(FILE* f) {
    SaveHeader header;
    if (fread(&header, sizeof(SaveHeader), 1, f) != 1) return false;
    if (memcmp(header.magic, SAVESTATE_MAGIC, 8) != 0) return false;
    if (header.version != SAVESTATE_VERSION) return false;
    return true;
}

static bool save_cpu(FILE* f, CPU* cpu) {
    if (fwrite(&cpu->A, sizeof(cpu->A), 1, f) != 1) return false;
    if (fwrite(&cpu->X, sizeof(cpu->X), 1, f) != 1) return false;
    if (fwrite(&cpu->Y, sizeof(cpu->Y), 1, f) != 1) return false;
    if (fwrite(&cpu->P, sizeof(cpu->P), 1, f) != 1) return false;
    if (fwrite(&cpu->S, sizeof(cpu->S), 1, f) != 1) return false;
    if (fwrite(&cpu->PC, sizeof(cpu->PC), 1, f) != 1) return false;
    if (fwrite(&cpu->cycles, sizeof(cpu->cycles), 1, f) != 1) return false;
    if (fwrite(&cpu->nmi_pending, sizeof(cpu->nmi_pending), 1, f) != 1) return false;
    if (fwrite(&cpu->irq_pending, sizeof(cpu->irq_pending), 1, f) != 1) return false;
    return true;
}

static bool load_cpu(FILE* f, CPU* cpu) {
    Bus* saved_bus = cpu->bus;
    
    if (fread(&cpu->A, sizeof(cpu->A), 1, f) != 1) return false;
    if (fread(&cpu->X, sizeof(cpu->X), 1, f) != 1) return false;
    if (fread(&cpu->Y, sizeof(cpu->Y), 1, f) != 1) return false;
    if (fread(&cpu->P, sizeof(cpu->P), 1, f) != 1) return false;
    if (fread(&cpu->S, sizeof(cpu->S), 1, f) != 1) return false;
    if (fread(&cpu->PC, sizeof(cpu->PC), 1, f) != 1) return false;
    if (fread(&cpu->cycles, sizeof(cpu->cycles), 1, f) != 1) return false;
    if (fread(&cpu->nmi_pending, sizeof(cpu->nmi_pending), 1, f) != 1) return false;
    if (fread(&cpu->irq_pending, sizeof(cpu->irq_pending), 1, f) != 1) return false;
    
    cpu->bus = saved_bus;
    return true;
}

static bool save_ppu(FILE* f, PPU* ppu) {
    if (fwrite(&ppu->scanline, sizeof(ppu->scanline), 1, f) != 1) return false;
    if (fwrite(&ppu->dot, sizeof(ppu->dot), 1, f) != 1) return false;
    if (fwrite(&ppu->frame, sizeof(ppu->frame), 1, f) != 1) return false;
    
    if (fwrite(&ppu->ctrl, sizeof(ppu->ctrl), 1, f) != 1) return false;
    if (fwrite(&ppu->mask, sizeof(ppu->mask), 1, f) != 1) return false;
    if (fwrite(&ppu->status, sizeof(ppu->status), 1, f) != 1) return false;
    if (fwrite(&ppu->oam_addr, sizeof(ppu->oam_addr), 1, f) != 1) return false;
    
    if (fwrite(ppu->vram, sizeof(ppu->vram), 1, f) != 1) return false;
    if (fwrite(ppu->palette, sizeof(ppu->palette), 1, f) != 1) return false;
    if (fwrite(ppu->oam, sizeof(ppu->oam), 1, f) != 1) return false;
    if (fwrite(ppu->secondary_oam, sizeof(ppu->secondary_oam), 1, f) != 1) return false;
    
    if (fwrite(&ppu->v, sizeof(ppu->v), 1, f) != 1) return false;
    if (fwrite(&ppu->t, sizeof(ppu->t), 1, f) != 1) return false;
    if (fwrite(&ppu->fine_x, sizeof(ppu->fine_x), 1, f) != 1) return false;
    if (fwrite(&ppu->w, sizeof(ppu->w), 1, f) != 1) return false;
    if (fwrite(&ppu->data_buffer, sizeof(ppu->data_buffer), 1, f) != 1) return false;
    
    if (fwrite(&ppu->nt_byte, sizeof(ppu->nt_byte), 1, f) != 1) return false;
    if (fwrite(&ppu->at_byte, sizeof(ppu->at_byte), 1, f) != 1) return false;
    if (fwrite(&ppu->bg_lo, sizeof(ppu->bg_lo), 1, f) != 1) return false;
    if (fwrite(&ppu->bg_hi, sizeof(ppu->bg_hi), 1, f) != 1) return false;
    if (fwrite(&ppu->bg_shift_lo, sizeof(ppu->bg_shift_lo), 1, f) != 1) return false;
    if (fwrite(&ppu->bg_shift_hi, sizeof(ppu->bg_shift_hi), 1, f) != 1) return false;
    if (fwrite(&ppu->at_latch_lo, sizeof(ppu->at_latch_lo), 1, f) != 1) return false;
    if (fwrite(&ppu->at_latch_hi, sizeof(ppu->at_latch_hi), 1, f) != 1) return false;
    if (fwrite(&ppu->at_shift_lo, sizeof(ppu->at_shift_lo), 1, f) != 1) return false;
    if (fwrite(&ppu->at_shift_hi, sizeof(ppu->at_shift_hi), 1, f) != 1) return false;
    
    if (fwrite(&ppu->sprite_count, sizeof(ppu->sprite_count), 1, f) != 1) return false;
    if (fwrite(ppu->sprite_patterns_lo, sizeof(ppu->sprite_patterns_lo), 1, f) != 1) return false;
    if (fwrite(ppu->sprite_patterns_hi, sizeof(ppu->sprite_patterns_hi), 1, f) != 1) return false;
    if (fwrite(ppu->sprite_positions, sizeof(ppu->sprite_positions), 1, f) != 1) return false;
    if (fwrite(ppu->sprite_attributes, sizeof(ppu->sprite_attributes), 1, f) != 1) return false;
    if (fwrite(ppu->sprite_indices, sizeof(ppu->sprite_indices), 1, f) != 1) return false;
    
    if (fwrite(ppu->framebuffer, sizeof(ppu->framebuffer), 1, f) != 1) return false;
    if (fwrite(&ppu->frame_ready, sizeof(ppu->frame_ready), 1, f) != 1) return false;
    
    if (fwrite(&ppu->nmi_occurred, sizeof(ppu->nmi_occurred), 1, f) != 1) return false;
    if (fwrite(&ppu->nmi_output, sizeof(ppu->nmi_output), 1, f) != 1) return false;
    if (fwrite(&ppu->nmi_pending, sizeof(ppu->nmi_pending), 1, f) != 1) return false;
    if (fwrite(&ppu->odd_frame, sizeof(ppu->odd_frame), 1, f) != 1) return false;
    
    return true;
}

static bool load_ppu(FILE* f, PPU* ppu) {
    Cartridge* saved_cart = ppu->cart;
    
    if (fread(&ppu->scanline, sizeof(ppu->scanline), 1, f) != 1) return false;
    if (fread(&ppu->dot, sizeof(ppu->dot), 1, f) != 1) return false;
    if (fread(&ppu->frame, sizeof(ppu->frame), 1, f) != 1) return false;
    
    if (fread(&ppu->ctrl, sizeof(ppu->ctrl), 1, f) != 1) return false;
    if (fread(&ppu->mask, sizeof(ppu->mask), 1, f) != 1) return false;
    if (fread(&ppu->status, sizeof(ppu->status), 1, f) != 1) return false;
    if (fread(&ppu->oam_addr, sizeof(ppu->oam_addr), 1, f) != 1) return false;
    
    if (fread(ppu->vram, sizeof(ppu->vram), 1, f) != 1) return false;
    if (fread(ppu->palette, sizeof(ppu->palette), 1, f) != 1) return false;
    if (fread(ppu->oam, sizeof(ppu->oam), 1, f) != 1) return false;
    if (fread(ppu->secondary_oam, sizeof(ppu->secondary_oam), 1, f) != 1) return false;
    
    if (fread(&ppu->v, sizeof(ppu->v), 1, f) != 1) return false;
    if (fread(&ppu->t, sizeof(ppu->t), 1, f) != 1) return false;
    if (fread(&ppu->fine_x, sizeof(ppu->fine_x), 1, f) != 1) return false;
    if (fread(&ppu->w, sizeof(ppu->w), 1, f) != 1) return false;
    if (fread(&ppu->data_buffer, sizeof(ppu->data_buffer), 1, f) != 1) return false;
    
    if (fread(&ppu->nt_byte, sizeof(ppu->nt_byte), 1, f) != 1) return false;
    if (fread(&ppu->at_byte, sizeof(ppu->at_byte), 1, f) != 1) return false;
    if (fread(&ppu->bg_lo, sizeof(ppu->bg_lo), 1, f) != 1) return false;
    if (fread(&ppu->bg_hi, sizeof(ppu->bg_hi), 1, f) != 1) return false;
    if (fread(&ppu->bg_shift_lo, sizeof(ppu->bg_shift_lo), 1, f) != 1) return false;
    if (fread(&ppu->bg_shift_hi, sizeof(ppu->bg_shift_hi), 1, f) != 1) return false;
    if (fread(&ppu->at_latch_lo, sizeof(ppu->at_latch_lo), 1, f) != 1) return false;
    if (fread(&ppu->at_latch_hi, sizeof(ppu->at_latch_hi), 1, f) != 1) return false;
    if (fread(&ppu->at_shift_lo, sizeof(ppu->at_shift_lo), 1, f) != 1) return false;
    if (fread(&ppu->at_shift_hi, sizeof(ppu->at_shift_hi), 1, f) != 1) return false;
    
    if (fread(&ppu->sprite_count, sizeof(ppu->sprite_count), 1, f) != 1) return false;
    if (fread(ppu->sprite_patterns_lo, sizeof(ppu->sprite_patterns_lo), 1, f) != 1) return false;
    if (fread(ppu->sprite_patterns_hi, sizeof(ppu->sprite_patterns_hi), 1, f) != 1) return false;
    if (fread(ppu->sprite_positions, sizeof(ppu->sprite_positions), 1, f) != 1) return false;
    if (fread(ppu->sprite_attributes, sizeof(ppu->sprite_attributes), 1, f) != 1) return false;
    if (fread(ppu->sprite_indices, sizeof(ppu->sprite_indices), 1, f) != 1) return false;
    
    if (fread(ppu->framebuffer, sizeof(ppu->framebuffer), 1, f) != 1) return false;
    if (fread(&ppu->frame_ready, sizeof(ppu->frame_ready), 1, f) != 1) return false;
    
    if (fread(&ppu->nmi_occurred, sizeof(ppu->nmi_occurred), 1, f) != 1) return false;
    if (fread(&ppu->nmi_output, sizeof(ppu->nmi_output), 1, f) != 1) return false;
    if (fread(&ppu->nmi_pending, sizeof(ppu->nmi_pending), 1, f) != 1) return false;
    if (fread(&ppu->odd_frame, sizeof(ppu->odd_frame), 1, f) != 1) return false;
    
    ppu->cart = saved_cart;
    return true;
}

static bool save_pulse_channel(FILE* f, PulseChannel* ch) {
    return fwrite(ch, sizeof(PulseChannel), 1, f) == 1;
}

static bool load_pulse_channel(FILE* f, PulseChannel* ch) {
    return fread(ch, sizeof(PulseChannel), 1, f) == 1;
}

static bool save_triangle_channel(FILE* f, TriangleChannel* ch) {
    return fwrite(ch, sizeof(TriangleChannel), 1, f) == 1;
}

static bool load_triangle_channel(FILE* f, TriangleChannel* ch) {
    return fread(ch, sizeof(TriangleChannel), 1, f) == 1;
}

static bool save_noise_channel(FILE* f, NoiseChannel* ch) {
    return fwrite(ch, sizeof(NoiseChannel), 1, f) == 1;
}

static bool load_noise_channel(FILE* f, NoiseChannel* ch) {
    return fread(ch, sizeof(NoiseChannel), 1, f) == 1;
}

static bool save_dmc_channel(FILE* f, DMCChannel* ch) {
    return fwrite(ch, sizeof(DMCChannel), 1, f) == 1;
}

static bool load_dmc_channel(FILE* f, DMCChannel* ch) {
    return fread(ch, sizeof(DMCChannel), 1, f) == 1;
}

static bool save_apu(FILE* f, APU* apu) {
    if (!save_pulse_channel(f, &apu->pulse1)) return false;
    if (!save_pulse_channel(f, &apu->pulse2)) return false;
    if (!save_triangle_channel(f, &apu->triangle)) return false;
    if (!save_noise_channel(f, &apu->noise)) return false;
    if (!save_dmc_channel(f, &apu->dmc)) return false;
    
    if (fwrite(&apu->frame_count, sizeof(apu->frame_count), 1, f) != 1) return false;
    if (fwrite(&apu->frame_counter_mode, sizeof(apu->frame_counter_mode), 1, f) != 1) return false;
    if (fwrite(&apu->irq_inhibit, sizeof(apu->irq_inhibit), 1, f) != 1) return false;
    if (fwrite(&apu->frame_irq, sizeof(apu->frame_irq), 1, f) != 1) return false;
    if (fwrite(&apu->audio_time, sizeof(apu->audio_time), 1, f) != 1) return false;
    if (fwrite(&apu->audio_time_per_sample, sizeof(apu->audio_time_per_sample), 1, f) != 1) return false;
    
    return true;
}

static bool load_apu(FILE* f, APU* apu) {
    if (!load_pulse_channel(f, &apu->pulse1)) return false;
    if (!load_pulse_channel(f, &apu->pulse2)) return false;
    if (!load_triangle_channel(f, &apu->triangle)) return false;
    if (!load_noise_channel(f, &apu->noise)) return false;
    if (!load_dmc_channel(f, &apu->dmc)) return false;
    
    if (fread(&apu->frame_count, sizeof(apu->frame_count), 1, f) != 1) return false;
    if (fread(&apu->frame_counter_mode, sizeof(apu->frame_counter_mode), 1, f) != 1) return false;
    if (fread(&apu->irq_inhibit, sizeof(apu->irq_inhibit), 1, f) != 1) return false;
    if (fread(&apu->frame_irq, sizeof(apu->frame_irq), 1, f) != 1) return false;
    if (fread(&apu->audio_time, sizeof(apu->audio_time), 1, f) != 1) return false;
    if (fread(&apu->audio_time_per_sample, sizeof(apu->audio_time_per_sample), 1, f) != 1) return false;
    
    apu->sample_count = 0;
    
    return true;
}

static bool save_bus(FILE* f, Bus* bus) {
    if (fwrite(bus->ram, sizeof(bus->ram), 1, f) != 1) return false;
    if (fwrite(bus->controller, sizeof(bus->controller), 1, f) != 1) return false;
    if (fwrite(bus->controller_state, sizeof(bus->controller_state), 1, f) != 1) return false;
    if (fwrite(&bus->controller_strobe, sizeof(bus->controller_strobe), 1, f) != 1) return false;
    if (fwrite(&bus->open_bus, sizeof(bus->open_bus), 1, f) != 1) return false;
    if (fwrite(&bus->dma_pending, sizeof(bus->dma_pending), 1, f) != 1) return false;
    if (fwrite(&bus->dma_page, sizeof(bus->dma_page), 1, f) != 1) return false;
    if (fwrite(&bus->dma_cycles, sizeof(bus->dma_cycles), 1, f) != 1) return false;
    return true;
}

static bool load_bus(FILE* f, Bus* bus) {
    CPU* saved_cpu = bus->cpu;
    PPU* saved_ppu = bus->ppu;
    APU* saved_apu = bus->apu;
    Cartridge* saved_cart = bus->cart;
    
    if (fread(bus->ram, sizeof(bus->ram), 1, f) != 1) return false;
    if (fread(bus->controller, sizeof(bus->controller), 1, f) != 1) return false;
    if (fread(bus->controller_state, sizeof(bus->controller_state), 1, f) != 1) return false;
    if (fread(&bus->controller_strobe, sizeof(bus->controller_strobe), 1, f) != 1) return false;
    if (fread(&bus->open_bus, sizeof(bus->open_bus), 1, f) != 1) return false;
    if (fread(&bus->dma_pending, sizeof(bus->dma_pending), 1, f) != 1) return false;
    if (fread(&bus->dma_page, sizeof(bus->dma_page), 1, f) != 1) return false;
    if (fread(&bus->dma_cycles, sizeof(bus->dma_cycles), 1, f) != 1) return false;
    
    bus->cpu = saved_cpu;
    bus->ppu = saved_ppu;
    bus->apu = saved_apu;
    bus->cart = saved_cart;
    return true;
}

static bool save_cartridge(FILE* f, Cartridge* cart) {
    uint32_t prg_ram_size = cart->prg_ram_size;
    uint32_t chr_ram_size = (cart->chr_ram && cart->chr_rom_size == 0) ? 8192 : 0;
    
    if (fwrite(&prg_ram_size, sizeof(prg_ram_size), 1, f) != 1) return false;
    if (prg_ram_size > 0 && cart->prg_ram) {
        if (fwrite(cart->prg_ram, prg_ram_size, 1, f) != 1) return false;
    }
    
    if (fwrite(&chr_ram_size, sizeof(chr_ram_size), 1, f) != 1) return false;
    if (chr_ram_size > 0 && cart->chr_ram) {
        if (fwrite(cart->chr_ram, chr_ram_size, 1, f) != 1) return false;
    }
    
    if (fwrite(&cart->mirroring, sizeof(cart->mirroring), 1, f) != 1) return false;
    
    return true;
}

static bool load_cartridge(FILE* f, Cartridge* cart) {
    uint32_t prg_ram_size, chr_ram_size;
    
    if (fread(&prg_ram_size, sizeof(prg_ram_size), 1, f) != 1) return false;
    if (prg_ram_size > 0 && cart->prg_ram) {
        if (prg_ram_size != cart->prg_ram_size) return false;
        if (fread(cart->prg_ram, prg_ram_size, 1, f) != 1) return false;
    }
    
    if (fread(&chr_ram_size, sizeof(chr_ram_size), 1, f) != 1) return false;
    if (chr_ram_size > 0 && cart->chr_ram) {
        if (fread(cart->chr_ram, chr_ram_size, 1, f) != 1) return false;
    }
    
    if (fread(&cart->mirroring, sizeof(cart->mirroring), 1, f) != 1) return false;
    
    return true;
}

static bool save_mapper(FILE* f, Cartridge* cart) {
    if (!cart->mapper) return true;
    
    if (cart->mapper->save_state) {
        return cart->mapper->save_state(cart->mapper, f);
    }
    return true;
}

static bool load_mapper(FILE* f, Cartridge* cart) {
    if (!cart->mapper) return true;
    
    if (cart->mapper->load_state) {
        return cart->mapper->load_state(cart->mapper, f);
    }
    return true;
}

bool savestate_save(CPU* cpu, PPU* ppu, APU* apu, Bus* bus, Cartridge* cart,
                    const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open save file: %s\n", filename);
        return false;
    }
    
    bool success = true;
    
    if (!write_header(f)) { success = false; goto cleanup; }
    if (!save_cpu(f, cpu)) { success = false; goto cleanup; }
    if (!save_ppu(f, ppu)) { success = false; goto cleanup; }
    if (!save_apu(f, apu)) { success = false; goto cleanup; }
    if (!save_bus(f, bus)) { success = false; goto cleanup; }
    if (!save_cartridge(f, cart)) { success = false; goto cleanup; }
    if (!save_mapper(f, cart)) { success = false; goto cleanup; }

cleanup:
    fclose(f);
    if (!success) {
        fprintf(stderr, "Failed to write save state: %s\n", filename);
    }
    return success;
}

bool savestate_load(CPU* cpu, PPU* ppu, APU* apu, Bus* bus, Cartridge* cart,
                    const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open save file: %s\n", filename);
        return false;
    }
    
    bool success = true;
    
    if (!read_header(f)) {
        fprintf(stderr, "Invalid save file header: %s\n", filename);
        success = false;
        goto cleanup;
    }
    if (!load_cpu(f, cpu)) { success = false; goto cleanup; }
    if (!load_ppu(f, ppu)) { success = false; goto cleanup; }
    if (!load_apu(f, apu)) { success = false; goto cleanup; }
    if (!load_bus(f, bus)) { success = false; goto cleanup; }
    if (!load_cartridge(f, cart)) { success = false; goto cleanup; }
    if (!load_mapper(f, cart)) { success = false; goto cleanup; }

cleanup:
    fclose(f);
    if (!success) {
        fprintf(stderr, "Failed to load save state: %s\n", filename);
    }
    return success;
}
