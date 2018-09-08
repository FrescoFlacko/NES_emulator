#include "cpu.h"

uint8_t* memory;

int initialize_cpu()
{
  memory = calloc(65535, 8);
  return 0;
}

int deinitialize_cpu()
{
  free(memory);
  return 0;
}

uint8_t read8(uint16_t address)
{
  return memory[address];
}

void write(uint16_t address, uint8_t data)
{
  memory[address] = data;
}

void print_address(uint16_t address)
{
  printf("%#06x\n", address);
}
