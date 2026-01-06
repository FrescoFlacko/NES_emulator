/*
 * Module: src/main.c
 * Responsibility: SDL2 game loop - window, audio, input handling, frame timing.
 * Key invariants:
 *  - PPU ticks 3× per CPU cycle; APU ticks 1× per CPU cycle
 *  - OAM DMA suspends CPU for 513-514 cycles (odd cycle alignment)
 *  - NMI checked after each CPU instruction and during DMA
 * Notes:
 *  - Audio sync: waits if SDL queue > 4096 samples to prevent buffer overflow
 *  - Controller: shift register with strobe, standard NES button mapping
 */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu/cpu.h"
#include "bus/bus.h"
#include "ppu/ppu.h"
#include "apu/apu.h"
#include "cartridge/cartridge.h"
#include "mapper/mapper.h"

#define WINDOW_SCALE 3
#define WINDOW_WIDTH (256 * WINDOW_SCALE)
#define WINDOW_HEIGHT (240 * WINDOW_SCALE)

#define BTN_A      0x01
#define BTN_B      0x02
#define BTN_SELECT 0x04
#define BTN_START  0x08
#define BTN_UP     0x10
#define BTN_DOWN   0x20
#define BTN_LEFT   0x40
#define BTN_RIGHT  0x80

typedef struct NES {
    CPU cpu;
    PPU ppu;
    APU apu;
    Bus bus;
    Cartridge cart;

    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    SDL_AudioDeviceID audio_device;

    bool running;
    bool debug_enabled;
    uint64_t frame_count;
    uint64_t nmi_count;
    uint64_t controller_read_count;
} NES;

static void nes_init(NES* nes) {
    memset(nes, 0, sizeof(NES));
    
    bus_init(&nes->bus);
    ppu_init(&nes->ppu);
    apu_init(&nes->apu);
    cpu_init(&nes->cpu, &nes->bus);
    
    nes->bus.cpu = &nes->cpu;
    nes->bus.ppu = &nes->ppu;
    nes->bus.apu = &nes->apu;
    nes->ppu.cart = &nes->cart;
    
    nes->debug_enabled = false;
    nes->frame_count = 0;
    nes->nmi_count = 0;
    nes->controller_read_count = 0;
}

static bool nes_load_rom(NES* nes, const char* filename) {
    if (!cartridge_load(&nes->cart, filename)) {
        fprintf(stderr, "Failed to load ROM: %s\n", filename);
        return false;
    }
    
    nes->bus.cart = &nes->cart;
    nes->ppu.cart = &nes->cart;
    
    cpu_reset(&nes->cpu);
    ppu_reset(&nes->ppu);
    apu_reset(&nes->apu);
    
    printf("Loaded ROM: %s\n", filename);
    printf("  PRG ROM: %u KB\n", nes->cart.prg_rom_size / 1024);
    printf("  CHR ROM: %u KB\n", nes->cart.chr_rom_size / 1024);
    printf("  Mapper: %u\n", nes->cart.mapper_id);
    printf("  Mirroring: %s\n", nes->cart.mirroring ? "Vertical" : "Horizontal");
    
    return true;
}

static bool nes_init_sdl(NES* nes) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    
    nes->window = SDL_CreateWindow(
        "NES Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    
    if (!nes->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    
    nes->renderer = SDL_CreateRenderer(
        nes->window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    
    if (!nes->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }
    
    nes->texture = SDL_CreateTexture(
        nes->renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        256,
        240
    );
    
    if (!nes->texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return false;
    }
    
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = 44100;
    want.format = AUDIO_F32SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = NULL;
    want.userdata = nes;
    
    nes->audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (nes->audio_device == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return false;
    }
    
    SDL_PauseAudioDevice(nes->audio_device, 0);
    
    return true;
}

static void nes_cleanup(NES* nes) {
    if (nes->audio_device) SDL_CloseAudioDevice(nes->audio_device);
    if (nes->texture) SDL_DestroyTexture(nes->texture);
    if (nes->renderer) SDL_DestroyRenderer(nes->renderer);
    if (nes->window) SDL_DestroyWindow(nes->window);
    SDL_Quit();
    
    cartridge_free(&nes->cart);
}

static void handle_key(NES* nes, SDL_Keycode key, bool pressed) {
    uint8_t button = 0;
    
    switch (key) {
        case SDLK_z:      button = BTN_A;      break;
        case SDLK_x:      button = BTN_B;      break;
        case SDLK_RSHIFT: button = BTN_SELECT; break;
        case SDLK_RETURN: button = BTN_START;  break;
        case SDLK_UP:     button = BTN_UP;     break;
        case SDLK_DOWN:   button = BTN_DOWN;   break;
        case SDLK_LEFT:   button = BTN_LEFT;   break;
        case SDLK_RIGHT:  button = BTN_RIGHT;  break;
        
        case SDLK_ESCAPE:
            nes->running = false;
            return;
            
        case SDLK_r:
            if (pressed) {
                cpu_reset(&nes->cpu);
                ppu_reset(&nes->ppu);
                apu_reset(&nes->apu);
                printf("Reset!\n");
            }
            return;
        
        case SDLK_d:
            if (pressed) {
                nes->debug_enabled = !nes->debug_enabled;
                printf("Debug: %s\n", nes->debug_enabled ? "ON" : "OFF");
            }
            return;
            
        case SDLK_s:
            if (pressed) {
                char filename[64];
                snprintf(filename, sizeof(filename), "screenshot_%06llu.bmp", (unsigned long long)nes->frame_count);
                SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
                    nes->ppu.framebuffer,
                    256, 240, 32, 256 * 4,
                    SDL_PIXELFORMAT_ARGB8888
                );
                if (surface) {
                    SDL_SaveBMP(surface, filename);
                    SDL_FreeSurface(surface);
                    printf("Saved screenshot: %s\n", filename);
                } else {
                    fprintf(stderr, "Failed to create screenshot surface: %s\n", SDL_GetError());
                }
            }
            return;

        default:
            return;
    }
    
    if (pressed) {
        nes->bus.controller[0] |= button;
        if (nes->debug_enabled) {
            printf("KEY DOWN: %02X -> controller[0]=%02X\n", button, nes->bus.controller[0]);
        }
    } else {
        nes->bus.controller[0] &= ~button;
        if (nes->debug_enabled) {
            printf("KEY UP: %02X -> controller[0]=%02X\n", button, nes->bus.controller[0]);
        }
    }
}

static void handle_events(NES* nes) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                nes->running = false;
                break;
                
            case SDL_KEYDOWN:
                handle_key(nes, event.key.keysym.sym, true);
                break;
                
            case SDL_KEYUP:
                handle_key(nes, event.key.keysym.sym, false);
                break;
        }
    }
}

