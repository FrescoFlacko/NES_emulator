#include "cpu.h"

#ifndef C_OPCODES_H
#define C_OPCODES_H

enum address_mode { zero_page, ind_zero_page, absolute, ind_absolute, indirect, immediate, relative, ind_indirect};

struct instruction
{
  char name[3];
  enum address_mode mode;
  int size;
  int cycles;
};

extern struct instruction instruction_set[256];

void ADC(uint8_t value);
void ALR(uint8_t value);
void ANC(uint8_t value);
void AND(uint8_t value);
void ASL(uint8_t value, uint16_t address, int mode);
void Branch(uint8_t value);
void BCC(uint8_t value);
void BCS(uint8_t value);
void BEQ(uint8_t value);
void BIT(uint8_t value);
void BMI(uint8_t value);
void BNE(uint8_t value);
void BPL(uint8_t value);
void BRK(uint8_t value);
void BVC(uint8_t value);
void BVS(uint8_t value);
void CLC(uint8_t value);
void CLD(uint8_t value);
void CLI(uint8_t value);
void CLV(uint8_t value);
void CMP(uint8_t value);
void CPX(uint8_t value);
void CPY(uint8_t value);
void DCP(uint8_t value);
void DEC(uint16_t address, uint8_t value);
void DEX();
void DEY();
void EOR(uint8_t value);
void INC(uint16_t address, uint8_t value);
void INX();
void INY();
void ISC(uint8_t value);
void JMP(uint16_t address);
void JSR(uint16_t address);
void LAX(uint8_t value);
void LDA(uint8_t value);
void LDX(uint8_t value);
void LDY(uint8_t value);
void LSR(uint8_t value, uint16_t address, int mode);
void NOP(uint8_t value);
void ORA(uint8_t value);
void PHA();
void PHP();
void PLA(uint8_t value);
void PLP(uint8_t value);
void RLA(uint8_t value);
void ROL(uint8_t value, uint16_t address, int mode);
void ROR(uint8_t value, uint16_t address, int mode);
void RRA(uint8_t value);
void RTI(uint8_t value);
void RTS(uint8_t value);
void SAX(uint8_t value);
void SBC(uint8_t value);
void SEC(uint8_t value);
void SED(uint8_t value);
void SEI(uint8_t value);
void SLO(uint8_t value);
void SRE(uint8_t value);
void STA(uint16_t address, uint8_t value);
void STX(uint16_t address);
void STY(uint16_t address);
void TAX();
void TAY();
void TSX();
void TXA();
void TXS();
void TYA();

#endif
