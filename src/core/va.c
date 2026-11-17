/* Vector renderer: display lists, clipping, colours, explosions and the
   per-depth line and pixel loops. See va.h. */
#include <stddef.h>
#include "va.h"

VAGlobalsRecord VA;
int16 Sins[ANGLES];
int16 Cosins[ANGLES];

/* Line ends and points are stored clipped, in framebuffer pixels, so a
   frame is erased by drawing exactly the same pixels again. Lines (VASeg,
   in va.h) run left to right (top to bottom when vertical). */
typedef struct {
	int16 x, y;             /* left and bottom, framebuffer pixels */
	uint8 c, w, h, col;
} VAGlyph;

typedef struct {
	int16 x, y;
} VAPt;

typedef struct {
	VASeg *lines;
	uint8 *linecol;
	VAPt  *pix;
	uint8 *pixcol;
	VAPt  *spots;
	uint8 *spotcol;
	VAGlyph *glyphs;
	int16  nlines, npix, nspots, nglyphs;
	/* Box round the lines, spots and pixels queued (framebuffer pixels,
	   x0 > x1 when empty), the rough cost of drawing them again in
	   pixels, and the box round the glyphs. */
	int16  x0, y0, x1, y1;
	int32  cost;
	int16  gx0, gy0, gx1, gy1;
} VAList;

static VAList lists[2];
static int16 cur;               /* list being filled; the other is on screen */

/* The screen as last seen. */
static const PlatScreen *scr;
static uint8 **rows;            /* start of each framebuffer row */
static int16 rows_n;
static int16 offx, offy;        /* play area in the framebuffer */
static int16 size;              /* play area width and height */
static int16 vw, vh;            /* the view drawing is clipped to (the play area
                                   or the status area) */
static PlatRect playrect;
static cbool aligned4;          /* rows start on 4 byte boundaries */

/* Pixel value for each colour index: a grey level repeated across the
   byte (1, 2, 4 bit), a palette index (8 bit) or an RGB565 word already in
   the byte order the framebuffer wants (16 bit). bgvals is all
   background, for erasing. */
static uint8  rgb[NUMCOLORS][3];
static uint16 vals[NUMCOLORS];
static uint16 bgvals[NUMCOLORS];

/* The still picture (the field during play): baked once into keep, a copy
   of the whole framebuffer with only it on the background, then left out of
   the lists. While it is there, erasing puts back keep's pixels instead of
   the background. */
#define STILL_MISSES 20         /* frames of other colours before a new bake */
static const uint8 *keep;
static uint32 keep_size, keep_failed;   /* failed: the size that got no room */
static ptrdiff_t keep_off;      /* keep - framebuffer, in bytes */
static cbool still_ok;          /* keep holds still_key in still_vals */
static uint16 still_key, still_new;
static uint16 still_vals[NUMCOLORS];
static int16 still_misses;
static cbool still_bake;        /* bake the first still_lines lines and */
static int16 still_lines, still_glyphs;     /* still_glyphs glyphs of this frame */
static cbool still_used;        /* VAStillBegin was called this frame */
#ifdef VA_STILL_CHECK
uint32 va_still_checks, va_still_bad, va_still_bakes, va_still_begins;
#endif

/* Explosions: GNRL 1000 to 1003, frames of (count, count x/y pairs) up to
   a count of 0, kept scaled to the play area. */
#define NUMEXPLOSIONS 30
#define RESEXPLOSIONS 4

static int16 *boom[RESEXPLOSIONS];
static int16 expl_n;
static int16 *expl_at[NUMEXPLOSIONS];
static int16 expl_x[NUMEXPLOSIONS], expl_y[NUMEXPLOSIONS];
static uint8 expl_col[NUMEXPLOSIONS];
static int16 expl_ox[NUMEXPLOSIONS], expl_oy[NUMEXPLOSIONS];   /* the view it was made in */
static int16 expl_vw[NUMEXPLOSIONS], expl_vh[NUMEXPLOSIONS];

/* sin(k * 3 degrees) * 16384 for k = 0 to 30, a quarter turn. */
static int16 quarter_sin[31] = {
	0, 857, 1713, 2563, 3406, 4240, 5063, 5872, 6664, 7438, 8192, 8923, 9630,
	10311, 10963, 11585, 12176, 12733, 13255, 13741, 14189, 14598, 14968,
	15296, 15582, 15826, 16026, 16182, 16294, 16362, 16384
};

SEG_VA static void empty_list(VAList *l);
SEG_VA static void erase_box(int16 x0, int16 y0, int16 x1, int16 y1);
SEG_VA static void draw_glyphs(VAList *l, const uint16 *tab);
SEG_VA static void draw_glyphs_n(VAList *l, int16 count, const uint16 *tab);
SEG_VA static void free_lists(void);
SEG_VA static void free_explosions(void);
SEG_VA static cbool take_screen(void);
SEG_VA static void make_values(void);
SEG_VA static uint8 colour_index(int16 c);
SEG_VA static cbool clip_line(int16 *x1, int16 *y1, int16 *x2, int16 *y2);
SEG_VA static void queue_line(int16 x1, int16 y1, int16 x2, int16 y2);
SEG_VA static void put_pixel(int16 x, int16 y, uint16 v);
SEG_VA static void draw_list(VAList *l, const uint16 *tab);
SEG_VA static void draw_segs(const VASeg *s, const uint8 *col, int16 n, const uint16 *tab);
SEG_VA static void lines1(const VASeg *s, const uint8 *col, int16 n, const uint16 *tab);
SEG_VA static void lines2(const VASeg *s, const uint8 *col, int16 n, const uint16 *tab);
SEG_VA static void lines4(const VASeg *s, const uint8 *col, int16 n, const uint16 *tab);
SEG_VA static void lines8(const VASeg *s, const uint8 *col, int16 n, const uint16 *tab);
SEG_VA static void lines16(const VASeg *s, const uint8 *col, int16 n, const uint16 *tab);
SEG_VA static void load_explosions(void);
SEG_VA static void insert_explosions(void);
SEG_VA static void init_sins(void);
SEG_VA static void keep_drop(void);
SEG_VA static uint16 keep_value(int16 x, int16 y);
SEG_VA static void restore_box(int16 x0, int16 y0, int16 x1, int16 y1);
SEG_VA static void restore_segs(const VASeg *s, int16 n);
SEG_VA static void restore1(const VASeg *s, const uint8 *col, int16 n, const uint16 *tab);
SEG_VA static void restore2(const VASeg *s, const uint8 *col, int16 n, const uint16 *tab);
SEG_VA static void restore4(const VASeg *s, const uint8 *col, int16 n, const uint16 *tab);
SEG_VA static void restore8(const VASeg *s, const uint8 *col, int16 n, const uint16 *tab);
SEG_VA static void restore16(const VASeg *s, const uint8 *col, int16 n, const uint16 *tab);
SEG_VA static void present_all(void);
#ifdef VA_STILL_CHECK
SEG_VA static void check_still(void);
#endif

