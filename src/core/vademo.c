/* See vademo.h. The tube is built as GridTest's MakeGrid builds it: the
   shape's turns (in ANGLES) walked as unit steps, fitted to the play area
   with a margin, then drawn as FieldRide draws the field: near rim, far
   rim at STARTDEPTH / (STARTDEPTH + DEPTH) of the size towards the twist
   offset, and a line down each lane edge. */
#include "vademo.h"
#include "va.h"
#include "parser.h"
#include "internalvars.h"

#define MAXSEGS    32
#define DEPTH      120
#define STARTDEPTH 20

static int16 nsegs;
static cbool wraps;
static int16 wx[MAXSEGS + 1], wy[MAXSEGS + 1];   /* pixels * 16 from the centre */
static int16 twx, twy;                            /* twist offset, pixels * 16 */
static int16 spin;
static uint32 t_start;
static uint32 f_start;
static int32 fps10, ms10;
static int32 draw10, present10;     /* ms * 10 per frame in VAStep's parts */
static uint32 draw_start, present_start;
static uint32 queue_ticks, queue_start;
static uint32 erase_start, glyph_start_t;
static int32 erase10, glyph10;
static int32 queue10;              /* ms * 10 per frame queueing it */
static uint16 seed = 12345;

SEG_PAT static void load_shape(int16 field);
SEG_PAT static int16 demo_random(void);
SEG_PAT static int16 put_num(char *s, int32 v, int16 tenths);

SEG_PAT static int16 demo_random(void)
{
	seed = (uint16)(seed * 25173u + 13849u);
	return (int16)(seed >> 1);
}

/* The shape's points, fitted to the play area. */
SEG_PAT static void load_shape(int16 field)
{
	uint32 bytes = 0;
	const uint8 *d = plat_resource(FOURCC('S', 'P', 'C', 'E'), field, &bytes);
	int16 ang[MAXSEGS];
	int32 px[MAXSEGS + 1], py[MAXSEGS + 1];   /* units of 1/256 */
	int32 xmin, xmax, ymin, ymax, ext, cx, cy;
	int16 i, a, xoff = 0, yoff = 0;
	int16 size = VA.frame.right;

	nsegs = 0;
	if (d && bytes >= 8) {
		nsegs = gets16(d);
		wraps = d[2] != 0;
		xoff = (int8)d[4];
		yoff = (int8)d[5];
		if (nsegs > MAXSEGS)
			nsegs = MAXSEGS;
		if ((uint32)(8 + 2 * nsegs) > bytes)
			nsegs = (int16)((bytes - 8) / 2);
		for (i = 0; i < nsegs; i++)
			ang[i] = gets16(d + 8 + 2 * i);
	}
	if (d)
		plat_release(d);
	if (nsegs < 2) {
		/* No shape: a closed 16-sided tube. */
		nsegs = 16;
		wraps = 1;
		for (i = 0; i < nsegs; i++)
			ang[i] = (int16)((i & 1) ? 7 : 8);
	}

	a = 0;
	px[0] = py[0] = 0;
	xmin = xmax = ymin = ymax = 0;
	for (i = 0; i < nsegs; i++) {
		a = (int16)((a + ang[i]) % ANGLES);
		if (a < 0)
			a = (int16)(a + ANGLES);
		px[i + 1] = px[i] + Cosins[a];
		py[i + 1] = py[i] - Sins[a];
		if (px[i + 1] < xmin) xmin = px[i + 1];
		if (px[i + 1] > xmax) xmax = px[i + 1];
		if (py[i + 1] < ymin) ymin = py[i + 1];
		if (py[i + 1] > ymax) ymax = py[i + 1];
	}
	if (wraps) {
		px[nsegs] = px[0];
		py[nsegs] = py[0];
	}
	/* 1.05 units of margin each side, as MakeGrid. */
	xmin -= 269;
	ymin -= 269;
	xmax += 269;
	ymax += 269;
	ext = xmax - xmin > ymax - ymin ? xmax - xmin : ymax - ymin;
	cx = (xmin + xmax) / 2;
	cy = (ymin + ymax) / 2;
	for (i = 0; i <= nsegs; i++) {
		wx[i] = (int16)((px[i] - cx) * 16 * size / ext);
		wy[i] = (int16)((py[i] - cy) * 16 * size / ext);
	}
	/* xoff / 32 units, in pixels * 16. */
	twx = (int16)((int32)xoff * 256 / 32 * 16 * size / ext);
	twy = (int16)((int32)yoff * 256 / 32 * 16 * size / ext);
}

