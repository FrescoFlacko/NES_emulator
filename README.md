# NES Emulator

A cycle-accurate NES emulator written in C, targeting trace-perfect 6502 CPU emulation with SDL2-based video/audio output.

## Status

| Component | Status |
|-----------|--------|
| CPU (6502) | Complete - passes nestest (8991/8991 lines) |
| Bus | Complete - RAM mirroring, PPU/APU routing, controller I/O |
| Cartridge | Complete - iNES loader |
| Mapper 0 (NROM) | Complete |
| PPU | Complete - background/sprite rendering, scrolling |
| APU | Complete - pulse, triangle, noise, DMC channels |
| Input | Complete - keyboard via SDL2 |
| Additional Mappers | Not implemented |

## Quick Start

### Build (Make)

```bash
make all          # Build test binaries
make nes          # Build full emulator (requires SDL2)
```

### Build (CMake)

```bash
cmake -B build && cmake --build build
```

### Run Tests

```bash
make test         # Run unit tests
make nestest      # Run CPU validation against nestest.log
```

Or with CMake:

```bash
cmake --build build --target run_tests
cmake --build build --target nestest
```

### Run Emulator

```bash
./build/nes <rom.nes>
```

Example:

```bash
./build/nes smb.nes
```

## Dependencies

- **C compiler**: GCC or Clang with C11 support
- **SDL2**: Required for the full emulator (video/audio)

### Install SDL2

macOS:
```bash
brew install sdl2
```

Ubuntu/Debian:
```bash
sudo apt install libsdl2-dev
```

## Repository Layout

```
NES_emulator/
├── src/                    # Emulator source code
│   ├── cpu/               # 6502 CPU implementation
│   ├── bus/               # Memory bus and I/O routing
│   ├── ppu/               # Picture Processing Unit
│   ├── apu/               # Audio Processing Unit
│   ├── cartridge/         # ROM loading (iNES format)
│   ├── mapper/            # Mapper implementations
│   └── main.c             # SDL2 game loop
├── test/                   # Unit tests
│   ├── test_cpu.c         # CPU/Bus/Cartridge unit tests
│   └── nestest_runner.c   # CPU trace validation
├── testdata/
│   └── nestest.log        # Reference trace for CPU validation
├── roms/test/              # Downloaded test ROMs (gitignored)
├── legacy/                 # Original code preserved for reference (read-only)
├── docs/                   # Documentation
├── PLAN.md                 # Detailed implementation plan and milestones
└── Makefile / CMakeLists.txt
```

## Useful Commands

| Command | Description |
|---------|-------------|
| `make all` | Build test binaries |
| `make nes` | Build full emulator |
| `make test` | Run unit tests |
| `make nestest` | Validate CPU against nestest.log |
| `make fetch-nestest` | Download nestest.nes ROM |
| `make clean` | Remove build artifacts |

## Documentation

- [Technical Design](docs/technical-design.md) - Architecture overview
- [Testing Guide](docs/testing.md) - How tests work and how to add more
- [Troubleshooting](docs/troubleshooting.md) - Common issues and solutions
- [PLAN.md](PLAN.md) - Detailed implementation plan with milestones

## References

- [NESDev Wiki](https://www.nesdev.org/wiki/) - Authoritative NES documentation
- [6502 Reference](http://www.obelisk.me.uk/6502/reference.html) - Instruction set reference
- [nestest](http://www.qmtpro.com/~nes/misc/) - CPU validation ROM and trace

## License

See [LICENSE](LICENSE) file.
