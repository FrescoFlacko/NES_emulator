#include "opcodes.h"

void ADC(uint8_t value)
{
  uint16_t result = value + accumulator + getflag(c);
  setflag(z, result & 0xFF);

  if (getflag(d))
  {
    if (((accumulator & 0x0F) + (value & 0x0F) + getflag(c)) > 0x09)
    {
      result += 0x06;
    }
    setflag(n, result & highbit(result));
    setflag(v, !((accumulator ^ value) & 0x80) && ((accumulator ^ result) & 0x80));

    if (result > 0x99)
    {
      result += 0x60;
    }

    setflag(c, result > 0x99);
  }
  else
  {
    setflag(n, result & highbit(result));
    setflag(v, !((accumulator ^ value) & 0x80) && ((accumulator ^ result) & 0x80));
    setflag(c, result > 0xFF);
  }

  accumulator = (uint8_t) result;
}

void AHX(uint16_t address)
{
  uint8_t temp = ((address >> 8) + 1) & accumulator & index_x;
  write(address, temp & 0xFF);
}

void ALR(uint8_t value)
{
  accumulator &= value;
  setflag(c, accumulator & 0x01);
  accumulator >>= 1;
  setflag(z, !accumulator);
  setflag(n, accumulator & highbit(accumulator));
}

/* And value with accumulator then move Negative flag to Carry flag */
void ANC(uint8_t value)
{
  accumulator &= value;
  setflag(n, accumulator & highbit(accumulator));
  setflag(c, accumulator & highbit(accumulator));
}

void AND(uint8_t value)
{
  uint8_t result = value & accumulator;
  setflag(n, result & highbit(result));
  setflag(z, result & highbit(result));
  accumulator = result;
}

void ARR(uint8_t value)
{
  uint8_t result = accumulator & value;
  result >>= 1;

  if (getflag(c))
  {
    result |= highbit(result);
  }

  setflag(n, getflag(c));
  setflag(z, !result);

  if (getflag(d))
  {
    setflag(v, (result ^ value) & 0x40);

    if ((value & 0x0F) + (value & 0x01) > 0x05)
      result = (result & 0xF0) | ((result + 0x06) & 0x0F);
    if ((value & 0xF0) + (value & 0x10) > 0x50) {
      result = (result & 0x0F) | ((result + 0x60) & 0xF0);
      setflag(c, 1);
    } else {
      setflag(c, 0);
    }
  }
  else
  {
    setflag(c, result & 0x40);
    setflag(v, ((result >> 6) ^ (result >> 5)) & 0x01);
  }

  accumulator = result;
}

void ASL(uint8_t value, uint16_t address, int mode)
{
  setflag(c, value & 0x80);
  value <<= 1;
  value &= 0xFF;
  setflag(n, value & highbit(value));
  setflag(z, !value);

  if (mode)
  {
    write(address, value);
  }
  else
  {
    accumulator = value;
  }
}

void AXS()
{
  uint8_t temp = accumulator;
  accumulator = index_x;
  index_x = temp;
}

void Branch(uint8_t value)
{
  cycles = ((pc & 0xFF00) != (RELATIVE(pc, value) & 0xFF00) ? 2 : 1);
  pc = RELATIVE(pc, value);
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
  setflag(n, value & highbit(value));
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

void BRK()
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
  setflag(n, val & highbit(val));
  setflag(c, val < 0x100);
  setflag(z, val &= 0xFF);
}

void CPX(uint8_t value)
{
  uint16_t val = index_x - value;
  setflag(n, val & highbit(val));
  setflag(c, val < 0x100);
  setflag(z, !val);
}

void CPY(uint8_t value)
{
  uint16_t val = index_y - value;
  setflag(n, val & highbit(val));
  setflag(c, val < 0x100);
  setflag(z, !val);
}

void DCP(uint8_t value)
{
  value--;
  setflag(n, (accumulator - value) & highbit(accumulator - value));
  setflag(z, accumulator == value);
  setflag(c, accumulator >= value);
}

void DEC(uint16_t address, uint8_t value)
{
  uint8_t val = (value - 1) & 0xFF;
  setflag(n, val & highbit(val));
  setflag(z, !val);
  write(address, val);
}

