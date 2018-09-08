#include "cpu.h"

int initialize_cpu()
{

}

uint8_t* read(uint16_t address)
{
  return &memory[address];
}

void write(uint16_t address, uint8_t data)
{
  memory[address] = data;
}
