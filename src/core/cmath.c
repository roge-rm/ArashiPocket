/* Double maths for the Arashi Script without libm. pow works in
   double-double (about 106 bits) through log and exp, so it rounds
   correctly except in rare near-halfway cases. floor, fmod and cm_div are
   exact or correctly rounded. sin and cos are plain double, within about
   an ulp; the shipped script doesn't use them.

   It relies on + and - being correctly rounded IEEE double (round to
   nearest), and * and / being within an ulp. The Palm's libgcc soft float
   (lb1sf68) gets this far but no further: its compare (__cmpdf2) only
   checks the low word for NaN, so NaN tests here look at the bits, and its
   multiply drops the lowest product bits before rounding, so it can be an
   ulp out; cm_mul and cm_div correct for that, and the exact products
   double-double needs come from a bit split. Must not be built with
   -ffast-math or anything that reorders float operations. */
#include "cmath.h"

#define TWO32  4294967296.0
#define TWOM32 (1.0 / 4294967296.0)
#define TWO52  4503599627370496.0

typedef union {
	double d;
	uint8  b[8];
} DBytes;

/* The two 32-bit halves of a double, for big- or little-endian storage
   (worked out from how 1.0 is stored). */
SEG_MATH static void get_words(double x, uint32 *hi, uint32 *lo)
{
	DBytes u, one;
	int16 i;

	one.d = 1.0;
	u.d = x;
	if (one.b[0] == 0x3F) {
		*hi = get32(u.b);
		*lo = get32(u.b + 4);
	} else {
		*hi = *lo = 0;
		for (i = 7; i >= 4; i--)
			*hi = (*hi << 8) | u.b[i];
		for (i = 3; i >= 0; i--)
			*lo = (*lo << 8) | u.b[i];
	}
}

SEG_MATH cbool cm_isnan(double x)
{
	uint32 hi, lo;
	get_words(x, &hi, &lo);
	return (cbool)((hi & 0x7FF00000UL) == 0x7FF00000UL && ((hi & 0x000FFFFFUL) | lo) != 0);
}

SEG_MATH cbool cm_lt(double a, double b)
{
	return (cbool)(!cm_isnan(a) && !cm_isnan(b) && a < b);
}

SEG_MATH cbool cm_eq(double a, double b)
{
	return (cbool)(!cm_isnan(a) && !cm_isnan(b) && a == b);
}

#ifndef CORE_LIBM

SEG_MATH static cbool is_inf(double x)
{
	uint32 hi, lo;
	get_words(x, &hi, &lo);
	return (cbool)((hi & 0x7FFFFFFFUL) == 0x7FF00000UL && lo == 0);
}

/* x * 2^k, exact unless the result over- or underflows. */
SEG_MATH static double scale2(double x, int32 k)
{
	double p = 1.0;

	while (k >= 32) {
		x *= TWO32;
		k -= 32;
	}
	while (k <= -32) {
		x *= TWOM32;
		k += 32;
	}
	while (k > 0) {
		p *= 2.0;
		k--;
	}
	while (k < 0) {
		p *= 0.5;
		k++;
	}
	return x * p;
}

/* Splits finite x > 0 into m * 2^e with m in [1, 2). */
SEG_MATH static double split_exp(double x, int32 *e)
{
	int32 n = 0;

	while (x >= TWO32) {
		x *= TWOM32;
		n += 32;
	}
	while (x < TWOM32) {
		x *= TWO32;
		n -= 32;
	}
	while (x >= 2.0) {
		x *= 0.5;
		n++;
	}
	while (x < 1.0) {
		x *= 2.0;
		n--;
	}
	*e = n;
	return x;
}

typedef struct {
	double hi, lo;
} dd;

/* Exact values, written as integer / power of two so no decimal
   conversion is involved. */
static dd LN2 = { (6243314768165359.0 / 4503599627370496.0 / 2.0),
	(7525737178955839.0 / 4503599627370496.0 / 4503599627370496.0 / 16.0) };
static dd INV3 = { (6004799503160661.0 / 4503599627370496.0 / 4.0),
	(6004799503160661.0 / 4503599627370496.0 / 4503599627370496.0 / 16.0) };
static dd INV5 = { (3602879701896397.0 / 4503599627370496.0 / 4.0),
	(-3602879701896397.0 / 4503599627370496.0 / 4503599627370496.0 / 16.0) };