/* ---- set-up ---- */

SEG_VA static void empty_list(VAList *l)
{
	l->nlines = l->npix = l->nspots = l->nglyphs = 0;
	l->x0 = l->y0 = l->gx0 = l->gy0 = 32767;
	l->x1 = l->y1 = l->gx1 = l->gy1 = -32768;
	l->cost = 0;
}

SEG_VA static void init_sins(void)
{
	int16 a, q, k, v;

	for (a = 0; a < ANGLES; a++) {
		q = (int16)(a / 30);
		k = (int16)(a % 30);
		switch (q) {
		case 0:  v = quarter_sin[k]; break;
		case 1:  v = quarter_sin[30 - k]; break;
		case 2:  v = (int16)-quarter_sin[k]; break;
		default: v = (int16)-quarter_sin[30 - k]; break;
		}
		Sins[a] = (int16)((v + 32) >> 6);
	}
	for (a = 0; a < ANGLES; a++)
		Cosins[a] = Sins[(a + ANGLES / 4) % ANGLES];
}

SEG_VA static void free_lists(void)
{
	int16 i;

	for (i = 0; i < 2; i++) {
		VAList *l = &lists[i];

		if (l->lines) plat_free(l->lines);
		if (l->linecol) plat_free(l->linecol);
		if (l->pix) plat_free(l->pix);
		if (l->pixcol) plat_free(l->pixcol);
		if (l->spots) plat_free(l->spots);
		if (l->spotcol) plat_free(l->spotcol);
		if (l->glyphs) plat_free(l->glyphs);
		l->glyphs = NULL;
		l->lines = NULL;
		l->linecol = l->pixcol = l->spotcol = NULL;
		l->pix = l->spots = NULL;
		l->nlines = l->npix = l->nspots = l->nglyphs = 0;
	}
	if (rows)
		plat_free(rows);
	rows = NULL;
	rows_n = 0;
}

SEG_VA static void free_explosions(void)
{
	int16 i;

	for (i = 0; i < RESEXPLOSIONS; i++) {
		if (boom[i])
			plat_free(boom[i]);
		boom[i] = NULL;
	}
	expl_n = 0;
}

/* Reads the screen: row table, play area, frame, segment scale. */
SEG_VA static cbool take_screen(void)
{
	int16 y;

	scr = plat_screen();
	if (!scr)
		return 0;
	if (rows && rows_n != scr->height) {
		plat_free(rows);
		rows = NULL;
	}
	if (!rows) {
		rows = (uint8 **)plat_alloc((uint32)scr->height * sizeof(uint8 *));
		if (!rows)
			return 0;
		rows_n = scr->height;
	}
	for (y = 0; y < scr->height; y++)
		rows[y] = scr->bits + (int32)y * scr->row_bytes;
	aligned4 = ((unsigned long)scr->bits & 3) == 0 && (scr->row_bytes & 3) == 0;
	offx = scr->play.x;
	offy = scr->play.y;
	size = scr->play.w;
	playrect = scr->play;
	vw = vh = size;
	VA.frame.left = 0;
	VA.frame.top = 0;
	VA.frame.right = size;
	VA.frame.bottom = size;
	VA.segmscale = (int16)(size >> 6);
	if (VA.segmscale < 1)
		VA.segmscale = 1;
	return 1;
}

SEG_VA cbool VAInit(void)
{
	int16 i;

	VA.color = 0;
	VA.CurrentX = VA.CurrentY = 0;
	VA.FrameCounter = 0;
	VA.DrawTicks = VA.PresentTicks = VA.EraseTicks = VA.GlyphTicks = 0;
	VA.overflow = 0;
	keep_failed = 0;
	init_sins();
	if (!take_screen())
		return 0;
	for (i = 0; i < 2; i++) {
		VAList *l = &lists[i];

		l->lines = (VASeg *)plat_alloc(VA_MAXLINES * (uint32)sizeof(VASeg));
		l->linecol = (uint8 *)plat_alloc(VA_MAXLINES);
		l->pix = (VAPt *)plat_alloc(VA_MAXPIX * (uint32)sizeof(VAPt));
		l->pixcol = (uint8 *)plat_alloc(VA_MAXPIX);
		l->spots = (VAPt *)plat_alloc(VA_MAXSPOTS * (uint32)sizeof(VAPt));
		l->spotcol = (uint8 *)plat_alloc(VA_MAXSPOTS);
		l->glyphs = (VAGlyph *)plat_alloc(VA_MAXGLYPHS * (uint32)sizeof(VAGlyph));
		if (!l->lines || !l->linecol || !l->pix || !l->pixcol || !l->spots || !l->spotcol ||
		    !l->glyphs) {
			VAClose();
			return 0;
		}
	}
	cur = 0;
	empty_list(&lists[0]);
	empty_list(&lists[1]);
	VALoadColors(1000);
	load_explosions();
	VAEraseBuffer();
	return 1;
}

SEG_VA void VAClose(void)
{
	keep_drop();
	free_lists();
	free_explosions();
	scr = NULL;
}

SEG_VA cbool VAScreenChanged(void)
{
	empty_list(&lists[0]);
	empty_list(&lists[1]);
	if (!take_screen())
		return 0;
	make_values();
	load_explosions();
	VAEraseBuffer();
	return 1;
}

/* The status area as the drawing view: calls draw into it (coordinates from
   its top left, VA.frame its size) until VAViewPlay. The lists hold
   framebuffer pixels, so a frame can mix both views. */
SEG_VA void VAViewStatus(void)
{
	if (!scr || scr->status.w <= 0 || scr->status.h <= 0)
		return;
	offx = scr->status.x;
	offy = scr->status.y;
	vw = scr->status.w;
	vh = scr->status.h;
	VA.frame.right = vw;
	VA.frame.bottom = vh;
}

SEG_VA void VAViewPlay(void)
{
	if (!scr)
		return;
	offx = playrect.x;
	offy = playrect.y;
	vw = vh = size;
	VA.frame.right = size;
	VA.frame.bottom = size;
}

SEG_VA void VAViewRect(int16 x, int16 y, int16 w, int16 h)
{
	if (!scr)
		return;
	offx = x;
	offy = y;
	vw = w;
	vh = h;
	VA.frame.right = w;
	VA.frame.bottom = h;
}

