#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "mapper/mapper.h"
#include "cartridge/cartridge.h"
#include "test_helpers.h"

static void test_mapper_create_valid(void) {
    char* path = create_temp_ines_rom(1, 0, 0, 0, NULL, 0, NULL, 0, false);
    ASSERT_NOT_NULL(path);

    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    cartridge_load(&cart, path);

    ASSERT_NOT_NULL(cart.mapper);
    ASSERT_NOT_NULL(cart.mapper->cpu_read);
    ASSERT_NOT_NULL(cart.mapper->cpu_write);
    ASSERT_NOT_NULL(cart.mapper->ppu_read);
    ASSERT_NOT_NULL(cart.mapper->ppu_write);

    cartridge_free(&cart);
    remove_temp_file(path);
    printf("test_mapper_create_valid: PASS\n");
}

static void test_mapper_create_unsupported(void) {
    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    cart.prg_rom = (uint8_t*)calloc(16384, 1);
    cart.prg_rom_size = 16384;

    Mapper* mapper = mapper_create(&cart, 99);
    ASSERT_NULL(mapper);

    free(cart.prg_rom);
    printf("test_mapper_create_unsupported: PASS\n");
}

static void test_nrom_prg_mapping_16kb(void) {
    uint8_t prg_data[16384];
    for (int i = 0; i < 16384; i++) {
        prg_data[i] = (uint8_t)(i & 0xFF);
    }

    char* path = create_temp_ines_rom(1, 0, 0, 0, prg_data, 16384, NULL, 0, false);
    ASSERT_NOT_NULL(path);

    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    cartridge_load(&cart, path);

    ASSERT_EQ_U8(0x00, cart.mapper->cpu_read(cart.mapper, 0x8000));
    ASSERT_EQ_U8(0x01, cart.mapper->cpu_read(cart.mapper, 0x8001));
    ASSERT_EQ_U8(0xFF, cart.mapper->cpu_read(cart.mapper, 0x80FF));

    ASSERT_EQ_U8(0x00, cart.mapper->cpu_read(cart.mapper, 0xC000));
    ASSERT_EQ_U8(0x01, cart.mapper->cpu_read(cart.mapper, 0xC001));

    cartridge_free(&cart);
    remove_temp_file(path);
    printf("test_nrom_prg_mapping_16kb: PASS\n");
}

static void test_nrom_prg_mapping_32kb(void) {
    uint8_t prg_data[32768];
    for (int i = 0; i < 32768; i++) {
        prg_data[i] = (uint8_t)(i >> 8);
    }

    char* path = create_temp_ines_rom(2, 0, 0, 0, prg_data, 32768, NULL, 0, false);
    ASSERT_NOT_NULL(path);

    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    cartridge_load(&cart, path);

    ASSERT_EQ_U8(0x00, cart.mapper->cpu_read(cart.mapper, 0x8000));
    ASSERT_EQ_U8(0x40, cart.mapper->cpu_read(cart.mapper, 0xC000));
    ASSERT_EQ_U8(0x7F, cart.mapper->cpu_read(cart.mapper, 0xFFFF));

    cartridge_free(&cart);
    remove_temp_file(path);
    printf("test_nrom_prg_mapping_32kb: PASS\n");
}

static void test_nrom_prg_ram(void) {
    char* path = create_temp_ines_rom(1, 0, 0, 0, NULL, 0, NULL, 0, false);
    ASSERT_NOT_NULL(path);

    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    cartridge_load(&cart, path);

    cart.mapper->cpu_write(cart.mapper, 0x6000, 0xAA);
    cart.mapper->cpu_write(cart.mapper, 0x7FFF, 0xBB);
    cart.mapper->cpu_write(cart.mapper, 0x6100, 0xCC);

    ASSERT_EQ_U8(0xAA, cart.mapper->cpu_read(cart.mapper, 0x6000));
    ASSERT_EQ_U8(0xBB, cart.mapper->cpu_read(cart.mapper, 0x7FFF));
    ASSERT_EQ_U8(0xCC, cart.mapper->cpu_read(cart.mapper, 0x6100));

    cartridge_free(&cart);
    remove_temp_file(path);
    printf("test_nrom_prg_ram: PASS\n");
}

static void test_chr_rom_readonly(void) {
    uint8_t chr_data[8192];
    for (int i = 0; i < 8192; i++) {
        chr_data[i] = (uint8_t)(i & 0xFF);
    }

    char* path = create_temp_ines_rom(1, 1, 0, 0, NULL, 0, chr_data, 8192, false);
    ASSERT_NOT_NULL(path);

    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    cartridge_load(&cart, path);

    ASSERT_EQ_U8(0x00, cart.mapper->ppu_read(cart.mapper, 0x0000));
    ASSERT_EQ_U8(0xFF, cart.mapper->ppu_read(cart.mapper, 0x00FF));
    ASSERT_EQ_U8(0x00, cart.mapper->ppu_read(cart.mapper, 0x1000));

    cart.mapper->ppu_write(cart.mapper, 0x0000, 0x99);
    ASSERT_EQ_U8(0x00, cart.mapper->ppu_read(cart.mapper, 0x0000));

    cartridge_free(&cart);
    remove_temp_file(path);
    printf("test_chr_rom_readonly: PASS\n");
}

static void test_chr_ram_writable(void) {
    char* path = create_temp_ines_rom(1, 0, 0, 0, NULL, 0, NULL, 0, false);
    ASSERT_NOT_NULL(path);

    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    cartridge_load(&cart, path);

    ASSERT_EQ_U8(0x00, cart.mapper->ppu_read(cart.mapper, 0x0000));

    cart.mapper->ppu_write(cart.mapper, 0x0000, 0x42);
    cart.mapper->ppu_write(cart.mapper, 0x1FFF, 0x99);

    ASSERT_EQ_U8(0x42, cart.mapper->ppu_read(cart.mapper, 0x0000));
    ASSERT_EQ_U8(0x99, cart.mapper->ppu_read(cart.mapper, 0x1FFF));

    cartridge_free(&cart);
    remove_temp_file(path);
    printf("test_chr_ram_writable: PASS\n");
}

static void test_mapper_destroy_safe(void) {
    char* path = create_temp_ines_rom(1, 0, 0, 0, NULL, 0, NULL, 0, false);
    ASSERT_NOT_NULL(path);

    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    cartridge_load(&cart, path);

    Mapper* m = cart.mapper;
    cart.mapper = NULL;
    mapper_destroy(m);

    mapper_destroy(NULL);

    remove_temp_file(path);
    printf("test_mapper_destroy_safe: PASS\n");
}

int main(void) {
    test_mapper_create_valid();
    test_mapper_create_unsupported();
    test_nrom_prg_mapping_16kb();
    test_nrom_prg_mapping_32kb();
    test_nrom_prg_ram();
    test_chr_rom_readonly();
    test_chr_ram_writable();
    test_mapper_destroy_safe();

    printf("\nAll mapper tests passed.\n");
    return 0;
}