static dd INV7 = { (2573485501354569.0 / 4503599627370496.0 / 4.0),
	(2573485501354569.0 / 4503599627370496.0 / 4503599627370496.0 / 16.0) };
static dd INV9 = { (2001599834386887.0 / 4503599627370496.0 / 4.0),
	(2001599834386887.0 / 4503599627370496.0 / 4503599627370496.0 / 16.0) };
static dd INV6 = { (6004799503160661.0 / 4503599627370496.0 / 8.0),
	(6004799503160661.0 / 4503599627370496.0 / 4503599627370496.0 / 32.0) };

SEG_MATH static double make_inf(void)
{
	volatile double big = 1e300;
	return big * big;
}

SEG_MATH static double make_nan(void)
{
	double i = make_inf();
	return i - i;
}

/* Double-double helpers (Dekker, Knuth). */
SEG_MATH static dd dd_d(double a)
{
	dd r;
	r.hi = a;
	r.lo = 0.0;
	return r;
}

SEG_MATH static dd dd_neg(dd a)
{
	a.hi = -a.hi;
	a.lo = -a.lo;
	return a;
}

SEG_MATH static dd quick_two_sum(double a, double b)
{
	dd r;
	r.hi = a + b;
	r.lo = b - (r.hi - a);
	return r;
}

SEG_MATH static dd two_sum(double a, double b)
{
	dd r;
	double bb;
	r.hi = a + b;
	bb = r.hi - a;
	r.lo = (a - (r.hi - bb)) + (b - bb);
	return r;
}

SEG_MATH static double from_words(uint32 hi, uint32 lo)
{
	DBytes u, one;
	int16 i;

	one.d = 1.0;
	if (one.b[0] == 0x3F) {
		for (i = 3; i >= 0; i--) {
			u.b[i] = (uint8)hi;
			u.b[i + 4] = (uint8)lo;
			hi >>= 8;
			lo >>= 8;
		}
	} else {
		for (i = 0; i < 4; i++) {
			u.b[i + 4] = (uint8)hi;
			u.b[i] = (uint8)lo;
			hi >>= 8;
			lo >>= 8;
		}
	}
	return u.d;
}

/* Splits a into hi + lo, each with at most 26 significant bits, so their
   products are exact. Done on the bits (hi is a rounded to 26 bits)
   rather than Veltkamp's multiply by 2^27 + 1, which needs a correctly
   rounded multiply. */
SEG_MATH static void split(double a, double *hi, double *lo)
{
	uint32 h, l;

	get_words(a, &h, &l);
	if ((h & 0x7FF00000UL) == 0 || (h & 0x7FF00000UL) == 0x7FF00000UL) {
		*hi = a;        /* zero, subnormal, infinity or NaN */
		*lo = 0.0;
		return;
	}
	/* Keep 1 + 20 + 5 significand bits, rounding on the next one. */
	if (l & 0x04000000UL) {
		l = (l & 0xF8000000UL) + 0x08000000UL;
		if (l == 0)
			h++;
	} else
		l &= 0xF8000000UL;
	*hi = from_words(h, l);
	*lo = a - *hi;          /* exact */
}

/* The exact error of p = a * b, given p within an ulp of the product.
   The partial products are exact, and so are the sums, since each result
   fits in a double. */
SEG_MATH static double prod_error(double a, double b, double p)
{
	double ah, al, bh, bl;
	split(a, &ah, &al);
	split(b, &bh, &bl);
	return ((ah * bh - p) + ah * bl + al * bh) + al * bl;
}

SEG_MATH static dd two_prod(double a, double b)
{
	dd r;
	r.hi = a * b;
	r.lo = prod_error(a, b, r.hi);
	return r;
}

SEG_MATH static dd dd_add(dd a, dd b)
{
	dd s, t;
	s = two_sum(a.hi, b.hi);
	t = two_sum(a.lo, b.lo);
	s.lo += t.hi;
	s = quick_two_sum(s.hi, s.lo);
	s.lo += t.lo;
	return quick_two_sum(s.hi, s.lo);
}

SEG_MATH static dd dd_mul(dd a, dd b)
{
	dd p = two_prod(a.hi, b.hi);
	p.lo += a.hi * b.lo + a.lo * b.hi;
	return quick_two_sum(p.hi, p.lo);
}

SEG_MATH static dd dd_mul_d(dd a, double b)
{
	dd p = two_prod(a.hi, b);
	p.lo += a.lo * b;
	return quick_two_sum(p.hi, p.lo);
}

