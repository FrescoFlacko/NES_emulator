#include "../cpu/cpu.h"
#include <assert.h>
#include <stdio.h>

void test_addresses();

int main()
{
  test_addresses();

  return 0;
}

void test_addresses()
{
  memory[0x00FD] = 0x4D;

  uint8_t* value = READ(0x00FD);
  printf("%s\n", value);
  /*assert(*value == 0x4D);  */
}
