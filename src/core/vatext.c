/* Text and numbers in our own stroke font, queued as lines so they are
   erased with the rest of the frame. Same sizes and spacing as the
   original's 14 and 7 segment routines: text cells are 2s+2 wide and 4s+2
   high on a step of 3s+3, digits s+2 by 2s+2 on a step of s+7, where s is
   VA.segmscale. */
#include "va.h"

/* Glyphs for characters 32 to 95 (lower case is drawn as upper case).
   Each is a run of strokes from glyph_start[c - 32] up to 255. A stroke is
   two points on a 3 by 5 grid, a point being row * 3 + column, row 0 at
   the bottom: (first << 4) | second. */
static uint16 glyph_start[64] = {
	0, 1, 4, 7, 12, 19, 23, 30, 32, 36, 40, 44, 47, 49, 51, 53, 55, 60, 62, 68,
	73, 77, 83, 89, 92, 98, 104, 107, 110, 113, 116, 119, 125, 132, 137, 145,
	149, 156, 161, 165, 171, 175, 179, 183, 187, 190, 195, 199, 204, 209, 215,
	221, 227, 230, 234, 237, 242, 245, 249, 253, 257, 259, 263, 266
};

static uint8 glyph_strokes[268] = {
	255, 215, 17, 255, 201, 235, 255, 53, 155, 13, 30, 255, 236, 198, 104, 130,
	32, 209, 255, 14, 201, 82, 255, 41, 157, 219, 179, 49, 21, 255, 218, 255,
	234, 164, 66, 255, 202, 164, 64, 255, 59, 149, 74, 255, 104, 74, 255, 64,
	255, 104, 255, 17, 255, 14, 255, 2, 46, 236, 192, 255, 29, 255, 206, 232,
	134, 96, 2, 255, 206, 226, 32, 104, 255, 198, 104, 226, 255, 236, 198, 104,
	130, 32, 255, 236, 192, 2, 40, 134, 255, 206, 226, 255, 2, 46, 236, 192,
	104, 255, 134, 108, 206, 226, 32, 255, 68, 170, 255, 170, 64, 255, 230, 98,
	255, 53, 155, 255, 200, 128, 255, 206, 232, 135, 116, 17, 255, 94, 236, 192,
	2, 71, 120, 255, 12, 206, 226, 104, 255, 12, 205, 219, 183, 104, 130, 32,
	255, 236, 192, 2, 255, 12, 205, 219, 181, 81, 16, 255, 236, 192, 2, 103,
	255, 236, 192, 103, 255, 236, 192, 2, 40, 135, 255, 12, 46, 104, 255, 206,
	209, 2, 255, 226, 32, 3, 255, 12, 110, 98, 255, 192, 2, 255, 12, 199, 126,
	226, 255, 12, 194, 46, 255, 2, 46, 236, 192, 255, 12, 206, 232, 134, 255, 2,
	46, 236, 192, 66, 255, 12, 206, 232, 134, 114, 255, 236, 198, 104, 130, 32,
	255, 206, 209, 255, 192, 2, 46, 255, 193, 30, 255, 192, 7, 114, 46, 255,
	194, 14, 255, 199, 231, 113, 255, 206, 224, 2, 255, 237, 209, 18, 255, 194,
	255, 205, 209, 16, 255, 109, 216, 255, 2, 255
};

/* Column and row of each grid point. */
static uint8 point_col[15] = { 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2 };
static uint8 point_row[15] = { 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4 };

SEG_VA static void draw_glyph(int16 c, int16 left, int16 bottom, int16 w, int16 h);
SEG_VA static void draw_digits(int32 num, int16 x, int16 y, int16 width);