SEG_MATH static dd dd_div(dd a, dd b)
{
	double q1, q2, q3;
	dd r;
	q1 = a.hi / b.hi;
	r = dd_add(a, dd_neg(dd_mul_d(b, q1)));
	q2 = r.hi / b.hi;
	r = dd_add(r, dd_neg(dd_mul_d(b, q2)));
	q3 = r.hi / b.hi;
	r = quick_two_sum(q1, q2);
	return dd_add(r, dd_d(q3));
}

/* The next double from x (finite, nonzero) away from zero (dir > 0) or
   towards it (dir < 0). */
SEG_MATH static double step_ulp(double x, int dir)
{
	uint32 hi, lo;

	get_words(x, &hi, &lo);
	if (dir > 0) {
		if (++lo == 0)
			hi++;
	} else {
		if (lo-- == 0)
			hi--;
	}
	return from_words(hi, lo);
}

/* a - q * b, exact or nearly when q is close to a / b. */
SEG_MATH static double remainder_of(double a, double b, double q)
{
	dd p = two_prod(q, b);
	return (a - p.hi) - p.lo;
}

/* a / b correctly rounded. Starts from the platform's divide, q, and steps
   an ulp at a time while that brings q * b closer to a. */
SEG_MATH double cm_div(double a, double b)
{
	return cm_div_refine(a, b, a / b);
}

SEG_MATH double cm_div_refine(double a, double b, double q)
{
	double r, q2, r2;
	uint32 hi, lo;
	int i, dir;

	get_words(q, &hi, &lo);
	hi &= 0x7FFFFFFFUL;
	/* Leave zero, infinity, NaN, and quotients near under- or overflow
	   where two_prod could overflow. */
	if (hi < 0x00200000UL || hi >= 0x7FD00000UL || is_inf(a) || is_inf(b))
		return q;
	r = remainder_of(a, b, q);
	for (i = 0; i < 16 && r != 0.0; i++) {
		/* a / b - q = r / b; step away from zero when that has the
		   sign of q. */
		dir = (((r < 0.0) == (b < 0.0)) == (q > 0.0)) ? 1 : -1;
		q2 = step_ulp(q, dir);
		r2 = remainder_of(a, b, q2);
		if ((r2 < 0.0 ? -r2 : r2) >= (r < 0.0 ? -r : r))
			break;
		q = q2;
		r = r2;
	}
	return q;
}

/* a * b correctly rounded. The platform's multiply can be an ulp out
   (Palm's drops low product bits before rounding), so its result is
   corrected with the exact error. */
SEG_MATH double cm_mul(double a, double b)
{
	return cm_mul_refine(a, b, a * b);
}

SEG_MATH double cm_mul_refine(double a, double b, double p)
{
	double e, p2, e2, ae, ae2;
	uint32 hi, lo;
	int i, dir;

	get_words(p, &hi, &lo);
	hi &= 0x7FFFFFFFUL;
	if (hi < 0x00200000UL || hi >= 0x7FD00000UL || is_inf(a) || is_inf(b))
		return p;
	e = prod_error(a, b, p);
	for (i = 0; i < 4 && e != 0.0; i++) {
		dir = ((e < 0.0) == (p < 0.0)) ? 1 : -1;
		p2 = step_ulp(p, dir);
		e2 = e - (p2 - p);      /* both exact */
		ae = e < 0.0 ? -e : e;
		ae2 = e2 < 0.0 ? -e2 : e2;
		/* On a tie take the even one. */
		get_words(p2, &hi, &lo);
		if (ae2 > ae || (ae2 == ae && (lo & 1)))
			break;
		p = p2;
		e = e2;
	}
	return p;
}

/* log(x) for finite x > 0, to about 2^-75 relative. x = m * 2^e with m in
   [sqrt(1/2), sqrt(2)], log m = 2 atanh(s) with s = (m-1)/(m+1), so
   |s| <= 0.172 and the series in z = s^2 drops by about 5 bits a term. */
