# Documentation Plan (NES_emulator)

This plan describes how to add **trackable, maintainable documentation** across the project without turning the code into a wall of comments.

## 0) Current Context (from repo)

- Codebase is a C NES emulator with modules in `src/`:
  - `src/cpu`, `src/bus`, `src/cartridge`, `src/mapper`, `src/ppu`, `src/apu`, plus `src/main.c`
- There is a large existing planning document: `PLAN.md` (very detailed implementation plan and milestones).
- There is a tiny note file: `Notes`.
- There is currently **no** `README.md` in the repo.
- Commenting style differs:
  - `src/` implementation files use `//` section comments (notably `src/cpu/cpu.c`).
  - `legacy/` uses more traditional `/* ... */` blocks (and has some TODO notes).

## 1) Documentation goals

1. **Onboarding**: A newcomer can build, run, and test within minutes.
2. **Architecture clarity**: Emulation “shape” is obvious: CPU ↔ Bus ↔ Cartridge/Mapper ↔ PPU/APU.
3. **Traceability**: Document key correctness decisions (timing quirks, addressing mode bugs, mirroring, etc.) with references.
4. **Maintainability**: Comments explain *why* and *invariants*, not obvious *what*.

Non-goals:
- Rewriting `PLAN.md` (we’ll link to it and optionally extract the current state into a shorter TDD).
- Commenting every line.

## 2) Documentation deliverables

### 2.1 `README.md` (how to run)

Create a top-level `README.md` with:

- Project summary (what it is, what’s implemented, what’s missing)
- Quick start
  - Build (Make and/or CMake)
  - Run tests
  - Run `nestest`
  - Run emulator (SDL2 requirement)
- Dependencies
  - compiler requirements
  - SDL2 installation (macOS via `brew install sdl2`)
- Repo layout
  - highlight `src/`, `test/`, `testdata/`, `roms/test/` (downloaded), `legacy/`
- Useful commands (copy/pastable)

Ground truth commands (from current repo files):

- Make:
  - `make all`
  - `make test`
  - `make nestest`
  - `make nes`
  - `make fetch-nestest`
- CMake:
  - `cmake -B build && cmake --build build`
  - `cmake --build build --target run_tests`
  - `cmake --build build --target nestest`

### 2.2 `docs/technical-design.md` (Technical Design Document)

Add a concise TDD that reflects the **current implemented design**, not aspirational design.

Recommended sections:

1. **Overview**
   - Emulator components and responsibilities
2. **Core data flow**
   - CPU reads/writes through `Bus`
   - `Bus` routes to RAM/PPU regs/cartridge
   - Cartridge delegates to Mapper
3. **Timing model**
   - CPU cycles tracked
   - PPU ticks at 3× CPU cycles (if implemented)
4. **Memory maps** (short, with links)
   - CPU map relevant ranges ($0000-$1FFF, $2000-$3FFF, $4020-$FFFF)
5. **Cartridge format**
   - iNES support assumptions
6. **Testing strategy**
   - `test/test_cpu.c`
   - `test/nestest_runner.c` + `testdata/nestest.log`
7. **Known limitations / roadmap**
   - rendering, input, APU, additional mappers
8. **References**
   - nesdev wiki pages, nestest sources

Notes:
- `PLAN.md` already contains a very extensive design narrative; TDD should link to it and summarize the stable architecture.

### 2.3 Module-level documentation (file headers)

Add a short “module header” comment at the top of each `.c` and `.h` in `src/`.

**Template (C block comment):**

```c
/*
 * Module: src/<area>/<file>
 * Responsibility: <one sentence>
 * Key invariants:
 *  - <invariant 1>
 *  - <invariant 2>
 * Notes:
 *  - <non-obvious design choice; link to docs/technical-design.md section>
 */
```

Guidelines:
- Keep headers small (5–15 lines).
- Prefer invariants and “why”, not “what”.

