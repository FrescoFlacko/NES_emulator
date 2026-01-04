cmake_minimum_required(VERSION 3.16)
project(nes_emulator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

set(CPU_SOURCES
    src/cpu/cpu.c
)

set(BUS_SOURCES
    src/bus/bus.c
)

set(CARTRIDGE_SOURCES
    src/cartridge/cartridge.c
)

set(MAPPER_SOURCES
    src/mapper/mapper.c
)

set(PPU_SOURCES
    src/ppu/ppu.c
)

set(APU_SOURCES
    src/apu/apu.c
)

add_library(nes_core STATIC
    ${CPU_SOURCES}
    ${BUS_SOURCES}
    ${CARTRIDGE_SOURCES}
    ${MAPPER_SOURCES}
    ${PPU_SOURCES}
    ${APU_SOURCES}
)
target_include_directories(nes_core PUBLIC src)

add_executable(nestest_runner test/nestest_runner.c)
target_link_libraries(nestest_runner nes_core)

add_executable(test_cpu test/test_cpu.c)
target_link_libraries(test_cpu nes_core)

add_executable(test_bus test/test_bus.c)
target_link_libraries(test_bus nes_core)

add_executable(test_cartridge test/test_cartridge.c)
target_link_libraries(test_cartridge nes_core)

add_executable(test_mapper test/test_mapper.c)
target_link_libraries(test_mapper nes_core)

add_executable(test_ppu test/test_ppu.c)
target_link_libraries(test_ppu nes_core)

add_executable(test_apu test/test_apu.c)
target_link_libraries(test_apu nes_core m)

find_package(SDL2 QUIET)
if(SDL2_FOUND)
    add_executable(nes_emulator src/main.c)
    target_link_libraries(nes_emulator nes_core SDL2::SDL2)
    message(STATUS "SDL2 found - building full emulator")
else()
    message(STATUS "SDL2 not found - skipping full emulator build")
    message(STATUS "Install with: brew install sdl2")
endif()

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

add_custom_target(run_tests
    COMMAND $<TARGET_FILE:test_cpu>
    COMMAND $<TARGET_FILE:test_bus>
    COMMAND $<TARGET_FILE:test_cartridge>
    COMMAND $<TARGET_FILE:test_mapper>
    COMMAND $<TARGET_FILE:test_ppu>
    COMMAND $<TARGET_FILE:test_apu>
    DEPENDS test_cpu test_bus test_cartridge test_mapper test_ppu test_apu
    COMMENT "Running all unit tests"
)
