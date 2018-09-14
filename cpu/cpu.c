#include "cpu.h"
#include "opcodes.h"

int initialize_cpu()
{
  memory = calloc(65535, 8);
  sp = 0x0100;
  accumulator = 0;
  pc = 0;
  processor_status = 0x20;
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

void print_value(uint16_t address)
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

uint8_t highbit(uint8_t value)
{
  uint8_t ret = value >> 7;
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
  /* Special thanks to Jeremy Ruten from StackOverflow for this code */
  uint8_t newbit = !!value;
  processor_status ^= (-newbit ^ processor_status) & (0x01 << flag);
}

void perform_instruction(uint8_t opcode, uint16_t address)
{
  switch (opcode) {
    case 0x00:
      BRK();
      // size = 2;
      cycles += 7;
      break;
    case 0x01:
      ORA(INDEXED_INDIRECT_X(address));
      // size = 2;
      cycles += 6;
      break;
    case 0x03:
      SLO(INDEXED_INDIRECT_X(address));
      // size = 2;
      cycles += 8;
      break;
    case 0x04:
      NOP(ZERO_PAGE(address));
      // size = 1;
      cycles += 1;
      break;
    case 0x05:
      ORA(ZERO_PAGE(address));
      // size = 2;
      cycles += 3;
      break;
    case 0x06:
      ASL(ZERO_PAGE(address), address, 1);
      // size = 2;
      cycles += 5;
      break;
    case 0x07:
      SLO(ZERO_PAGE(address));
      // size = 2;
      cycles += 5;
      break;
    case 0x08:
      PHP();
      // size = 1;
      cycles += 3;
      break;
    case 0x09:
      ORA(IMMEDIATE(address));
      // size = 2;
      cycles += 2;
      break;
    case 0x0A:
      // ASL with implied addressing; what to do here?
      // size = 1;
      cycles += 2;
      break;
    case 0x0B:
      ANC(IMMEDIATE(address));
      // size = 2;
      cycles += 2;
      break;
    case 0x0C:
      NOP(ABSOLUTE(address));
      // size = 1;
      cycles += 2;
      break;
    case 0x0D:
      ORA(ABSOLUTE(address));
      // size = 3;
      cycles += 4;
      break;
    case 0x0E:
      ASL(ABSOLUTE(address), address, 1);
      // size = 3;
      cycles += 6;
      break;
    case 0x0F:
      SLO(ABSOLUTE(address));
      // size = 3;
      cycles += 6;
      break;
    default:
      break;
  }
}
