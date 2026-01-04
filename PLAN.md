# NES Emulator CPU Implementation Plan

## Goal

Build a **trace-perfect** 6502/2A03 CPU core that passes `nestest.nes` validation by matching the reference `nestest.log` **line-for-line**, including:
- PC, opcode bytes, disassembly
- A, X, Y, P, SP registers
- PPU scanline/dot columns
- CPU cycle count

The architecture will support **mapper extensibility** from day one.

---

## Success Criteria

1. ✅ `make nestest` produces zero diff against committed `nestest.log`
2. ✅ All CPU columns match exactly (PC, opcodes, registers, SP, CYC)
3. ✅ PPU columns (scanline, dot) match exactly (requires basic PPU timing model)
4. ✅ Architecture cleanly supports adding mappers beyond NROM

---

## Phase 0: Project Setup & Test Artifacts

### 0.1 Directory Structure

```
NES_emulator/
├── src/
│   ├── cpu/
│   │   ├── cpu.h          # Refactored from cpu/cpu.h
│   │   ├── cpu.c          # Refactored from cpu/cpu.c
│   │   ├── opcodes.h      # Refactored from cpu/opcodes.h
│   │   └── opcodes.c      # Refactored from cpu/opcodes.c
│   ├── bus/
│   │   ├── bus.h
│   │   └── bus.c
│   ├── cartridge/
│   │   ├── cartridge.h
│   │   ├── cartridge.c
│   │   └── ines.c
│   ├── mapper/
│   │   ├── mapper.h
│   │   ├── mapper.c
│   │   ├── mapper000.c    # NROM
│   │   ├── mapper001.c    # MMC1
│   │   ├── mapper002.c    # UxROM
│   │   ├── mapper003.c    # CNROM
│   │   └── mapper004.c    # MMC3
│   ├── ppu/
│   │   ├── ppu.h
│   │   └── ppu.c
│   ├── apu/
│   │   ├── apu.h
│   │   └── apu.c          # Stubbed initially
│   ├── input/
│   │   ├── controller.h
│   │   └── controller.c
│   └── main.c             # SDL2 game loop for SMB
├── test/
│   ├── test_cpu.c         # Unit tests (refactored from test/)
│   ├── test_opcodes.c
│   └── nestest_runner.c   # nestest trace validator
├── legacy/                 # Original code preserved for sentiment
│   ├── cpu/               # Copy of original cpu/ before refactor
│   └── README.md          # Notes about original implementation
├── testdata/
│   └── nestest.log        # Committed reference log
├── roms/
│   └── test/
│       └── .gitkeep       # nestest.nes downloaded here (gitignored)
├── build/                  # CMake build directory (gitignored)
├── CMakeLists.txt
├── Makefile               # Wrapper for CMake convenience
├── PLAN.md
├── .clang-format
├── .gitignore
└── README.md
```

### 0.2 Fetch nestest ROM

Add Makefile target:

```makefile
fetch-nestest:
	@mkdir -p roms/test
	curl -L -o roms/test/nestest.nes http://www.qmtpro.com/~nes/misc/nestest.nes
	@echo "Downloaded nestest.nes"
```

### 0.3 Commit nestest.log

- Download `http://www.qmtpro.com/~nes/misc/nestest.log`
- Place in `testdata/nestest.log`
- Commit to repo

### 0.4 Update .gitignore

```
# Build artifacts
*.o
*.out
src/**/*.o
test/test
test/nestest_runner

# ROMs (downloaded, not committed)
roms/test/*.nes

# Editor/OS
.DS_Store
*.swp
```

**Deliverable:** `make fetch-nestest` downloads ROM; `testdata/nestest.log` is committed.

---

## Phase 1: Core Architecture (CPU + Bus + Cartridge + Mapper Interface)

### 1.1 CPU Structure

```c
// src/cpu/cpu.h

#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>

// Status flag masks
#define FLAG_C 0x01  // Carry
#define FLAG_Z 0x02  // Zero
#define FLAG_I 0x04  // Interrupt Disable
#define FLAG_D 0x08  // Decimal (ignored on NES but flag exists)
#define FLAG_B 0x10  // Break (not a real flag, only on stack pushes)
#define FLAG_U 0x20  // Unused (always 1)
#define FLAG_V 0x40  // Overflow
#define FLAG_N 0x80  // Negative

typedef struct Bus Bus;  // Forward declaration

typedef struct CPU {
    uint8_t  A;      // Accumulator
    uint8_t  X;      // Index X
    uint8_t  Y;      // Index Y
    uint8_t  P;      // Status (flags)
    uint8_t  S;      // Stack pointer (8-bit, stack at $0100-$01FF)
    uint16_t PC;     // Program counter
    
    uint64_t cycles; // Total cycles elapsed
    
    // Interrupt flags
    bool nmi_pending;
    bool irq_pending;
    
    // Reference to bus for memory access
    Bus* bus;
} CPU;

// Core functions
void cpu_init(CPU* cpu, Bus* bus);
void cpu_reset(CPU* cpu);
int  cpu_step(CPU* cpu);  // Execute one instruction, return cycles consumed

// Interrupts
void cpu_nmi(CPU* cpu);
void cpu_irq(CPU* cpu);

// Debug/trace
void cpu_trace(CPU* cpu, char* buffer, size_t size);

#endif
```

### 1.2 Bus Structure

```c
// src/bus/bus.h

#ifndef BUS_H
#define BUS_H

#include <stdint.h>

typedef struct CPU CPU;
typedef struct PPU PPU;
typedef struct Cartridge Cartridge;

typedef struct Bus {
    CPU* cpu;
    PPU* ppu;
    Cartridge* cart;
    
    uint8_t ram[2048];        // $0000-$07FF (mirrored to $1FFF)
    uint8_t controller[2];    // Controller state (stubbed initially)
    
    // Open bus value (last value on data bus)
    uint8_t open_bus;
} Bus;

void    bus_init(Bus* bus);
uint8_t bus_read(Bus* bus, uint16_t addr);
void    bus_write(Bus* bus, uint16_t addr, uint8_t val);

// For CPU to tick PPU (3 PPU cycles per CPU cycle)
void    bus_tick(Bus* bus, int cpu_cycles);

#endif
```

### 1.3 Cartridge Structure

```c
// src/cartridge/cartridge.h

#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct Mapper Mapper;

typedef struct Cartridge {
    uint8_t* prg_rom;
    uint8_t* chr_rom;      // NULL if CHR RAM
    uint8_t* chr_ram;      // Used if chr_rom_size == 0
    uint8_t* prg_ram;      // Battery-backed or not
    
    uint32_t prg_rom_size;
    uint32_t chr_rom_size;
    uint32_t prg_ram_size;
    
    uint8_t mapper_id;
    uint8_t mirroring;     // 0=horizontal, 1=vertical, 2=four-screen
    bool    has_battery;
    
    Mapper* mapper;
} Cartridge;

// Load from iNES file
bool cartridge_load(Cartridge* cart, const char* filename);
void cartridge_free(Cartridge* cart);

// CPU/PPU access (delegated to mapper)
uint8_t cartridge_cpu_read(Cartridge* cart, uint16_t addr);
void    cartridge_cpu_write(Cartridge* cart, uint16_t addr, uint8_t val);
uint8_t cartridge_ppu_read(Cartridge* cart, uint16_t addr);
void    cartridge_ppu_write(Cartridge* cart, uint16_t addr, uint8_t val);

#endif
```

### 1.4 Mapper Interface

