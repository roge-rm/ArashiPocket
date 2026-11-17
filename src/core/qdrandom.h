/* The Mac QuickDraw Random() generator, with its seed (randSeed) held here
   so runs are reproducible. */
#ifndef CORE_QDRANDOM_H
#define CORE_QDRANDOM_H

#include "types.h"

SEG_SCRIPT void  qd_seed(int32 seed);
SEG_SCRIPT int32 qd_get_seed(void);
/* -32767..32767, as Random() returns. */
SEG_SCRIPT int16 qd_random(void);

#endif
