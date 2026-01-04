#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define ASSERT_EQ_U8(expected, actual) do { \
    uint8_t _e = (expected), _a = (actual); \
    if (_e != _a) { \
        fprintf(stderr, "FAIL: %s:%d: expected 0x%02X, got 0x%02X\n", \
                __FILE__, __LINE__, _e, _a); \
        exit(1); \
    } \
} while(0)

#define ASSERT_EQ_U16(expected, actual) do { \
    uint16_t _e = (expected), _a = (actual); \
    if (_e != _a) { \
        fprintf(stderr, "FAIL: %s:%d: expected 0x%04X, got 0x%04X\n", \
                __FILE__, __LINE__, _e, _a); \
        exit(1); \
    } \
} while(0)

#define ASSERT_EQ_U32(expected, actual) do { \
    uint32_t _e = (expected), _a = (actual); \
    if (_e != _a) { \
        fprintf(stderr, "FAIL: %s:%d: expected 0x%08X, got 0x%08X\n", \
                __FILE__, __LINE__, _e, _a); \
        exit(1); \
    } \
} while(0)

#define ASSERT_EQ_INT(expected, actual) do { \
    int _e = (expected), _a = (actual); \
    if (_e != _a) { \
        fprintf(stderr, "FAIL: %s:%d: expected %d, got %d\n", \
                __FILE__, __LINE__, _e, _a); \
        exit(1); \
    } \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s:%d: expected true\n", __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

#define ASSERT_FALSE(cond) do { \
    if (cond) { \
        fprintf(stderr, "FAIL: %s:%d: expected false\n", __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

#define ASSERT_NULL(ptr) do { \
    if ((ptr) != NULL) { \
        fprintf(stderr, "FAIL: %s:%d: expected NULL\n", __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

#define ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        fprintf(stderr, "FAIL: %s:%d: expected non-NULL\n", __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

static inline char* create_temp_ines_rom(
    uint8_t prg_banks,
    uint8_t chr_banks,
    uint8_t flags6,
    uint8_t flags7,
    const uint8_t* prg_data,
    size_t prg_data_size,
    const uint8_t* chr_data,
    size_t chr_data_size,
    bool has_trainer
) {
    static char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "/tmp/test_rom_%d.nes", rand());
    
    FILE* f = fopen(temp_path, "wb");
    if (!f) return NULL;
    
    uint8_t header[16] = {
        'N', 'E', 'S', 0x1A,
        prg_banks,
        chr_banks,
        flags6,
        flags7,
        0, 0, 0, 0, 0, 0, 0, 0
    };
    fwrite(header, 1, 16, f);
    
    if (has_trainer) {
        uint8_t trainer[512] = {0};
        fwrite(trainer, 1, 512, f);
    }
    
    size_t prg_size = prg_banks * 16384;
    uint8_t* prg = (uint8_t*)calloc(prg_size, 1);
    if (prg_data && prg_data_size > 0) {
        size_t copy_size = prg_data_size < prg_size ? prg_data_size : prg_size;
        memcpy(prg, prg_data, copy_size);
    }
    fwrite(prg, 1, prg_size, f);
    free(prg);
    
    if (chr_banks > 0) {
        size_t chr_size = chr_banks * 8192;
        uint8_t* chr = (uint8_t*)calloc(chr_size, 1);
        if (chr_data && chr_data_size > 0) {
            size_t copy_size = chr_data_size < chr_size ? chr_data_size : chr_size;
            memcpy(chr, chr_data, copy_size);
        }
        fwrite(chr, 1, chr_size, f);
        free(chr);
    }
    
    fclose(f);
    return temp_path;
}

static inline void remove_temp_file(const char* path) {
    if (path) remove(path);
}

#endif
