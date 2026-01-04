# Unit Test Implementation Plan (NES_emulator)

This plan expands unit test coverage beyond the current CPU-focused tests (`test/test_cpu.c`) and the CPU correctness harness (`test/nestest_runner.c`).

Constraints / assumptions (based on repo):
- Language: C
- Current style: plain `assert()` + printing `PASS` per test
- Build entrypoints:
  - `make test` runs `build/test_cpu`
  - `make nestest` runs `build/nestest_runner` and diffs output
- Goal: add targeted unit tests for **Bus**, **Cartridge**, **Mapper**, **PPU**, **APU** (and minimal integration smoke tests where unit isolation is unrealistic).

---

## 0) Current coverage snapshot

### Existing tests
- **CPU primitives and helpers** (in `test/test_cpu.c`):
  - `cpu_init`
  - stack helpers (`cpu_push8/16`, `cpu_pop8/16`)
  - flag helpers (`cpu_set_flag`, `cpu_get_flag`)
  - 16-bit read helper variants (`cpu_read16`, `cpu_read16_zp`, `cpu_read16_jmp_bug`)
- **Bus RAM mirroring** (in `test/test_cpu.c`):
  - `bus_read`, `bus_write` on `$0000-$1FFF`
- **Cartridge load “happy path”** (in `test/test_cpu.c`):
  - `cartridge_load` against `roms/test/nestest.nes` (skips if missing)
  - basic mapper presence + a single `cartridge_cpu_read`
- **nestest runner** validates CPU instruction trace end-to-end (not a unit test, but a strong correctness harness).

### Gaps
- **Bus**: controller shift behavior, PPU register access routing, OAM DMA trigger behavior, APU register routing, `bus_tick` behavior.
- **Cartridge**: invalid headers, CHR RAM vs CHR ROM behavior, trainer skip behavior, `cartridge_free` correctness, CPU writes delegation.
- **Mapper**: mapper 0 behavior beyond a single read, CHR RAM writes, PRG RAM reads/writes, edge-case mirroring.
- **PPU**: register semantics (especially $2002/$2005/$2006/$2007), VRAM/palette mirroring, vblank/NMI timing, OAM DMA copy.
- **APU**: register writes configuration, status register behavior ($4015), tick-driven envelope/length counters (at least sanity checks), sample/buffer output behavior.

---

## 1) Test architecture & organization

### 1.1 Add new standalone test executables

Keep tests small and focused by module:

- `test/test_bus.c`
- `test/test_cartridge.c`
- `test/test_mapper.c`
- `test/test_ppu.c`
- `test/test_apu.c`

Rationale:
- Avoid a single giant `test_cpu.c` becoming a dumping ground.
- Keep failure localization crisp.

### 1.2 Shared test utilities (minimal)

Add a tiny internal helper header for tests only:
- `test/test_helpers.h`

Contents (small, no frameworks):
- `ASSERT_EQ_U8(expected, actual)` etc. (optional)
- helper to create temp files for ROM headers (for cartridge tests)

Note: avoid over-engineering. Keep it to what’s needed to test `cartridge_load` without requiring external ROMs.

### 1.3 Build integration

Update build system to compile/run all test executables:
- **Makefile**:
  - add objects/targets for each `test/test_*.c`
  - create a `make test` target that runs all unit test binaries (or a `make test-all` and keep `test` backward-compatible)
- **CMakeLists.txt**:
  - add `add_executable(test_bus ...)`, etc.
  - update `run_tests` target to run all of them

(Implementation detail: keep the plan consistent with the existing pattern: one binary per test file, run from build dir.)

---

## 2) Bus tests (`test/test_bus.c`)

### 2.1 RAM mirroring (already exists)
- Keep the existing tests in `test/test_cpu.c` OR move them to `test/test_bus.c`.
- Ensure `$0000-$07FF` mirrors through `$1FFF`.

### 2.2 PPU register routing `$2000-$3FFF`
Goal: ensure bus forwards reads/writes to `ppu_read_register`/`ppu_write_register` when `bus->ppu != NULL`.

Approach:
- Create a minimal `PPU` instance with known initial state.
- Write via `bus_write(bus, 0x2000, value)` and assert `ppu.ctrl` updated.
- Read via `bus_read(bus, 0x2002)` and validate expected side effects (vblank clear + `w=false`), but keep deep semantics in PPU tests.