```c
// src/mapper/mapper.h

#ifndef MAPPER_H
#define MAPPER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct Cartridge Cartridge;

typedef struct Mapper {
    Cartridge* cart;
    
    // Function pointers for mapper-specific behavior
    uint8_t (*cpu_read)(struct Mapper* m, uint16_t addr);
    void    (*cpu_write)(struct Mapper* m, uint16_t addr, uint8_t val);
    uint8_t (*ppu_read)(struct Mapper* m, uint16_t addr);
    void    (*ppu_write)(struct Mapper* m, uint16_t addr, uint8_t val);
    
    // Optional hooks
    void    (*reset)(struct Mapper* m);
    void    (*scanline)(struct Mapper* m);  // For MMC3 IRQ
    bool    (*irq_pending)(struct Mapper* m);
    void    (*irq_clear)(struct Mapper* m);
    
    // Mapper-specific state (union or void* for extensibility)
    void* state;
} Mapper;

// Factory function
Mapper* mapper_create(Cartridge* cart, uint8_t mapper_id);
void    mapper_destroy(Mapper* mapper);

// Mapper implementations (in separate files)
Mapper* mapper000_create(Cartridge* cart);  // NROM
Mapper* mapper001_create(Cartridge* cart);  // MMC1
Mapper* mapper002_create(Cartridge* cart);  // UxROM
Mapper* mapper003_create(Cartridge* cart);  // CNROM
Mapper* mapper004_create(Cartridge* cart);  // MMC3

#endif
```

**Deliverable:** Architecture compiles with stubs; no execution yet.

---

## Phase 2: CPU Primitives (Stack, Flags, Memory Helpers)

### 2.1 Stack Operations

6502 stack is at `$0100-$01FF`, indexed by 8-bit S register.

```c
// Push: write to stack, then decrement S
static inline void push8(CPU* cpu, uint8_t val) {
    bus_write(cpu->bus, 0x0100 | cpu->S, val);
    cpu->S--;
}

// Pop: increment S, then read from stack
static inline uint8_t pop8(CPU* cpu) {
    cpu->S++;
    return bus_read(cpu->bus, 0x0100 | cpu->S);
}

// Push 16-bit: high byte first, then low byte
static inline void push16(CPU* cpu, uint16_t val) {
    push8(cpu, (val >> 8) & 0xFF);  // High byte
    push8(cpu, val & 0xFF);          // Low byte
}

// Pop 16-bit: low byte first, then high byte
static inline uint16_t pop16(CPU* cpu) {
    uint16_t lo = pop8(cpu);
    uint16_t hi = pop8(cpu);
    return (hi << 8) | lo;
}
```

### 2.2 Flag Helpers

```c
static inline void set_flag(CPU* cpu, uint8_t flag, bool val) {
    if (val)
        cpu->P |= flag;
    else
        cpu->P &= ~flag;
}

static inline bool get_flag(CPU* cpu, uint8_t flag) {
    return (cpu->P & flag) != 0;
}

// Set Z and N flags based on value
static inline void set_ZN(CPU* cpu, uint8_t val) {
    set_flag(cpu, FLAG_Z, val == 0);
    set_flag(cpu, FLAG_N, val & 0x80);
}
```

### 2.3 Memory Read Helpers

```c
// Normal 16-bit read (little endian)
static inline uint16_t read16(Bus* bus, uint16_t addr) {
    uint8_t lo = bus_read(bus, addr);
    uint8_t hi = bus_read(bus, addr + 1);
    return (hi << 8) | lo;
}

// Zero-page 16-bit read with wrap (for indirect modes)
static inline uint16_t read16_zp(Bus* bus, uint8_t addr) {
    uint8_t lo = bus_read(bus, addr);
    uint8_t hi = bus_read(bus, (addr + 1) & 0xFF);  // Wrap within ZP
    return (hi << 8) | lo;
}

// JMP indirect bug: if low byte is $FF, high byte wraps within page
static inline uint16_t read16_jmp_bug(Bus* bus, uint16_t addr) {
    uint8_t lo = bus_read(bus, addr);
    // If addr is $xxFF, high byte comes from $xx00, not $xx00+$0100
    uint16_t hi_addr = (addr & 0xFF00) | ((addr + 1) & 0x00FF);
    uint8_t hi = bus_read(bus, hi_addr);
    return (hi << 8) | lo;
}
```

**Deliverable:** Unit tests pass for stack push/pop order, flag manipulation, and read16 variants.

---

## Phase 3: Addressing Modes

### 3.1 Addressing Mode Enum

```c
typedef enum {
    ADDR_IMP,    // Implied
    ADDR_ACC,    // Accumulator
    ADDR_IMM,    // Immediate
    ADDR_ZP,     // Zero Page
    ADDR_ZPX,    // Zero Page,X
    ADDR_ZPY,    // Zero Page,Y
    ADDR_ABS,    // Absolute
    ADDR_ABX,    // Absolute,X
    ADDR_ABY,    // Absolute,Y
    ADDR_IND,    // Indirect (JMP only)
    ADDR_IZX,    // Indexed Indirect (zp,X)
    ADDR_IZY,    // Indirect Indexed (zp),Y
    ADDR_REL,    // Relative (branches)
} AddrMode;
```

### 3.2 Addressing Mode Functions

Each function:
- Reads operand bytes from PC
- Advances PC appropriately
- Returns effective address
- Sets `page_crossed` flag where applicable

```c
typedef struct {
    uint16_t addr;
    bool page_crossed;
} AddrResult;

// Example: Absolute,X
static AddrResult addr_abx(CPU* cpu) {
    uint16_t base = read16(cpu->bus, cpu->PC);
    cpu->PC += 2;
    uint16_t addr = base + cpu->X;
    bool crossed = (base & 0xFF00) != (addr & 0xFF00);
    return (AddrResult){ addr, crossed };
}

// Example: Indirect Indexed (zp),Y
static AddrResult addr_izy(CPU* cpu) {
    uint8_t zp = bus_read(cpu->bus, cpu->PC++);
    uint16_t base = read16_zp(cpu->bus, zp);
    uint16_t addr = base + cpu->Y;
    bool crossed = (base & 0xFF00) != (addr & 0xFF00);
    return (AddrResult){ addr, crossed };
}
```

**Deliverable:** All 13 addressing modes implemented and tested.

---

## Phase 4: Opcode Table & Instruction Implementations

### 4.1 Opcode Table Structure

```c
typedef void (*OpcodeHandler)(CPU* cpu, AddrMode mode);

typedef struct {
    const char* mnemonic;
    OpcodeHandler handler;
    AddrMode mode;
    uint8_t cycles;
    bool page_cycle;   // Add cycle on page cross
} Opcode;

extern const Opcode opcode_table[256];
```

### 4.2 Instruction Implementation Strategy

Group instructions by operation type:

**Load/Store:** LDA, LDX, LDY, STA, STX, STY  
**Transfer:** TAX, TAY, TSX, TXA, TXS, TYA  
**Stack:** PHA, PHP, PLA, PLP  
**Arithmetic:** ADC, SBC  
**Logic:** AND, ORA, EOR  
**Shift/Rotate:** ASL, LSR, ROL, ROR  
**Inc/Dec:** INC, DEC, INX, INY, DEX, DEY  
**Compare:** CMP, CPX, CPY, BIT  
**Branch:** BCC, BCS, BEQ, BMI, BNE, BPL, BVC, BVS  
**Jump:** JMP, JSR, RTS, RTI  
**Flag:** CLC, CLD, CLI, CLV, SEC, SED, SEI  
**System:** BRK, NOP  
**Illegal:** LAX, SAX, DCP, ISC, SLO, RLA, SRE, RRA, ANC, ALR, ARR, AXS, etc.

### 4.3 Critical Correctness Points

#### ADC (Add with Carry)
```c
void op_adc(CPU* cpu, uint8_t val) {
    uint16_t sum = cpu->A + val + (get_flag(cpu, FLAG_C) ? 1 : 0);
    
    set_flag(cpu, FLAG_C, sum > 0xFF);
    set_flag(cpu, FLAG_V, (~(cpu->A ^ val) & (cpu->A ^ sum)) & 0x80);
    
    cpu->A = sum & 0xFF;
    set_ZN(cpu, cpu->A);
}
```

#### SBC (Subtract with Carry)
```c
void op_sbc(CPU* cpu, uint8_t val) {
    // SBC is ADC with inverted operand
    op_adc(cpu, ~val);
}
```

