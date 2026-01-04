#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "cartridge/cartridge.h"
#include "test_helpers.h"

static void test_reject_invalid_magic(void) {
    char* path = create_temp_ines_rom(1, 0, 0, 0, NULL, 0, NULL, 0, false);
    ASSERT_NOT_NULL(path);

    FILE* f = fopen(path, "r+b");
    fseek(f, 0, SEEK_SET);
    fputc('X', f);
    fclose(f);

    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    bool result = cartridge_load(&cart, path);
    ASSERT_FALSE(result);

    remove_temp_file(path);
    printf("test_reject_invalid_magic: PASS\n");
}

static void test_prg_size_calculation(void) {
    uint8_t prg_data[4] = {0xEA, 0xEA, 0xEA, 0xEA};
    char* path = create_temp_ines_rom(2, 0, 0, 0, prg_data, 4, NULL, 0, false);
    ASSERT_NOT_NULL(path);

    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    bool result = cartridge_load(&cart, path);
    ASSERT_TRUE(result);
    ASSERT_EQ_U32(2 * 16384, cart.prg_rom_size);

    cartridge_free(&cart);
    remove_temp_file(path);
    printf("test_prg_size_calculation: PASS\n");
}

static void test_chr_rom_present(void) {
    uint8_t chr_data[4] = {0x11, 0x22, 0x33, 0x44};
    char* path = create_temp_ines_rom(1, 1, 0, 0, NULL, 0, chr_data, 4, false);
    ASSERT_NOT_NULL(path);

    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    bool result = cartridge_load(&cart, path);
    ASSERT_TRUE(result);
    ASSERT_NOT_NULL(cart.chr_rom);
    ASSERT_NULL(cart.chr_ram);
    ASSERT_EQ_U32(1 * 8192, cart.chr_rom_size);
    ASSERT_EQ_U8(0x11, cart.chr_rom[0]);
    ASSERT_EQ_U8(0x22, cart.chr_rom[1]);

    cartridge_free(&cart);
    remove_temp_file(path);
    printf("test_chr_rom_present: PASS\n");
}

static void test_chr_ram_fallback(void) {
    char* path = create_temp_ines_rom(1, 0, 0, 0, NULL, 0, NULL, 0, false);
    ASSERT_NOT_NULL(path);

    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    bool result = cartridge_load(&cart, path);
    ASSERT_TRUE(result);
    ASSERT_NULL(cart.chr_rom);
    ASSERT_NOT_NULL(cart.chr_ram);

    for (int i = 0; i < 8192; i++) {
        ASSERT_EQ_U8(0, cart.chr_ram[i]);
    }

    cartridge_free(&cart);
    remove_temp_file(path);
    printf("test_chr_ram_fallback: PASS\n");
}

static void test_trainer_skip(void) {
    uint8_t prg_data[2] = {0xCA, 0xFE};
    uint8_t flags6 = 0x04;
    char* path = create_temp_ines_rom(1, 0, flags6, 0, prg_data, 2, NULL, 0, true);
    ASSERT_NOT_NULL(path);

    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    bool result = cartridge_load(&cart, path);
    ASSERT_TRUE(result);
    ASSERT_EQ_U8(0xCA, cart.prg_rom[0]);
    ASSERT_EQ_U8(0xFE, cart.prg_rom[1]);

    cartridge_free(&cart);
    remove_temp_file(path);
    printf("test_trainer_skip: PASS\n");
}

static void test_mapper_id_extraction(void) {
    uint8_t flags6 = 0x00;
    uint8_t flags7 = 0x00;
    char* path = create_temp_ines_rom(1, 0, flags6, flags7, NULL, 0, NULL, 0, false);
    ASSERT_NOT_NULL(path);

    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    bool result = cartridge_load(&cart, path);
    ASSERT_TRUE(result);
    ASSERT_EQ_U8(0, cart.mapper_id);
    cartridge_free(&cart);
    remove_temp_file(path);

    flags6 = 0x20;
    flags7 = 0x30;
    path = create_temp_ines_rom(1, 0, flags6, flags7, NULL, 0, NULL, 0, false);
    memset(&cart, 0, sizeof(cart));
    result = cartridge_load(&cart, path);
    ASSERT_FALSE(result);
    ASSERT_EQ_U8(0x32, cart.mapper_id);
    remove_temp_file(path);

    printf("test_mapper_id_extraction: PASS\n");
}