/* ---- colours ---- */

/* VA.color to an index into the 10 colours: 0 to 6 as they are, then BG1,
   BG2 and BGC by the low byte, as the Mac palette mapped them. */
#define COLOUR_INDEX(c) ((uint16)(c) < 7 ? (uint8)(c) : colour_index(c))
SEG_VA static uint8 colour_index(int16 c)
{
	uint16 v = (uint16)c & 255;

	if (v < 7)
		return (uint8)v;
	if (v < 128)
		return 7;
	if (v < 192)
		return 8;
	return 9;
}

SEG_VA static void make_values(void)
{
	int16 i;
	uint16 one = 1;
	cbool cpu_le = *(uint8 *)&one == 1;

	if (!scr)
		return;
	for (i = 0; i < NUMCOLORS; i++) {
		uint8 r = rgb[i][0], g = rgb[i][1], b = rgb[i][2];
		uint16 v;

		if (scr->format == PLAT_PIX_GREY) {
			/* Brightest component, as the original's grey mode, with 0
			   white on Palm. Anything lit stays visible. */
			int16 levels = (int16)((1 << scr->depth) - 1);
			uint8 m = r > g ? r : g;
			int16 grey;

			if (b > m)
				m = b;
			grey = (int16)(((int16)m * levels + 127) / 255);
			if (m > 0 && grey == 0)
				grey = 1;
			v = (uint16)(levels - grey);
			if (scr->depth == 1)
				v = v ? 0xFF : 0;
			else if (scr->depth == 2)
				v = (uint16)(v * 0x55);
			else
				v = (uint16)(v * 0x11);
		} else if (scr->format == PLAT_PIX_PALETTE) {
			v = plat_palette_index(r, g, b);
		} else {
			v = (uint16)(((uint16)(r >> 3) << 11) | ((uint16)(g >> 2) << 5) | (b >> 3));
			if ((scr->le16 != 0) != (cpu_le != 0))
				v = (uint16)((v >> 8) | (v << 8));
		}
		vals[i] = v;
	}
	for (i = 0; i < NUMCOLORS; i++)
		bgvals[i] = vals[9];
}

SEG_VA void VASetColors(const uint8 *clot, uint32 n)
{
	int16 i;

	for (i = 0; i < NUMCOLORS; i++) {
		if ((uint32)(i * 8 + 8) <= n) {
			/* ColorSpec: value, then red, green, blue as 16 bit. */
			rgb[i][0] = clot[i * 8 + 2];
			rgb[i][1] = clot[i * 8 + 4];
			rgb[i][2] = clot[i * 8 + 6];
		} else {
			rgb[i][0] = rgb[i][1] = rgb[i][2] = (uint8)(i == 9 ? 0 : 255);
		}
	}
	make_values();
}

SEG_VA void VALoadColors(int16 clot_id)
{
	uint32 n = 0;
	const uint8 *clot = plat_resource(FOURCC('C', 'L', 'O', 'T'), clot_id, &n);

	if (clot) {
		VASetColors(clot, n);
		plat_release(clot);
	} else {
		VASetColors((const uint8 *)"", 0);
	}
}

SEG_VA uint16 VAPixelValue(int16 c)
{
	return vals[colour_index(c)];
}

/* ---- queueing ---- */

#define OUT_L 1
#define OUT_R 2
#define OUT_T 4
#define OUT_B 8

/* Cohen-Sutherland against the view, 0..vw-1 by 0..vh-1. */
SEG_VA static cbool clip_line(int16 *px1, int16 *py1, int16 *px2, int16 *py2)
{
	int32 x1 = *px1, y1 = *py1, x2 = *px2, y2 = *py2;
	int32 hix = vw - 1, hiy = vh - 1;
	int16 c1, c2, c;
	int32 x, y;

	for (;;) {
		c1 = (int16)((x1 < 0 ? OUT_L : x1 > hix ? OUT_R : 0) | (y1 < 0 ? OUT_T : y1 > hiy ? OUT_B : 0));
		c2 = (int16)((x2 < 0 ? OUT_L : x2 > hix ? OUT_R : 0) | (y2 < 0 ? OUT_T : y2 > hiy ? OUT_B : 0));
		if (!(c1 | c2))
			break;
		if (c1 & c2)
			return 0;
		c = c1 ? c1 : c2;
		if (c & OUT_L) {
			x = 0;
			y = y1 + (y2 - y1) * (0 - x1) / (x2 - x1);
		} else if (c & OUT_R) {
			x = hix;
			y = y1 + (y2 - y1) * (hix - x1) / (x2 - x1);
		} else if (c & OUT_T) {
			y = 0;
			x = x1 + (x2 - x1) * (0 - y1) / (y2 - y1);
		} else {
			y = hiy;
			x = x1 + (x2 - x1) * (hiy - y1) / (y2 - y1);
		}
		if (c == c1) {
			x1 = x;
			y1 = y;
		} else {
			x2 = x;
			y2 = y;
		}
	}
	*px1 = (int16)x1;
	*py1 = (int16)y1;
	*px2 = (int16)x2;
	*py2 = (int16)y2;
	return 1;
}

SEG_VA static void queue_line(int16 x1, int16 y1, int16 x2, int16 y2)
{
	VAList *l = &lists[cur];
	VASeg *s;
	int16 t, dx, dy;

	if (l->nlines >= VA_MAXLINES) {
		VA.overflow = 1;
		return;
	}
	/* Most lines are inside; the unsigned compares test 0 <= v < view. */
	if ((uint16)x1 >= (uint16)vw || (uint16)y1 >= (uint16)vh ||
	    (uint16)x2 >= (uint16)vw || (uint16)y2 >= (uint16)vh) {
		if (!scr || !clip_line(&x1, &y1, &x2, &y2))
			return;
	}
	if (x1 > x2 || (x1 == x2 && y1 > y2)) {
		t = x1; x1 = x2; x2 = t;
		t = y1; y1 = y2; y2 = t;
	}
	x1 = (int16)(x1 + offx);
	x2 = (int16)(x2 + offx);
	y1 = (int16)(y1 + offy);
	y2 = (int16)(y2 + offy);
	s = l->lines + l->nlines;
	s->x1 = x1;
	s->y1 = y1;
	s->x2 = x2;
	s->y2 = y2;
	l->linecol[l->nlines++] = COLOUR_INDEX(VA.color);
	if (x1 < l->x0) l->x0 = x1;
	if (x2 > l->x1) l->x1 = x2;
	if (y1 < y2) {
		if (y1 < l->y0) l->y0 = y1;
		if (y2 > l->y1) l->y1 = y2;
		dy = (int16)(y2 - y1);
	} else {
		if (y2 < l->y0) l->y0 = y2;
		if (y1 > l->y1) l->y1 = y1;
		dy = (int16)(y1 - y2);
	}
	dx = (int16)(x2 - x1);
	/* Setting up a line costs about as much as eight pixels. */
	l->cost += (dx > dy ? dx : dy) + 9;
}

