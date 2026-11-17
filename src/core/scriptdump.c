/* See scriptdump.h. */
#include "scriptdump.h"
#include "parser.h"
#include "internalvars.h"
#include "cmath.h"

typedef struct {
	char  *buf;
	uint16 len, size;
} Out;

SEG_SCRIPT static void put(Out *o, char c)
{
	if (o->len < o->size)
		o->buf[o->len++] = c;
}

SEG_SCRIPT static void put_bytes(Out *o, const uint8 *s, int16 n)
{
	while (n-- > 0)
		put(o, (char)*s++);
}

SEG_SCRIPT static void put_str(Out *o, const char *s)
{
	while (*s)
		put(o, *s++);
}

SEG_SCRIPT static void put_hex(Out *o, uint32 v, int16 digits)
{
	while (digits-- > 0)
		put(o, "0123456789ABCDEF"[(v >> (digits * 4)) & 15]);
}

/* Decimal, at least width digits with leading zeros. */
SEG_SCRIPT static void put_dec(Out *o, int32 v, int16 width)
{
	char tmp[12];
	int16 n = 0;
	uint32 u;

	if (v < 0) {
		put(o, '-');
		u = (uint32)0 - (uint32)v;
	} else
		u = (uint32)v;
	do {
		tmp[n++] = (char)('0' + u % 10);
		u /= 10;
	} while (u);
	while (width-- > n)
		put(o, '0');
	while (n)
		put(o, tmp[--n]);
}

SEG_SCRIPT uint16 script_dump_level(int16 level, char *buf, uint16 size)
{
	Out o;
	int32 t;
	double v;
	uint32 hi, lo;
	const uint8 *name;
	int16 len;

	o.buf = buf;
	o.len = 0;
	o.size = size;

	parser_load_level(level);
	parser_write(kConstDEPTH, 120.0);       /* DEPTH in STORM.h */

	for (t = kConstDEPTH; t <= kVarLast; t++) {
		v = parser_eval(t);
		cm_bits(v, &hi, &lo);
		name = parser_token_name(t, &len);
		put_dec(&o, level, 3);
		put(&o, ' ');
		put_dec(&o, t, 2);
		put(&o, ' ');
		put_bytes(&o, name, len);
		put(&o, ' ');
		put_hex(&o, hi, 8);
		put(&o, ':');
		put_hex(&o, lo, 8);
		put(&o, ' ');
		if (cm_isnan(v))
			put_str(&o, "nan");
		else if (v >= 2147483648.0 || v <= -2147483649.0)
			put_str(&o, "big");
		else
			put_dec(&o, (int32)v, 1);
		put(&o, '\n');
	}
	if (parser_error())
		put_str(&o, "parse error\n");
	return o.len;
}

static uint32 lcg_state;

SEG_SCRIPT static uint32 lcg_next(void)
{
	lcg_state = lcg_state * 1664525UL + 1013904223UL;
	return lcg_state;
}

/* A double in [0, 8) with a full 53-bit mantissa, made with exact
   operations only. */
SEG_SCRIPT static double test_value(void)
{
	double hi = (double)(lcg_next() >> 5);         /* 27 bits */
	double lo = (double)(lcg_next() >> 6);         /* 26 bits */
	return (hi * 67108864.0 + lo) * (1.0 / 1125899906842624.0);    /* 2^-50 */
}

SEG_SCRIPT static cbool same_bits(double x, double y)
{
	uint32 xh, xl, yh, yl;
	cm_bits(x, &xh, &xl);
	cm_bits(y, &yh, &yl);
	return (cbool)(xh == yh && xl == yl);
}

SEG_SCRIPT static uint32 hash_double(uint32 h, double v)
{
	uint32 hi, lo;
	cm_bits(v, &hi, &lo);
	h = (h ^ hi) * 16777619UL;
	return (h ^ lo) * 16777619UL;
}

SEG_SCRIPT static void put_case(Out *o, const char *name, double v)
{
	uint32 hi, lo;
	cm_bits(v, &hi, &lo);
	put_str(o, "maths case ");
	put_str(o, name);
	put(o, ' ');
	put_hex(o, hi, 8);
	put(o, ':');
	put_hex(o, lo, 8);
	put(o, '\n');
}

SEG_SCRIPT uint16 script_dump_maths(int16 n, char *buf, uint16 size)
{
	/* In .data on purpose: a table of string pointers would point into this segment. */
	static char names[] = "add\0sub\0mul\0cm_div\0cm_pow\0cm_fmod\0cm_floor\0"
	                      "cm_mul_small\0cm_div_small\0add_shift";
	const char *name = names;
	uint32 h[10];
	int32 raw_div_off = 0, raw_mul_off = 0;
	int16 i, k;
	double a, b, q;
	Out o;

	o.buf = buf;
	o.len = 0;
	o.size = size;
	lcg_state = 1;
	for (k = 0; k < 10; k++)
		h[k] = 2166136261UL;
	for (i = 0; i < n; i++) {
		a = test_value();
		b = test_value() + 0.001;
		if (i & 1)
			a = -a;
		if (i & 2)
			b = -b;
		h[0] = hash_double(h[0], a + b);
		h[1] = hash_double(h[1], a - b);
		h[2] = hash_double(h[2], a * b);
		q = cm_div(a, b);
		h[3] = hash_double(h[3], q);
		if (!same_bits(q, a / b))
			raw_div_off++;
		h[4] = hash_double(h[4], cm_pow(a < 0 ? -a : a, b * 0.25));
		h[5] = hash_double(h[5], cm_fmod(a * 64.0, b));
		h[6] = hash_double(h[6], cm_floor(a * 1024.0));
		/* Small whole numbers, as the script uses. */
		b = (double)(int32)(lcg_next() % 100UL + 1);
		q = cm_mul(a, b);
		h[7] = hash_double(h[7], q);
		if (!same_bits(q, a * b))
			raw_mul_off++;
		h[8] = hash_double(h[8], cm_div(a, b));
		/* Sums with the smaller operand shifted 0 to 63 places. */
		b = test_value();
		for (k = (int16)(i & 63); k > 0; k--)
			b *= 0.5;
		h[9] = hash_double(h[9], (i & 4) ? a + b : a - b);
	}
	for (k = 0; k < 10; k++) {
		put_str(&o, "maths ");
		put_str(&o, name);
		while (*name++)
			;
		put(&o, ' ');
		put_dec(&o, n, 1);
		put(&o, ' ');
		put_hex(&o, h[k], 8);
		put(&o, '\n');
	}
	/* EndTimer at level 21 is 3.2 / 21 * 60. */
	q = cm_div(32.0, 10.0);
	put_case(&o, "3.2", q);
	q = cm_div(q, 21.0);
	put_case(&o, "3.2/21", q);
	put_case(&o, "3.2/21*60", cm_mul(q, 60.0));
	put_str(&o, "# platform divide differs from cm_div in ");
	put_dec(&o, raw_div_off, 1);
	put_str(&o, " of ");
	put_dec(&o, n, 1);
	put_str(&o, ", multiply by a whole number from cm_mul in ");
	put_dec(&o, raw_mul_off, 1);
	put_str(&o, ", raw 3.2/21*60 ");
	{
		uint32 hi, lo;
		cm_bits(q * 60.0, &hi, &lo);
		put_hex(&o, hi, 8);
		put(&o, ':');
		put_hex(&o, lo, 8);
	}
	put(&o, '\n');
	return o.len;
}