#### Branch Instructions
```c
void op_branch(CPU* cpu, bool condition) {
    int8_t offset = (int8_t)bus_read(cpu->bus, cpu->PC++);
    
    if (condition) {
        cpu->cycles++;  // +1 for taken branch
        
        uint16_t new_pc = cpu->PC + offset;
        if ((cpu->PC & 0xFF00) != (new_pc & 0xFF00)) {
            cpu->cycles++;  // +1 for page cross
        }
        cpu->PC = new_pc;
    }
}
```

#### JSR (Jump to Subroutine)
```c
void op_jsr(CPU* cpu) {
    uint16_t addr = read16(cpu->bus, cpu->PC);
    cpu->PC++;  // Point to last byte of JSR instruction
    push16(cpu, cpu->PC);  // Push return address - 1
    cpu->PC = addr;
}
```

#### BRK (Break)
```c
void op_brk(CPU* cpu) {
    cpu->PC++;  // BRK has padding byte
    push16(cpu, cpu->PC);
    push8(cpu, cpu->P | FLAG_B | FLAG_U);  // B and U set when pushing
    set_flag(cpu, FLAG_I, true);
    cpu->PC = read16(cpu->bus, 0xFFFE);
}
```

### 4.4 Complete Opcode Table

All 256 entries filled. Official opcodes fully implemented. Illegal opcodes implemented for nestest coverage.

**Deliverable:** `cpu_step()` executes any opcode correctly with proper cycle counts.

---

## Phase 5: PPU Timing Model (for nestest trace matching)

### 5.1 PPU Timing Basics

- PPU runs at 3× CPU clock speed
- 341 dots per scanline, 262 scanlines per frame
- nestest.log shows `PPU: scanline, dot` format

### 5.2 Minimal PPU Structure

```c
// src/ppu/ppu.h

typedef struct PPU {
    int scanline;   // 0-261 (0-239 visible, 240 post-render, 241-260 vblank, 261 pre-render)
    int dot;        // 0-340
    uint64_t frame;
    
    // Registers (stubbed for nestest, full impl later)
    uint8_t ctrl;      // $2000
    uint8_t mask;      // $2001
    uint8_t status;    // $2002
    uint8_t oam_addr;  // $2003
    // ... etc
} PPU;

void ppu_init(PPU* ppu);
void ppu_tick(PPU* ppu);  // Advance by 1 dot
void ppu_reset(PPU* ppu);
```

### 5.3 PPU/CPU Synchronization

```c
// In bus.c
void bus_tick(Bus* bus, int cpu_cycles) {
    // 3 PPU cycles per CPU cycle
    for (int i = 0; i < cpu_cycles * 3; i++) {
        ppu_tick(bus->ppu);
    }
}
```

### 5.4 nestest Trace Initial PPU State

First line of nestest.log shows `PPU:  0, 21 CYC:7`

This means:
- Scanline 0, dot 21
- CPU cycle 7

So at start:
- CPU cycles = 7 (or track from reset)
- PPU has already run 21 dots (7 CPU cycles × 3 = 21 PPU dots)

**Deliverable:** PPU timing matches nestest.log columns exactly.

---

## Phase 6: nestest Runner & Trace Comparison

### 6.1 Trace Output Format

Match nestest.log format exactly:

```
C000  4C F5 C5  JMP $C5F5                       A:00 X:00 Y:00 P:24 SP:FD PPU:  0, 21 CYC:7
```

Columns:
- PC: 4 hex digits, uppercase
- Opcode bytes: 2-digit hex, space-separated, left-aligned in 9-char field
- Mnemonic + operand: left-aligned, specific formatting per mode
- Registers: `A:XX X:XX Y:XX P:XX SP:XX` format
- PPU: `PPU:%3d,%3d` format (right-aligned numbers)
- CYC: total CPU cycles

### 6.2 Disassembly Formatting

```c
void cpu_trace(CPU* cpu, char* buffer, size_t size) {
    uint16_t pc = cpu->PC;
    uint8_t opcode = bus_read(cpu->bus, pc);
    const Opcode* op = &opcode_table[opcode];
    
    // Format opcode bytes
    // Format mnemonic and operand based on addressing mode
    // Format registers
    // Format PPU state
    // Format cycle count
    
    snprintf(buffer, size, 
        "%04X  %-8s  %-31s  A:%02X X:%02X Y:%02X P:%02X SP:%02X PPU:%3d,%3d CYC:%llu",
        pc, opcode_bytes, disasm, 
        cpu->A, cpu->X, cpu->Y, cpu->P, cpu->S,
        cpu->bus->ppu->scanline, cpu->bus->ppu->dot,
        cpu->cycles);
}
```

### 6.3 Runner Program

```c
// test/nestest_runner.c

int main(int argc, char** argv) {
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
    
    // Set initial state per nestest expectations
    cpu.PC = 0xC000;
    cpu.P = 0x24;       // Interrupt disable set, unused bit set
    cpu.S = 0xFD;
    cpu.cycles = 7;     // nestest starts at cycle 7
    
    // PPU starts at scanline 0, dot 21 (7 * 3 = 21)
    ppu.scanline = 0;
    ppu.dot = 21;
    
    char trace_buf[256];
    
    // Run until we hit end condition or max instructions
    while (cpu.cycles < 30000) {  // Adjust as needed
        cpu_trace(&cpu, trace_buf, sizeof(trace_buf));
        printf("%s\n", trace_buf);
        
        int cycles = cpu_step(&cpu);
        bus_tick(&bus, cycles);
    }
    
    cartridge_free(&cart);
    return 0;
}
```

### 6.4 Makefile Targets

```makefile
nestest_runner: test/nestest_runner.c src/**/*.c
	$(CC) $(CFLAGS) -o $@ $^

nestest: nestest_runner
	./nestest_runner > nestest_out.log
	diff -u testdata/nestest.log nestest_out.log | head -50
	@echo "Diff complete. Empty output = PASS"

nestest-firstdiff: nestest_runner
	./nestest_runner > nestest_out.log
	@diff testdata/nestest.log nestest_out.log | head -5 || true
```

**Deliverable:** `make nestest` shows first mismatch or confirms pass.

---

## Phase 7: Mapper Implementations

### 7.1 Mapper 0 (NROM) — Required for nestest

```c
// src/mapper/mapper000.c

typedef struct {
    // No additional state needed for NROM
} Mapper000State;

static uint8_t nrom_cpu_read(Mapper* m, uint16_t addr) {
    Cartridge* cart = m->cart;
    
    if (addr >= 0x8000) {
        // Mirror if 16KB PRG
        uint32_t offset = (addr - 0x8000) % cart->prg_rom_size;
        return cart->prg_rom[offset];
    }
    if (addr >= 0x6000) {
        // PRG RAM
        return cart->prg_ram[addr - 0x6000];
    }
    return 0;  // Open bus
}

static void nrom_cpu_write(Mapper* m, uint16_t addr, uint8_t val) {
    Cartridge* cart = m->cart;
    
    if (addr >= 0x6000 && addr < 0x8000) {
        cart->prg_ram[addr - 0x6000] = val;
    }
    // Writes to ROM are ignored (or bus conflict, ignored for simplicity)
}
```

### 7.2 Mapper 1 (MMC1) — Second priority

Key features:
- Serial shift register (5 writes to load)
- Consecutive write detection (ignore if within 2 cycles)
- PRG/CHR bank modes

### 7.3 Mapper 4 (MMC3) — Scaffold hooks

Key features:
- PRG bank switching
- CHR bank switching
- Scanline counter IRQ (needs `mapper->scanline()` hook)

**Deliverable:** Mapper architecture supports all planned mappers; NROM fully working.

---

## Phase 8: Implementation Order & Checkpoints

### Milestone 1: Project Setup (nestest preparation) ✅ COMPLETE