SEG_VA void VALine(int16 x1, int16 y1, int16 x2, int16 y2)
{
	queue_line(x1, y1, x2, y2);
}

SEG_VA void VALineTo(int16 x, int16 y)
{
	queue_line(VA.CurrentX, VA.CurrentY, x, y);
	VA.CurrentX = x;
	VA.CurrentY = y;
}

/* Every line is clipped, so the safe and plain calls are the same. */
SEG_VA void VASafeLineTo(int16 x, int16 y)
{
	VALineTo(x, y);
}

SEG_VA void VAPixel(int16 x, int16 y)
{
	VAList *l = &lists[cur];

	if (!scr || x < 0 || y < 0 || x >= vw || y >= vh)
		return;
	if (l->npix >= VA_MAXPIX) {
		VA.overflow = 1;
		return;
	}
	x = (int16)(x + offx);
	y = (int16)(y + offy);
	l->pix[l->npix].x = x;
	l->pix[l->npix].y = y;
	l->pixcol[l->npix++] = COLOUR_INDEX(VA.color);
	if (x < l->x0) l->x0 = x;
	if (x > l->x1) l->x1 = x;
	if (y < l->y0) l->y0 = y;
	if (y > l->y1) l->y1 = y;
	l->cost += 4;
}

SEG_VA void VASafePixel(int16 x, int16 y)
{
	VAPixel(x, y);
}

/* A spot is a plus of five pixels around x, y, dropped unless it all fits. */
SEG_VA void VASpot(int16 x, int16 y)
{
	VAList *l = &lists[cur];

	if (!scr || x < 1 || y < 1 || x >= vw - 1 || y >= vh - 1)
		return;
	if (l->nspots >= VA_MAXSPOTS) {
		VA.overflow = 1;
		return;
	}
	x = (int16)(x + offx);
	y = (int16)(y + offy);
	l->spots[l->nspots].x = x;
	l->spots[l->nspots].y = y;
	l->spotcol[l->nspots++] = COLOUR_INDEX(VA.color);
	if (x - 1 < l->x0) l->x0 = (int16)(x - 1);
	if (x + 1 > l->x1) l->x1 = (int16)(x + 1);
	if (y - 1 < l->y0) l->y0 = (int16)(y - 1);
	if (y + 1 > l->y1) l->y1 = (int16)(y + 1);
	l->cost += 20;
}

SEG_VA void VASafeSpot(int16 x, int16 y)
{
	VASpot(x, y);
}

SEG_VA void va_queue_glyph(int16 c, int16 left, int16 bottom, int16 w, int16 h)
{
	VAList *l = &lists[cur];
	VAGlyph *g;

	if (l->nglyphs >= VA_MAXGLYPHS) {
		VA.overflow = 1;
		return;
	}
	left = (int16)(left + offx);
	bottom = (int16)(bottom + offy);
	g = l->glyphs + l->nglyphs++;
	g->x = left;
	g->y = bottom;
	g->c = (uint8)c;
	g->w = (uint8)w;
	g->h = (uint8)h;
	g->col = COLOUR_INDEX(VA.color);
	if (left < l->gx0) l->gx0 = left;
	if (left + w > l->gx1) l->gx1 = (int16)(left + w);
	if (bottom - h < l->gy0) l->gy0 = (int16)(bottom - h);
	if (bottom > l->gy1) l->gy1 = bottom;
}

/* ---- explosions ---- */

SEG_VA static void load_explosions(void)
{
	int16 n, i;
	int16 f = (int16)(((int32)size * 256 + VA_REF_SIZE / 2) / VA_REF_SIZE);

	free_explosions();
	for (n = 0; n < RESEXPLOSIONS; n++) {
		uint32 bytes = 0;
		const uint8 *d = plat_resource(FOURCC('G', 'N', 'R', 'L'), (int16)(1000 + n), &bytes);
		int16 words = (int16)(bytes / 2), at = 0, m;
		int16 *t;

		if (!d)
			continue;
		t = (int16 *)plat_alloc((uint32)(words + 1) * 2);
		if (t) {
			/* Copy frame by frame so a cut-short table still ends in 0. */
			while (at < words) {
				m = gets16(d + 2 * at);
				if (m <= 0 || at + 1 + 2 * m > words)
					break;
				t[at] = m;
				for (i = 1; i <= 2 * m; i++) {
					int32 v = gets16(d + 2 * (at + i));

					t[at + i] = (int16)((v * f + (v < 0 ? -128 : 128)) / 256);
				}
				at = (int16)(at + 1 + 2 * m);
			}
			t[at] = 0;
			boom[n] = t;
		}
		plat_release(d);
	}
}

SEG_VA void VAExplosion(int16 x, int16 y, int16 sz, int16 color)
{
	if (sz < 0 || sz >= RESEXPLOSIONS || !boom[sz] || expl_n >= NUMEXPLOSIONS || !*boom[sz])
		return;
	expl_at[expl_n] = boom[sz];
	expl_x[expl_n] = x;
	expl_y[expl_n] = y;
	expl_ox[expl_n] = offx;
	expl_oy[expl_n] = offy;
	expl_vw[expl_n] = vw;
	expl_vh[expl_n] = vh;
	expl_col[expl_n] = colour_index(color);
	expl_n++;
}

/* Puts each explosion's next frame into the pixel list. */
SEG_VA static void insert_explosions(void)
{
	VAList *l = &lists[cur];
	int16 e, m, x, y, i;
	int16 *p;

	for (e = 0; e < expl_n; e++) {
		p = expl_at[e];
		m = *p++;
		if (l->npix + m <= VA_MAXPIX) {
			for (i = 0; i < m; i++, p += 2) {
				x = (int16)(expl_x[e] + p[0]);
				y = (int16)(expl_y[e] + p[1]);
				if (x < 0 || y < 0 || x >= expl_vw[e] || y >= expl_vh[e])
					continue;
				x = (int16)(x + expl_ox[e]);
				y = (int16)(y + expl_oy[e]);
				l->pix[l->npix].x = x;
				l->pix[l->npix].y = y;
				l->pixcol[l->npix++] = expl_col[e];
				if (x < l->x0) l->x0 = x;
				if (x > l->x1) l->x1 = x;
				if (y < l->y0) l->y0 = y;
				if (y > l->y1) l->y1 = y;
			}
			l->cost += 4 * m;
		} else {
			p += 2 * m;
			VA.overflow = 1;
		}
		expl_at[e] = p;
		if (*p == 0) {
			expl_n--;
			expl_at[e] = expl_at[expl_n];
			expl_x[e] = expl_x[expl_n];
			expl_y[e] = expl_y[expl_n];
			expl_col[e] = expl_col[expl_n];
			expl_ox[e] = expl_ox[expl_n];
			expl_oy[e] = expl_oy[expl_n];
			expl_vw[e] = expl_vw[expl_n];
			expl_vh[e] = expl_vh[expl_n];
			e--;
		}
	}
}

