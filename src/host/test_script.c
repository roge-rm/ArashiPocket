/* Host test for the Arashi Script interpreter. Loads levels 1 to 99 with a
   fixed random seed and writes every InternalVars.h variable per level to
   OUT (see scriptdump.h). Checks the tokens the prescript gives those
   variables match InternalVars.h. Without CORE_LIBM it also compares
   cm_pow, cm_fmod, cm_floor, cm_sin, cm_cos and cm_strtod with the C
   library.

   With MATHS, also writes the script_dump_maths lines there.

   Usage: test_script DATADIR OUT [MATHS] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../core/parser.h"
#include "../core/internalvars.h"
#include "../core/scriptdump.h"
#include "../core/qdrandom.h"
#include "../core/cmath.h"
#include "plat_host.h"

/* Script names of the InternalVars.h variables, in token order. */
static const char *const names[] = {
	"DEPTH", "iLevelNumber", "iLevelField", "iLevelColor", "iLevelBonus",
	"iLevelStartBonus", "iLevelNext", "iPlayerMaxMove", "iPlayerSuperZaps",
	"iShotSpeed", "iShotPower", "iShotColor[0]", "iShotColor[1]",
	"iShotColor[2]", "iShotColor[3]", "iShotColor[4]",
	"iFlipperPoints", "iFlipperCount", "iFlipperProb", "iFlipperSpeed",
	"iFlipperRot", "iFlipperColor",
	"iTank[0].points", "iTank[0].count", "iTank[0].prob", "iTank[0].speed", "iTank[0].color",
	"iTank[1].points", "iTank[1].count", "iTank[1].prob", "iTank[1].speed", "iTank[1].color",
	"iTank[2].points", "iTank[2].count", "iTank[2].prob", "iTank[2].speed", "iTank[2].color",
	"iFusePoints", "iFuseBullseye", "iFuseCount", "iFuseProb", "iFuseWarpP",
	"iFusePlayerPlus", "iFuseColor[0]", "iFuseColor[1]", "iFuseColor[2]",
	"iFuseColor[3]", "iFuseColor[4]",
	"iPulsarPoints", "iPulsarCount", "iPulsarProb", "iPulsarSpeed",
	"iPulsarRot", "iPulsarColor", "iPulsarTime", "iPulsarPulsDepth",
	"iSpikerPoints", "iSpikerProb", "iSpikerSpeed", "iSpikerStart",
	"iSpikerTop", "iSpikerPlasma", "iSpikerPlSpeed", "iSpikerPlPoints",
	"iSpikerSpikePoints", "iSpikerColor[0]", "iSpikerColor[1]", "iSpikerColor[2]",
	"iBoredomCount", "iBoredProb", "iEndTimer",
	"iAllowPracticeRestart", "iAllowArcadeRestart", "iShowSpikesMsg", "iDoomsday"
};

static int check_names(void)
{
	int32 t;
	int16 len;
	const uint8 *w;
	int bad = 0;

	if (sizeof names / sizeof names[0] != (size_t)(kVarLast - kConstDEPTH + 1)) {
		printf("FAIL name table has %d entries\n", (int)(sizeof names / sizeof names[0]));
		return 1;
	}
	for (t = kConstDEPTH; t <= kVarLast; t++) {
		const char *want = names[t - kConstDEPTH];
		w = parser_token_name(t, &len);
		if (!w || (size_t)len != strlen(want) || memcmp(w, want, (size_t)len) != 0) {
			printf("FAIL token %d is %.*s, InternalVars.h wants %s\n",
			       (int)t, w ? (int)len : 0, w ? (const char *)w : "", want);
			bad = 1;
		}
	}
	return bad;
}

#ifndef CORE_LIBM

static int same(double a, double b)
{
	return (a != a && b != b) || memcmp(&a, &b, sizeof a) == 0;
}

