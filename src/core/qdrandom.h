/* The Mac QuickDraw Random() generator, with its seed (randSeed) held here
   so runs are reproducible. */
#ifndef CORE_QDRANDOM_H
#define CORE_QDRANDOM_H

#include "types.h"

void  qd_seed(int32 seed);
int32 qd_get_seed(void);
/* -32767..32767, as Random() returns. */
int16 qd_random(void);

#endif