SEG_MATH static dd log_dd(double x)
{
	static dd *inv[4] = { &INV3, &INV5, &INV7, &INV9 };
	int32 e;
	double m, zd, pd, tail, k;
	dd s, z, p, sum;
	int i;

	m = split_exp(x, &e);
	if (m > 1.4142135623730951) {
		m *= 0.5;
		e++;
	}
	s = dd_div(dd_d(m - 1.0), two_sum(m, 1.0));   /* m - 1 is exact */
	z = dd_mul(s, s);

	/* sum = z/3 + z^2/5 + ...: the first four terms in double-double, the
	   rest (below 2^-29) in double. */
	sum = dd_d(0.0);
	p = z;
	for (i = 0; i < 4; i++) {
		sum = dd_add(sum, dd_mul(p, *inv[i]));
		p = dd_mul(p, z);
	}
	zd = z.hi;
	pd = p.hi;
	tail = 0.0;
	for (k = 11.0; k < 40.0; k += 2.0) {
		tail += pd / k;
		pd *= zd;
	}
	sum = dd_add(sum, dd_d(tail));

	/* 2s + 2s * sum + e * ln2 */
	s = dd_mul_d(s, 2.0);
	s = dd_add(s, dd_mul(s, sum));
	return dd_add(s, dd_mul_d(LN2, (double)e));
}

/* exp(t) rounded to double. t = k ln2 + r, then exp(r) = (1 + u)^256 with
   u = expm1(r / 256) from its series, squared up as 2u + u^2. */
SEG_MATH static double exp_dd(dd t)
{
	double kd, rd, p, tail;
	dd r, r2, r3, u;
	int i;

	if (t.hi > 709.8)
		return make_inf();
	if (t.hi < -745.2)
		return 0.0;
	kd = cm_floor(t.hi / LN2.hi + 0.5);
	r = dd_add(t, dd_neg(dd_mul_d(LN2, kd)));
	r.hi *= 1.0 / 256.0;
	r.lo *= 1.0 / 256.0;

	/* |r| <= 0.00136: r + r^2/2 + r^3/6 in double-double, r^4/24 on
	   (below 2^-42) in double. */
	r2 = dd_mul(r, r);
	r3 = dd_mul(r2, r);
	u = dd_add(r, dd_add(dd_mul_d(r2, 0.5), dd_mul(r3, INV6)));
	rd = r.hi;
	p = r3.hi * rd;
	tail = p / 24.0;
	p *= rd;
	tail += p / 120.0;
	p *= rd;
	tail += p / 720.0;
	p *= rd;
	tail += p / 5040.0;
	u = dd_add(u, dd_d(tail));

	for (i = 0; i < 8; i++)
		u = dd_add(dd_mul_d(u, 2.0), dd_mul(u, u));
	u = dd_add(dd_d(1.0), u);
	return scale2(u.hi, (int32)kd);
}

SEG_MATH double cm_floor(double x)
{
	double ax, t;

	if (cm_isnan(x) || x == 0.0)
		return x;
	ax = x < 0.0 ? -x : x;
	if (ax >= TWO52)        /* already whole, or infinite */
		return x;
	t = (ax + TWO52) - TWO52;       /* ax rounded to nearest */
	if (x < 0.0)
		t = -t;
	if (t > x)
		t -= 1.0;
	return t;
}

/* Exact: subtracts y * 2^k, the largest that fits, until under |y|. Each
   subtraction is exact since the operands are within a factor of two. */
SEG_MATH double cm_fmod(double x, double y)
{
	double ax, ay, d;

	if (cm_isnan(x) || cm_isnan(y) || y == 0.0 || is_inf(x))
		return make_nan();
	if (is_inf(y))
		return x;
	ax = x < 0.0 ? -x : x;
	ay = y < 0.0 ? -y : y;
	while (ax >= ay) {
		d = ay;
		while (d * 2.0 <= ax)
			d *= 2.0;
		ax -= d;
	}
	return x < 0.0 ? -ax : ax;
}

SEG_MATH double cm_pow(double x, double y)
{
	double r, half;
	cbool neg = 0;

	if (y == 0.0 || x == 1.0)
		return 1.0;
	if (cm_isnan(x) || cm_isnan(y))
		return make_nan();
	if (is_inf(y)) {
		r = x < 0.0 ? -x : x;
		if (r == 1.0)
			return 1.0;
		return ((r > 1.0) == (y > 0.0)) ? make_inf() : 0.0;
	}
	if (x < 0.0) {
		if (cm_floor(y) != y)
			return make_nan();
		half = y * 0.5;
		neg = (cbool)(cm_floor(half) != half);
		x = -x;
	}
	if (x == 0.0 || is_inf(x))
		r = ((x == 0.0) == (y > 0.0)) ? 0.0 : make_inf();
	else
		r = exp_dd(dd_mul_d(log_dd(x), y));
	return neg ? -r : r;
}

