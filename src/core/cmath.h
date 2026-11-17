/* The double maths the Arashi Script needs, in plain C with no libm, so the
   same code runs on every platform (soft float on Palm). Build with
   CORE_LIBM defined to use the C library instead, for comparison on the
   host. */
#ifndef CORE_CMATH_H
#define CORE_CMATH_H

#include "types.h"

#ifdef CORE_LIBM
#include <math.h>
#include <stdlib.h>
#define cm_floor floor
#define cm_fmod  fmod
#define cm_pow   pow
#define cm_sin   sin
#define cm_cos   cos
#define cm_div(a, b) ((a) / (b))
#define cm_mul(a, b) ((a) * (b))
SEG_MATH double cm_strtod(const uint8 *s, int16 len);
#else
SEG_MATH double cm_floor(double x);
SEG_MATH double cm_fmod(double x, double y);
SEG_MATH double cm_pow(double x, double y);
SEG_MATH double cm_sin(double x);
SEG_MATH double cm_cos(double x);
/* a * b and a / b, correctly rounded on every platform. */
SEG_MATH double cm_mul(double a, double b);
SEG_MATH double cm_div(double a, double b);
/* The same from a first guess within an ulp (cm_mul) or a few (cm_div),
   for tests. */
SEG_MATH double cm_mul_refine(double a, double b, double p);
SEG_MATH double cm_div_refine(double a, double b, double q);
/* Decimal text of len bytes, as matched by the script's MatchFloat:
   [+-]digits[.digits][(e|E)[+-]digits]. Correctly rounded when there are at
   most 15 significant digits and the decimal exponent is within 22. */
SEG_MATH double cm_strtod(const uint8 *s, int16 len);
#endif

/* The IEEE 754 bits of x as two 32-bit halves, whatever the byte order.
   Any NaN comes out as 7FF80000 00000000. */
SEG_MATH void cm_bits(double x, uint32 *hi, uint32 *lo);
SEG_MATH cbool cm_isnan(double x);
/* a < b and a == b, false when either is NaN, as IEEE says. Plain
   comparisons get NaN wrong on Palm. */
SEG_MATH cbool cm_lt(double a, double b);
SEG_MATH cbool cm_eq(double a, double b);

#endif