/* Where cm_pow and libm differ, checks against long double (x87, 64-bit
   mantissa) which is nearer. glibc's pow is within 0.52 ulp, not always
   correctly rounded, so cmath may be the right one. Returns 1 if libm is
   nearer. */
static int pow_worse(double x, double y, double mine, double lib)
{
	long double t = __builtin_powl((long double)x, (long double)y);
	long double em = (long double)mine - t, el = (long double)lib - t;
	if (em < 0) em = -em;
	if (el < 0) el = -el;
	return em > el;
}

static long pow_libm_off;

static int pow_differs(double x, double y)
{
	double a = cm_pow(x, y), b = pow(x, y);
	if (same(a, b))
		return 0;
	if (a == a && b == b && !pow_worse(x, y, a, b)) {
		pow_libm_off++;
		return 0;
	}
	printf("  pow(%.17g, %.17g) = %.17g, libm %.17g\n", x, y, a, b);
	return 1;
}

/* Small deterministic generator for test inputs. */
static unsigned long lcg = 12345;

static double urand(void)
{
	lcg = lcg * 1103515245UL + 12345UL;
	return (double)((lcg >> 8) & 0xFFFFFFUL) / 16777216.0;
}

static int check_maths(void)
{
	static const double ys[] = { 0.3, 0.4, 0.6, 0.7, 1.5 };
	long i, n, pow_bad = 0, pow_n = 0, misc_bad = 0, misc_n = 0;
	double x, y, worst_trig = 0;
	char buf[64];

	/* pow over the script's range: bases 0 to 200 in 1/240 steps (covers
	   Level/1.5, Level/12, Level/20, Level/3 and whole numbers) with the
	   script's exponents, then random bases and exponents. */
	for (i = 0; i <= 48000; i++) {
		x = i / 240.0;
		for (n = 0; n < 5; n++) {
			pow_n++;
			pow_bad += pow_differs(x, ys[n]);
		}
	}
	for (i = 0; i < 200000; i++) {
		x = urand() * 1000.0;
		y = (urand() - 0.5) * 20.0;
		pow_n++;
		pow_bad += pow_differs(x, y);
	}
	/* Special cases. */
	{
		static const double sx[] = { -2, -1, -0.5, 0, 0.5, 1, 2, 10 };
		static const double sy[] = { -3, -2, -1.5, -1, -0.5, 0, 0.5, 1, 2, 3 };
		size_t a, b;
		for (a = 0; a < sizeof sx / sizeof sx[0]; a++)
			for (b = 0; b < sizeof sy / sizeof sy[0]; b++) {
				pow_n++;
				pow_bad += pow_differs(sx[a], sy[b]);
			}
	}

	for (i = 0; i < 200000; i++) {
		x = (urand() - 0.5) * 2000.0;
		y = (urand() - 0.5) * 40.0;
		if (i % 4 == 0)
			x = floor(x);
		if (i % 8 == 0)
			y = floor(y);
		misc_n += 2;
		if (y != 0 && !same(cm_fmod(x, y), fmod(x, y)))
			misc_bad++, printf("  fmod(%.17g, %.17g)\n", x, y);
		if (!same(cm_floor(x), floor(x)))
			misc_bad++, printf("  floor(%.17g)\n", x);
		{
			double e1 = fabs(cm_sin(x) - sin(x)), e2 = fabs(cm_cos(x) - cos(x));
			if (e1 > worst_trig) worst_trig = e1;
			if (e2 > worst_trig) worst_trig = e2;
		}
	}
	/* cm_div from guesses up to 3 ulps out, over a wide range. */
	for (i = 0; i < 200000; i++) {
		double q, g;
		int k;
		x = (urand() + urand() * 1e-7) * pow(2.0, (int)(urand() * 200) - 100);
		y = (urand() + urand() * 1e-7) * pow(2.0, (int)(urand() * 200) - 100);
		if (i & 1)
			x = -x;
		if (i & 2)
			y = -y;
		if (y == 0)
			continue;
		q = x / y;
		g = q;
		for (k = (int)(urand() * 7) - 3; k > 0; k--)
			g = g + fabs(g) * 2.220446049250313e-16;
		for (k = (int)(urand() * 7) - 3; k < 0; k++)
			g = g - fabs(g) * 1.1102230246251565e-16;
		misc_n++;
		if (!same(cm_div_refine(x, y, g), q))
			misc_bad++, printf("  div(%.17g, %.17g) from %.17g\n", x, y, g);
	}

	/* cm_mul from products an ulp out, including small whole numbers. */
	for (i = 0; i < 200000; i++) {
		double p, g;
		x = (urand() + urand() * 1e-7) * pow(2.0, (int)(urand() * 200) - 100);
		y = (i & 4) ? (double)(int)(urand() * 100 + 1)
		            : (urand() + urand() * 1e-7) * pow(2.0, (int)(urand() * 200) - 100);
		if (i & 1)
			x = -x;
		if (i & 2)
			y = -y;
		p = x * y;
		g = p;
		if (i % 3 == 1)
			g = p + fabs(p) * 1.1102230246251565e-16;
		else if (i % 3 == 2)
			g = p - fabs(p) * 1.1102230246251565e-16;
		misc_n++;
		if (!same(cm_mul_refine(x, y, g), p))
			misc_bad++, printf("  mul(%.17g, %.17g) from %.17g\n", x, y, g);
	}

	/* Decimal constants in the forms the script uses. */
	for (i = 0; i < 100000; i++) {
		unsigned long whole = (unsigned long)(urand() * 100000.0);
		unsigned long frac = (unsigned long)(urand() * 1000000.0);
		int k = (int)(urand() * 7.0);
		if (i % 3 == 0)
			sprintf(buf, "%lu", whole);
		else
			sprintf(buf, "%lu.%0*lu", whole % 1000, k ? k : 1, frac % 1000000UL);
		if (i % 7 == 0)
			strcat(buf, "e-3");
		misc_n++;
		if (!same(cm_strtod((const uint8 *)buf, (int16)strlen(buf)), strtod(buf, NULL)))
			misc_bad++, printf("  strtod(%s)\n", buf);
	}

	printf("maths: pow %ld/%ld worse than libm, %ld differ where libm is"
	       " the one off; fmod, floor, mul, div, strtod %ld/%ld differ;"
	       " sin and cos worst abs error %.3g\n", pow_bad, pow_n, pow_libm_off, misc_bad, misc_n, worst_trig);
	return pow_bad != 0 || misc_bad != 0 || worst_trig > 1e-15;
}