#### Checkpoint 1.1: Project scaffolding
- [x] Create `src/` directory structure
- [x] Move existing `cpu/` code to `legacy/` for preservation
- [x] Create `CMakeLists.txt`
- [x] Create Makefile wrapper
- [x] Add `.clang-format`
- [x] Update `.gitignore`
- [x] Verify `cmake --build .` succeeds with stubs

#### Checkpoint 1.2: Test artifacts
- [x] Add `make fetch-nestest` target
- [x] Download and commit `nestest.log` to `testdata/`
- [x] Verify ROM downloads correctly

### Milestone 2: Core Architecture (compiles, no execution) ✅ COMPLETE

#### Checkpoint 2.1: Bus + RAM
- [x] Implement `Bus` struct with 2KB RAM
- [x] Implement RAM mirroring ($0000-$1FFF)
- [x] Stub PPU register reads/writes ($2000-$3FFF)
- [x] Stub cartridge interface ($4020-$FFFF)
- [x] Unit test: write/read RAM at mirrored addresses

#### Checkpoint 2.2: Cartridge + Mapper 0
- [x] Implement iNES header parser
- [x] Load PRG/CHR ROM into memory
- [x] Implement Mapper interface
- [x] Implement NROM (Mapper 0)
- [x] Unit test: load nestest.nes, verify PRG size

### Milestone 3: CPU Core (primitives working) ✅ COMPLETE

#### Checkpoint 3.1: CPU primitives
- [x] Refactor CPU into struct (salvage `getflag`/`setflag`)
- [x] Fix stack operations (8-bit S, push decrements)
- [x] Implement `read16`, `read16_zp`, `read16_jmp_bug`
- [x] Unit tests for all primitives

#### Checkpoint 3.2: Addressing modes
- [x] Implement all 13 addressing modes as functions
- [x] Return effective address + page-cross flag
- [x] Unit tests for each mode

### Milestone 4: Instruction Execution ✅ COMPLETE

#### Checkpoint 4.1: Opcode table
- [x] Create 256-entry opcode table structure
- [x] Implement table-driven `cpu_step()`
- [x] Populate table with official opcodes (fix from salvaged code)

#### Checkpoint 4.2: Official opcodes
- [x] All load/store ops (LDA, LDX, LDY, STA, STX, STY)
- [x] All transfer ops (TAX, TAY, TSX, TXA, TXS, TYA)
- [x] All stack ops (PHA, PHP, PLA, PLP)
- [x] All arithmetic (ADC, SBC)
- [x] All logic (AND, ORA, EOR)
- [x] All shifts (ASL, LSR, ROL, ROR)
- [x] All inc/dec (INC, DEC, INX, INY, DEX, DEY)
- [x] All compare (CMP, CPX, CPY, BIT)
- [x] All branches (BCC, BCS, BEQ, BMI, BNE, BPL, BVC, BVS)
- [x] All jumps (JMP, JSR, RTS, RTI)
- [x] All flag ops (CLC, CLD, CLI, CLV, SEC, SED, SEI)
- [x] System (BRK, NOP)

#### Checkpoint 4.3: Cycle accuracy
- [x] Base cycles correct for all opcodes
- [x] Page-cross penalties implemented
- [x] Branch taken/page-cross penalties

### Milestone 5: PPU Timing ✅ COMPLETE

#### Checkpoint 5.1: Minimal PPU
- [x] Implement PPU struct with scanline/dot
- [x] Implement `ppu_tick()` (341 dots × 262 scanlines)
- [x] Bus calls `ppu_tick()` 3× per CPU cycle
- [x] Verify timing matches nestest.log PPU columns

### Milestone 6: nestest Validation (MAJOR MILESTONE) ✅ COMPLETE

#### Checkpoint 6.1: Trace output
- [x] Implement `cpu_trace()` matching exact format
- [x] Disassembly formatting for all addressing modes
- [x] Memory access display (e.g., `STA $01 = 00`)

#### Checkpoint 6.2: nestest runner
- [x] Create `nestest_runner.c`
- [x] Initialize to nestest start state (PC=$C000, etc.)
- [x] Run until completion/max cycles
- [x] Output to stdout

#### Checkpoint 6.3: Trace-perfect official opcodes
- [x] `make nestest` runs successfully
- [x] Diff against nestest.log
- [x] Fix bugs until lines 1-5003 match exactly

#### Checkpoint 6.4: Illegal opcodes (for full log)
- [x] Implement NOP variants (Priority 1)
- [x] Implement LAX, SAX (Priority 2-3)
- [x] Implement DCP, ISB (Priority 4-5)
- [x] Implement SLO, RLA, SRE, RRA (Priority 6-9)
- [x] Implement ANC, ALR, ARR, AXS, illegal SBC (Priority 10)
- [x] Full nestest.log match (all 8991 lines) — **ZERO DIFF ACHIEVED**

### Milestone 7: Super Mario Bros (FINAL GOAL)

#### Checkpoint 7.1: PPU Rendering
- [ ] Background rendering (nametables, pattern tables)
- [ ] Attribute table handling
- [ ] Scrolling (coarse X/Y, fine X)
- [ ] Sprite rendering (OAM evaluation, 8-sprite limit)
- [ ] Sprite 0 hit detection
- [ ] Sprite priority

#### Checkpoint 7.2: PPU Registers
- [ ] $2000 PPUCTRL writes
- [ ] $2001 PPUMASK writes  
- [ ] $2002 PPUSTATUS reads (vblank clear, w reset)
- [ ] $2003/$2004 OAM access
- [ ] $2005/$2006 scroll/address double writes
- [ ] $2007 VRAM read/write

#### Checkpoint 7.3: NMI
- [ ] VBlank flag set at scanline 241, dot 1
- [ ] NMI triggered when enabled in $2000
- [ ] CPU handles NMI vector ($FFFA)

#### Checkpoint 7.4: SDL2 Display
- [ ] Install SDL2 (`brew install sdl2`)
- [ ] Create window (256×240, scaled 2-3×)
- [ ] Create texture for framebuffer
- [ ] Render PPU output to texture
- [ ] 60 FPS frame limiting

#### Checkpoint 7.5: Controller Input
- [ ] Implement Controller struct
- [ ] $4016 strobe write
- [ ] $4016 serial read
- [ ] Keyboard mapping (arrows, Z, X, Enter, Shift)

#### Checkpoint 7.6: Integration
- [ ] Main game loop
- [ ] Load Super Mario Bros ROM
- [ ] Title screen displays correctly
- [ ] Game responds to input
- [ ] First level playable!

### Milestone 8: Polish (post-SMB)

#### Checkpoint 8.1: Additional mappers
- [ ] MMC1 (Mapper 1) — Zelda, Metroid
- [ ] UxROM (Mapper 2) — Mega Man, Castlevania
- [ ] CNROM (Mapper 3) — Various
- [ ] MMC3 (Mapper 4) — SMB3, requires IRQ

#### Checkpoint 8.2: APU (optional for SMB but nice)
- [ ] Pulse channels (2)
- [ ] Triangle channel
- [ ] Noise channel
- [ ] DMC channel
- [ ] SDL2 audio output

---

## Phase 9: Super Mario Bros Milestone

### 9.1 Target Game
**Super Mario Bros (1985)** — The classic! Uses:
- Mapper 0 (NROM-256): 32KB PRG, 8KB CHR
- Horizontal mirroring
- No special hardware

### 9.2 Requirements Beyond nestest

| Component | Status After nestest | Work Needed for SMB |
|-----------|---------------------|---------------------|
| CPU | ✅ **COMPLETE** (trace-perfect, 8991/8991 lines) | None |
| Bus | ✅ Basic (RAM, PPU stub, cartridge) | Add full PPU register access |
| Cartridge | ✅ NROM (Mapper 0) | Already done |
| PPU Timing | ✅ Scanline/dot counter | Full rendering pipeline |
| PPU Rendering | ❌ None | **Major work** |
| Controller | ❌ None | Keyboard input via SDL2 |
| APU | ❌ None | Stub (can be silent initially) |
| Display | ❌ None | SDL2 window + texture |
| Game Loop | ❌ None | 60 FPS timing |