void DEX()
{
  uint8_t val = index_x;
  val = (val - 1) & 0xFF;
  setflag(n, val & highbit(val));
  setflag(z, !val);
  index_x = val;
}

void DEY()
{
  uint8_t val = index_y;
  val = (val - 1) & 0xFF;
  setflag(n, val & highbit(val));
  setflag(z, !val);
  index_y = val;
}

void EOR(uint8_t value)
{
  uint8_t val = value ^ accumulator;
  setflag(n, val & highbit(val));
  setflag(z, !val);
  accumulator = val;
}

void INC(uint16_t address, uint8_t value)
{
  uint8_t val = (value + 1) & 0xFF;
  setflag(n, val & highbit(val));
  setflag(z, !val);
  write(address, value);
}

void INX()
{
  uint8_t val = index_x;
  val = (val + 1) & 0xFF;
  setflag(n, val & highbit(val));
  setflag(z, !val);
  index_x = val;
}

void INY()
{
  uint8_t val = index_y;
  val = (val + 1) & 0xFF;
  setflag(n, val & highbit(val));
  setflag(z, !val);
  index_y = val;
}

void ISC(uint8_t value)
{
  value++;
  uint8_t result = value + !getflag(c);
  setflag(c, result <= accumulator);

  if (!getflag(d))
  {
    result = (accumulator - result);
    setflag(v, ((accumulator ^ value) & (accumulator ^ result)) & highbit((accumulator ^ value) & (accumulator ^ result)));
    setflag(n, result & highbit(result));
    setflag(z, !result);
  }
  else
  {
    uint16_t temp1 = (accumulator & 0x0F) - (result & 0x0F);
    uint16_t temp2 = (accumulator >> 4) - (result >> 4);

    if (temp1 & 0x10)
    {
      temp1 -= 0x06;
      temp2--;
    }

    if (temp2 & 0x10)
    {
      temp2 -= 0x06;
    }

    result = ((temp2 << 4) | (temp1 & 0x0F));

    uint8_t temp0 = accumulator - result;
    setflag(v, ((accumulator ^ value) & (accumulator ^ temp0)) & highbit((accumulator ^ value) & (accumulator ^ temp0)));
    setflag(n, temp0 & highbit(temp0));
    setflag(z, !temp0);

  }

  accumulator = result;
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

void LAS(uint8_t value)
{
  accumulator = index_x = sp &= value;
  setflag(n, accumulator & highbit(accumulator));
  setflag(z, !accumulator);
}

void LAX(uint8_t value)
{
  accumulator = index_x = value;
  setflag(n, accumulator & highbit(accumulator));
  setflag(z, !accumulator);
}

void LDA(uint8_t value)
{
  setflag(n, value & highbit(value));
  setflag(z, !value);
  accumulator = value;
}

void LDX(uint8_t value)
{
  setflag(n, value & highbit(value));
  setflag(z, !value);
  index_x = value;
}

void LDY(uint8_t value)
{
  setflag(n, value & highbit(value));
  setflag(z, !value);
  index_y = value;
}

void LSR(uint8_t value, uint16_t address, int mode)
{
  uint8_t val = value;
  setflag(c, val & 0x01);
  val >>= 1;
  setflag(n, val & highbit(val));
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
  setflag(n, val & highbit(val));
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
  setflag(n, accumulator & highbit(accumulator));
  setflag(z, !accumulator);
}

void PLP(uint8_t value)
{
  processor_status = pop_stack8();
}

void RLA(uint8_t value)
{
  uint8_t val = value;
  setflag(c, val & highbit(val));

  if (getflag(c))
  {
    val += val + 1;
  }
  else
  {
    val <<= 1;
  }

  accumulator &= val;
  setflag(n, accumulator & highbit(accumulator));
  setflag(z, !accumulator);
}

void ROL(uint8_t value, uint16_t address, int mode)
{
  uint16_t _val = value << 1;
  if (getflag(c)) _val |= 0x1;
  setflag(c, _val > 0xFF);
  _val &= 0xFF;
  uint8_t val = (uint8_t) _val;
  setflag(n, val & highbit(val));
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
  setflag(n, val & highbit(val));
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

void RRA(uint8_t value)
{
  setflag(c, value & 0x01);

  if (getflag(c))
  {
    value = (value >> 1) + highbit(value);
  }
  else
  {
    value >>= 1;
  }

  if (getflag(d))
  {
    uint16_t result = (accumulator & 0x0F) + (value & 0x0F) + getflag(c);

    if (result > 0x09)
    {
      result = (result - 0x0A) | 0x10;
    }

    result += (accumulator & 0xF0) + (value & 0xF0);

    setflag(z, !(accumulator + value + c));
    setflag(n, result & highbit(result));
    setflag(v, (~(accumulator ^ value) & (accumulator ^ result)) & highbit(~(accumulator ^ value) & (accumulator ^ result)));

    if (result > 0x9F)
    {
      result += 0x60;
    }

    setflag(c, result > 0xFF);
    accumulator = (uint8_t) result;
  }
  else
  {
    uint16_t result = accumulator + value + getflag(c);
    setflag(n, result & highbit(result));
    setflag(v, (~(accumulator ^ value) & (accumulator ^ result)) & highbit(~(accumulator ^ value) & (accumulator ^ result)));
    setflag(c, result > 0xFF);
    accumulator = (uint8_t) result;
    setflag(z, !accumulator);

  }
}

void RTI(uint8_t value)
{
  processor_status = pop_stack8();
  pc = pop_stack16();
}

void RTS(uint8_t value)
{
  pc = pop_stack16() + 1;
}

void SAX(uint8_t value)
{
  index_x &= accumulator;
  setflag(c, index_x >= value);
  index_x -= value;
  setflag(n, index_x & highbit(index_x));
  setflag(z, !index_x);
}

void SBC(uint8_t value)
{
  uint16_t val = accumulator - value - (getflag(c) ? 0 : 1);
  setflag(n, val & highbit(val));
  setflag(z, val & 0xFF);
  setflag(v, ((accumulator ^ val) & 0x80) && ((accumulator ^ value) & 0x80));

  if (getflag(d))
  {
    if (((accumulator & 0xF) - (getflag(c) ? 0 : 1)) < (value & 0xF))
      val -= 0x06;

    if (val > 0x99)
      val -= 0x60;
  }

  setflag(c, val < 0x100);
  accumulator = (uint8_t) (val & 0xFF);
}

void SEC(uint8_t value)
{
  setflag(c, 1);
}

void SED(uint8_t value)
{
  setflag(d, 1);
}

void SEI(uint8_t value)
{
  setflag(i, 1);
}

void SLO(uint8_t value)
{
  uint8_t val = value;
  setflag(c, val & highbit(val));
  val <<= 1;
  accumulator |= val;
  setflag(n, accumulator & highbit(val));
  setflag(z, !accumulator);
}

void SRE(uint8_t value)
{
  setflag(c, value & 0x01);
  value >>= 1;
  accumulator ^= value;
  setflag(n, accumulator & highbit(accumulator));
  setflag(z, !accumulator);
}

void STA(uint16_t address, uint8_t value)
{
  write(address, value);
}

void STX(uint16_t address)
{
  write(address, index_x);
}

void STY(uint16_t address)
{
  write(address, index_y);
}

void TAS(uint16_t address)
{
  sp = accumulator & index_x;
  uint8_t result = sp & ((address >> 8) + 1);
  write(address + index_y, result);
}

void TAX()
{
  uint8_t val = accumulator;
  setflag(n, val & highbit(val));
  setflag(z, !val);
  index_x = val;
}

void TAY()
{
  uint8_t val = accumulator;
  setflag(n, val & highbit(val));
  setflag(z, !val);
  index_y = val;
}

void TSX()
{
  uint8_t val = sp;
  setflag(n, val & highbit(val));
  setflag(z, !val);
  index_x = val;
}

void TXA()
{
  uint8_t val = index_x;
  setflag(n, val & highbit(val));
  setflag(z, !val);
  accumulator = val;
}

void TXS()
{
  uint8_t val = index_x;
  sp = val;
}

void TYA()
{
  uint8_t val = index_y;
  setflag(n, val & highbit(val));
  setflag(z, !val);
  accumulator = val;
}

void XAA(uint8_t value)
{
  uint8_t result = accumulator & index_x & value;
  setflag(n, result & highbit(result));
  setflag(z, !result);
  accumulator &= index_x & (value | 0xEF);
}
