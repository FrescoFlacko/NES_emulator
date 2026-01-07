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
#include "savestate/savestate.h"

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

#define NOTIFY_DURATION_FRAMES 120
#define NOTIFY_MAX_LEN 32

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

    char notify_message[NOTIFY_MAX_LEN];
    int notify_frames_left;
} NES;

static const uint8_t font_5x7[96][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // space
    {0x04,0x04,0x04,0x04,0x00,0x04,0x00}, // !
    {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00}, // "
    {0x0A,0x1F,0x0A,0x1F,0x0A,0x00,0x00}, // #
    {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04}, // $
    {0x18,0x19,0x02,0x04,0x08,0x13,0x03}, // %
    {0x08,0x14,0x14,0x08,0x15,0x12,0x0D}, // &
    {0x04,0x04,0x00,0x00,0x00,0x00,0x00}, // '
    {0x02,0x04,0x08,0x08,0x08,0x04,0x02}, // (
    {0x08,0x04,0x02,0x02,0x02,0x04,0x08}, // )
    {0x00,0x04,0x15,0x0E,0x15,0x04,0x00}, // *
    {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}, // +
    {0x00,0x00,0x00,0x00,0x04,0x04,0x08}, // ,
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x04,0x00}, // .
    {0x00,0x01,0x02,0x04,0x08,0x10,0x00}, // /
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, // 0
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, // 1
    {0x0E,0x11,0x01,0x0E,0x10,0x10,0x1F}, // 2
    {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E}, // 3
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, // 4
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, // 5
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, // 6
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, // 7
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, // 8
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, // 9
    {0x00,0x04,0x00,0x00,0x04,0x00,0x00}, // :
    {0x00,0x04,0x00,0x00,0x04,0x04,0x08}, // ;
    {0x02,0x04,0x08,0x10,0x08,0x04,0x02}, // <
    {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}, // =
    {0x08,0x04,0x02,0x01,0x02,0x04,0x08}, // >
    {0x0E,0x11,0x01,0x06,0x04,0x00,0x04}, // ?
    {0x0E,0x11,0x17,0x15,0x17,0x10,0x0E}, // @
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, // A
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, // B
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, // C
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}, // D
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, // E
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, // F
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, // G
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, // H
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, // I
    {0x01,0x01,0x01,0x01,0x01,0x11,0x0E}, // J
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, // K
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, // L
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, // M
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, // N
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, // O
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, // P
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, // Q
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, // R
    {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E}, // S
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, // T
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, // U
    {0x11,0x11,0x11,0x11,0x0A,0x0A,0x04}, // V
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}, // W
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, // X
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, // Y
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, // Z
    {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}, // [
    {0x00,0x10,0x08,0x04,0x02,0x01,0x00}, // backslash
    {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}, // ]
    {0x04,0x0A,0x11,0x00,0x00,0x00,0x00}, // ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}, // _
    {0x08,0x04,0x00,0x00,0x00,0x00,0x00}, // `
    {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F}, // a
    {0x10,0x10,0x1E,0x11,0x11,0x11,0x1E}, // b
    {0x00,0x00,0x0E,0x11,0x10,0x11,0x0E}, // c
    {0x01,0x01,0x0F,0x11,0x11,0x11,0x0F}, // d
    {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E}, // e
    {0x06,0x08,0x1E,0x08,0x08,0x08,0x08}, // f
    {0x00,0x00,0x0F,0x11,0x0F,0x01,0x0E}, // g
    {0x10,0x10,0x1E,0x11,0x11,0x11,0x11}, // h
    {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E}, // i
    {0x02,0x00,0x06,0x02,0x02,0x12,0x0C}, // j
    {0x10,0x10,0x12,0x14,0x18,0x14,0x12}, // k
    {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E}, // l
    {0x00,0x00,0x1A,0x15,0x15,0x15,0x15}, // m
    {0x00,0x00,0x1E,0x11,0x11,0x11,0x11}, // n
    {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E}, // o
    {0x00,0x00,0x1E,0x11,0x1E,0x10,0x10}, // p
    {0x00,0x00,0x0F,0x11,0x0F,0x01,0x01}, // q
    {0x00,0x00,0x16,0x19,0x10,0x10,0x10}, // r
    {0x00,0x00,0x0F,0x10,0x0E,0x01,0x1E}, // s
    {0x08,0x08,0x1E,0x08,0x08,0x09,0x06}, // t
    {0x00,0x00,0x11,0x11,0x11,0x11,0x0F}, // u
    {0x00,0x00,0x11,0x11,0x11,0x0A,0x04}, // v
    {0x00,0x00,0x11,0x11,0x15,0x15,0x0A}, // w
    {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11}, // x
    {0x00,0x00,0x11,0x11,0x0F,0x01,0x0E}, // y
    {0x00,0x00,0x1F,0x02,0x04,0x08,0x1F}, // z
    {0x02,0x04,0x04,0x08,0x04,0x04,0x02}, // {
    {0x04,0x04,0x04,0x04,0x04,0x04,0x04}, // |
    {0x08,0x04,0x04,0x02,0x04,0x04,0x08}, // }
    {0x00,0x08,0x15,0x02,0x00,0x00,0x00}, // ~
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // DEL
};