### 9.3 PPU Implementation (Required for SMB)

```c
// Full PPU state for rendering
typedef struct PPU {
    // Timing
    int scanline;
    int dot;
    uint64_t frame;
    
    // Memory
    uint8_t vram[2048];        // 2KB nametable RAM
    uint8_t palette[32];       // Palette RAM
    uint8_t oam[256];          // Sprite OAM
    uint8_t secondary_oam[32]; // Sprite evaluation
    
    // Registers
    uint8_t ctrl;       // $2000 PPUCTRL
    uint8_t mask;       // $2001 PPUMASK
    uint8_t status;     // $2002 PPUSTATUS
    uint8_t oam_addr;   // $2003 OAMADDR
    
    // Internal registers
    uint16_t v;         // Current VRAM address (15 bits)
    uint16_t t;         // Temporary VRAM address
    uint8_t x;          // Fine X scroll (3 bits)
    bool w;             // Write toggle
    uint8_t data_buffer; // $2007 read buffer
    
    // Rendering state
    uint8_t nt_byte;
    uint8_t at_byte;
    uint8_t bg_lo;
    uint8_t bg_hi;
    uint16_t bg_shift_lo;
    uint16_t bg_shift_hi;
    uint16_t at_shift_lo;
    uint16_t at_shift_hi;
    
    // Output
    uint32_t framebuffer[256 * 240];
    bool frame_ready;
    
    // NMI
    bool nmi_occurred;
    bool nmi_output;
} PPU;
```

### 9.4 SDL2 Integration

```c
// Main emulator structure
typedef struct NES {
    CPU cpu;
    PPU ppu;
    APU apu;  // Stubbed initially
    Bus bus;
    Cartridge cart;
    
    // SDL
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    
    // Timing
    uint64_t frame_start;
    bool running;
} NES;

// Game loop
void nes_run(NES* nes) {
    while (nes->running) {
        // Run one frame
        while (!nes->ppu.frame_ready) {
            int cycles = cpu_step(&nes->cpu);
            for (int i = 0; i < cycles * 3; i++) {
                ppu_tick(&nes->ppu);
            }
        }
        nes->ppu.frame_ready = false;
        
        // Render to screen
        SDL_UpdateTexture(nes->texture, NULL, 
                          nes->ppu.framebuffer, 256 * sizeof(uint32_t));
        SDL_RenderCopy(nes->renderer, nes->texture, NULL, NULL);
        SDL_RenderPresent(nes->renderer);
        
        // Handle input
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) nes->running = false;
            handle_input(nes, &event);
        }
        
        // Frame timing (~16.67ms for 60 FPS)
        frame_limit(nes);
    }
}
```

### 9.5 Controller Input

```c
// Controller state (directly memory-mapped at $4016/$4017)
typedef struct Controller {
    uint8_t buttons;     // Current button state
    uint8_t shift_reg;   // Shift register for serial read
    bool strobe;         // Strobe mode
} Controller;

// Button mapping
#define BTN_A      0x01
#define BTN_B      0x02
#define BTN_SELECT 0x04
#define BTN_START  0x08
#define BTN_UP     0x10
#define BTN_DOWN   0x20
#define BTN_LEFT   0x40
#define BTN_RIGHT  0x80

// Keyboard mapping (configurable)
// Default: Arrow keys, Z=A, X=B, Enter=Start, Shift=Select
```

### 9.6 SMB Checkpoint Tasks

- [ ] **PPU Background Rendering**
  - [ ] Nametable fetching
  - [ ] Attribute table handling
  - [ ] Pattern table access via mapper
  - [ ] Scrolling (coarse X/Y, fine X)
  
- [ ] **PPU Sprite Rendering**
  - [ ] OAM evaluation
  - [ ] 8 sprites per scanline limit
  - [ ] Sprite 0 hit detection
  - [ ] Sprite priority (front/back of background)
  
- [ ] **PPU Register Behavior**
  - [ ] $2000/$2001 writes
  - [ ] $2002 reads (clear vblank, reset w)
  - [ ] $2005/$2006 double writes
  - [ ] $2007 read/write with increment
  
- [ ] **NMI Timing**
  - [ ] VBlank flag set at scanline 241
  - [ ] NMI triggered if enabled in $2000
  
- [ ] **SDL2 Display**
  - [ ] Create window (256x240 scaled 2-3x)
  - [ ] Texture for framebuffer
  - [ ] 60 FPS frame limiting
  
- [ ] **Controller**
  - [ ] Keyboard input polling
  - [ ] $4016 strobe and serial read

### 9.7 Build with SDL2 (CMake)

```cmake
# CMakeLists.txt addition for SDL2
find_package(SDL2 REQUIRED)
target_link_libraries(nes_emulator SDL2::SDL2)
```

Install SDL2 on macOS:
```bash
brew install sdl2
```

---

## Open Questions

<!-- Questions to resolve during implementation -->

1. **Decimal mode behavior**: NES CPU ignores decimal flag in hardware. nestest.log was created with a specific emulator's behavior. Should we:
   - Ignore D flag entirely (NES-accurate)?
   - Implement D flag behavior (6502-accurate)?
   - Test against log to determine which matches?
   - **RECOMMENDATION**: Ignore D flag (NES-accurate). The flag bit exists but ADC/SBC always use binary mode.

2. **Cycle counting start point**: nestest.log starts at `CYC:7`. Is this:
   - Cycles consumed by reset sequence?
   - Arbitrary starting point?
   - **ANSWER**: This represents 7 CPU cycles elapsed before first instruction. Initialize `cpu.cycles = 7` at start.

3. ~~**Illegal opcode coverage**: How many lines of nestest.log require illegal opcodes?~~
   - **RESOLVED**: See "Illegal Opcode Implementation Priority" section below.

4. **PPU initial state**: First line shows `PPU:  0, 21`. Verify:
   - **ANSWER**: Scanline 0, dot 21. This is 7 CPU cycles × 3 PPU dots = 21. Initialize PPU to `scanline=0, dot=21`.

5. **Test ROM variants**: Are there multiple versions of nestest.nes?
   - Pin checksum if so
   - Document which version we target
   - **TODO**: Calculate and document SHA256 of downloaded ROM

---

## Resolved Questions

6. **Existing code disposition**: 
   - **DECISION**: Salvage what we can — the code is sentimental!
   - Keep `cpu/` directory structure, refactor in place
   - Preserve function signatures where possible, fix implementations
   - Move to `src/` structure but keep git history
   - See "Appendix E: Salvageable Code Analysis" for what to keep

7. **Build system**: 
   - **DECISION**: Use **CMake** — optimal for macOS development
   - Better IDE integration (Xcode, CLion, VS Code)
   - Easier dependency management for SDL2 (needed for Super Mario Bros)
   - Cross-platform if you ever want Linux/Windows builds
   - Keep a simple Makefile wrapper for convenience (`make` → `cmake --build`)

8. **Testing framework**: 
   - **DECISION**: Plain `assert()` — keep it simple
   - Add a lightweight test runner script to run all test executables
   - Use `assert()` with descriptive messages

9. **Code style**: 
   - **DECISION**: Clean, consistent C style:
   - **Naming**: `snake_case` for functions and variables
   - **Types**: `PascalCase` for structs/enums (e.g., `CPU`, `Bus`, `AddrMode`)
   - **Constants/Macros**: `UPPER_SNAKE_CASE`
   - **Brace style**: K&R (opening brace on same line)
   - **Max line length**: 100 characters
   - **Indentation**: 4 spaces (no tabs)
   - Apply `clang-format` with project config

10. **Next milestone after nestest**: 
    - **DECISION**: Run **Super Mario Bros** on macOS!
    - This requires:
      - Full PPU rendering (sprites, backgrounds, scrolling)
      - Controller input (keyboard/gamepad via SDL2)
      - Mapper 0 (NROM) — already planned
      - Basic APU (can be stubbed initially, but SMB needs DMC silence)
      - ~60 FPS game loop with proper timing
    - See "Phase 9: Super Mario Bros Milestone" below