/* ---- drawing ---- */

SEG_VA static void put_pixel(int16 x, int16 y, uint16 v)
{
	uint8 *p = rows[y];

	switch (scr->depth) {
	case 1: {
		uint8 m = (uint8)(0x80 >> (x & 7));

		p += x >> 3;
		*p = (uint8)(*p ^ ((*p ^ v) & m));
		break;
	}
	case 2: {
		uint8 m = (uint8)(0xC0 >> ((x & 3) << 1));

		p += x >> 2;
		*p = (uint8)(*p ^ ((*p ^ v) & m));
		break;
	}
	case 4: {
		uint8 m = (uint8)((x & 1) ? 0x0F : 0xF0);

		p += x >> 1;
		*p = (uint8)(*p ^ ((*p ^ v) & m));
		break;
	}
	case 8:
		p[x] = (uint8)v;
		break;
	default:
		((uint16 *)p)[x] = v;
		break;
	}
}

/* Bresenham, left to right. x-major lines step x every pixel and y when
   the error runs out; y-major the other way round. The grey depths keep a
   byte pointer and a pixel mask; v is the level repeated across the byte,
   so a pixel is *p ^= (*p ^ v) & m. Each loop comes twice: drawing with
   tab's values (V is v), and putting back keep's pixels (V is the byte or
   word at the same place in keep). */
#define GREY_LINES(name, D, TOP, SHIFT, SETV, V) \
SEG_VA static void name(const VASeg *s, const uint8 *col, int16 n, const uint16 *tab) \
{ \
	uint8 **rw = rows; \
	int16 rb = scr->row_bytes; \
	ptrdiff_t ko = keep_off; \
	uint8 v = 0; \
	(void)ko; (void)v; (void)col; (void)tab; \
	for (; n > 0; n--, s++) { \
		int16 dx = (int16)(s->x2 - s->x1); \
		int16 dy = (int16)(s->y2 - s->y1); \
		int16 step = rb, err, k; \
		uint8 *p = rw[s->y1] + (s->x1 >> SHIFT); \
		uint8 m = (uint8)(TOP >> ((s->x1 & ((8 / D) - 1)) * D)); \
		SETV \
		if (dy < 0) { \
			dy = (int16)-dy; \
			step = (int16)-rb; \
		} \
		if (dx >= dy) { \
			/* Pixels in the same byte and row go in one write. */ \
			uint8 acc = 0; \
			err = (int16)(dx >> 1); \
			k = dx; \
			for (;;) { \
				acc |= m; \
				if (--k < 0) \
					break; \
				m = (uint8)(m >> D); \
				if (!m) { \
					*p = (uint8)(*p ^ ((*p ^ V) & acc)); \
					acc = 0; \
					m = TOP; \
					p++; \
				} \
				err = (int16)(err - dy); \
				if (err < 0) { \
					if (acc) { \
						*p = (uint8)(*p ^ ((*p ^ V) & acc)); \
						acc = 0; \
					} \
					p += step; \
					err = (int16)(err + dx); \
				} \
			} \
			*p = (uint8)(*p ^ ((*p ^ V) & acc)); \
		} else if (dx == 0) { \
			for (k = dy; k >= 0; k--) { \
				*p = (uint8)(*p ^ ((*p ^ V) & m)); \
				p += step; \
			} \
		} else { \
			err = (int16)(dy >> 1); \
			k = dy; \
			for (;;) { \
				*p = (uint8)(*p ^ ((*p ^ V) & m)); \
				if (--k < 0) \
					break; \
				p += step; \
				err = (int16)(err - dx); \
				if (err < 0) { \
					m = (uint8)(m >> D); \
					if (!m) { \
						m = TOP; \
						p++; \
					} \
					err = (int16)(err + dy); \
				} \
			} \
		} \
	} \
}

#define TAB_V(T) v = (T)tab[*col++];
#define NO_V

GREY_LINES(lines1, 1, 0x80, 3, TAB_V(uint8), v)
GREY_LINES(lines2, 2, 0xC0, 2, TAB_V(uint8), v)
GREY_LINES(lines4, 4, 0xF0, 1, TAB_V(uint8), v)
GREY_LINES(restore1, 1, 0x80, 3, NO_V, p[ko])
GREY_LINES(restore2, 2, 0xC0, 2, NO_V, p[ko])
GREY_LINES(restore4, 4, 0xF0, 1, NO_V, p[ko])

/* 8 and 16 bit, T the pixel type. 16 bit rows are an even number of bytes
   and word aligned, so pixels are written as words; tab holds them in
   framebuffer byte order. */
#define CHUNKY_LINES(name, T, SETV, V) \
SEG_VA static void name(const VASeg *s, const uint8 *col, int16 n, const uint16 *tab) \
{ \
	uint8 **rw = rows; \
	int16 rs = (int16)(scr->row_bytes / (int16)sizeof(T)); \
	ptrdiff_t ko = keep_off / (ptrdiff_t)sizeof(T); \
	T v = 0; \
	(void)ko; (void)v; (void)col; (void)tab; \
	for (; n > 0; n--, s++) { \
		int16 dx = (int16)(s->x2 - s->x1); \
		int16 dy = (int16)(s->y2 - s->y1); \
		int16 step = rs, err, k; \
		T *p = (T *)rw[s->y1] + s->x1; \
		SETV \
		if (dy < 0) { \
			dy = (int16)-dy; \
			step = (int16)-rs; \
		} \
		if (dx >= dy) { \
			err = (int16)(dx >> 1); \
			k = dx; \
			for (;;) { \
				*p = V; \
				if (--k < 0) \
					break; \
				p++; \
				err = (int16)(err - dy); \
				if (err < 0) { \
					p += step; \
					err = (int16)(err + dx); \
				} \
			} \
		} else { \
			err = (int16)(dy >> 1); \
			k = dy; \
			for (;;) { \
				*p = V; \
				if (--k < 0) \
					break; \
				p += step; \
				err = (int16)(err - dx); \
				if (err < 0) { \
					p++; \
					err = (int16)(err + dy); \
				} \
			} \
		} \
	} \
}

