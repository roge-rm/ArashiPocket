#include "testpat.h"

static uint8 colour_rgb[7][3] = {
	{ 255, 0, 0 }, { 0, 200, 0 }, { 0, 0, 255 }, { 255, 255, 0 },
	{ 0, 255, 255 }, { 255, 0, 255 }, { 255, 128, 0 }
};

SEG_PAT static uint16 rgb565(uint8 r, uint8 g, uint8 b)
{
	return (uint16)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

SEG_PAT uint16 testpat_black(const PlatScreen *s)
{
	if (s->format == PLAT_PIX_GREY)
		return (uint16)((1 << s->depth) - 1);
	if (s->format == PLAT_PIX_PALETTE)
		return s->pal[0];
	return 0;
}

SEG_PAT uint16 testpat_white(const PlatScreen *s)
{
	if (s->format == PLAT_PIX_GREY)
		return 0;
	if (s->format == PLAT_PIX_PALETTE)
		return s->pal[15];
	return 0xFFFF;
}

SEG_PAT int16 testpat_levels(const PlatScreen *s)
{
	return s->format == PLAT_PIX_GREY ? (int16)(1 << s->depth) : 16;
}

SEG_PAT uint16 testpat_grey(const PlatScreen *s, int16 i, int16 n)
{
	if (s->format == PLAT_PIX_GREY)
		return (uint16)((n - 1) - i);       /* n is 1 << depth */
	if (s->format == PLAT_PIX_PALETTE)
		return s->pal[(i * 15) / (n - 1)];
	{
		/* Whole steps of each field, so the greys stay grey. */
		uint16 r = (uint16)((i * 31L) / (n - 1));
		uint16 g = (uint16)((i * 63L) / (n - 1));

		return (uint16)((r << 11) | (g << 5) | r);
	}
}

SEG_PAT uint16 testpat_colour(const PlatScreen *s, int16 k)
{
	if (s->format == PLAT_PIX_PALETTE)
		return s->pal[16 + k];
	if (s->format == PLAT_PIX_RGB565)
		return rgb565(colour_rgb[k][0], colour_rgb[k][1], colour_rgb[k][2]);
	return testpat_black(s);
}

SEG_PAT uint16 testpat_get(const PlatScreen *s, int16 x, int16 y)
{
	const uint8 *row = s->bits + (int32)y * s->row_bytes;

	if (s->format == PLAT_PIX_GREY) {
		int16 bit = (int16)(x * s->depth);
		int16 shift = (int16)(8 - s->depth - (bit & 7));

		return (uint16)((row[bit >> 3] >> shift) & ((1 << s->depth) - 1));
	}
	if (s->format == PLAT_PIX_PALETTE)
		return row[x];
	if (s->le16)
		return (uint16)(row[2 * x] | (row[2 * x + 1] << 8));
	return (uint16)((row[2 * x] << 8) | row[2 * x + 1]);
}

SEG_PAT void testpat_put(const PlatScreen *s, int16 x, int16 y, uint16 v)
{
	uint8 *row = s->bits + (int32)y * s->row_bytes;

	if (x < 0 || y < 0 || x >= s->width || y >= s->height)
		return;
	if (s->format == PLAT_PIX_GREY) {
		int16 bit = (int16)(x * s->depth);
		int16 shift = (int16)(8 - s->depth - (bit & 7));
		uint8 mask = (uint8)(((1 << s->depth) - 1) << shift);

		row[bit >> 3] = (uint8)((row[bit >> 3] & ~mask) | ((v << shift) & mask));
	} else if (s->format == PLAT_PIX_PALETTE) {
		row[x] = (uint8)v;
	} else if (s->le16) {
		row[2 * x] = (uint8)v;
		row[2 * x + 1] = (uint8)(v >> 8);
	} else {
		row[2 * x] = (uint8)(v >> 8);
		row[2 * x + 1] = (uint8)v;
	}
}

SEG_PAT static void fill(const PlatScreen *s, int16 x, int16 y, int16 w, int16 h, uint16 v)
{
	int16 i, j;

	for (j = 0; j < h; j++)
		for (i = 0; i < w; i++)
			testpat_put(s, (int16)(x + i), (int16)(y + j), v);
}

SEG_PAT static void frame(const PlatScreen *s, const PlatRect *r, uint16 v)
{
	fill(s, r->x, r->y, r->w, 1, v);
	fill(s, r->x, (int16)(r->y + r->h - 1), r->w, 1, v);
	fill(s, r->x, r->y, 1, r->h, v);
	fill(s, (int16)(r->x + r->w - 1), r->y, 1, r->h, v);
}

/* The ramp is a row of swatches a quarter of the way down the play area. */
SEG_PAT void testpat_ramp_rect(const PlatScreen *s, int16 i, PlatRect *r)
{
	int16 n = testpat_levels(s);
	int16 size = s->play.w;
	int16 margin = (int16)(size / 10);
	int16 span = (int16)(size - 2 * margin);

	r->x = (int16)(margin + (int16)(((int32)span * i) / n));
	r->w = (int16)(margin + (int16)(((int32)span * (i + 1)) / n) - r->x);
	r->y = (int16)(size / 4);
	r->h = (int16)(size / 8);
}

SEG_PAT void testpat_draw(const PlatScreen *s, cbool cross, int16 cx, int16 cy)
{
	uint16 black = testpat_black(s);
	uint16 white = testpat_white(s);
	int16 n = testpat_levels(s);
	int16 size = s->play.w;
	int16 i, arm, lo, hi;
	PlatRect r;

	fill(s, 0, 0, s->width, s->height, white);

	/* Status area: outline and a light chequer so its extent shows. */
	if (s->status.w > 0 && s->status.h > 0) {
		uint16 light = testpat_grey(s, (int16)(n - 2 > 0 ? n - 2 : 0), n);

		r = s->status;
		for (i = 0; i < r.h; i += 2) {
			int16 j;

			for (j = (int16)((i >> 1) & 1); j < r.w; j += 2)
				testpat_put(s, (int16)(r.x + j), (int16)(r.y + i), light);
		}
		frame(s, &r, black);
	}

	/* Play area outline, with a second line two pixels in. */
	r = s->play;
	frame(s, &r, black);
	r.x += 2;
	r.y += 2;
	r.w -= 4;
	r.h -= 4;
	if (r.w > 2 && r.h > 2)
		frame(s, &r, testpat_grey(s, (int16)(n / 2), n));

	/* Grey ramp, black to white, each swatch outlined where it's too light
	   to see against the page. */
	for (i = 0; i < n; i++) {
		testpat_ramp_rect(s, i, &r);
		r.x += s->play.x;
		r.y += s->play.y;
		fill(s, r.x, r.y, r.w, r.h, testpat_grey(s, i, n));
	}
	testpat_ramp_rect(s, 0, &r);
	{
		PlatRect all;
		PlatRect last;

		testpat_ramp_rect(s, (int16)(n - 1), &last);
		all.x = (int16)(s->play.x + r.x - 1);
		all.y = (int16)(s->play.y + r.y - 1);
		all.w = (int16)(last.x + last.w - r.x + 2);
		all.h = (int16)(r.h + 2);
		frame(s, &all, black);
	}

	/* Colour swatches on colour depths. */
	if (s->format != PLAT_PIX_GREY && s->depth >= 8) {
		int16 margin = (int16)(size / 10);
		int16 span = (int16)(size - 2 * margin);
		int16 y = (int16)(s->play.y + size / 2);

		for (i = 0; i < 7; i++) {
			int16 x0 = (int16)(margin + (int16)(((int32)span * i) / 7));
			int16 x1 = (int16)(margin + (int16)(((int32)span * (i + 1)) / 7));

			fill(s, (int16)(s->play.x + x0), y, (int16)(x1 - x0), (int16)(size / 8),
			     testpat_colour(s, i));
		}
	}

	/* Cross at the last stylus point. */
	if (cross) {
		arm = (int16)(size / 16 < 4 ? 4 : size / 16);
		lo = (int16)(-arm);
		hi = arm;
		for (i = lo; i <= hi; i++) {
			testpat_put(s, (int16)(s->play.x + cx + i), (int16)(s->play.y + cy), black);
			testpat_put(s, (int16)(s->play.x + cx), (int16)(s->play.y + cy + i), black);
		}
	}
}