---

## References

### Authoritative Sources
- nestest.nes: `http://www.qmtpro.com/~nes/misc/nestest.nes`
- nestest.log: `http://www.qmtpro.com/~nes/misc/nestest.log`
- nestest.txt: `http://www.qmtpro.com/~nes/misc/nestest.txt`

### 6502 Documentation
- 6502 instruction reference: http://www.obelisk.me.uk/6502/reference.html
- 6502 addressing modes: http://www.obelisk.me.uk/6502/addressing.html
- Overflow flag explained: http://www.righto.com/2012/12/the-6502-overflow-flag-explained.html

### NES Documentation
- NESDev Wiki: https://www.nesdev.org/wiki/
- CPU memory map: https://www.nesdev.org/wiki/CPU_memory_map
- PPU frame timing: https://www.nesdev.org/wiki/PPU_frame_timing
- Mapper list: https://www.nesdev.org/wiki/Mapper

### Reference Implementations
- TenES (clean academic): https://github.com/tenes
- FCEU: https://github.com/TASEmulators/fceux

---

## Appendix A: nestest.log Format Specification

```
PPPP  OO OO OO  MMM OOOOOOOOOOOOOOOOOOOOOOOO  A:AA X:XX Y:YY P:PP SP:SS PPU:LLL,DDD CYC:CCCCC
```

| Field | Width | Description |
|-------|-------|-------------|
| PPPP | 4 | PC in hex, uppercase |
| OO OO OO | 8 | Opcode bytes, space-separated |
| MMM | 3 | Mnemonic (uppercase) |
| OOOO... | 27 | Operand (variable format by mode) |
| A:AA | 4 | Accumulator |
| X:XX | 4 | X register |
| Y:YY | 4 | Y register |
| P:PP | 4 | Status flags |
| SP:SS | 5 | Stack pointer |
| PPU:LLL,DDD | ~11 | Scanline (3 digits), dot (3 digits) |
| CYC:CCCCC | ~10 | Total CPU cycles |

---

## Appendix B: Opcode Table (Official)

| Opcode | Mnemonic | Mode | Bytes | Cycles | +Page |
|--------|----------|------|-------|--------|-------|
| 00 | BRK | IMP | 1 | 7 | - |
| 01 | ORA | IZX | 2 | 6 | - |
| 05 | ORA | ZP | 2 | 3 | - |
| 06 | ASL | ZP | 2 | 5 | - |
| 08 | PHP | IMP | 1 | 3 | - |
| 09 | ORA | IMM | 2 | 2 | - |
| 0A | ASL | ACC | 1 | 2 | - |
| 0D | ORA | ABS | 3 | 4 | - |
| 0E | ASL | ABS | 3 | 6 | - |
| 10 | BPL | REL | 2 | 2* | - |
| ... | ... | ... | ... | ... | ... |

*(Full table in implementation)*

---

## Appendix C: Illegal Opcode Implementation Priority

Based on analysis of nestest.log:

### Key Statistics
- **Total instructions in nestest.log**: ~8991
- **First illegal opcode appears**: Line 5004
- **Official opcodes coverage**: Lines 1-5003 (100% of initial test portion)
- **Illegal opcodes in log**: ~197 instructions (2.2% of total)

**Strategy**: Implement all official opcodes first. This achieves 97.8% nestest coverage and validates the core CPU. Then add illegal opcodes incrementally.

### Priority 1: NOP Variants (First illegals at line 5004+)
These are the simplest—just consume cycles and advance PC.

| Opcode | Mnemonic | Mode | Bytes | Cycles |
|--------|----------|------|-------|--------|
| 04 | NOP | ZP | 2 | 3 |
| 14 | NOP | ZPX | 2 | 4 |
| 34 | NOP | ZPX | 2 | 4 |
| 44 | NOP | ZP | 2 | 3 |
| 54 | NOP | ZPX | 2 | 4 |
| 64 | NOP | ZP | 2 | 3 |
| 74 | NOP | ZPX | 2 | 4 |
| 80 | NOP | IMM | 2 | 2 |
| D4 | NOP | ZPX | 2 | 4 |
| F4 | NOP | ZPX | 2 | 4 |
| 0C | NOP | ABS | 3 | 4 |
| 1C | NOP | ABX | 3 | 4+ |
| 3C | NOP | ABX | 3 | 4+ |
| 5C | NOP | ABX | 3 | 4+ |
| 7C | NOP | ABX | 3 | 4+ |
| DC | NOP | ABX | 3 | 4+ |
| FC | NOP | ABX | 3 | 4+ |
| 1A | NOP | IMP | 1 | 2 |
| 3A | NOP | IMP | 1 | 2 |
| 5A | NOP | IMP | 1 | 2 |
| 7A | NOP | IMP | 1 | 2 |
| DA | NOP | IMP | 1 | 2 |
| FA | NOP | IMP | 1 | 2 |

### Priority 2: LAX (Load A and X)
`A = X = memory`

| Opcode | Mode | Bytes | Cycles |
|--------|------|-------|--------|
| A3 | IZX | 2 | 6 |
| A7 | ZP | 2 | 3 |
| AF | ABS | 3 | 4 |
| B3 | IZY | 2 | 5+ |
| B7 | ZPY | 2 | 4 |
| BF | ABY | 3 | 4+ |

### Priority 3: SAX (Store A AND X)
`memory = A & X`

| Opcode | Mode | Bytes | Cycles |
|--------|------|-------|--------|
| 83 | IZX | 2 | 6 |
| 87 | ZP | 2 | 3 |
| 8F | ABS | 3 | 4 |
| 97 | ZPY | 2 | 4 |

### Priority 4: DCP (Decrement + Compare)
`memory--; CMP(memory)`

| Opcode | Mode | Bytes | Cycles |
|--------|------|-------|--------|
| C3 | IZX | 2 | 8 |
| C7 | ZP | 2 | 5 |
| CF | ABS | 3 | 6 |
| D3 | IZY | 2 | 8 |
| D7 | ZPX | 2 | 6 |
| DB | ABY | 3 | 7 |
| DF | ABX | 3 | 7 |

### Priority 5: ISB/ISC (Increment + SBC)
`memory++; SBC(memory)`

| Opcode | Mode | Bytes | Cycles |
|--------|------|-------|--------|
| E3 | IZX | 2 | 8 |
| E7 | ZP | 2 | 5 |
| EF | ABS | 3 | 6 |
| F3 | IZY | 2 | 8 |
| F7 | ZPX | 2 | 6 |
| FB | ABY | 3 | 7 |
| FF | ABX | 3 | 7 |

### Priority 6: SLO (ASL + ORA)
`ASL memory; ORA(memory)`

| Opcode | Mode | Bytes | Cycles |
|--------|------|-------|--------|
| 03 | IZX | 2 | 8 |
| 07 | ZP | 2 | 5 |
| 0F | ABS | 3 | 6 |
| 13 | IZY | 2 | 8 |
| 17 | ZPX | 2 | 6 |
| 1B | ABY | 3 | 7 |
| 1F | ABX | 3 | 7 |

### Priority 7: RLA (ROL + AND)
`ROL memory; AND(memory)`

| Opcode | Mode | Bytes | Cycles |
|--------|------|-------|--------|
| 23 | IZX | 2 | 8 |
| 27 | ZP | 2 | 5 |
| 2F | ABS | 3 | 6 |
| 33 | IZY | 2 | 8 |
| 37 | ZPX | 2 | 6 |
| 3B | ABY | 3 | 7 |
| 3F | ABX | 3 | 7 |

### Priority 8: SRE (LSR + EOR)
`LSR memory; EOR(memory)`

| Opcode | Mode | Bytes | Cycles |
|--------|------|-------|--------|
| 43 | IZX | 2 | 8 |
| 47 | ZP | 2 | 5 |
| 4F | ABS | 3 | 6 |
| 53 | IZY | 2 | 8 |
| 57 | ZPX | 2 | 6 |
| 5B | ABY | 3 | 7 |
| 5F | ABX | 3 | 7 |