#endif

/* The three streams against Park and Miller's 16807 * s mod (2^31 - 1) in 64
   bits, that FastRandom shares randSeed with Random and VARandom does not,
   and the -32768 rule that only Random has. */
static int check_streams(void)
{
	static const long starts[] = { 1L, 12345L, 0x12345678L, 0x7FFFFFFEL, 0x40000000L };
	uint32 s, found = 0;
	int bad = 0, i, k;
	int16 v;

	for (k = 0; k < 5; k++) {
		uint32 a = (uint32)starts[k], b = a;

		qd_seed((int32)a);
		va_seed((int32)b);
		for (i = 0; i < 20000; i++) {
			a = (uint32)(((unsigned long long)a * 16807ULL) % 2147483647ULL);
			b = (uint32)(((unsigned long long)b * 16807ULL) % 2147483647ULL);
			v = va_random();
			if ((int32)b != va_get_seed() || v != (int16)(b & 0xFFFF)) {
				printf("FAIL VARandom step %d from %ld\n", i, starts[k]);
				bad = 1;
				break;
			}
			if (i & 1) {
				v = fast_random();
				if ((int32)a != qd_get_seed() || v != (int16)(a & 0xFFFF)) {
					printf("FAIL FastRandom step %d from %ld\n", i, starts[k]);
					bad = 1;
					break;
				}
			} else {
				v = qd_random();
				if ((int32)a != qd_get_seed() || v != (int16)((a & 0xFFFFUL) == 0x8000UL ? 0 : (a & 0xFFFFUL))) {
					printf("FAIL Random step %d from %ld\n", i, starts[k]);
					bad = 1;
					break;
				}
			}
		}
	}

	/* A seed whose next value has the low word 0x8000. */
	for (s = 1; s < 300000UL && !found; s++) {
		uint32 n = (uint32)(((unsigned long long)s * 16807ULL) % 2147483647ULL);

		if ((n & 0xFFFFUL) == 0x8000UL)
			found = s;
	}
	if (!found) {
		printf("FAIL no seed gives 0x8000\n");
		return 1;
	}
	va_seed((int32)found);
	qd_seed((int32)found);
	if (va_random() != -32768 || qd_random() != 0) {
		printf("FAIL -32768 rule\n");
		bad = 1;
	}
	/* The streams don't touch each other. */
	va_seed(77);
	qd_seed(99);
	(void)va_random();
	(void)fast_random();
	if (va_get_seed() == 77 || qd_get_seed() == 99 ||
	    va_get_seed() != (int32)((77ULL * 16807ULL) % 2147483647ULL) ||
	    qd_get_seed() != (int32)((99ULL * 16807ULL) % 2147483647ULL)) {
		printf("FAIL streams share a seed\n");
		bad = 1;
	}
	return bad;
}

