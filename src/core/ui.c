/* Menu drawing primitives: see ui.h. Pixels go straight into the
   framebuffer; the caller presents. */
#include "ui.h"

int16 ui_w, ui_h, ui_s, ui_lh, ui_bh;

static const PlatScreen *us;
static const uint8 *font;
static uint32 font_n;
static uint16 cval[UIC_COUNT];
static int16 zone[UI_ZONES][5];
static int16 nzones;

/* r, g, b, then grey at 4 bit, at 2 bit and at 1 bit (0 black). The
   2 bit panels are black: the IIIx LCD shows dark grey nearly as light as
   white, so text on it washes out. */
static const uint8 ui_cols[UIC_COUNT][6] = {
	{ 0, 0, 0,          0, 0, 0 },      /* BG */
	{ 10, 16, 58,       3, 0, 0 },      /* PANEL */
	{ 22, 32, 92,       5, 0, 0 },      /* STRIPE */
	{ 2, 4, 24,         1, 0, 0 },      /* BAR */
	{ 235, 20, 90,      9, 2, 1 },      /* ACCENT */
	{ 0, 200, 255,      12, 2, 1 },     /* LINE */
	{ 4, 8, 34,         1, 0, 0 },      /* BTN */
	{ 255, 210, 0,      15, 3, 1 },     /* SEL */
	{ 240, 244, 255,    15, 3, 1 },     /* TEXT */
	{ 130, 160, 220,    10, 2, 1 },     /* DIM */
	{ 24, 8, 40,        0, 0, 0 },      /* INK */
	{ 0, 96, 170,       10, 2, 1 },     /* HEAD */
	{ 255, 255, 255,    0, 0, 0 },      /* HEADTXT */
	{ 255, 220, 40,     15, 3, 1 }      /* GOLD */
};

SEG_UI static uint16 pixel_value(uint8 r, uint8 g, uint8 b, int16 g4, int16 g2, int16 g1);
SEG_UI static void span(int16 x0, int16 x1, int16 y, uint16 v);
SEG_UI static void put(int16 x, int16 y, uint16 v);
SEG_UI static void shape(int16 x, int16 y, int16 w, int16 h, const uint8 *ins, int16 n, uint16 v);

/* The pixel value of a colour on this screen. Grey screens take the grey
   for their depth (0 is white there). */
SEG_UI static uint16 pixel_value(uint8 r, uint8 g, uint8 b, int16 g4, int16 g2, int16 g1)
{
	uint16 v, one = 1;

	if (us->format == PLAT_PIX_GREY) {
		if (us->depth == 1)
			return (uint16)(g1 ? 0 : 1);
		if (us->depth == 2)
			return (uint16)(3 - g2);
		return (uint16)(15 - g4);
	}
	if (us->format == PLAT_PIX_PALETTE)
		return plat_palette_index(r, g, b);
	v = (uint16)(((uint16)(r >> 3) << 11) | ((uint16)(g >> 2) << 5) | (b >> 3));
	if ((us->le16 != 0) != (*(uint8 *)&one == 1))
		v = (uint16)((v >> 8) | (v << 8));
	return v;
}

SEG_UI void ui_metrics(void)
{
	const PlatScreen *sc = plat_screen();

	ui_w = sc ? sc->width : 160;
	ui_h = sc ? sc->height : 160;
	ui_s = (int16)(ui_w >= 320 && ui_h >= 320 ? 2 : 1);
	ui_lh = (int16)(11 * ui_s);
	ui_bh = (int16)(13 * ui_s);
}

SEG_UI cbool ui_begin(void)
{
	int16 i;

	us = plat_screen();
	if (!us)
		return 0;
	ui_clip_off();
	ui_metrics();
	font = plat_resource(FOURCC('U', 'I', 'F', 'T'), (int16)(127 + ui_s), &font_n);
	if (font && (font_n < 4 || font_n < 4 + 4 * (uint32)font[3])) {
		plat_release(font);
		font = NULL;
	}
	for (i = 0; i < UIC_COUNT; i++) {
		const uint8 *c = ui_cols[i];

		cval[i] = pixel_value(c[0], c[1], c[2], c[3], c[4], c[5]);
	}
	return 1;
}

SEG_UI void ui_end(void)
{
	if (font)
		plat_release(font);
	font = NULL;
}

SEG_UI void ui_present(void)
{
	if (us)
		plat_present();
}

/* Painting is limited to this box (the screen unless ui_clip is set). */
static int16 clip_x0, clip_y0, clip_x1, clip_y1;

