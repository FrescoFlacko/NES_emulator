# Troubleshooting

Common issues and solutions when building or running the NES emulator.

## Build Issues

### SDL2 not found

**Error:**
```
SDL2 not found - skipping full emulator build
```

or

```
fatal error: 'SDL.h' file not found
```

**Solution:**

Install SDL2:

macOS:
```bash
brew install sdl2
```

Ubuntu/Debian:
```bash
sudo apt install libsdl2-dev
```

Fedora:
```bash
sudo dnf install SDL2-devel
```

After installing, rebuild:
```bash
make clean && make nes
```

### Compiler not found / wrong version

**Error:**
```
gcc: command not found
```

or C11 features not supported.

**Solution:**

Ensure you have a C11-compatible compiler:

macOS:
```bash
xcode-select --install
```

Ubuntu/Debian:
```bash
sudo apt install build-essential
```

### Missing nestest.nes

**Error:**
```
test_cartridge_load: SKIP (nestest.nes not found, run 'make fetch-nestest')
```

**Solution:**
```bash
make fetch-nestest
```

This downloads `nestest.nes` to `roms/test/`.

## Runtime Issues

### Emulator crashes on startup

**Possible causes:**

1. **No ROM specified**
   ```
   Usage: ./build/nes <rom.nes>
   ```
   Solution: Provide a ROM file path.

2. **Invalid ROM file**
   - Ensure the file is a valid iNES format (`.nes`)
   - Check that the file starts with `NES\x1A` magic bytes

3. **Unsupported mapper**
   - Currently only Mapper 0 (NROM) is supported
   - Games like Zelda, Mega Man 2 won't work (they use MMC1, MMC3, etc.)

### Black screen / no video

**Possible causes:**

1. **SDL2 display issue**
   - Try running in a different terminal
   - Check if other SDL2 applications work

2. **ROM not rendering**
   - Try a known-working NROM game (Super Mario Bros, Donkey Kong)

### No audio

**Possible causes:**

1. **Audio device busy**
   - Close other audio applications
   - Check system audio settings

2. **SDL2 audio initialization failed**
   - Check console output for SDL audio errors

## Test Failures

### nestest diff shows mismatches

The CPU trace doesn't match the reference. Common causes:

1. **Cycle count mismatch**
   - Check instruction cycle counts
   - Verify page-cross penalties

2. **PPU timing mismatch**
   - Ensure `bus_tick` calls `ppu_tick` exactly 3× per CPU cycle
   - Check initial PPU state (scanline=0, dot=21 for nestest)

3. **Register state mismatch**
   - Check flag calculations (especially V flag for ADC/SBC)
   - Verify stack operations (push decrements, pop increments)

**Debugging approach:**
1. Find the first differing line
2. Check the instruction at that PC
3. Add debug prints before/after the instruction
4. Compare against 6502 reference documentation

### Unit test assertion failure

Example:
```
Assertion failed: (cpu.S == 0xFD), function test_cpu_init, file test/test_cpu.c, line 18.
```

**Solution:**
1. Read the assertion condition
2. Check the relevant code path
3. Fix the bug in the implementation (not the test, unless the test is wrong)

## Performance Issues

### Emulator runs too fast/slow

**Possible causes:**

1. **VSync not working**
   - The emulator uses `SDL_RENDERER_PRESENTVSYNC`
   - If your display isn't 60Hz, timing may be off

2. **Missing frame limiting**
   - Check that the game loop waits for VBlank

### High CPU usage

**Possible causes:**

1. **Busy-wait loop**
   - Normal behavior if VSync is disabled
   - Enable VSync in renderer creation

## Getting Help

1. Check [NESDev Wiki](https://www.nesdev.org/wiki/) for NES hardware details
2. Compare against reference implementations
3. Use `cpu_trace` to generate instruction-by-instruction output
4. Diff against known-good traces (nestest.log)
