#include "test_cpu.h"

int main()
{

  /*
  test_addresses();
  test_stack(); */
  test_bitman();


  return 0;
}

void test_addresses()
{
  /* Set up */
  initialize_cpu();
  memory[0x00FD] = 0x4D;
  memory[0x00FE] = 0xE3;

  /* Test */

  /* Read from memory */
  uint8_t value = READ(0x00FD);
  assert(value == 0x4D);

  /* Get 16-bit address from memory given address */
  uint16_t addr_16 = ADDR_16(0x00FD);
  assert(addr_16 == 0xE34D);

  /* Write to memory */
  write(0x003D, 0x4C);
  value = READ(0x003D);
  assert(value == 0x4C);

  /* Tear down */
  deinitialize_cpu();
}

void test_stack()
{
  /* Set up */
  initialize_cpu();

  /* Test */

  /* Push byte to stack */
  push_stack8(0x10);
  assert(sp == 0x0101);
  assert(memory[0x0100] == 0x10);

  /* Push word to stack */
  push_stack16(0xFFED);
  assert(sp == 0x0103);
  assert(READ(0x0101) == 0xED);
  assert(READ(0x0102) == 0xFF);

  /* Pop word from stack */
  uint16_t value16 = pop_stack16();
  assert(sp == 0x0101);
  assert(value16 == 0xFFED);

  /* Pop byte from stack */
  uint8_t value8 = pop_stack8();
  assert(sp == 0x0100);
  assert(value8 == 0x10);

  /* Tear down */
  deinitialize_cpu();
}

void test_bitman()
{
  /* Set up */
  initialize_cpu();
  uint8_t value8;
  uint16_t value16;
  processor_status = 0x80;

  /* Test */

  /* Get most significant bit */
  value8 = highbit(0xF000);
  assert(value8 == 0x01);

  /* Get bit */
  value8 = getflag(n);
  assert(value8 == 0x01);

  /* Set bit */
  processor_status = 0;
  setflag(n, 1);
  value8 = getflag(n);
  assert(value8 == 0x01);
  setflag(n, 0);
  value8 = getflag(n);
  assert(value8 == 0);

  /* Tear down */
  deinitialize_cpu();
}
