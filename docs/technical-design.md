# Technical Design Document

This document describes the current implemented architecture of the NES emulator. For detailed implementation plans, milestones, and code examples, see [PLAN.md](../PLAN.md).

## 1. Overview

The emulator models the NES hardware as interconnected components:

```
┌─────────────────────────────────────────────────────────────┐
│                          Bus                                 │
│  ┌───────┐  ┌───────┐  ┌───────┐  ┌───────┐  ┌───────────┐ │
│  │  CPU  │  │  PPU  │  │  APU  │  │  RAM  │  │ Cartridge │ │
│  │(6502) │  │       │  │       │  │ 2KB   │  │  ┌──────┐ │ │
│  └───┬───┘  └───┬───┘  └───┬───┘  └───────┘  │  │Mapper│ │ │
│      │          │          │                  │  └──────┘ │ │
│      └──────────┴──────────┴──────────────────┴───────────┘ │
└─────────────────────────────────────────────────────────────┘
```

| Component | Responsibility | Source |
|-----------|---------------|--------|
| CPU | Execute 6502 instructions, manage cycles | `src/cpu/` |
| Bus | Route memory reads/writes, tick peripherals | `src/bus/` |
| PPU | Render backgrounds/sprites, handle VRAM | `src/ppu/` |
| APU | Generate audio (pulse, triangle, noise, DMC) | `src/apu/` |
| Cartridge | Load iNES ROMs, delegate to mapper | `src/cartridge/` |
| Mapper | Bank switching, address translation | `src/mapper/` |

## 2. Core Data Flow

### Memory Access

All CPU memory access goes through the Bus:

```c
uint8_t value = bus_read(bus, address);
bus_write(bus, address, value);
```

The Bus decodes addresses and routes to the appropriate component:

| Address Range | Target | Notes |
|---------------|--------|-------|
| `$0000-$1FFF` | RAM | 2KB mirrored 4× |
| `$2000-$3FFF` | PPU registers | 8 regs mirrored |
| `$4000-$4017` | APU/IO | APU regs + controller |
| `$4020-$FFFF` | Cartridge | PRG ROM/RAM via mapper |

### Cartridge → Mapper Delegation

The Cartridge does not directly handle address mapping. It delegates to a Mapper:

```c
uint8_t cartridge_cpu_read(Cartridge* cart, uint16_t addr) {
    return cart->mapper->cpu_read(cart->mapper, addr);
}
```

This allows different mappers to implement bank switching without changing Cartridge code.

## 3. Timing Model

### CPU Cycles

Each instruction consumes a known number of cycles. The CPU tracks total cycles:

```c
int cycles = cpu_step(&cpu);  // Execute one instruction
cpu.cycles += cycles;         // Already done internally
```

### PPU Synchronization

The PPU runs at 3× the CPU clock. After each CPU instruction:

```c
void bus_tick(Bus* bus, int cpu_cycles) {
    for (int i = 0; i < cpu_cycles * 3; i++) {
        ppu_tick(bus->ppu);
    }
    for (int i = 0; i < cpu_cycles; i++) {
        apu_tick(bus->apu);
    }
}
```

### Frame Timing

- 341 PPU dots per scanline
- 262 scanlines per frame
- Scanlines 0-239: visible
- Scanline 240: post-render (idle)
- Scanlines 241-260: VBlank
- Scanline 261: pre-render

## 4. Memory Maps

### CPU Memory Map

| Range | Size | Description |
|-------|------|-------------|
| `$0000-$07FF` | 2KB | Internal RAM |
| `$0800-$1FFF` | - | Mirrors of RAM |
| `$2000-$2007` | 8 | PPU registers |
| `$2008-$3FFF` | - | Mirrors of PPU registers |
| `$4000-$4015` | - | APU registers |
| `$4016` | 1 | Controller 1 |
| `$4017` | 1 | Controller 2 / APU frame counter |
| `$4018-$401F` | - | Normally disabled |
| `$4020-$FFFF` | - | Cartridge space |

### PPU Memory Map

| Range | Size | Description |
|-------|------|-------------|
| `$0000-$1FFF` | 8KB | Pattern tables (CHR ROM/RAM) |
| `$2000-$2FFF` | 4KB | Nametables (2KB + mirroring) |
| `$3000-$3EFF` | - | Mirrors of nametables |
| `$3F00-$3F1F` | 32 | Palette RAM |
| `$3F20-$3FFF` | - | Mirrors of palette |

## 5. Cartridge Format

The emulator supports iNES format (`.nes` files):

```
Header (16 bytes):
  0-3: "NES\x1A" magic
  4:   PRG ROM size (16KB units)
  5:   CHR ROM size (8KB units, 0 = CHR RAM)
  6:   Flags 6 (mapper low, mirroring, battery, trainer)
  7:   Flags 7 (mapper high, format)
  8-15: Usually zero

Trainer (512 bytes, optional)
PRG ROM (N × 16KB)
CHR ROM (M × 8KB, or none if CHR RAM)
```

### Supported Mappers

| ID | Name | Games |
|----|------|-------|
| 0 | NROM | Super Mario Bros, Donkey Kong |

## 6. Testing Strategy

### Unit Tests (`test/test_cpu.c`)

Tests for:
- CPU initialization and register state
- Stack operations (push/pop 8-bit and 16-bit)
- Flag manipulation
- RAM mirroring
- 16-bit read helpers (normal, zero-page wrap, JMP bug)
- Cartridge loading

Run with: `make test`

### CPU Validation (`test/nestest_runner.c`)

Runs the nestest ROM and produces a trace. The trace is compared line-by-line against `testdata/nestest.log`:

```bash
make nestest
# Empty diff output = PASS
```

The nestest ROM exercises all official and unofficial 6502 opcodes. A passing test means cycle-accurate CPU emulation.

See [testing.md](testing.md) for details on adding new tests.

## 7. Known Limitations

### Not Implemented
- Mappers beyond NROM (no MMC1, MMC3, etc.)
- Save states
- Rewind
- Netplay

### Accuracy Trade-offs
- PPU sprite overflow bug not emulated
- Some obscure unofficial opcodes may have edge-case differences
- APU DMC DMA timing simplified

## 8. References

- [NESDev Wiki](https://www.nesdev.org/wiki/) - Primary reference
- [6502 Instruction Reference](http://www.obelisk.me.uk/6502/reference.html)
- [PPU Frame Timing](https://www.nesdev.org/wiki/PPU_frame_timing)
- [APU Reference](https://www.nesdev.org/wiki/APU)
- [nestest](http://www.qmtpro.com/~nes/misc/) - CPU validation ROM
