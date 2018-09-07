#include "cpu.h"

uint8_t* read(uint16_t address)
{
  return &memory[address];
}

void write(uint16_t address, uint8_t data)
{
  memory[address] = data;
}

#define READ8(address) ({ read(address); })
