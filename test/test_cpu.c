#include "test_cpu.h"

int main()
{
  test_addresses();

  push_stack8(0x004D);

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
  print_address(addr_16);
  assert(addr_16 == 0xE34D);

  /* Write to memory */
  write(0x003D, 0x4C);
  value = READ(0x003D);
  assert(value == 0x4C);

  /* Tear down */
  deinitialize_cpu();
}

void test_stack();