SEG_PAT cbool vademo_init(int16 level)
{
	int16 field = 1, colours = 1000;

	if (parser_alloc()) {
		parser_load_level(level);
		parser_write(kConstDEPTH, 120.0);
		field = (int16)parser_eval(kVarLevelField);
		colours = (int16)parser_eval(kVarLevelColor);
		parser_free();
	}
	VALoadColors(colours);
	load_shape(field);
	spin = 0;
	t_start = plat_ticks();
	f_start = 0;
	fps10 = ms10 = draw10 = present10 = 0;
	draw_start = VA.DrawTicks;
	present_start = VA.PresentTicks;
	return nsegs > 0;
}

SEG_PAT int32 vademo_fps10(void)
{
	return fps10;
}

/* Writes v (with one decimal when tenths is set) and returns its length. */
SEG_PAT static int16 put_num(char *s, int32 v, int16 tenths)
{
	char tmp[12];
	int16 n = 0, len = 0;
	uint16 u, d;

	if (v < 0)
		v = 0;
	if (v > 32767)
		v = 32767;
	u = (uint16)v;
	do {
		d = (uint16)(u / 10);
		tmp[n++] = (char)('0' + (int16)(u - d * 10));
		u = d;
		if (tenths && n == 1)
			tmp[n++] = '.';
	} while (u != 0 || (tenths && n < 3));
	while (n)
		s[len++] = tmp[--n];
	return len;
}

