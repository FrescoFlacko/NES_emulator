#include "../cpu/cpu.h"
#include <assert.h>

void test_addresses();

int main()
{
  read8(0x0000);

  test_addresses();

  return 0;
}

void test_addresses()
{
  /* Set up */
  memory = calloc(65535, 8);
  memory[0x00FD] = 0x4D;

  /* Test */
  uint8_t* value = read8(0x00FD);

  /* printf("%hhu\n", *value);
  /*assert(*value == 0x4D);  */

  /* Tear down */
  free(memory);
}