CHUNKY_LINES(lines8, uint8, TAB_V(uint8), v)
CHUNKY_LINES(lines16, uint16, TAB_V(uint16), v)
CHUNKY_LINES(restore8, uint8, NO_V, p[ko])
CHUNKY_LINES(restore16, uint16, NO_V, p[ko])

SEG_VA static void draw_segs(const VASeg *s, const uint8 *col, int16 n, const uint16 *tab)
{
	switch (scr->depth) {
	case 1:  lines1(s, col, n, tab); break;
	case 2:  lines2(s, col, n, tab); break;
	case 4:  lines4(s, col, n, tab); break;
	case 8:  lines8(s, col, n, tab); break;
	default: lines16(s, col, n, tab); break;
	}
}

SEG_VA static void restore_segs(const VASeg *s, int16 n)
{
	switch (scr->depth) {
	case 1:  restore1(s, NULL, n, NULL); break;
	case 2:  restore2(s, NULL, n, NULL); break;
	case 4:  restore4(s, NULL, n, NULL); break;
	case 8:  restore8(s, NULL, n, NULL); break;
	default: restore16(s, NULL, n, NULL); break;
	}
}

/* keep's byte (1, 2, 4, 8 bit) or word (16 bit) holding pixel x, y, for
   put_pixel: its mask picks the pixel out of the byte. */
SEG_VA static uint16 keep_value(int16 x, int16 y)
{
	const uint8 *k = rows[y] + keep_off;

	switch (scr->depth) {
	case 1:  return k[x >> 3];
	case 2:  return k[x >> 2];
	case 4:  return k[x >> 1];
	case 8:  return k[x];
	default: return ((const uint16 *)k)[x];
	}
}

/* Spots, lines, then pixels, as the original drew them; glyphs go after
   (text was drawn straight away there). A NULL tab puts back keep's
   pixels. */
SEG_VA static void draw_list(VAList *l, const uint16 *tab)
{
	int16 i;

	for (i = 0; i < l->nspots; i++) {
		int16 x = l->spots[i].x, y = l->spots[i].y;

		if (tab) {
			uint16 v = tab[l->spotcol[i]];

			put_pixel(x, (int16)(y - 1), v);
			put_pixel((int16)(x - 1), y, v);
			put_pixel(x, y, v);
			put_pixel((int16)(x + 1), y, v);
			put_pixel(x, (int16)(y + 1), v);
		} else {
			put_pixel(x, (int16)(y - 1), keep_value(x, (int16)(y - 1)));
			put_pixel((int16)(x - 1), y, keep_value((int16)(x - 1), y));
			put_pixel(x, y, keep_value(x, y));
			put_pixel((int16)(x + 1), y, keep_value((int16)(x + 1), y));
			put_pixel(x, (int16)(y + 1), keep_value(x, (int16)(y + 1)));
		}
	}
	if (tab)
		draw_segs(l->lines, l->linecol, l->nlines, tab);
	else
		restore_segs(l->lines, l->nlines);
	for (i = 0; i < l->npix; i++) {
		int16 x = l->pix[i].x, y = l->pix[i].y;

		put_pixel(x, y, tab ? tab[l->pixcol[i]] : keep_value(x, y));
	}
}

SEG_VA static void draw_glyphs(VAList *l, const uint16 *tab)
{
	draw_glyphs_n(l, l->nglyphs, tab);
}

/* The first count glyphs of l. */
SEG_VA static void draw_glyphs_n(VAList *l, int16 count, const uint16 *tab)
{
	int16 i;
	uint32 tg = plat_ticks();

	/* Glyphs are turned into lines a batch at a time. */
	i = 0;
	while (i < count) {
		VASeg segs[8 * VA_GLYPH_SEGS];
		uint8 cols[8 * VA_GLYPH_SEGS];
		int16 n = 0, k, m;

		for (; i < count && n <= 7 * VA_GLYPH_SEGS; i++) {
			VAGlyph *g = l->glyphs + i;

			m = va_glyph_segs(g->c, g->x, g->y, g->w, g->h, segs + n);
			for (k = 0; k < m; k++)
				cols[n + k] = g->col;
			n = (int16)(n + m);
		}
		if (tab)
			draw_segs(segs, cols, n, tab);
		else
			restore_segs(segs, n);
	}
	if (tab == vals)
		VA.GlyphTicks += plat_ticks() - tg;
}

SEG_VA static void fill_all(void);

/* Fills the whole framebuffer, status area included, with the background. */
SEG_VA static void fill_all(void)
{
	int16 y, i, n;
	uint16 v = vals[9];

	if (!scr || !rows)
		return;
	VA.Wipes++;
	for (y = 0; y < scr->height; y++) {
		if (scr->depth == 16) {
			uint16 *p = (uint16 *)rows[y];

			n = (int16)(scr->row_bytes >> 1);
			for (i = 0; i < n; i++)
				p[i] = v;
		} else {
			uint8 *p = rows[y];

			n = scr->row_bytes;
			for (i = 0; i < n; i++)
				p[i] = (uint8)v;
		}
	}
}

/* The whole screen to the display, by the backend's direct path. */
SEG_VA static void present_all(void)
{
	PlatRect r;

	r.x = 0;
	r.y = 0;
	r.w = scr->width;
	r.h = scr->height;
	plat_present_rect(&r);
}

SEG_VA void VAEraseBuffer(void)
{
	still_ok = 0;
	still_bake = 0;
	if (!scr || !rows)
		return;
	fill_all();
	/* The screen under the offscreen may hold anything; later frames only
	   present what they change. */
	plat_present();
}

SEG_VA void VARepaint(void)
{
	VAList *on = &lists[1 - cur];

	/* The still picture isn't in the list: it is baked again next frame. */
	still_ok = 0;
	still_bake = 0;
	if (!scr || !rows)
		return;
	fill_all();
	draw_list(on, vals);
	draw_glyphs(on, vals);
	plat_present();
}

/* Fills x0..x1, y0..y1 (framebuffer pixels, inclusive) with the
   background, whole words at a time, longs where the rows allow. */