Targets:
- `src/cpu/cpu.[ch]`
- `src/bus/bus.[ch]`
- `src/cartridge/cartridge.[ch]`
- `src/mapper/mapper.[ch]`
- `src/ppu/ppu.[ch]`
- `src/apu/apu.[ch]`
- `src/main.c`

### 2.4 Function/API comments (selective)

Focus on **public API functions** (declared in `.h`) and any tricky internal helpers.

Prefer a Doxygen-ish style (works even if you don’t generate docs yet):

```c
/**
 * cpu_step - Execute a single CPU instruction.
 * @cpu: CPU instance.
 *
 * Returns: Number of CPU cycles consumed by the executed instruction.
 *
 * Notes:
 * - Updates cpu->cycles.
 * - May tick peripherals via the bus depending on architecture.
 */
int cpu_step(CPU *cpu);
```

Document especially:
- Cycle behavior / penalties
- Page-cross rules
- Any “hardware quirk” emulation (e.g., indirect JMP bug, open bus)

### 2.5 “Decision log” (optional but recommended)

Add `docs/decisions.md`:
- Short entries for notable decisions (e.g., how decimal mode is handled, initial PPU timing assumptions, mapper interfaces).
- Each entry: context, decision, consequences.

This is extremely useful for tracking and reviewing changes over time.

### 2.6 Testing documentation

Add a short `docs/testing.md`:
- What `test_cpu` covers
- What `nestest` validates
- How to add a new test

### 2.7 “Developer notes” / troubleshooting

Add `docs/troubleshooting.md` with:
- Common build issues (missing SDL2, compiler flags)
- Typical emulator debugging approaches (trace comparison, instruction stepping)

## 3) Documentation workflow & conventions

### 3.1 Commenting rules (avoid noise)

- Comment *why* and *invariants*, not obvious operations.
- Don’t restate types/variable names.
- If a function name is clear but the algorithm isn’t, document the algorithm.
- Prefer referencing authoritative sources for hardware quirks.

### 3.2 Documentation structure

Create a `docs/` folder (single-level is fine):

- `docs/technical-design.md`
- `docs/decisions.md` (optional)
- `docs/testing.md`
- `docs/troubleshooting.md`

(Keep `PLAN.md` as-is; link to it from README and the TDD.)

### 3.3 Ownership

- Each module “owns” its doc section in the TDD.
- Changes to module behavior should update:
  1. Relevant `.h` function comment
  2. Any design decision in `docs/decisions.md`
  3. README/troubleshooting if build steps change

## 4) Implementation steps (incremental, trackable)

1. **Add top-level README.md**
   - Ensure commands match `Makefile` and `CMakeLists.txt` exactly.
2. **Add `docs/technical-design.md`**
   - Summarize current architecture; link to `PLAN.md` for deep detail.
3. **Add `docs/testing.md`**
   - Document `make test` and `make nestest` expectations.
4. **Add module headers** to `src/**` `.c/.h` (start with CPU/Bus/Cartridge/Mapper, then PPU/APU).
5. **Add public API function comments** in `src/**.h`.
6. **Add selective internal comments** in tricky areas (timing, addressing modes, mapper routing).
7. **(Optional) Add `docs/decisions.md`** and seed it with current important decisions.
8. **Consistency pass**
   - Keep format consistent across modules.
   - Ensure no contradictory statements compared to code.

## 5) Verification checklist

- README commands are verified against:
  - `Makefile` targets
  - `CMakeLists.txt` targets
  - `src/main.c` expected usage (`./build/nes <rom.nes>`)
- Comments do not drift from code:
  - For each documented invariant, confirm it is enforced in code.
- Documentation is discoverable:
  - README links to docs.
  - docs link back to relevant source paths.

## 6) Recommended prioritization

If time is limited, do in this order:

1. `README.md`
2. `docs/technical-design.md`
3. Headers + API comments for: `bus`, `cpu`, `cartridge`, `mapper`
4. Testing docs
5. PPU/APU docs, decision log
