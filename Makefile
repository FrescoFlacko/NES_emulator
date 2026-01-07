CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g
BUILD_DIR = build

SOURCES = src/cpu/cpu.c src/bus/bus.c src/cartridge/cartridge.c src/mapper/mapper.c src/ppu/ppu.c src/apu/apu.c src/savestate/savestate.c
OBJECTS = $(SOURCES:%.c=$(BUILD_DIR)/%.o)
TEST_OBJECTS = $(BUILD_DIR)/src/cpu/cpu_test.o $(BUILD_DIR)/src/bus/bus.o $(BUILD_DIR)/src/cartridge/cartridge.o $(BUILD_DIR)/src/mapper/mapper.o $(BUILD_DIR)/src/ppu/ppu.o $(BUILD_DIR)/src/apu/apu.o $(BUILD_DIR)/src/savestate/savestate.o

TEST_BINARIES = $(BUILD_DIR)/test_cpu $(BUILD_DIR)/test_bus $(BUILD_DIR)/test_cartridge $(BUILD_DIR)/test_mapper $(BUILD_DIR)/test_ppu $(BUILD_DIR)/test_apu

.PHONY: all clean test nestest fetch-nestest test-all

all: $(TEST_BINARIES) $(BUILD_DIR)/nestest_runner

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I src -c $< -o $@

$(BUILD_DIR)/src/cpu/cpu_test.o: src/cpu/cpu.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DCPU_TEST_HELPERS -I src -c $< -o $@

$(BUILD_DIR)/test_cpu: test/test_cpu.c $(TEST_OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DCPU_TEST_HELPERS -I src $^ -o $@

$(BUILD_DIR)/test_bus: test/test_bus.c $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -I src $^ -o $@

$(BUILD_DIR)/test_cartridge: test/test_cartridge.c $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -I src $^ -o $@

$(BUILD_DIR)/test_mapper: test/test_mapper.c $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -I src $^ -o $@

$(BUILD_DIR)/test_ppu: test/test_ppu.c $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -I src $^ -o $@

$(BUILD_DIR)/test_apu: test/test_apu.c $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -I src $^ -o $@ -lm

$(BUILD_DIR)/nestest_runner: test/nestest_runner.c $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -I src $^ -o $@

clean:
	@rm -rf $(BUILD_DIR)

test: $(TEST_BINARIES)
	@echo "Running test_cpu..."
	@$(BUILD_DIR)/test_cpu
	@echo ""
	@echo "Running test_bus..."
	@$(BUILD_DIR)/test_bus
	@echo ""
	@echo "Running test_cartridge..."
	@$(BUILD_DIR)/test_cartridge
	@echo ""
	@echo "Running test_mapper..."
	@$(BUILD_DIR)/test_mapper
	@echo ""
	@echo "Running test_ppu..."
	@$(BUILD_DIR)/test_ppu
	@echo ""
	@echo "Running test_apu..."
	@$(BUILD_DIR)/test_apu
	@echo ""
	@echo "All tests passed!"

nestest: $(BUILD_DIR)/nestest_runner
	$(BUILD_DIR)/nestest_runner > nestest_out.log
	diff -u testdata/nestest.log nestest_out.log | head -50
	@echo "Diff complete. Empty output = PASS"

fetch-nestest:
	@mkdir -p roms/test
	@curl -L -o roms/test/nestest.nes http://www.qmtpro.com/~nes/misc/nestest.nes
	@echo "Downloaded nestest.nes"

SDL2_CFLAGS = $(shell sdl2-config --cflags)
SDL2_LIBS = $(shell sdl2-config --libs)

$(BUILD_DIR)/nes: src/main.c $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SDL2_CFLAGS) -I src $^ $(SDL2_LIBS) -o $@

nes: $(BUILD_DIR)/nes
	@echo "NES emulator built at $(BUILD_DIR)/nes"
	@echo "Usage: $(BUILD_DIR)/nes <rom.nes>"
