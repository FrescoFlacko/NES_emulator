/*
 * Module: src/savestate/savestate.h
 * Responsibility: Save state serialization - snapshot/restore emulator state to/from disk.
 * Key invariants:
 *  - File format starts with "NESSAVE1" magic + version for validation
 *  - Pointers in structs are preserved during load (not serialized, re-linked after)
 *  - All component state (CPU, PPU, APU, Bus, Cartridge RAM, Mapper) captured
 * Notes:
 *  - F5 = Save State, F8 = Load State (key bindings in main.c)
 *  - Save file includes framebuffer for visual continuity on load
 */
#ifndef SAVESTATE_H
#define SAVESTATE_H

#include <stdbool.h>

/* Forward declarations - NES struct is defined in main.c */
typedef struct CPU CPU;
typedef struct PPU PPU;
typedef struct APU APU;
typedef struct Bus Bus;
typedef struct Cartridge Cartridge;

/*
 * Save current emulator state to file.
 * Returns true on success, false on failure (file error, etc.).
 * 
 * Components saved:
 *  - CPU: registers, cycles, interrupt flags
 *  - PPU: registers, VRAM, OAM, palette, scroll state, framebuffer
 *  - APU: all channel state, frame counter, timers
 *  - Bus: RAM, controller state
 *  - Cartridge: PRG-RAM, CHR-RAM (if present)
 *  - Mapper: internal registers (via mapper save_state hook)
 */
bool savestate_save(CPU* cpu, PPU* ppu, APU* apu, Bus* bus, Cartridge* cart,
                    const char* filename);

/*
 * Load emulator state from file.
 * Returns true on success, false on failure (file not found, invalid format, etc.).
 * 
 * IMPORTANT: Caller must ensure pointers between components are re-established
 * after load if needed (this function preserves existing pointer linkages).
 */
bool savestate_load(CPU* cpu, PPU* ppu, APU* apu, Bus* bus, Cartridge* cart,
                    const char* filename);

#endif