static int check_random(void)
{
	/* 16807^n mod (2^31-1) from seed 1: 16807, 282475249, 1622650073. */
	static const long seeds[] = { 16807L, 282475249L, 1622650073L };
	int i, bad = 0;

	qd_seed(1);
	for (i = 0; i < 3; i++) {
		int16 r = qd_random();
		if (qd_get_seed() != seeds[i] || r != (int16)(seeds[i] & 0xFFFF)) {
			printf("FAIL random step %d: seed %ld value %d\n", i, (long)qd_get_seed(), r);
			bad = 1;
		}
	}
	qd_seed(0x7FFF8000L / 16807L);   /* any seed works; check the 0x8000 rule */
	for (i = 0; i < 100000; i++)
		if (qd_random() == -32768)
			bad = 1, printf("FAIL random returned -32768\n");
	bad |= check_streams();
	qd_seed(1);
	return bad;
}

int main(int argc, char **argv)
{
	static char buf[SCRIPTDUMP_LEVEL_BYTES];
	FILE *out;
	int16 level;
	int fail = 0;

	if (argc != 3 && argc != 4) {
		fprintf(stderr, "usage: test_script DATADIR OUT [MATHS]\n");
		return 2;
	}
	host_set_data_dir(argv[1]);
	fail |= check_random();
#ifndef CORE_LIBM
	fail |= check_maths();
#endif
	qd_seed(1);
	if (!parser_alloc()) {
		printf("FAIL can't load the script from %s\n", argv[1]);
		return 1;
	}
	out = fopen(argv[2], "w");
	if (!out) {
		perror(argv[2]);
		return 1;
	}
	for (level = 1; level <= 99; level++) {
		uint16 n = script_dump_level(level, buf, sizeof buf);
		if (n >= sizeof buf) {
			printf("FAIL dump buffer full at level %d\n", level);
			fail = 1;
		}
		fwrite(buf, 1, n, out);
		if (level == 1)
			fail |= check_names();
	}
	fclose(out);
	if (parser_error()) {
		printf("FAIL parse error\n");
		fail = 1;
	}
	parser_free();
	if (argc == 4) {
		uint16 n = script_dump_maths(SCRIPT_MATHS_N, buf, sizeof buf);
		out = fopen(argv[3], "w");
		if (!out) {
			perror(argv[3]);
			return 1;
		}
		fwrite(buf, 1, n, out);
		fclose(out);
	}
	printf("%s: levels 1-99 dumped to %s\n", fail ? "FAIL" : "ok", argv[2]);
	return fail;
}
