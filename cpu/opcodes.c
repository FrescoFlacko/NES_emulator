#include "opcodes.h"

void ADC(uint8_t value)
{
  uint16_t temp = value + accumulator + getflag(c);
  setflag(z, temp & 0xFF);

  if (getflag(d))
  {
    if (((accumulator & 0x0F) + (value & 0x0F) + getflag(c)) > 0x09)
    {
      temp += 0x06;
    }
    setflag(n, temp & highbit8(temp));
    setflag(v, !((accumulator ^ value) & 0x80) && ((accumulator ^ temp) & 0x80));

    if (temp > 0x99)
    {
      temp += 0x60;
    }

    setflag(c, temp > 0x99);
  }
  else
  {
    setflag(n, temp & highbit8(temp));
    setflag(v, !((accumulator ^ value) & 0x80) && ((accumulator ^ temp) & 0x80));
    setflag(c, temp > 0xFF);
  }

  accumulator = (uint8_t) temp;
}

void AND(uint8_t value)
{
  uint8_t result = value & accumulator;
  setflag(n, result & highbit8(result));
  setflag(z, result & highbit8(result));
  accumulator = result;
}

void ASL(uint8_t value, uint16_t address)
{
  setflag(c, value & 0x80);
  value <<= 1;
  value &= 0xFF;
  setflag(n, value & highbit8(value));
  setflag(z, !value);
  write(address, value);
  /* Save data */
}