SEG_VA int16 va_glyph_segs(int16 c, int16 left, int16 bottom, int16 w, int16 h, VASeg *out)
{
	int16 gx[3], gy[5], n = 0, t;
	uint8 *p;
	uint8 a, b;
	VASeg *o;

	gx[0] = left;
	gx[1] = (int16)(left + (w >> 1));
	gx[2] = (int16)(left + w);
	gy[0] = bottom;
	gy[1] = (int16)(bottom - (h >> 2));
	gy[2] = (int16)(bottom - (h >> 1));
	gy[3] = (int16)(bottom - ((3 * h) >> 2));
	gy[4] = (int16)(bottom - h);
	for (p = glyph_strokes + glyph_start[c - 32]; *p != 255 && n < VA_GLYPH_SEGS; p++) {
		a = (uint8)(*p >> 4);
		b = (uint8)(*p & 15);
		o = out + n++;
		o->x1 = gx[point_col[a]];
		o->y1 = gy[point_row[a]];
		o->x2 = gx[point_col[b]];
		o->y2 = gy[point_row[b]];
		if (o->x1 > o->x2 || (o->x1 == o->x2 && o->y1 > o->y2)) {
			t = o->x1; o->x1 = o->x2; o->x2 = t;
			t = o->y1; o->y1 = o->y2; o->y2 = t;
		}
	}
	return n;
}

/* The cell spans left to left + w and bottom - h to bottom. Inside the
   frame it is queued whole, else as clipped lines. */
SEG_VA static void draw_glyph(int16 c, int16 left, int16 bottom, int16 w, int16 h)
{
	VASeg segs[VA_GLYPH_SEGS];
	int16 n, i;

	if (c >= 'a' && c <= 'z')
		c = (int16)(c - 32);
	if (c < 32 || c > 95)
		c = '?';
	if (left >= 0 && left + w < VA.frame.right && bottom < VA.frame.bottom && bottom - h >= 0) {
		va_queue_glyph(c, left, bottom, w, h);
		return;
	}
	n = va_glyph_segs(c, left, bottom, w, h, segs);
	for (i = 0; i < n; i++)
		VALine(segs[i].x1, segs[i].y1, segs[i].x2, segs[i].y2);
}

SEG_VA void VADrawText(const char *p, int32 a, int32 b)
{
	int16 s = VA.segmscale;
	int16 step = (int16)(s * 3 + 3);

	if (VA.CurrentX < 0 || VA.CurrentY < s * 4 + 4 || VA.CurrentY >= VA.frame.bottom)
		return;
	for (p += a; a < b; a++, p++) {
		if (VA.CurrentX >= VA.frame.right - step)
			break;
		if (*p != ' ')
			draw_glyph((uint8)*p, (int16)(VA.CurrentX + 1), VA.CurrentY,
			           (int16)(2 * s + 1), (int16)(4 * s + 2));
		VA.CurrentX = (int16)(VA.CurrentX + step);
	}
}

/* Digits right to left from x; 16 bit division once the number fits, as
   32 bit division is a library call on the 68k. */
SEG_VA static void draw_digits(int32 num, int16 x, int16 y, int16 width)
{
	int16 s = VA.segmscale;
	int16 step = (int16)(s + 7), w = (int16)(s + 1), h = (int16)(2 * s + 2);
	cbool neg = num < 0;
	uint32 u = neg ? (uint32)0 - (uint32)num : (uint32)num;
	uint16 v, d;

	while (u > 0xFFFF) {
		draw_glyph((int16)('0' + (int16)(u % 10)), (int16)(x + 1), y, w, h);
		u /= 10;
		x = (int16)(x - step);
		if (x <= 0 || --width == 0)
			return;
	}
	v = (uint16)u;
	for (;;) {
		d = (uint16)(v / 10);
		draw_glyph((int16)('0' + (int16)(v - d * 10)), (int16)(x + 1), y, w, h);
		v = d;
		x = (int16)(x - step);
		if (v == 0 || x <= 0 || --width == 0)
			break;
	}
	if (x > 0 && neg && width != 0)
		draw_glyph('-', (int16)(x + 1), y, w, h);
}

SEG_VA void VADrawPadNumber(int32 num, int16 x, int16 y, int16 width)
{
	draw_digits(num, x, y, width);
}

SEG_VA void VADrawNumber(int32 num, int16 x, int16 y)
{
	draw_digits(num, x, y, -1);
}
