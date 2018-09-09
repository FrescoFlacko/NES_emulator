#include "cpu.h"

#define ANC (value) ({ accumulator &= value; \
  setflag(n, accumulator & highbit(accumulator)); \
  setflag(c, accumulator & highbit(accumulator)); })
  
#define AND (value) ({ accumulator &= value; \
  setflag(n, accumulator & highbit(accumulator)); \
  setflag(z, !accumulator); })