### 2.3 Controller shift register behavior ($4016/$4017)
Test cases:
1. **Strobe high** (`bus_write 0x4016, 1`): reads should always return LSB of `bus->controller[n]` (no shifting).
2. **Strobe falling edge** (`1 -> 0`): `controller_state[]` latched from `controller[]`.
3. **Shift when strobe low**: sequential reads shift right and fill with 1s (`| 0x80` behavior in code).

Assertions:
- Returned value includes bit0 and the constant `0x40` bit set.

### 2.4 APU routing ($4000-$4017 excluding $4014/$4016)
Goal: verify bus forwards writes/reads to APU when present.

Approach:
- Instantiate an `APU` and attach to `bus->apu`.
- Write to `0x4015` and verify channel enable toggles in `apu` struct.
- Read from `0x4015` and check status bits reflect current counters.

### 2.5 `bus_tick` fanout (PPU 3×, APU 1×)
Goal: validate that bus tick calls `ppu_tick` 3× per CPU cycle and `apu_tick` 1×.

Because `ppu_tick`/`apu_tick` are concrete functions, easiest validation is **state change**:
- PPU: start from known `dot/scanline` and check after `bus_tick(bus, 1)` dot advanced by 3 (or the equivalent across scanline boundaries).
- APU: start from `frame_count=0` and check after `bus_tick(bus, N)` frame_count incremented by N.

---

## 3) Cartridge tests (`test/test_cartridge.c`)

### 3.1 ROM-independent tests using generated iNES files
Avoid relying on `roms/test/nestest.nes` by writing small synthetic ROMs in the test.

Test helper should create a temporary file containing:
- valid iNES header
- PRG ROM bytes
- optional CHR ROM bytes

Test cases:
1. **Reject invalid magic** (`'N''E''S'0x1A` mismatch)
2. **PRG size math**: header PRG banks `N` → `prg_rom_size == N * 16384`
3. **CHR ROM present**: CHR banks `M>0` → `chr_rom != NULL`, `chr_ram == NULL`, `chr_rom_size == M*8192`
4. **CHR RAM fallback**: CHR banks `0` → `chr_rom == NULL`, `chr_ram != NULL`, and CHR RAM is zeroed
5. **Trainer skip**: flags6 bit2 set → loader skips 512 bytes; verify by placing a canary byte at start of PRG and ensuring it’s read correctly
6. **Mapper id extraction**: verify `(flags7 & 0xF0) | (flags6 >> 4)` behavior for a couple of values

### 3.2 `cartridge_free` correctness
- After free: all pointers NULL/zeroed (code does `memset(cart,0,...)`).
- Ensure does not crash if called on a partially-initialized cartridge (e.g., only `prg_rom` allocated).

### 3.3 Delegation tests
Using mapper 0 only (since `mapper_create` supports just 0 today):
- Verify `cartridge_cpu_read` matches NROM PRG mapping (`$8000+` pulls from PRG)
- Verify `cartridge_cpu_write` writes PRG RAM region (`$6000-$7FFF`) and can be read back

---

## 4) Mapper tests (`test/test_mapper.c`)

(Mapper is currently embedded in `src/mapper/mapper.c` and only supports mapper 0.)

### 4.1 mapper_create returns expected values
- `mapper_create(cart, 0)` returns non-NULL
- `mapper_create(cart, unknown)` returns NULL

### 4.2 NROM PRG mapping
Given a cartridge with known PRG ROM pattern:
- For 16KB PRG, verify mirroring across `$8000-$BFFF` and `$C000-$FFFF` via modulo.
- For PRG RAM, verify `$6000-$7FFF` reads/writes.

### 4.3 CHR behavior
- CHR ROM: ensure `ppu_write` does not modify; reads return original.
- CHR RAM: ensure `ppu_write` stores and `ppu_read` returns.

### 4.4 mapper_destroy
- Ensure it safely frees `mapper` and `mapper->state`.

---

## 5) PPU tests (`test/test_ppu.c`)

PPU is complex; start with register-level semantics and memory mapping (deterministic), then move to timing.

### 5.1 Reset/init state
- `ppu_init` should zero everything
- `ppu_reset` should set registers/internal latches to expected documented values in code

### 5.2 VRAM mirroring (nametable mirroring)
Function under test is effectively `ppu_read/ppu_write` in combination with `ppu_mirror_vram_addr`.

