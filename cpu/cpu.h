#ifndef _CPU_H
#define _CPU_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#define STACK 0x100
#define IO_REGISTERS 0x2000
#define PRG_ROM 0x8000
#define NMI_VECTOR 0xFFFA
#define RESET_VECTOR 0xFFFC
#define IRQ_VECTOR 0xFFFE

uint8_t* memory;
uint16_t sp;
uint16_t pc;

uint8_t accumulator;
uint8_t index_x;
uint8_t index_y;
uint8_t processor_status;

enum program_flag {c, z, i, d, b, e, v, n};

/* CPU functions */
uint8_t read8(uint16_t address);
void write(uint16_t address, uint8_t data);
int initialize_cpu();
int deinitialize_cpu();
void print_value(uint16_t address);

/* Stack functions */
void push_stack8(uint8_t value);
void push_stack16(uint16_t value);
uint8_t pop_stack8();
uint16_t pop_stack16();

/* Bit manipulation functions */
uint8_t highbit8(uint8_t value);
uint8_t highbit16(uint16_t value);
uint8_t getflag(enum program_flag flag);
void setflag(enum program_flag flag, uint8_t value);

/* Addressing modes */
#define READ(address) ({ read8(address); })
#define ADDR_16(address) ({ \
  uint16_t addr = (uint16_t) READ(address + 1);\
  addr = addr << 8;\
  addr += (uint16_t) READ(address);\
  addr;\
})
#define ZERO_PAGE (address) ({ pc += 2; READ(address); })
#define IND_ZERO_PAGE_X (address) ({ pc += 2; READ((address + index_x % 256); })
#define IND_ZERO_PAGE_Y (address) ({ pc += 2; READ((address + index_y % 256); })
#define ABSOLUTE (address) ({ pc += 3; uint16_t addr = ADDR_16(address + 1); READ(addr); })
#define IND_ABSOLUTE_X (address) ({ pc += 3; \
  uint16_t addr = ADDR_16(address + 1) + index_x; \
  READ(addr); \
})
#define IND_ABSOLUTE_Y (address) ({ pc += 3; \
  uint16_t addr = ADDR_16(address + 1) + index_y; \
  READ(addr);\
})
#define INDIRECT (address) ({ pc += 3; \
  uint16_t addr = ADDR_16(address + 1); \
  uint16_t addr2 = ADDR_16(addr); \
  READ(addr2);\
})
/* TODO: Figure out how to use this addressing mode */
#define RELATIVE (address) ({ pc += 2; })
#define INDEXED_INDIRECT_X (address) ({ pc += 2; \
  uint16_t addr = ADDR_16(READ(address) + index_x); \
  uint16_t addr2 = ADDR_16(addr); \
  READ(addr2);\
})
#define INDEXED_INDIRECT_Y (address) ({ pc += 2; \
  uint16_t addr = ADDR_16(READ(address) + index_y); \
  uint16_t addr2 = ADDR_16(addr); \
  READ(addr2);\
})

#endif
