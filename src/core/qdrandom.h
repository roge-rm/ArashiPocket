/* The original's random streams, each with its own seed:
   qd_random   QuickDraw Random(), used by the script (randSeed)
   fast_random FastRandom(), steps the same randSeed (fuseoff)
   va_random   VARandom(), used by the enemies and the level field (VARandSeed) */
#ifndef CORE_QDRANDOM_H
#define CORE_QDRANDOM_H

#include "types.h"

SEG_SCRIPT void  qd_seed(int32 seed);
SEG_SCRIPT int32 qd_get_seed(void);
/* -32767..32767, as Random() returns. */
SEG_SCRIPT int16 qd_random(void);
/* -32768..32767, as FastRandom() returns. */
SEG_SCRIPT int16 fast_random(void);

SEG_SCRIPT void  va_seed(int32 seed);
SEG_SCRIPT int32 va_get_seed(void);
/* -32768..32767, as VARandom() returns. */
SEG_SCRIPT int16 va_random(void);

#endif