SEG_VA static void erase_box(int16 x0, int16 y0, int16 x1, int16 y1)
{
	int16 b0, b1, y, i, n;
	uint16 v = vals[9];
	uint32 v2;

	if (scr->depth == 16) {
		b0 = x0;
		b1 = x1;
	} else {
		int16 sh = (int16)(scr->depth == 8 ? 1 : scr->depth == 4 ? 2 : scr->depth == 2 ? 3 : 4);

		v = (uint16)((v & 0xFF) | (v << 8));
		b0 = (int16)(x0 >> sh);
		b1 = (int16)(x1 >> sh);
	}
	v2 = ((uint32)v << 16) | v;
	/* b0 to b1 are now word indices in the row. */
	for (y = y0; y <= y1; y++) {
		uint16 *p = (uint16 *)rows[y] + b0;

		n = (int16)(b1 - b0 + 1);
		if (aligned4) {
			uint32 *q;

			if (b0 & 1) {
				*p++ = v;
				n--;
			}
			q = (uint32 *)p;
			for (i = n; i >= 16; i -= 16) {
				q[0] = v2; q[1] = v2; q[2] = v2; q[3] = v2;
				q[4] = v2; q[5] = v2; q[6] = v2; q[7] = v2;
				q += 8;
			}
			for (; i >= 2; i -= 2)
				*q++ = v2;
			if (i)
				*(uint16 *)q = v;
		} else {
			for (i = n; i >= 8; i -= 8) {
				p[0] = v; p[1] = v; p[2] = v; p[3] = v;
				p[4] = v; p[5] = v; p[6] = v; p[7] = v;
				p += 8;
			}
			while (i-- > 0)
				*p++ = v;
		}
	}
}

/* Copies x0..x1, y0..y1 (framebuffer pixels, inclusive) back from keep,
   whole words at a time, longs where the rows and keep allow. */
SEG_VA static void restore_box(int16 x0, int16 y0, int16 x1, int16 y1)
{
	int16 b0, b1, y, i, n;
	cbool longs = aligned4 && ((unsigned long)keep & 3) == 0;

	if (scr->depth == 16) {
		b0 = x0;
		b1 = x1;
	} else {
		int16 sh = (int16)(scr->depth == 8 ? 1 : scr->depth == 4 ? 2 : scr->depth == 2 ? 3 : 4);

		b0 = (int16)(x0 >> sh);
		b1 = (int16)(x1 >> sh);
	}
	for (y = y0; y <= y1; y++) {
		uint16 *p = (uint16 *)rows[y] + b0;
		const uint16 *k = (const uint16 *)(rows[y] + keep_off) + b0;

		n = (int16)(b1 - b0 + 1);
		if (longs) {
			uint32 *q;
			const uint32 *kq;

			if (b0 & 1) {
				*p++ = *k++;
				n--;
			}
			q = (uint32 *)p;
			kq = (const uint32 *)k;
			for (i = n; i >= 8; i -= 8) {
				q[0] = kq[0]; q[1] = kq[1]; q[2] = kq[2]; q[3] = kq[3];
				q += 4;
				kq += 4;
			}
			for (; i >= 2; i -= 2)
				*q++ = *kq++;
			if (i)
				*(uint16 *)q = *(const uint16 *)kq;
		} else {
			while (n-- > 0)
				*p++ = *k++;
		}
	}
}

/* ---- the still picture ---- */

SEG_VA static void keep_drop(void)
{
	if (keep)
		plat_keep_free(keep);
	keep = NULL;
	keep_size = 0;
	still_ok = 0;
	still_bake = 0;
}

SEG_VA void VAStillFree(void)
{
	keep_drop();
}

SEG_VA cbool VAStillBegin(uint16 key)
{
	VAList *l = &lists[cur];
	uint32 want;
	int16 i;
	cbool same = 1;

	still_used = 1;
#ifdef VA_STILL_CHECK
	va_still_begins++;
#endif
	if (!scr || !rows)
		return 1;
	for (i = 0; i < NUMCOLORS; i++)
		if (vals[i] != still_vals[i])
			same = 0;
	if (still_ok && key == still_key) {
		if (same) {
			still_misses = 0;
			return 0;
		}
		/* Other colours for a moment (a flash): drawn in the frame, over
		   the kept one. */
		if (++still_misses < STILL_MISSES)
			return 1;
	}
	/* An old picture goes (VAStep clears the screen) unless a new one is
	   baked; it is baked from the start of the list. */
	if (still_ok && key != still_key)
		still_used = 0;
	if (l->nlines || l->npix || l->nspots || l->nglyphs)
		return 1;
	want = (uint32)scr->row_bytes * scr->height;
	if (!keep || keep_size != want) {
		keep_drop();
		if (keep_failed == want)
			return 1;
		keep = plat_keep_alloc(want);
		if (!keep) {
			keep_failed = want;
			return 1;
		}
		keep_size = want;
		keep_failed = 0;
	}
	keep_off = keep - scr->bits;
	still_bake = 1;
	still_new = key;
	still_lines = -1;
	return 1;
}

SEG_VA void VAStillEnd(void)
{
	if (still_bake && still_lines < 0) {
		still_lines = lists[cur].nlines;
		still_glyphs = lists[cur].nglyphs;
	}
}

#ifdef VA_STILL_CHECK

/* Host tests: the screen as the still picture's copy with this frame drawn
   over it, against what erasing by keep left. */
SEG_VA static void check_still(void)
{
	uint32 n = keep_size, i;
	uint8 *was = (uint8 *)plat_alloc(n);
	VAList *c = &lists[cur];
	int16 y;

	if (!was)
		return;
	for (i = 0; i < n; i++)
		was[i] = scr->bits[i];
	for (y = 0; y < scr->height; y++)
		for (i = 0; i < (uint32)scr->row_bytes; i++)
			rows[y][i] = rows[y][i + keep_off];
	draw_list(c, vals);
	draw_glyphs(c, vals);
	va_still_checks++;
	/* The play area only: the status panel is painted over keep. */
	for (y = playrect.y; y < playrect.y + playrect.h; y++) {
		uint32 b0 = (uint32)y * scr->row_bytes + (uint32)playrect.x * scr->depth / 8;
		uint32 b1 = (uint32)y * scr->row_bytes + ((uint32)(playrect.x + playrect.w) * scr->depth + 7) / 8;

		for (i = b0; i < b1; i++)
			if (was[i] != scr->bits[i])
				break;
		if (i < b1) {
			va_still_bad++;
			break;
		}
	}
	for (i = 0; i < n; i++)
		scr->bits[i] = was[i];
	plat_free(was);
}