static void test_mirroring_flag(void) {
    uint8_t flags6_horizontal = 0x00;
    char* path = create_temp_ines_rom(1, 0, flags6_horizontal, 0, NULL, 0, NULL, 0, false);
    ASSERT_NOT_NULL(path);

    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    cartridge_load(&cart, path);
    ASSERT_EQ_U8(0, cart.mirroring);
    cartridge_free(&cart);
    remove_temp_file(path);

    uint8_t flags6_vertical = 0x01;
    path = create_temp_ines_rom(1, 0, flags6_vertical, 0, NULL, 0, NULL, 0, false);
    memset(&cart, 0, sizeof(cart));
    cartridge_load(&cart, path);
    ASSERT_EQ_U8(1, cart.mirroring);
    cartridge_free(&cart);
    remove_temp_file(path);

    printf("test_mirroring_flag: PASS\n");
}

static void test_cartridge_free_clears_state(void) {
    char* path = create_temp_ines_rom(1, 1, 0, 0, NULL, 0, NULL, 0, false);
    ASSERT_NOT_NULL(path);

    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    cartridge_load(&cart, path);
    
    ASSERT_NOT_NULL(cart.prg_rom);
    ASSERT_NOT_NULL(cart.chr_rom);
    ASSERT_NOT_NULL(cart.prg_ram);
    ASSERT_NOT_NULL(cart.mapper);

    cartridge_free(&cart);

    ASSERT_NULL(cart.prg_rom);
    ASSERT_NULL(cart.chr_rom);
    ASSERT_NULL(cart.chr_ram);
    ASSERT_NULL(cart.prg_ram);
    ASSERT_NULL(cart.mapper);
    ASSERT_EQ_U32(0, cart.prg_rom_size);

    remove_temp_file(path);
    printf("test_cartridge_free_clears_state: PASS\n");
}

static void test_cartridge_cpu_read_delegation(void) {
    uint8_t prg_data[16384];
    for (int i = 0; i < 16384; i++) {
        prg_data[i] = (uint8_t)(i & 0xFF);
    }

    char* path = create_temp_ines_rom(1, 0, 0, 0, prg_data, 16384, NULL, 0, false);
    ASSERT_NOT_NULL(path);

    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    cartridge_load(&cart, path);

    ASSERT_EQ_U8(0x00, cartridge_cpu_read(&cart, 0x8000));
    ASSERT_EQ_U8(0x01, cartridge_cpu_read(&cart, 0x8001));
    ASSERT_EQ_U8(0xFF, cartridge_cpu_read(&cart, 0x80FF));

    ASSERT_EQ_U8(0x00, cartridge_cpu_read(&cart, 0xC000));
    ASSERT_EQ_U8(0x01, cartridge_cpu_read(&cart, 0xC001));

    cartridge_free(&cart);
    remove_temp_file(path);
    printf("test_cartridge_cpu_read_delegation: PASS\n");
}

static void test_cartridge_prg_ram_access(void) {
    char* path = create_temp_ines_rom(1, 0, 0, 0, NULL, 0, NULL, 0, false);
    ASSERT_NOT_NULL(path);

    Cartridge cart;
    memset(&cart, 0, sizeof(cart));
    cartridge_load(&cart, path);

    cartridge_cpu_write(&cart, 0x6000, 0xAB);
    cartridge_cpu_write(&cart, 0x7FFF, 0xCD);

    ASSERT_EQ_U8(0xAB, cartridge_cpu_read(&cart, 0x6000));
    ASSERT_EQ_U8(0xCD, cartridge_cpu_read(&cart, 0x7FFF));

    cartridge_free(&cart);
    remove_temp_file(path);
    printf("test_cartridge_prg_ram_access: PASS\n");
}

int main(void) {
    test_reject_invalid_magic();
    test_prg_size_calculation();
    test_chr_rom_present();
    test_chr_ram_fallback();
    test_trainer_skip();
    test_mapper_id_extraction();
    test_mirroring_flag();
    test_cartridge_free_clears_state();
    test_cartridge_cpu_read_delegation();
    test_cartridge_prg_ram_access();

    printf("\nAll cartridge tests passed.\n");
    return 0;
}