static void render_frame(NES* nes) {
    SDL_UpdateTexture(
        nes->texture,
        NULL,
        nes->ppu.framebuffer,
        256 * sizeof(uint32_t)
    );
    
    SDL_RenderClear(nes->renderer);
    SDL_RenderCopy(nes->renderer, nes->texture, NULL, NULL);
    SDL_RenderPresent(nes->renderer);
}

static void run_frame(NES* nes) {
    while (!nes->ppu.frame_ready) {
        if (nes->bus.dma_pending) {
            nes->bus.dma_pending = false;
            
            uint16_t start_addr = (uint16_t)nes->bus.dma_page << 8;
            for (int i = 0; i < 256; i++) {
                uint8_t val = bus_read(&nes->bus, start_addr + i);
                ppu_write_register(&nes->ppu, 0x2004, val);
            }
            
            for (int i = 0; i < 513 * 3; i++) {
                 ppu_tick(&nes->ppu);
                 if (i % 3 == 0) apu_tick(&nes->apu);
                 if (nes->ppu.nmi_pending) {
                     nes->ppu.nmi_pending = false;
                     cpu_nmi(&nes->cpu);
                     nes->nmi_count++;
                 }
            }
        }

        if (nes->ppu.nmi_pending) {
            nes->ppu.nmi_pending = false;
            cpu_nmi(&nes->cpu);
            nes->nmi_count++;
        }
        
        int cycles = cpu_step(&nes->cpu);
        
        for (int i = 0; i < cycles; i++) {
            apu_tick(&nes->apu);
        }
        
        for (int i = 0; i < cycles * 3; i++) {
            ppu_tick(&nes->ppu);
            
            if (nes->ppu.nmi_pending) {
                nes->ppu.nmi_pending = false;
                cpu_nmi(&nes->cpu);
                nes->nmi_count++;
            }
        }
        
        if (nes->apu.frame_irq && !nes->apu.irq_inhibit) {
            cpu_irq(&nes->cpu);
        }

        /* Check for mapper IRQ (MMC3 scanline IRQ) */
        if (nes->bus.cart && nes->bus.cart->mapper) {
            Mapper* mapper = nes->bus.cart->mapper;
            if (mapper->irq_pending && mapper->irq_pending(mapper)) {
                cpu_irq(&nes->cpu);
                // if (mapper->irq_clear) mapper->irq_clear(mapper);
            }
        }
    }
    
    nes->ppu.frame_ready = false;
    nes->frame_count++;
}

static void print_usage(const char* program) {
    printf("Usage: %s <rom.nes>\n", program);
    printf("\nControls:\n");
    printf("  Arrow keys - D-pad\n");
    printf("  Z          - A button\n");
    printf("  X          - B button\n");
    printf("  Enter      - Start\n");
    printf("  Right Shift - Select\n");
    printf("  R          - Reset\n");
    printf("  D          - Toggle debug\n");
    printf("  Escape     - Quit\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    NES nes;
    nes_init(&nes);
    
    if (!nes_load_rom(&nes, argv[1])) {
        return 1;
    }
    
    if (!nes_init_sdl(&nes)) {
        nes_cleanup(&nes);
        return 1;
    }
    
    printf("\nStarting emulation...\n");
    printf("Press Escape to quit, R to reset, D to toggle debug\n\n");
    
    nes.running = true;
    nes.debug_enabled = true;
    
    float audio_buffer[4096];
    
    while (nes.running) {
        handle_events(&nes);
        run_frame(&nes);
        render_frame(&nes);
        
        int count = apu_get_buffer(&nes.apu, audio_buffer, 4096);
        if (count > 0) {
            SDL_QueueAudio(nes.audio_device, audio_buffer, count * sizeof(float));
        }
        
        uint32_t queued = SDL_GetQueuedAudioSize(nes.audio_device);
        while (queued > 4096 * sizeof(float)) {
            SDL_Delay(1);
            queued = SDL_GetQueuedAudioSize(nes.audio_device);
        }
        
        if (nes.debug_enabled && (nes.frame_count % 60 == 0)) {
        }
    }
    
    nes_cleanup(&nes);
    printf("Goodbye!\n");
    
    return 0;
}
