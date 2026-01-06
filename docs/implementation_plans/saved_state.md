# Implementation Plan: Saved State Logic

This document outlines the design and implementation steps for adding "Save State" and "Load State" functionality to the NES emulator.

## 1. Objective
Allow the user to snapshot the exact state of the emulator (CPU, PPU, APU, Memory, Mapper) to a disk file and restore it later, resuming execution seamlessly.

## 2. Architecture

### New Module: `src/savestate`
We will create a dedicated module to handle the orchestration of serializing different components.

-   **`src/savestate.h`**: Public interface.
-   **`src/savestate.c`**: Implementation of serialization logic.

### Mapper Interface Update
Mappers hold internal state (e.g., MMC3 IRQ counters, bank registers) that must be preserved. We will extend the `Mapper` struct to support this.

## 3. File Format
The save file will use a binary format with a header for validation.

| Section | Size (Bytes) | Description |
| :--- | :--- | :--- |
| **Header** | 16 | Magic ("NESSAVE1") + Version + Flags |
| **CPU** | `sizeof(CPU)` | Registers (A, X, Y, S, P, PC) + Cycles + Interrupt State |
| **PPU** | `sizeof(PPU)` | Registers (Ctrl, Mask, Status, V, T, FineX, W) + Latches + OAM + VRAM + Palette |
| **APU** | `sizeof(APU)` | Channel States + Frame Counter + Timer values |
| **Bus** | `sizeof(Bus)` | RAM (2KB) + Controller State |
| **Cartridge** | Varies | PRG-RAM (8KB) + CHR-RAM (if applicable) |
| **Mapper** | Varies | Mapper-specific registers (e.g., MMC3 IRQ state) |

> **Note on Pointers**: Structs containing pointers (e.g., `CPU.bus`, `PPU.cart`) cannot be blindly `memcpy`'d during restoration. We must backup the existing valid pointers before overwriting the struct with loaded data, then restore them.

## 4. Implementation Steps

### Step 1: Define Mapper Interface
Update `src/mapper/mapper.h` to include save/load function pointers.

```c
typedef struct Mapper {
    // ... existing fields ...
    bool (*save_state)(struct Mapper* m, FILE* f);
    bool (*load_state)(struct Mapper* m, FILE* f);
} Mapper;
```

Update `src/mapper/mapper.c`:
-   Implement `mmc3_save_state` / `mmc3_load_state` for Mapper 4.
-   Implement dummy functions for Mapper 0 (NROM).

### Step 2: Implement Save State Module
Create `src/savestate.c` with the following functions:

-   `bool nes_save_state(NES* nes, const char* filename)`
-   `bool nes_load_state(NES* nes, const char* filename)`

Helper functions will serialize specific components:
-   `save_cpu`, `load_cpu`
-   `save_ppu`, `load_ppu` (Note: Save `framebuffer` for visual continuity)
-   `save_apu`, `load_apu`
-   `save_bus`, `load_bus` (Saves RAM)
-   `save_cart`, `load_cart` (Saves PRG-RAM/CHR-RAM)

### Step 3: Main Loop Integration
Modify `src/main.c`:
-   Include `savestate.h`.
-   Add key bindings in `handle_key`:
    -   **F5**: Save State
    -   **F8**: Load State

## 5. Verification Plan
1.  **Boot Game**: Start *Super Mario Bros*.
2.  **Play**: Advance to World 1-2.
3.  **Save**: Trigger Save State.
4.  **Reset**: Reset the emulator (starts at title screen).
5.  **Load**: Trigger Load State.
6.  **Verify**:
    -   Player position is exact.
    -   Music continues without skipping (APU state correct).
    -   Visuals are correct (PPU state correct).
    -   Next level load works (Mapper state/Cartridge RAM correct).
