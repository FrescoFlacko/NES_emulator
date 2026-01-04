# Testing Guide

This document describes how tests are organized, how to run them, and how to add new tests.

## Test Structure

```
test/
├── test_cpu.c         # Unit tests for CPU, Bus, Cartridge
└── nestest_runner.c   # CPU trace validation against reference log

testdata/
└── nestest.log        # Reference trace (8991 lines)

roms/test/
└── nestest.nes        # Downloaded via 'make fetch-nestest'
```

## Running Tests

### Unit Tests

```bash
make test
```

This builds and runs `test/test_cpu.c`, which tests:

- CPU initialization (`cpu_init`)
- Stack operations (`cpu_push8`, `cpu_pop8`, `cpu_push16`, `cpu_pop16`)
- Flag operations (`cpu_set_flag`, `cpu_get_flag`)
- RAM mirroring via Bus
- 16-bit read helpers (`cpu_read16`, `cpu_read16_zp`, `cpu_read16_jmp_bug`)
- Cartridge loading (requires `nestest.nes`)

Expected output:

```
test_cpu_init: PASS
test_ram_mirroring: PASS
test_cartridge_load: PASS
test_stack_operations: PASS
test_flag_operations: PASS
test_read16_variants: PASS

All tests passed.
```

### CPU Validation (nestest)

```bash
make fetch-nestest   # Download nestest.nes (first time only)
make nestest
```

This runs the CPU against the nestest ROM and compares output to `testdata/nestest.log`.

**Pass condition**: Zero diff output.

The nestest ROM exercises:
- All 56 official 6502 opcodes
- All unofficial/illegal opcodes
- All addressing modes
- Cycle-accurate timing

## Test Style

Tests use plain C with `assert()`:

```c
#include <assert.h>
#include <stdio.h>

static void test_example(void) {
    // Setup
    Bus bus;
    bus_init(&bus);

    // Action
    bus_write(&bus, 0x0000, 0x42);

    // Assert
    assert(bus_read(&bus, 0x0000) == 0x42);

    printf("test_example: PASS\n");
}
```

Guidelines:
- One test function per behavior
- Descriptive function names (`test_<what>`)
- Print `PASS` on success (assert will abort on failure)
- Keep tests independent (no shared state between tests)

## Adding a New Unit Test

1. Add a test function to `test/test_cpu.c` (or create a new `test/test_<module>.c`):

```c
static void test_new_feature(void) {
    // Setup
    // ...

    // Action
    // ...

    // Assert
    assert(condition);

    printf("test_new_feature: PASS\n");
}
```

2. Call it from `main()`:

```c
int main(void) {
    // existing tests...
    test_new_feature();

    printf("\nAll tests passed.\n");
    return 0;
}
```

3. If creating a new test file:
   - Add it to `Makefile` (add target and link with `nes_core` objects)
   - Add it to `CMakeLists.txt` (`add_executable`, `target_link_libraries`)

## Adding a New Test Binary

In `Makefile`:

```makefile
$(BUILD_DIR)/test_ppu: test/test_ppu.c $(TEST_OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -I src $^ -o $@
```

In `CMakeLists.txt`:

```cmake
add_executable(test_ppu test/test_ppu.c)
target_link_libraries(test_ppu nes_core)
```

## Test Dependencies

| Test | Requires |
|------|----------|
| Unit tests | Nothing (self-contained) |
| nestest | `roms/test/nestest.nes` (download via `make fetch-nestest`) |
| Full emulator | SDL2 |

Tests do **not** require SDL2 and can run in headless/CI environments.

## Debugging Test Failures

### Unit test assertion failure

The program will abort with the failing assertion. Example:

```
Assertion failed: (cpu.S == 0xFD), function test_cpu_init, file test/test_cpu.c, line 18.
```

Fix: Check the condition and the code path that led to it.

### nestest diff output

If `make nestest` shows diff output, the CPU trace doesn't match. Example:

```diff
--- testdata/nestest.log
+++ nestest_out.log
@@ -1234,1 +1234,1 @@
-C5F5  A2 00     LDX #$00    A:00 X:00 Y:00 P:24 SP:FD PPU:  0, 30 CYC:10
+C5F5  A2 00     LDX #$00    A:00 X:00 Y:00 P:24 SP:FD PPU:  0, 31 CYC:10
```

This shows line 1234 has a PPU dot mismatch. Debug by:

1. Check the specific instruction
2. Compare cycles/timing
3. Look at PPU tick synchronization

## Coverage Goals

See [TEST_PLAN.md](../TEST_PLAN.md) for the full unit test expansion plan covering Bus, Cartridge, Mapper, PPU, and APU modules.