SEG_UI void ui_clip(int16 x, int16 y, int16 w, int16 h)
{
	if (!us)
		return;
	clip_x0 = x < 0 ? 0 : x;
	clip_y0 = y < 0 ? 0 : y;
	clip_x1 = (int16)(x + w < us->width ? x + w : us->width);
	clip_y1 = (int16)(y + h < us->height ? y + h : us->height);
}

SEG_UI void ui_clip_off(void)
{
	if (us)
		ui_clip(0, 0, us->width, us->height);
}

/* One pixel, clipped. Shifts and masks only: depths are powers of two. */
SEG_UI static void put(int16 x, int16 y, uint16 v)
{
	uint8 *p;
	int16 d;

	if (x < clip_x0 || x >= clip_x1 || y < clip_y0 || y >= clip_y1)
		return;
	p = us->bits + (int32)y * us->row_bytes;
	d = us->depth;
	if (d == 16) {
		((uint16 *)p)[x] = v;
	} else if (d == 8) {
		p[x] = (uint8)v;
	} else {
		/* d is 1, 2 or 4: pixels per byte is 8 >> log2(d). */
		int16 lg = (int16)(d == 1 ? 0 : (d == 2 ? 1 : 2));
		int16 sh = (int16)(8 - d - ((x & ((8 >> lg) - 1)) << lg));
		uint8 m = (uint8)(((1 << d) - 1) << sh);

		p += x >> (3 - lg);
		*p = (uint8)((*p & ~m) | ((v << sh) & m));
	}
}

/* x0 to x1 on row y, clipped. */
SEG_UI static void span(int16 x0, int16 x1, int16 y, uint16 v)
{
	uint8 *p;
	int16 d, x;

	if (y < clip_y0 || y >= clip_y1)
		return;
	if (x0 < clip_x0)
		x0 = clip_x0;
	if (x1 >= clip_x1)
		x1 = (int16)(clip_x1 - 1);
	if (x0 > x1)
		return;
	p = us->bits + (int32)y * us->row_bytes;
	d = us->depth;
	if (d == 16) {
		uint16 *q = (uint16 *)p + x0;

		for (x = x0; x <= x1; x++)
			*q++ = v;
	} else if (d == 8) {
		for (x = x0; x <= x1; x++)
			p[x] = (uint8)v;
	} else {
		int16 per = (int16)(8 / d), lg = (int16)(d == 1 ? 3 : (d == 2 ? 2 : 1));
		uint8 b = (uint8)v;

		for (x = 1; x < per; x++)
			b = (uint8)((b << d) | v);
		x = x0;
		while (x <= x1 && (x & (per - 1)))
			put(x++, y, v);
		while (x + per - 1 <= x1) {
			p[x >> lg] = b;
			x = (int16)(x + per);
		}
		while (x <= x1)
			put(x++, y, v);
	}
}

SEG_UI void ui_fill(int16 x, int16 y, int16 w, int16 h, int16 c)
{
	int16 i;

	if (!us)
		return;
	for (i = 0; i < h; i++)
		span(x, (int16)(x + w - 1), (int16)(y + i), cval[c]);
}

/* A box with its corners cut by ins[0..n-1] pixels on the rows at the top
   and bottom. */
SEG_UI static void shape(int16 x, int16 y, int16 w, int16 h, const uint8 *ins, int16 n, uint16 v)
{
	int16 i;

	for (i = 0; i < h; i++) {
		int16 k = i < n ? ins[i] : (h - 1 - i < n ? ins[h - 1 - i] : 0);

		span((int16)(x + k), (int16)(x + w - 1 - k), (int16)(y + i), v);
	}
}

/* Corner insets for the rounded boxes, outside and inside the outline, at
   each scale. At file scope so they stay in .text. */
static const uint8 out1[2] = { 2, 1 }, in1[1] = { 1 };
static const uint8 out2[4] = { 4, 2, 1, 1 }, in2[2] = { 2, 1 };

SEG_UI void ui_box(int16 x, int16 y, int16 w, int16 h, int16 fill, int16 edge)
{
	cbool big = ui_s > 1;

	if (!us)
		return;
	if (edge < 0) {
		shape(x, y, w, h, big ? out2 : out1, (int16)(big ? 4 : 2), cval[fill]);
		return;
	}
	shape(x, y, w, h, big ? out2 : out1, (int16)(big ? 4 : 2), cval[edge]);
	shape((int16)(x + ui_s), (int16)(y + ui_s), (int16)(w - 2 * ui_s), (int16)(h - 2 * ui_s),
	      big ? in2 : in1, (int16)(big ? 2 : 1), cval[fill]);
}

SEG_UI int16 ui_text_w(const char *s)
{
	int16 w = 0;

	if (!font)
		return 0;
	for (; *s; s++) {
		uint8 c = (uint8)*s;

		if (c < font[2] || c >= font[2] + font[3])
			c = '?';
		w = (int16)(w + font[4 + 4 * (c - font[2])] + ui_s);
	}
	return (int16)(w > 0 ? w - ui_s : 0);
}