static void notify_show(NES* nes, const char* message) {
    strncpy(nes->notify_message, message, NOTIFY_MAX_LEN - 1);
    nes->notify_message[NOTIFY_MAX_LEN - 1] = '\0';
    nes->notify_frames_left = NOTIFY_DURATION_FRAMES;
}

static void draw_char(uint32_t* fb, int x, int y, char c, uint32_t color) {
    if (c < 32 || c > 127) c = ' ';
    int idx = c - 32;
    for (int row = 0; row < 7; row++) {
        int py = y + row;
        if (py < 0 || py >= 240) continue;
        uint8_t bits = font_5x7[idx][row];
        for (int col = 0; col < 5; col++) {
            int px = x + col;
            if (px < 0 || px >= 256) continue;
            if (bits & (0x10 >> col)) {
                fb[py * 256 + px] = color;
            }
        }
    }
}

static void draw_text(uint32_t* fb, int x, int y, const char* text, uint32_t color) {
    while (*text) {
        draw_char(fb, x, y, *text, color);
        x += 6;
        text++;
    }
}

static void render_notification(NES* nes, uint32_t* display_fb) {
    if (nes->notify_frames_left <= 0) return;
    
    memcpy(display_fb, nes->ppu.framebuffer, sizeof(nes->ppu.framebuffer));
    
    int text_len = (int)strlen(nes->notify_message);
    int text_width = text_len * 6;
    int x = (256 - text_width) / 2;
    int y = 8;
    
    uint32_t bg_color = 0xFF000000;
    for (int row = y - 2; row < y + 9; row++) {
        if (row < 0 || row >= 240) continue;
        for (int col = x - 4; col < x + text_width + 4; col++) {
            if (col < 0 || col >= 256) continue;
            display_fb[row * 256 + col] = bg_color;
        }
    }
    
    uint32_t text_color = 0xFFFFFFFF;
    draw_text(display_fb, x, y, nes->notify_message, text_color);
    
    nes->notify_frames_left--;
}

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

        case SDLK_F5:
            if (pressed) {
                if (savestate_save(&nes->cpu, &nes->ppu, &nes->apu, &nes->bus, &nes->cart, "savestate.sav")) {
                    notify_show(nes, "State Saved");
                } else {
                    notify_show(nes, "Save Failed!");
                }
            }
            return;

        case SDLK_F8:
            if (pressed) {
                if (savestate_load(&nes->cpu, &nes->ppu, &nes->apu, &nes->bus, &nes->cart, "savestate.sav")) {
                    notify_show(nes, "State Loaded");
                } else {
                    notify_show(nes, "Load Failed!");
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
    static uint32_t display_buffer[256 * 240];
    
    if (nes->notify_frames_left > 0) {
        render_notification(nes, display_buffer);
        SDL_UpdateTexture(
            nes->texture,
            NULL,
            display_buffer,
            256 * sizeof(uint32_t)
        );
    } else {
        SDL_UpdateTexture(
            nes->texture,
            NULL,
            nes->ppu.framebuffer,
            256 * sizeof(uint32_t)
        );
    }
    
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
    printf("  F5         - Save State\n");
    printf("  F8         - Load State\n");
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