### Priority 9: RRA (ROR + ADC)
`ROR memory; ADC(memory)`

| Opcode | Mode | Bytes | Cycles |
|--------|------|-------|--------|
| 63 | IZX | 2 | 8 |
| 67 | ZP | 2 | 5 |
| 6F | ABS | 3 | 6 |
| 73 | IZY | 2 | 8 |
| 77 | ZPX | 2 | 6 |
| 7B | ABY | 3 | 7 |
| 7F | ABX | 3 | 7 |

### Priority 10: Other Illegals
| Opcode | Mnemonic | Description |
|--------|----------|-------------|
| 0B, 2B | ANC | AND + copy N to C |
| 4B | ALR | AND + LSR |
| 6B | ARR | AND + ROR (weird V/C) |
| CB | AXS | (A & X) - imm → X |
| EB | SBC | Same as E9 (official SBC) |
| 8B | XAA | Unstable, (A & X & imm) |
| 93, 9F | AHX | Unstable store |
| 9B | TAS | Unstable |
| 9C | SHY | Unstable |
| 9E | SHX | Unstable |
| BB | LAS | memory & S → A, X, S |
| 02,12,22,32,42,52,62,72,92,B2,D2,F2 | STP/KIL | Halt CPU |

---

## Appendix D: Exact nestest.log Format (Verified)

From actual file analysis, the precise format is:

```
C000  4C F5 C5  JMP $C5F5                       A:00 X:00 Y:00 P:24 SP:FD PPU:  0, 21 CYC:7
C5F5  A2 00     LDX #$00                        A:00 X:00 Y:00 P:24 SP:FD PPU:  0, 30 CYC:10
```

### Column Breakdown (character positions)

| Column | Start | Width | Content | Notes |
|--------|-------|-------|---------|-------|
| PC | 0 | 4 | `C000` | Uppercase hex |
| (space) | 4 | 2 | `  ` | Two spaces |
| Opcode bytes | 6 | 8 | `4C F5 C5` | Left-aligned, space-padded |
| (space) | 14 | 2 | `  ` | Separator |
| Mnemonic | 16 | 4 | `JMP ` | Uppercase, space after |
| Operand | 20 | 27 | `$C5F5` + padding | Mode-specific format |
| (space) | 47 | 1 | ` ` | Separator |
| A | 48 | 4 | `A:00` | |
| (space) | 52 | 1 | ` ` | |
| X | 53 | 4 | `X:00` | |
| (space) | 57 | 1 | ` ` | |
| Y | 58 | 4 | `Y:00` | |
| (space) | 62 | 1 | ` ` | |
| P | 63 | 4 | `P:24` | |
| (space) | 67 | 1 | ` ` | |
| SP | 68 | 5 | `SP:FD` | |
| (space) | 73 | 1 | ` ` | |
| PPU | 74 | ~11 | `PPU:  0, 21` | Scanline right-aligned 3 chars |
| (space) | ~85 | 1 | ` ` | |
| CYC | ~86 | varies | `CYC:7` | |

### Operand Formatting by Addressing Mode

| Mode | Example | Format |
|------|---------|--------|
| Implied | `CLC` | (no operand) |
| Accumulator | `ASL A` | `A` |
| Immediate | `LDA #$00` | `#$XX` |
| Zero Page | `LDA $00` | `$XX` |
| Zero Page,X | `LDA $00,X` | `$XX,X` |
| Zero Page,Y | `LDX $00,Y` | `$XX,Y` |
| Absolute | `JMP $C5F5` | `$XXXX` |
| Absolute,X | `LDA $0200,X` | `$XXXX,X` |
| Absolute,Y | `LDA $0200,Y` | `$XXXX,Y` |
| Indirect | `JMP ($0200)` | `($XXXX)` |
| Indexed Indirect | `LDA ($00,X)` | `($XX,X)` |
| Indirect Indexed | `LDA ($00),Y` | `($XX),Y` |
| Relative | `BNE $C010` | `$XXXX` (target address) |

### Memory Access Display
For instructions that read/write memory, nestest.log shows the value:
- `STA $01 = 00` — shows current value at address before write
- `LDA $0200 = 4C` — shows value being loaded

### Initial State (Line 1)
```
PC=C000  A=00  X=00  Y=00  P=24  SP=FD  PPU: scanline=0 dot=21  CYC=7
```

---

*Last updated: Milestone 6 COMPLETE — nestest passes with zero diff (8991/8991 lines)*

---

## Appendix E: Salvageable Code Analysis

Analysis of existing `cpu/` code to determine what can be preserved.

### cpu/cpu.h — Partially Salvageable

| Element | Status | Notes |
|---------|--------|-------|
| `#include` guards | ✅ Keep | Standard practice |
| Memory constants (`STACK`, `PRG_ROM`, etc.) | ✅ Keep | Correct values |
| Vector addresses (`NMI_VECTOR`, etc.) | ✅ Keep | Correct |
| Global variables (`memory`, `sp`, `pc`, etc.) | ❌ Refactor | Move into CPU/Bus structs |
| `enum program_flag` | ⚠️ Rework | Change to bitmask constants |
| Function declarations | ✅ Keep signatures | Implementations need fixing |
| Addressing mode macros | ❌ Replace | Buggy syntax, replace with functions |

**Plan**: Keep the file structure and constants, refactor globals into structs.

### cpu/cpu.c — Partially Salvageable

| Function | Status | Notes |
|----------|--------|-------|
| `initialize_cpu()` | ⚠️ Fix | Wrong SP init (should be 0xFD), needs struct |
| `deinitialize_cpu()` | ✅ Keep | Just free(), works fine |
| `read8()` | ⚠️ Refactor | Move to bus.c with address decoding |
| `write()` | ⚠️ Refactor | Move to bus.c with address decoding |
| `print_value()` | ✅ Keep | Debug helper, works fine |
| `push_stack8()` | ❌ Fix | Wrong direction (should decrement) |
| `push_stack16()` | ❌ Fix | Wrong direction + byte order |
| `pop_stack8()` | ❌ Fix | Wrong direction |
| `pop_stack16()` | ❌ Fix | Wrong direction + byte order |
| `highbit()` | ⚠️ Rework | Works but usage is wrong elsewhere |
| `getflag()` | ✅ Keep | Logic is correct |
| `setflag()` | ✅ Keep | Logic is correct (nice SO credit!) |
| `perform_instruction()` | ❌ Replace | Incomplete, use table-driven |

**Plan**: Preserve `getflag`/`setflag` (they work!), fix stack functions, refactor memory access.

### cpu/opcodes.h — Mostly Salvageable

| Element | Status | Notes |
|---------|--------|-------|
| `enum address_mode` | ✅ Keep | Good enum definition |
| `struct instruction` | ✅ Keep | Good structure, add handler pointer |
| `instruction_set[256]` extern | ⚠️ Populate | Declared but never defined |
| Function declarations | ⚠️ Fix signatures | Many have wrong parameter types |

**Plan**: Keep structure definitions, fix function signatures to use CPU* parameter.

### cpu/opcodes.c — Partially Salvageable

