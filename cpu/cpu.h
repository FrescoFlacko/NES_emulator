#ifndef C_CPU_H
#define C_CPU_H

#include <stdint.h>

#define STACK 0x100
#define IO_REGISTERS 0x2000
#define PRG_ROM 0x8000
#define NMI_VECTOR 0xFFFA
#define RESET_VECTOR 0xFFFC
#define IRQ_VECTOR 0xFFFE

uint8_t memory[65535];

uint8_t* read(uint16_t address);
void write(uint16_t address, uint8_t data);

uint8_t* sp = (uint8_t*) STACK;
uint16_t* pc;

uint8_t accumulator;
uint8_t index_x;
uint8_t index_y;
uint8_t processor_status;

#endif
