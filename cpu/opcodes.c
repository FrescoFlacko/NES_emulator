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

void Branch(uint8_t value)
{
  /* clk = ((pc & 0xFF00) != (RELATIVE(pc, value) & 0xFF00) ? 2 : 1);
  pc = RELATIVE(pc, value); */
}

void BCC(uint8_t value)
{
  if (!getflag(c))
  {
    Branch(value);
  }
}

void BCS(uint8_t value)
{
  if (getflag(c))
  {
    Branch(value);
  }
}

void BIT(uint8_t value)
{
  setflag(n, value & highbit8(value));
  setflag(v, 0x40 & value);
  setflag(z, value & accumulator);
}

void BMI(uint8_t value)
{
  if (getflag(n))
  {
    Branch(value);
  }
}

void BNE(uint8_t value)
{
  if (!getflag(z))
  {
    Branch(value);
  }
}

void BPL(uint8_t value)
{
  if (!getflag(n))
  {
    Branch(value);
  }
}

void BRK(uint8_t value)
{
  pc++;
  push_stack16(pc & 0xFF);
  setflag(b, 1);
  push_stack8(processor_status);
  setflag(i, 1);
  pc = ADDR_16(0xFFFE);
}

void BVC(uint8_t value)
{
  if (!getflag(v))
  {
    Branch(value);
  }
}

void BVS(uint8_t value)
{
  if (getflag(v))
  {
    Branch(value);
  }
}

void CLC(uint8_t value)
{
  setflag(c, 0);
}

void CLD(uint8_t value)
{
  setflag(d, 0);
}

void CLI(uint8_t value)
{
  setflag(i, 0);
}

void CLV(uint8_t value)
{
  setflag(v, 0);
}

void CMP(uint8_t value)
{
  uint16_t val = accumulator - value;

  /* If val = 0, accumulator = value. If val < 0, accumulator < value. */
  setflag(n, val & highbit8(val));
  setflag(c, val < 0x100);
  setflag(z, val &= 0xFF);
}

void CPX(uint8_t value)
{
  uint16_t val = index_x - value;
  setflag(n, val & highbit8(val));
  setflag(c, val < 0x100);
  setflag(z, !val);
}

void CPY(uint8_t value)
{
  uint16_t val = index_y - value;
  setflag(n, val & highbit8(val));
  setflag(c, val < 0x100);
  setflag(z, !val);
}

void DEC(uint16_t address, uint8_t value)
{
  uint8_t val = (value - 1) & 0xFF;
  setflag(n, val & highbit8(val));
  setflag(z, !val);
  write(address, val);
}

void DEX()
{
  uint8_t val = index_x;
  val = (val - 1) & 0xFF;
  setflag(n, val & highbit8(val));
  setflag(z, !val);
  index_x = val;
}

void DEY()
{
  uint8_t val = index_y;
  val = (val - 1) & 0xFF;
  setflag(n, val & highbit8(val));
  setflag(z, !val);
  index_y = val;
}

void EOR(uint8_t value)
{
  uint8_t val = value ^ accumulator;
  setflag(n, val & highbit8(val));
  setflag(z, !val);
  accumulator = val;
}

void INC(uint16_t address, uint8_t value)
{
  uint8_t val = (value + 1) & 0xFF;
  setflag(n, val & highbit8(val));
  setflag(z, !val);
  write(address, value);
}

void INX()
{
  uint8_t val = index_x;
  val = (val + 1) & 0xFF;
  setflag(n, val & highbit8(val));
  setflag(z, !val);
  index_x = val;
}

void INY()
{
  uint8_t val = index_y;
  val = (val + 1) & 0xFF;
  setflag(n, val & highbit8(val));
  setflag(z, !val);
  index_y = val;
}

void JMP(uint16_t address)
{
  pc = address;
}

void JSR(uint16_t address)
{
  pc--;
  push_stack16(pc);
  pc = address;
}

void LDA(uint8_t value)
{
  setflag(n, value & highbit8(value));
  setflag(z, !value);
  accumulator = value;
}

void LDX(uint8_t value)
{
  setflag(n, value & highbit8(value));
  setflag(z, !value);
  index_x = value;
}

void LDY(uint8_t value)
{
  setflag(n, value & highbit8(value));
  setflag(z, !value);
  index_y = value;
}

void LSR(uint8_t value, uint16_t address, int mode)
{
  uint8_t val = value;
  setflag(c, val & 0x01);
  val >>= 1;
  setflag(n, val & highbit8(val));
  setflag(z, !val);

  /* If mode = 1, then we write to address.  */
  if (mode)
  {
    write(address, val);
  }
  else
  {
    accumulator = val;
  }
}

void NOP(uint8_t value)
{
  /* Command does nothing */
}

void ORA(uint8_t value)
{
  uint8_t val = value | accumulator;
  setflag(n, val & highbit8(val));
  setflag(z, !val);
  accumulator = val;
}

void PHA()
{
  push_stack8(accumulator);
}

void PHP()
{
  push_stack8(processor_status);
}

void PLA(uint8_t value)
{
  accumulator = pop_stack8();
  setflag(n, accumulator & highbit8(accumulator));
  setflag(z, !accumulator);
}

void PLP(uint8_t value)
{
  processor_status = pop_stack8();
}

void ROL(uint8_t value, uint16_t address, int mode)
{
  uint16_t _val = value << 1;
  if (getflag(c)) _val |= 0x1;
  setflag(c, _val > 0xFF);
  _val &= 0xFF;
  uint8_t val = (uint8_t) _val;
  setflag(n, val & highbit8(val));
  setflag(z, !val);

  if (mode)
  {
    write(address, val);
  }
  else
  {
    accumulator = val;
  }
}

void ROR(uint8_t value, uint16_t address, int mode)
{
  uint8_t val = value;
  if (getflag(c)) val |= 0x100;
  setflag(c, val & 0x01);
  val >>= 1;
  setflag(n, val & highbit8(val));
  setflag(z, !val);

  if (mode)
  {
    write(address, val);
  }
  else
  {
    accumulator = val;
  }
}
/* void RTI(uint8_t value);
void RTS(uint8_t value);
void SBC(uint8_t value);
void SEC(uint8_t value);
void SED(uint8_t value);
void SEI(uint8_t value);
void STA(uint16_t address, uint8_t value);
void STX(uint16_t address, uint8_t value);
void STY(uint16_t address, uint8_t value);
void TAX(uint8_t value);
void TAY(uint8_t value);
void TSX(uint8_t value);
void TXA(uint8_t value);
void TXS(uint8_t value);
void TYA(uint8_t value);
*/
