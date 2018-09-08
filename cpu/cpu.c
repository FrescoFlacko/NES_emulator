#include "cpu.h"

int initialize_cpu()
{
  memory = calloc(65535, 8);
  sp = 0x0100;
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

void push_stack8(uint8_t value)
{
  write(sp++, value);
}

void push_stack16(uint16_t value)
{
  write(sp++, value & 0xFF);
  write(sp++, value >> 8);
}

uint8_t pop_stack8()
{
  uint8_t value = READ(--sp);
  return value;
}

uint16_t pop_stack16()
{
  uint16_t value = READ(--sp);
  value = value << 8;
  value += READ(--sp);
  return value;
}

uint8_t highbit(uint16_t value)
{
  uint8_t ret = value >> 15;
  return ret;
}

uint8_t getbit(enum program_flag flag)
{
  uint8_t ret;

  if (flag == c)
  {
    ret = processor_status & 0x01;
  }

  return ret;
}
