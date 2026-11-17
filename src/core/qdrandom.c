/* QuickDraw Random(): randSeed = randSeed * 16807 mod (2^31 - 1)
   (Park and Miller's minimal standard), result is the low 16 bits of the
   new seed as a signed word, with -32768 returned as 0. InitGraf sets
   randSeed to 1. */
#include "qdrandom.h"

static int32 rand_seed = 1;

SEG_SCRIPT void qd_seed(int32 seed)
{
	rand_seed = seed;
}

SEG_SCRIPT int32 qd_get_seed(void)
{
	return rand_seed;
}

SEG_SCRIPT int16 qd_random(void)
{
	/* Schrage's method: 16807 * seed mod m without overflowing 32 bits.
	   m = 127773 * 16807 + 2836. */
	uint32 s = (uint32)rand_seed & 0x7FFFFFFFUL;
	int32 hi, lo, t;
	uint16 w;

	if (s >= 0x7FFFFFFFUL)
		s -= 0x7FFFFFFFUL;
	hi = (int32)(s / 127773UL);
	lo = (int32)(s % 127773UL);
	t = 16807L * lo - 2836L * hi;
	if (t < 0)
		t += 0x7FFFFFFFL;
	rand_seed = t;
	w = (uint16)((uint32)t & 0xFFFFU);
	if (w == 0x8000U)
		return 0;
	return (int16)(w >= 0x8000U ? (int32)w - 0x10000L : (int32)w);
}
