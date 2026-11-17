/* Text dump of every InternalVars.h variable for one level, the same on
   every platform, for comparing the host and Palm interpreters. */
#ifndef CORE_SCRIPTDUMP_H
#define CORE_SCRIPTDUMP_H

#include "types.h"

#define SCRIPTDUMP_LEVEL_BYTES 6144

/* Loads the level the way STLoadLevel does (DEPTH written after the load),
   then writes one line per variable into buf:
     LLL TT NAME HHHHHHHH:LLLLLLLL INT
   level, token, script name, IEEE bits of the double, and the value
   truncated to an integer as the game's assignments do ("nan" or "big"
   where that has no value). Returns the length, without a terminator. */
SEG_SCRIPT uint16 script_dump_level(int16 level, char *buf, uint16 size);

/* Runs n inputs through +, -, *, cm_mul, cm_div, cm_pow, cm_fmod and
   cm_floor and writes one line per operation with a hash of all the result
   bits, so a platform whose float maths differs anywhere shows up in a
   diff. Then a "#" line counting how often the platform's own divide and
   multiply differ from cm_div and cm_mul (0 on IEEE hardware). */
SEG_SCRIPT uint16 script_dump_maths(int16 n, char *buf, uint16 size);

/* Inputs the host test and the Palm debug build use. */
#define SCRIPT_MATHS_N 1000

#endif