| Function | Status | Notes |
|----------|--------|-------|
| `ADC()` | ⚠️ Fix | Decimal mode logic, flag setting |
| `AND()` | ❌ Fix | Z flag logic wrong (`result & highbit` should be `!result`) |
| `ASL()` | ✅ Keep | Looks correct |
| `BCC/BCS/BEQ/BMI/BNE/BPL/BVC/BVS` | ⚠️ Fix | Branch() helper needs cycle fix |
| `BIT()` | ❌ Fix | Z flag logic inverted |
| `BRK()` | ❌ Fix | Stack order, P bits wrong |
| `CLC/CLD/CLI/CLV` | ✅ Keep | Simple flag clears, work |
| `CMP/CPX/CPY` | ⚠️ Fix | Z flag logic |
| `DEC/DEX/DEY` | ✅ Keep | Look correct |
| `EOR` | ✅ Keep | Looks correct |
| `INC` | ❌ Fix | Writes wrong value (`value` not `val`) |
| `INX/INY` | ✅ Keep | Look correct |
| `JMP` | ✅ Keep | Simple, correct |
| `JSR` | ⚠️ Fix | Needs PC adjustment verification |
| `LDA/LDX/LDY` | ✅ Keep | Look correct |
| `LSR` | ✅ Keep | Looks correct |
| `NOP` | ✅ Keep | Does nothing, correctly! |
| `ORA` | ✅ Keep | Looks correct |
| `PHA/PHP` | ⚠️ Fix | Depends on stack fix |
| `PLA/PLP` | ⚠️ Fix | Depends on stack fix |
| `ROL/ROR` | ⚠️ Verify | Logic looks okay |
| `RTI/RTS` | ⚠️ Fix | Stack order |
| `SBC` | ⚠️ Fix | Flag logic |
| `SEC/SED/SEI` | ✅ Keep | Simple flag sets, work |
| `STA/STX/STY` | ✅ Keep | Simple stores |
| `TAX/TAY/TSX/TXA/TXS/TYA` | ✅ Keep | Transfer ops look correct |

**Preservation Score**: ~40% directly usable, ~40% fixable, ~20% needs rewrite

### test/test_cpu.c — Salvageable Structure

| Element | Status | Notes |
|---------|--------|-------|
| Test structure | ✅ Keep | Good organization with setup/teardown |
| `test_addresses()` | ⚠️ Update | Tests for old macros, rewrite for new functions |
| `test_stack()` | ❌ Fix | Expectations are wrong (match wrong impl) |
| `test_bitman()` | ✅ Keep | Tests correct behavior |
| `test_opcodes()` | ⚠️ Expand | Good start, needs more coverage |

**Plan**: Keep test structure, fix expectations to match correct 6502 behavior.

### Other Files

| File | Status | Notes |
|------|--------|-------|
| `opcode_generator.py` | ❌ Replace | Generates incomplete switch, use table instead |
| `opcode` | 📚 Reference | Data file, useful as reference for opcode info |
| `Notes` | ✅ Keep | Historical notes, good context |
| `Makefile` | ⚠️ Migrate | Migrate to CMake, keep for reference |

### Sentimental Preservation Strategy

1. **Create a `legacy/` directory** for original code before major refactoring
2. **Git history**: All original code preserved in git history
3. **Inline credits**: Add comments like `// Original implementation preserved from v0.1`
4. **Keep the spirit**: Maintain similar function names and structure where sensible

### Files to Create Fresh

These don't exist and need to be created:
- `src/bus/bus.c` — New bus abstraction
- `src/cartridge/ines.c` — iNES loader
- `src/mapper/mapper*.c` — Mapper implementations
- `src/ppu/ppu.c` — PPU implementation
- `test/nestest_runner.c` — Test harness
- `CMakeLists.txt` — Build system

---

## Appendix F: CMake Build Configuration

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(nes_emulator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# Source files
set(CPU_SOURCES
    src/cpu/cpu.c
    src/cpu/opcodes.c
)

set(BUS_SOURCES
    src/bus/bus.c
)

set(CARTRIDGE_SOURCES
    src/cartridge/cartridge.c
    src/cartridge/ines.c
)

set(MAPPER_SOURCES
    src/mapper/mapper.c
    src/mapper/mapper000.c
    src/mapper/mapper001.c
)

set(PPU_SOURCES
    src/ppu/ppu.c
)

# Core library (for testing)
add_library(nes_core STATIC
    ${CPU_SOURCES}
    ${BUS_SOURCES}
    ${CARTRIDGE_SOURCES}
    ${MAPPER_SOURCES}
    ${PPU_SOURCES}
)
target_include_directories(nes_core PUBLIC src)

# nestest runner (CPU validation)
add_executable(nestest_runner test/nestest_runner.c)
target_link_libraries(nestest_runner nes_core)

# Unit tests
add_executable(test_cpu test/test_cpu.c)
target_link_libraries(test_cpu nes_core)

# Main emulator (with SDL2 for Super Mario Bros)
find_package(SDL2 QUIET)
if(SDL2_FOUND)
    add_executable(nes_emulator src/main.c)
    target_link_libraries(nes_emulator nes_core SDL2::SDL2)
    message(STATUS "SDL2 found - building full emulator")
else()
    message(STATUS "SDL2 not found - skipping full emulator build")
    message(STATUS "Install with: brew install sdl2")
endif()

# Custom targets
add_custom_target(fetch-nestest
    COMMAND mkdir -p ${CMAKE_SOURCE_DIR}/roms/test
    COMMAND curl -L -o ${CMAKE_SOURCE_DIR}/roms/test/nestest.nes 
            http://www.qmtpro.com/~nes/misc/nestest.nes
    COMMENT "Downloading nestest.nes"
)

add_custom_target(nestest
    COMMAND $<TARGET_FILE:nestest_runner> > nestest_out.log
    COMMAND diff -u ${CMAKE_SOURCE_DIR}/testdata/nestest.log nestest_out.log | head -50
    DEPENDS nestest_runner
    COMMENT "Running nestest validation"
)

add_custom_target(test
    COMMAND $<TARGET_FILE:test_cpu>
    DEPENDS test_cpu
    COMMENT "Running unit tests"
)
```

### Makefile Wrapper (for convenience)

```makefile
# Makefile - Wrapper for CMake
BUILD_DIR = build

.PHONY: all clean test nestest fetch-nestest

all:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. && cmake --build .

clean:
	@rm -rf $(BUILD_DIR)

test: all
	@cd $(BUILD_DIR) && cmake --build . --target test

nestest: all
	@cd $(BUILD_DIR) && cmake --build . --target nestest

fetch-nestest:
	@cd $(BUILD_DIR) && cmake --build . --target fetch-nestest

debug:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug .. && cmake --build .

release:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build .
```

---

## Appendix G: Code Style Reference

### .clang-format

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
UseTab: Never
BreakBeforeBraces: Attach
AllowShortFunctionsOnASingleLine: None
AllowShortIfStatementsOnASingleLine: false
AllowShortLoopsOnASingleLine: false
ColumnLimit: 100
PointerAlignment: Left
SpaceAfterCStyleCast: false
IncludeBlocks: Regroup
```

### Example Code Style

```c
// src/cpu/cpu.c

#include "cpu.h"
#include "../bus/bus.h"

// Constants use UPPER_SNAKE_CASE
#define FLAG_C 0x01
#define FLAG_Z 0x02

// Structs use PascalCase
typedef struct CPU {
    uint8_t a;      // Registers use lowercase
    uint8_t x;
    uint8_t y;
    uint8_t p;
    uint8_t s;
    uint16_t pc;
    uint64_t cycles;
    Bus* bus;       // Pointer to bus
} CPU;

// Functions use snake_case
void cpu_init(CPU* cpu, Bus* bus) {
    cpu->a = 0;
    cpu->x = 0;
    cpu->y = 0;
    cpu->p = FLAG_U | FLAG_I;  // Unused always set, IRQ disabled
    cpu->s = 0xFD;
    cpu->pc = 0;
    cpu->cycles = 0;
    cpu->bus = bus;
}

// Static helpers are also snake_case
static inline void set_flag(CPU* cpu, uint8_t flag, bool value) {
    if (value) {
        cpu->p |= flag;
    } else {
        cpu->p &= ~flag;
    }
}

static inline bool get_flag(CPU* cpu, uint8_t flag) {
    return (cpu->p & flag) != 0;
}

// K&R brace style, 4-space indent
int cpu_step(CPU* cpu) {
    uint8_t opcode = bus_read(cpu->bus, cpu->pc++);
    const Opcode* op = &opcode_table[opcode];
    
    // Execute instruction
    int cycles = op->cycles;
    op->handler(cpu, op->mode);
    
    cpu->cycles += cycles;
    return cycles;
}
```
