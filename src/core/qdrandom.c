/* The three random streams of the original.
   qd_random is QuickDraw Random(), on randSeed. fast_random is FastRandom(),
   on the same randSeed. va_random is VARandom(), on its own VARandSeed.
   All three step seed = seed * 16807 mod (2^31 - 1) the way the original's
   68k code does (same arithmetic, so any saved seed gives the same run) and
   return the low 16 bits as a signed word. Only Random() maps -32768 to 0.
   InitGraf sets randSeed to 1. */
#include "qdrandom.h"

#ifdef RAND_TRACE
/* Host tests only: sees every call, stream 0 Random, 1 FastRandom, 2 VARandom. */
void (*rand_trace)(int stream, int16 value);
#define TRACE(s, v) do { if (rand_trace) rand_trace((s), (v)); } while (0)
#else
#define TRACE(s, v) do { } while (0)
#endif

static uint32 rand_seed = 1;      /* randSeed */
static uint32 va_rand_seed = 1;   /* VARandSeed */

/* The step of VARandom and FastRandom, written out as the 68k code does. */
SEG_SCRIPT static int16 lehmer_step(uint32 *seedp)
{
	uint32 s = *seedp;
	uint32 d0 = 0x41A7UL * (s & 0xFFFFUL);
	uint32 d1 = d0 >> 16;
	uint32 d2 = 0x41A7UL * (s >> 16);

	d2 += d1;
	d1 = ((d2 + d2) & 0xFFFFFFFFUL) >> 16;
	d0 = ((d0 & 0xFFFFUL) - 0x7FFFFFFFUL) & 0xFFFFFFFFUL;
	d2 = (d2 & 0x7FFFUL) << 16;
	d2 += d1;
	d0 = (d0 + d2) & 0xFFFFFFFFUL;
	if (d0 & 0x80000000UL)
		d0 = (d0 + 0x7FFFFFFFUL) & 0xFFFFFFFFUL;
	*seedp = d0;
	{
		uint16 w = (uint16)(d0 & 0xFFFFUL);
		return (int16)(w >= 0x8000U ? (int32)w - 0x10000L : (int32)w);
	}
}

SEG_SCRIPT void qd_seed(int32 seed)
{
	rand_seed = (uint32)seed;
}

SEG_SCRIPT int32 qd_get_seed(void)
{
	return (int32)rand_seed;
}

SEG_SCRIPT int16 qd_random(void)
{
	int16 r = lehmer_step(&rand_seed);

	if (r == -32768)
		r = 0;
	TRACE(0, r);
	return r;
}

SEG_SCRIPT int16 fast_random(void)
{
	int16 r = lehmer_step(&rand_seed);

	TRACE(1, r);
	return r;
}

SEG_SCRIPT void va_seed(int32 seed)
{
	va_rand_seed = (uint32)seed;
}

SEG_SCRIPT int32 va_get_seed(void)
{
	return (int32)va_rand_seed;
}

SEG_SCRIPT int16 va_random(void)
{
	int16 r = lehmer_step(&va_rand_seed);

	TRACE(2, r);
	return r;
}