SEG_UI int16 ui_text(int16 x, int16 y, const char *s, int16 c)
{
	int16 x0 = x, rows, first, count;
	const uint8 *bits;
	uint16 v;

	if (!us || !font)
		return 0;
	rows = font[0];
	first = font[2];
	count = font[3];
	bits = font + 4 + 4 * count;
	v = cval[c];
	for (; *s; s++) {
		int16 ch = (uint8)*s, w, stride, r, k;
		const uint8 *g;

		if (ch < first || ch >= first + count)
			ch = '?';
		w = font[4 + 4 * (ch - first)];
		g = bits + get16(font + 6 + 4 * (ch - first));
		stride = (int16)((w + 7) >> 3);
		if ((uint32)(g - font) + (uint32)rows * stride > font_n)
			break;
		for (r = 0; r < rows; r++, g += stride)
			for (k = 0; k < w; k++)
				if (g[k >> 3] & (0x80 >> (k & 7)))
					put((int16)(x + k), (int16)(y + r), v);
		x = (int16)(x + w + ui_s);
	}
	return (int16)(x - x0 - ui_s);
}

SEG_UI void ui_text_c(int16 cx, int16 y, const char *s, int16 c)
{
	ui_text((int16)(cx - ui_text_w(s) / 2), y, s, c);
}

SEG_UI void ui_text_r(int16 rx, int16 y, const char *s, int16 c)
{
	ui_text((int16)(rx - ui_text_w(s)), y, s, c);
}

/* Capitals are 7 rows high at scale 1. */
SEG_UI int16 ui_text_y(int16 y, int16 h)
{
	return (int16)(y + (h - 7 * ui_s + 1) / 2);
}

SEG_UI void ui_logo_size(int16 *w, int16 *h)
{
	uint32 n = 0;
	const uint8 *b = plat_resource(FOURCC('U', 'I', 'B', 'M'), (int16)(127 + ui_s), &n);

	*w = *h = 0;
	if (b && n >= 4) {
		*w = gets16(b);
		*h = gets16(b + 2);
	}
	if (b)
		plat_release(b);
}

SEG_UI int16 ui_logo(int16 cx, int16 y)
{
	uint32 n = 0;
	const uint8 *b = plat_resource(FOURCC('U', 'I', 'B', 'M'), (int16)(127 + ui_s), &n);
	int16 w, h, i, x, r, rb, x0;
	uint16 v[16];

	if (!b)
		return 0;
	w = gets16(b);
	h = gets16(b + 2);
	rb = (int16)((w + 1) / 2);
	if (n < 68 + (uint32)rb * h || !us) {
		plat_release(b);
		return 0;
	}
	for (i = 0; i < 16; i++) {
		const uint8 *c = b + 4 + 4 * i;
		int16 g = c[3];

		v[i] = pixel_value(c[0], c[1], c[2], g, (int16)((g * 3 + 7) / 15), (int16)(g >= 8));
	}
	x0 = (int16)(cx - w / 2);
	for (r = 0; r < h; r++) {
		const uint8 *row = b + 68 + (int32)r * rb;

		for (x = 0; x < w; x++) {
			int16 k = (int16)((row[x >> 1] >> ((x & 1) ? 0 : 4)) & 15);

			if (k)
				put((int16)(x0 + x), (int16)(y + r), v[k]);
		}
	}
	plat_release(b);
	return h;
}

SEG_UI void ui_zones_clear(void)
{
	nzones = 0;
}

SEG_UI void ui_zone(int16 id, int16 x, int16 y, int16 w, int16 h)
{
	if (nzones >= UI_ZONES)
		return;
	zone[nzones][0] = id;
	zone[nzones][1] = x;
	zone[nzones][2] = y;
	zone[nzones][3] = w;
	zone[nzones][4] = h;
	nzones++;
}

SEG_UI int16 ui_zone_at(int16 x, int16 y)
{
	int16 i;

	for (i = 0; i < nzones; i++)
		if (x >= zone[i][1] && y >= zone[i][2] &&
		    x < zone[i][1] + zone[i][3] && y < zone[i][2] + zone[i][4])
			return zone[i][0];
	return -1;
}

SEG_UI cbool ui_zone_rect(int16 id, PlatRect *r)
{
	int16 i;

	for (i = 0; i < nzones; i++)
		if (zone[i][0] == id) {
			r->x = zone[i][1];
			r->y = zone[i][2];
			r->w = zone[i][3];
			r->h = zone[i][4];
			return 1;
		}
	return 0;
}