SEG_PAT void vademo_frame(void)
{
	int16 size = VA.frame.right;
	int16 cx = (int16)(size / 2), cy = (int16)(size / 2);
	int16 nx[MAXSEGS + 1], ny[MAXSEGS + 1], fx[MAXSEGS + 1], fy[MAXSEGS + 1];
	int16 c = Cosins[spin], s = Sins[spin];
	int16 tx, ty, i, sc, len;
	char line[40];
	uint32 frames = VA.FrameCounter;
	uint32 t_frame = plat_ticks();

	/* Spin the shape and its twist about the centre. */
	tx = (int16)(((int32)twx * c - (int32)twy * s) >> 8);
	ty = (int16)(((int32)twx * s + (int32)twy * c) >> 8);
	for (i = 0; i <= nsegs; i++) {
		int16 rx = (int16)(((int32)wx[i] * c - (int32)wy[i] * s) >> 8);
		int16 ry = (int16)(((int32)wx[i] * s + (int32)wy[i] * c) >> 8);

		nx[i] = (int16)(cx + (rx >> 4));
		ny[i] = (int16)(cy + (ry >> 4));
		/* STARTDEPTH / (STARTDEPTH + DEPTH) is 1/7, 2341 / 16384. */
		fx[i] = (int16)(cx + (int16)((((int32)(int16)(rx - tx) * 2341 >> 14) + tx) >> 4));
		fy[i] = (int16)(cy + (int16)((((int32)(int16)(ry - ty) * 2341 >> 14) + ty) >> 4));
	}
	spin = (int16)((spin + 1) % ANGLES);

	for (i = 0; i <= nsegs; i++) {
		VA.color = (int16)(i % 7);
		VALine(nx[i], ny[i], fx[i], fy[i]);
	}
	VA.color = 1;
	VAMoveTo(nx[0], ny[0]);
	for (i = 1; i <= nsegs; i++)
		VALineTo(nx[i], ny[i]);
	VA.color = 3;
	VAMoveTo(fx[0], fy[0]);
	for (i = 1; i <= nsegs; i++)
		VALineTo(fx[i], fy[i]);
	VA.color = 5;
	for (i = 0; i < nsegs; i++)
		VASpot((int16)((nx[i] + nx[i + 1]) / 2), (int16)((ny[i] + ny[i + 1]) / 2));
	VA.color = 0;
	for (i = 0; i < 24; i++)
		VAPixel((int16)(demo_random() % size), (int16)(demo_random() % size));
	if (frames % 30 == 0)
		VAExplosion((int16)(cx - size / 4 + demo_random() % (size / 2)),
		            (int16)(cy - size / 4 + demo_random() % (size / 2)),
		            (int16)((frames / 30) & 3), (int16)((frames / 30) % 7));

	/* Frame rate over each 32 frames. */
	if (frames - f_start >= 32) {
		uint32 now = plat_ticks();
		uint32 dt = now - t_start;
		uint32 tps = plat_ticks_per_second();

		if (dt > 0) {
			uint32 per = (frames - f_start) * tps;

			fps10 = (int32)((frames - f_start) * 10 * tps / dt);
			ms10 = (int32)(dt * 10000 / per);
			draw10 = (int32)((VA.DrawTicks - draw_start) * 10000 / per);
			present10 = (int32)((VA.PresentTicks - present_start) * 10000 / per);
			queue10 = (int32)((queue_ticks - queue_start) * 10000 / per);
			erase10 = (int32)((VA.EraseTicks - erase_start) * 10000 / per);
			glyph10 = (int32)((VA.GlyphTicks - glyph_start_t) * 10000 / per);
		}
		t_start = now;
		f_start = frames;
		draw_start = VA.DrawTicks;
		present_start = VA.PresentTicks;
		queue_start = queue_ticks;
		erase_start = VA.EraseTicks;
		glyph_start_t = VA.GlyphTicks;
	}

	sc = VA.segmscale;
	VA.color = BG2;
	VAMoveTo(2, (int16)(sc * 4 + 4));
	VADrawText("ARASHI", 0, 6);
	VA.color = 0;
	len = 0;
	line[len++] = 'F'; line[len++] = 'P'; line[len++] = 'S'; line[len++] = ' ';
	len = (int16)(len + put_num(line + len, fps10, 1));
	VAMoveTo(2, (int16)(size - 2 - (sc * 4 + 4)));
	VADrawText(line, 0, len);
	len = 0;
	line[len++] = 'M'; line[len++] = 'S'; line[len++] = ' ';
	len = (int16)(len + put_num(line + len, ms10, 1));
	VAMoveTo(2, (int16)(size - 2));
	VADrawText(line, 0, len);
#ifdef VADEMO_PROFILE
	/* Parts of the frame time in ms: D erase and draw, P present, Q
	   queueing, E erase, G glyphs. */
	len = 0;
	line[len++] = 'D'; line[len++] = ' ';
	len = (int16)(len + put_num(line + len, draw10, 1));
	line[len++] = ' '; line[len++] = 'P'; line[len++] = ' ';
	len = (int16)(len + put_num(line + len, present10, 1));
	VAMoveTo(2, (int16)(2 * (sc * 4 + 4) + 2));
	VADrawText(line, 0, len);
	len = 0;
	line[len++] = 'Q'; line[len++] = ' ';
	len = (int16)(len + put_num(line + len, queue10, 1));
	VAMoveTo(2, (int16)(3 * (sc * 4 + 4) + 4));
	VADrawText(line, 0, len);
	len = 0;
	line[len++] = 'E'; line[len++] = ' ';
	len = (int16)(len + put_num(line + len, erase10, 1));
	line[len++] = ' '; line[len++] = 'G'; line[len++] = ' ';
	len = (int16)(len + put_num(line + len, glyph10, 1));
	VAMoveTo(2, (int16)(4 * (sc * 4 + 4) + 6));
	VADrawText(line, 0, len);
#endif
	VA.color = 6;
	VADrawNumber((int32)VA.FrameCounter, (int16)(size - sc - 9), (int16)(size - 2));
	queue_ticks += plat_ticks() - t_frame;
	VAStep();
}