#endif
SEG_VA void VAStep(void)
{
	VAList *prev;
	uint32 t0, t1, te;
	int16 i;
	cbool full;

	if (!scr)
		return;
	t0 = plat_ticks();
	insert_explosions();
	prev = &lists[1 - cur];
	te = plat_ticks();
	if (still_bake && still_lines < 0)
		still_bake = 0;
	/* A new still picture, or the old one no longer wanted: the whole
	   screen starts again from the background. */
	full = (cbool)(still_bake || (still_ok && !still_used));
	if (full) {
		if (VA.KeepStatus && scr->status.w > 0 && scr->status.h > 0)
			erase_box(playrect.x, playrect.y, (int16)(playrect.x + playrect.w - 1),
			          (int16)(playrect.y + playrect.h - 1));
		else
			fill_all();
		still_ok = 0;
		if (still_bake) {
			VAList *c = &lists[cur];

			draw_segs(c->lines, c->linecol, still_lines, vals);
			draw_glyphs_n(c, still_glyphs, vals);
			plat_keep_write(keep, 0, scr->bits, keep_size);
			still_ok = 1;
#ifdef VA_STILL_CHECK
			va_still_bakes++;
#endif
			still_key = still_new;
			still_misses = 0;
			for (i = 0; i < NUMCOLORS; i++)
				still_vals[i] = vals[i];
		}
	} else if (still_ok) {
		/* Put back keep's pixels under the last frame: by its lines, or the
		   box round it where that is cheaper (a word copied costs about a
		   quarter of a pixel on a line). */
		if (prev->x0 <= prev->x1) {
			int32 words = (int32)(prev->y1 - prev->y0 + 1) *
			              ((int32)(prev->x1 - prev->x0) * scr->depth / 16 + 1);

			if (VA.EraseMode == VA_ERASE_BOX ||
			    (VA.EraseMode == VA_ERASE_AUTO && words / 4 < prev->cost))
				restore_box(prev->x0, prev->y0, prev->x1, prev->y1);
			else
				draw_list(prev, NULL);
		}
		if (VA.EraseMode == VA_ERASE_REDRAW) {
			draw_glyphs(prev, NULL);
		} else {
			VAGlyph *g = prev->glyphs;

			for (i = 0; i < prev->nglyphs; ) {
				int16 x0 = g[i].x, x1 = (int16)(g[i].x + g[i].w), y = g[i].y, h = g[i].h, j;

				for (j = (int16)(i + 1); j < prev->nglyphs && g[j].y == y && g[j].h == h &&
				     g[j].x >= x0 && g[j].x <= x1 + 2 * h; j++)
					x1 = (int16)(g[j].x + g[j].w);
				restore_box(x0, (int16)(y - h), x1, y);
				i = j;
			}
		}
	} else {
		/* Erase the last frame by drawing it again in the background, or
		   by filling the box round it where that is cheaper: a word store
		   costs about an eighth of a pixel on a line. */
		if (prev->x0 <= prev->x1) {
			int32 words = (int32)(prev->y1 - prev->y0 + 1) *
			              ((int32)(prev->x1 - prev->x0) * scr->depth / 16 + 1);

			if (VA.EraseMode == VA_ERASE_BOX ||
			    (VA.EraseMode == VA_ERASE_AUTO && words / 8 < prev->cost))
				erase_box(prev->x0, prev->y0, prev->x1, prev->y1);
			else
				draw_list(prev, bgvals);
		}
		/* Glyphs in a row on one baseline are erased with one box each run. */
		if (VA.EraseMode == VA_ERASE_REDRAW) {
			draw_glyphs(prev, bgvals);
		} else {
			VAGlyph *g = prev->glyphs;

			for (i = 0; i < prev->nglyphs; ) {
				int16 x0 = g[i].x, x1 = (int16)(g[i].x + g[i].w), y = g[i].y, h = g[i].h, j;

				for (j = (int16)(i + 1); j < prev->nglyphs && g[j].y == y && g[j].h == h &&
				     g[j].x >= x0 && g[j].x <= x1 + 2 * h; j++)
					x1 = (int16)(g[j].x + g[j].w);
				erase_box(x0, (int16)(y - h), x1, y);
				i = j;
			}
		}
	}
	still_bake = 0;
	still_used = 0;
	VA.EraseTicks += plat_ticks() - te;
	draw_list(&lists[cur], vals);
	draw_glyphs(&lists[cur], vals);
	t1 = plat_ticks();
	/* Only what changed goes to the screen. */
	if (full) {
		present_all();
	} else {
		VAList *c = &lists[cur];
		int16 x0 = prev->x0, y0 = prev->y0, x1 = prev->x1, y1 = prev->y1;
		PlatRect r;

		if (prev->gx0 < x0) x0 = prev->gx0;
		if (prev->gy0 < y0) y0 = prev->gy0;
		if (prev->gx1 > x1) x1 = prev->gx1;
		if (prev->gy1 > y1) y1 = prev->gy1;
		if (c->x0 < x0) x0 = c->x0;
		if (c->y0 < y0) y0 = c->y0;
		if (c->x1 > x1) x1 = c->x1;
		if (c->y1 > y1) y1 = c->y1;
		if (c->gx0 < x0) x0 = c->gx0;
		if (c->gy0 < y0) y0 = c->gy0;
		if (c->gx1 > x1) x1 = c->gx1;
		if (c->gy1 > y1) y1 = c->gy1;
		if (x0 <= x1) {
			r.x = x0;
			r.y = y0;
			r.w = (int16)(x1 - x0 + 1);
			r.h = (int16)(y1 - y0 + 1);
			plat_present_rect(&r);
		}
	}
#ifdef VA_STILL_CHECK
	if (still_ok && !full)
		check_still();
#endif
	VA.DrawTicks += t1 - t0;
	VA.PresentTicks += plat_ticks() - t1;
	empty_list(prev);
	cur = (int16)(1 - cur);
	VA.FrameCounter++;
	VA.overflow = 0;
}

SEG_VA void VABake(void)
{
	VAList *l = &lists[cur];

	if (!scr || !rows)
		return;
	draw_list(l, vals);
	draw_glyphs(l, vals);
	empty_list(l);
}

SEG_VA void VAForget(void)
{
	still_ok = 0;
	empty_list(&lists[1 - cur]);
}

SEG_VA cbool VAShown(PlatRect *r)
{
	VAList *l = &lists[1 - cur];
	int16 x0 = l->x0, y0 = l->y0, x1 = l->x1, y1 = l->y1;

	if (l->gx0 < x0) x0 = l->gx0;
	if (l->gy0 < y0) y0 = l->gy0;
	if (l->gx1 > x1) x1 = l->gx1;
	if (l->gy1 > y1) y1 = l->gy1;
	if (x0 > x1 || y0 > y1)
		return 0;
	r->x = x0;
	r->y = y0;
	r->w = (int16)(x1 - x0 + 1);
	r->h = (int16)(y1 - y0 + 1);
	return 1;
}

SEG_VA void VACancelFrame(void)
{
	VAList *l = &lists[cur];

	insert_explosions();
	empty_list(l);
	still_bake = 0;
	still_used = 0;
}
