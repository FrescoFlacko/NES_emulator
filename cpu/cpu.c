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

/* Get flag from processor status */
uint8_t getflag(enum program_flag flag)
{
  uint8_t ret;

  /* AND program flags to single out the flag we need, then shift to LSB */
  ret = (processor_status & (0x01 << flag)) >> flag;

  return ret;
}

void setflag(enum program_flag flag, uint8_t value)
{
  /* Depending on the value, we perform different operations to set the bit*/
  if (value == 1)
  {
    processor_status |= (value << flag);
  }
  else
  {
    processor_status &= (value << flag);
  }
}