/* sin and cos: reduce by pi/2 (fdlibm's 33 + 53 bit split), then the
   fdlibm kernel polynomials on [-pi/4, pi/4]. Fine for |x| up to about
   2^20. */
SEG_MATH static double k_sin(double r)
{
	double z = r * r;
	return r + r * z * (-1.66666666666666324348e-01 + z * (8.33333333332248946124e-03 +
		z * (-1.98412698298579493134e-04 + z * (2.75573137070700676789e-06 +
		z * (-2.50507602534068634195e-08 + z * 1.58969099521155010221e-10)))));
}

SEG_MATH static double k_cos(double r)
{
	double z = r * r;
	return 1.0 - 0.5 * z + z * z * (4.16666666666666019037e-02 + z * (-1.38888888888741095749e-03 +
		z * (2.48015872894767294178e-05 + z * (-2.75573143513906633035e-07 +
		z * (2.08757232129817482790e-09 + z * -1.13596475577881948265e-11)))));
}

SEG_MATH static double trig(double x, int quadrant)
{
	double k, r;
	int n;

	if (cm_isnan(x) || is_inf(x))
		return make_nan();
	k = cm_floor(x * 6.36619772367581382433e-01 + 0.5);       /* 2/pi */
	r = (x - k * 1.57079632673412561417e+00) - k * 6.07710050650619224932e-11;
	n = (int)(cm_fmod(k, 4.0) + 4.0) & 3;
	n = (n + quadrant) & 3;
	switch (n) {
	case 0:
		return k_sin(r);
	case 1:
		return k_cos(r);
	case 2:
		return -k_sin(r);
	default:
		return -k_cos(r);
	}
}

SEG_MATH double cm_sin(double x)
{
	return trig(x, 0);
}

SEG_MATH double cm_cos(double x)
{
	return trig(x, 1);
}

SEG_MATH double cm_strtod(const uint8 *s, int16 len)
{
	double mant = 0.0, p, r;
	int32 exp10 = 0, e = 0;
	int16 i = 0, digits = 0;
	cbool neg = 0, eneg = 0;

	if (i < len && (s[i] == '+' || s[i] == '-'))
		neg = (cbool)(s[i++] == '-');
	for (; i < len && s[i] >= '0' && s[i] <= '9'; i++) {
		if (digits < 18) {
			mant = mant * 10.0 + (s[i] - '0');
			if (mant != 0.0)
				digits++;
		} else
			exp10++;
	}
	if (i < len && s[i] == '.') {
		for (i++; i < len && s[i] >= '0' && s[i] <= '9'; i++) {
			if (digits < 18) {
				mant = mant * 10.0 + (s[i] - '0');
				if (mant != 0.0)
					digits++;
				exp10--;
			}
		}
	}
	if (i < len && (s[i] == 'e' || s[i] == 'E')) {
		i++;
		if (i < len && (s[i] == '+' || s[i] == '-'))
			eneg = (cbool)(s[i++] == '-');
		for (; i < len && s[i] >= '0' && s[i] <= '9'; i++)
			if (e < 10000)
				e = e * 10 + (s[i] - '0');
		exp10 += eneg ? -e : e;
	}

	/* With up to 15 digits mant is exact, and so is 10^n for n <= 22, so
	   one multiply or divide rounds correctly. Outside that it is close
	   but not always correctly rounded. */
	r = mant;
	if (mant != 0.0) {
		int32 n = exp10 < 0 ? -exp10 : exp10;
		p = 1.0;
		if (n > 400)
			n = 400;
		while (n > 22 && r != 0.0 && !is_inf(r)) {
			r = exp10 < 0 ? cm_div(r, 1e22) : cm_mul(r, 1e22);
			n -= 22;
		}
		while (n-- > 0)
			p *= 10.0;
		r = exp10 < 0 ? cm_div(r, p) : cm_mul(r, p);
	}
	return neg ? -r : r;
}

#else /* CORE_LIBM */

SEG_MATH double cm_strtod(const uint8 *s, int16 len)
{
	char buf[64];
	int16 i;

	if (len > 63)
		len = 63;
	for (i = 0; i < len; i++)
		buf[i] = (char)s[i];
	buf[len] = 0;
	return strtod(buf, NULL);
}

#endif /* CORE_LIBM */

SEG_MATH void cm_bits(double x, uint32 *hi, uint32 *lo)
{
	if (cm_isnan(x)) {
		*hi = 0x7FF80000UL;
		*lo = 0;
	} else
		get_words(x, hi, lo);
}