Approach:
- Attach a `Cartridge` struct with `mirroring` set (0=horizontal, 1=vertical per this code)
- Write to `$2000/$2400/$2800/$2C00` and verify they mirror to expected underlying `ppu->vram[]` indices.

### 5.3 Palette mirroring
- Writes/reads for `$3F10/$3F14/$3F18/$3F1C` mirror down by `0x10`.

### 5.4 Register semantics
Target behaviors present in code:
- **$2002 read**:
  - returns `(status & 0xE0) | (data_buffer & 0x1F)`
  - clears vblank bit
  - resets `w=false`
- **$2000 write**:
  - updates `ctrl`, `nmi_output`, and nametable bits into `t`
  - sets `nmi_pending` when enabling NMI while in vblank
- **$2005 write**:
  - 1st write: coarse X + fine_x, sets `w=true`
  - 2nd write: coarse Y + fine Y, sets `w=false`
- **$2006 write**:
  - 1st write: upper bits of `t`, sets `w=true`
  - 2nd write: lower bits of `t`, copies to `v`, sets `w=false`
- **$2007 read/write**:
  - buffered reads for < `$3F00` and immediate reads for palette
  - increments `v` by 1 or 32 depending on ctrl increment bit

Write tests that:
- set initial `v/t/w/data_buffer` deterministically
- assert updated `v/t/w` after each register access

### 5.5 VBlank/NMI timing sanity
In `ppu_tick`:
- At `scanline==241 && dot==1` set vblank and maybe nmi_pending.
- At pre-render (`scanline==261 && dot==1`) clear vblank/sprite0/overflow.

Tests:
- Step PPU until those coordinates and assert status changes.

### 5.6 OAM DMA
- Call `ppu_oam_dma(ppu, page)` and assert `ppu->oam[0..255]` equals `page`.

---

## 6) APU tests (`test/test_apu.c`)

APU is also complex; focus on deterministic, easy-to-assert properties.

### 6.1 Init/reset
- `apu_init` zeros state and sets `noise.shift_register=1`
- sample timing configured: `audio_time_per_sample == 1/44100`

### 6.2 Register write decode
For a selection of registers, verify fields update:
- pulse1 volume register (`APU_PULSE1_VOL`): duty, envelope_loop, constant_volume, volume
- pulse1 hi register loads length counter when enabled
- status register (`APU_STATUS` / $4015): enable bits toggle channels and clearing counters when disabling

### 6.3 Status read ($4015)
Set counters manually and assert returned status bits reflect the struct:
- pulse1 length_counter → bit0
- pulse2 length_counter → bit1
- triangle length_counter → bit2
- noise length_counter → bit3
- dmc bytes_remaining → bit4
- frame_irq → bit6 (and cleared on read)

### 6.4 Tick-driven invariants (sanity tests)
Not aiming for audio-perfect output yet, but confirm:
- `apu_tick` increments `frame_count`
- With channels enabled + configured, after enough ticks `apu_get_sample` returns a finite value (not NaN), and buffer is produced via `apu_get_buffer`.

(Keep tolerances loose; don’t bake in exact waveforms unless you introduce a reference.)

---

## 7) Minimal integration tests (optional)

Some behaviors are cross-module by nature. Add one or two smoke tests if useful:

- `test/test_integration_tick.c` (optional):
  - Construct Bus + PPU + APU, call `bus_tick` in a loop, assert:
    - PPU frame eventually becomes ready (`frame_ready==true`)
    - APU sample buffer accumulates (>0)

This is not a substitute for unit tests; it’s a regression catch.

---

## 8) Rollout order (prioritized)

1. **Bus controller + routing tests** (high ROI, small)
2. **Cartridge ROM-independent loader tests** (removes dependence on external files)
3. **Mapper 0 tests** (memory mapping correctness)
4. **PPU register semantics + mirroring** (moderate effort, high value)
5. **APU init/register/status tests** (moderate)
6. Optional integration smoke tests

---

## 9) Definition of done

- Each module has at least one dedicated test file/binary.
- `make test` runs all unit test binaries and reports failures clearly.
- Tests do not require SDL2.
- Tests do not require `nestest.nes` (except the existing optional/skip test).
- CI-friendly: deterministic, no network fetch, no reliance on local audio/video.
