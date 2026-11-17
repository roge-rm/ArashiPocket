/* The front end. One state machine; every screen draws lines and text
   through the renderer into the play area, UI screens step 20 times a second
   like the game. */
#include "front.h"
#include "game.h"
#include "va.h"
#include "store.h"
#include "qdrandom.h"

#define UI_HZ 20
#define NUM_DEMOS 8
#define MAX_CHOICES 95
#define MENU_ITEMS 5
#define TITLE_STEPS 100
#define SCORES_STEPS 120
#define OVER_STEPS 70
#define TEXT_STEPS 60
#define TEXT_PAGES 4

typedef struct {
	int16 x0, y0, x1, y1;
} Seg;

static int16 screen;
static uint32 ui_next;          /* ticks when the next UI step is due */
static uint32 frame;            /* UI steps on this screen */
static uint16 prev_act;
static cbool prev_pen;
static int16 last_x, last_y;
static int16 spin_acc;
static int16 sel;               /* highlighted item */
static int16 attract_n;         /* which demo is next */
static int16 page;              /* high score page, level choice */
static int16 text_page;         /* attract text page 0 to 3 (TEXT 128 to 131) */
static int16 hl_rank = -1;      /* the new score's row */
static int16 hl_table;
static char name[STORE_NAME + 1];
static int16 name_pos;
/* The name wheel, from GetInitials: '@' to ']' round a drum, one every 4
   angles, turned in fifths of an angle (wheel_big). The letter at angles 88
   to 91 is the one picked; it settles on angles 4n + 1. Letters are laid out
   and turned as in the original: dragging right turns the drum right to
   left, the alphabet reads right to left, > brings the next letter. */
#define NAME_LEN     5
#define WHEEL_CHARS  30
#define SUBROTATION  5
#define MAXROTLETTER 20
static int16 wheel_big, wheel_hist, wheel_dir, wheel_acc, wheel_px, wheel_moved;
static cbool wheel_drag;
static const uint8 *logo;
static uint32 logo_size;
static int16 logo_n;
#define TITLE_REPEAT 400        /* steps before the logo sparks in again */
#define TITLE_ZOOM   60         /* step the box starts zooming in */
#define ZOOMSTEPS    20
#define JITTERTIME   6
static uint32 title_seed = 1;
static int8 jitter[256];
static cbool jitter_ready;
static int16 jitter_point, jitter_step, sizzle_level, sizzle_factor;
static LevelChoice choices[MAX_CHOICES];
/* The carousel: sel_off pixels past start_choice, gliding back to 0. */
static int16 sel_off, sel_px, sel_moved, sel_coloured = -1;
static cbool sel_drag;
static int16 nchoices;
static cbool choices_ready;
static int16 start_choice;      /* the level select's current entry */
static int16 ts;                /* text scale */
static int16 lh;                /* line height */
static cbool tap;               /* a tap this step (tap_x, tap_y) */
static int16 tap_x, tap_y;
static uint8 pressed;           /* bit 0 prev, 1 next, 2 ok, 3 back, set once per press */

#define P_PREV 1
#define P_NEXT 2
#define P_OK   4
#define P_BACK 8

SEG_FRONT static void set_screen(int16 s);
SEG_FRONT static cbool tap_pauses(void);
SEG_FRONT static void read_input(PlatInput *in);
SEG_FRONT static void metrics(void);
SEG_FRONT static void text(const char *s, int16 cx, int16 y, int16 color);
SEG_FRONT static void name_begin(void);
SEG_FRONT static void wheel_turn(int16 delta);
SEG_FRONT static char wheel_char(void);
SEG_FRONT static void name_step(const PlatInput *in);
SEG_FRONT static void text_left(const char *s, int16 x, int16 y, int16 color);
SEG_FRONT static int16 slen(const char *s);
SEG_FRONT static int16 fmt_num(char *out, int32 v);
SEG_FRONT static void draw_logo(int16 top, int16 height);
SEG_FRONT static int16 title_random(void);
SEG_FRONT static void sizzle(int16 x1, int16 y1, int16 x2, int16 y2);
SEG_FRONT static void fractal_line(int16 x1, int16 y1, int16 x2, int16 y2, int16 factor, int16 level);
SEG_FRONT static void draw_zoom(int16 t, int16 x0, int16 y0, int16 x1, int16 y1);
SEG_FRONT static int16 row_at(int16 y, int16 y0, int16 n);
SEG_FRONT static void draw_title(void);
SEG_FRONT static void draw_menu(void);
SEG_FRONT static void draw_levels(void);
SEG_FRONT static void draw_shape(int16 field, int16 cx, int16 cy, int16 box);
SEG_FRONT static void level_colours(void);
SEG_FRONT static void levels_step(const PlatInput *in);
SEG_FRONT static void draw_scores(cbool full);
SEG_FRONT static void draw_text_page(void);
SEG_FRONT static void firework(void);
SEG_FRONT static void draw_options(void);
SEG_FRONT static void draw_about(void);
SEG_FRONT static void draw_pause(void);
SEG_FRONT static void draw_over(void);
SEG_FRONT static void draw_name(void);
SEG_FRONT static void ui_step(const PlatInput *in);
SEG_FRONT static void start_game(int16 level);
SEG_FRONT static void load_choices(void);
SEG_FRONT static void play_step(const PlatInput *in);
SEG_FRONT static void end_game(void);
SEG_FRONT static void begin_demo(void);
SEG_FRONT static void set_colors(void);

SEG_FRONT static int16 slen(const char *s)
{
	int16 n = 0;

	while (s[n])
		n++;
	return n;
}

SEG_FRONT static int16 fmt_num(char *out, int32 v)
{
	char tmp[12];
	int16 n = 0, len = 0;
	uint32 u = v < 0 ? 0 : (uint32)v;

	do {
		tmp[n++] = (char)('0' + (int16)(u % 10));
		u /= 10;
	} while (u);
	while (n)
		out[len++] = tmp[--n];
	out[len] = 0;
	return len;
}

/* 16 characters across the frame, bigger on bigger screens. */
SEG_FRONT static void metrics(void)
{
	ts = (int16)((VA.frame.right / 16 - 3) / 3);
	if (ts < 1)
		ts = 1;
	if (ts > 12)
		ts = 12;
	lh = (int16)(ts * 6 + 8);
}

SEG_FRONT static void text_left(const char *s, int16 x, int16 y, int16 color)
{
	VA.color = color;
	VA.segmscale = ts;
	VA.CurrentX = x;
	VA.CurrentY = y;
	VADrawText(s, 0, slen(s));
}

SEG_FRONT static void text(const char *s, int16 cx, int16 y, int16 color)
{
	int16 w = (int16)(slen(s) * (ts * 3 + 3));

	text_left(s, (int16)(cx - w / 2), y, color);
}

SEG_FRONT static void set_colors(void)
{
	VALoadColors(1000);
	VAEraseBuffer();
}

SEG_FRONT static void set_screen(int16 s)
{
	screen = s;
	frame = 0;
	sel = 0;
	tap = 0;
}

/* Whether the tap at tap_x, tap_y (play area coordinates) is the pause tap. */
SEG_FRONT static cbool tap_pauses(void)
{
	const PlatScreen *sc = plat_screen();

	if (sc && sc->status.w > 0 && sc->status.h > 0) {
		int16 x = (int16)(tap_x + sc->play.x), y = (int16)(tap_y + sc->play.y);

		return (cbool)(x >= sc->status.x && y >= sc->status.y &&
		               x < sc->status.x + sc->status.w && y < sc->status.y + sc->status.h);
	}
	return (cbool)(tap_x < 0 || tap_y < 0 || tap_x >= VA.frame.right || tap_y >= VA.frame.bottom);
}

SEG_FRONT static void read_input(PlatInput *in)
{
	uint16 edge;

	plat_input(in);
	edge = (uint16)(in->actions & ~prev_act);
	prev_act = in->actions;
	pressed = 0;
	if (edge & PLAT_ACT_LEFT)
		pressed |= P_PREV;
	if (edge & PLAT_ACT_RIGHT)
		pressed |= P_NEXT;
	if (edge & (PLAT_ACT_FIRE | PLAT_ACT_ZAP))
		pressed |= P_OK;
	if (edge & (PLAT_ACT_PAUSE | PLAT_ACT_MENU))
		pressed |= P_BACK;
	/* Fire and superzap held together also pause, for devices with no menu
	   button or status area to tap. */
	if ((in->actions & (PLAT_ACT_FIRE | PLAT_ACT_ZAP)) == (PLAT_ACT_FIRE | PLAT_ACT_ZAP) &&
	    (edge & (PLAT_ACT_FIRE | PLAT_ACT_ZAP)))
		pressed |= P_BACK;
	if (in->spin > 0)
		pressed |= P_NEXT;
	else if (in->spin < 0)
		pressed |= P_PREV;
	tap = 0;
	if (in->pen_down && in->pen_seen) {
		last_x = in->pen_x;
		last_y = in->pen_y;
	}
	if (in->tapped) {
		tap = 1;
		tap_x = in->tap_x;
		tap_y = in->tap_y;
	} else if (prev_pen && !in->pen_down) {
		tap = 1;
		tap_x = last_x;
		tap_y = last_y;
	}
	prev_pen = in->pen_down;
}

/* Which of n rows starting at y0 (the first baseline) a point is in, or -1. */
SEG_FRONT static int16 row_at(int16 y, int16 y0, int16 n)
{
	int16 r = (int16)((y - (y0 - lh)) / lh);

	if (y < y0 - lh || r < 0 || r >= n)
		return -1;
	return r;
}

SEG_FRONT static void draw_logo(int16 top, int16 height)
{
	int16 i, minx = 32767, maxx = -32768, miny = 32767, maxy = -32768;
	int32 w, h, scale;
	int16 left, cw = VA.frame.right;

	if (!logo || logo_n <= 0)
		return;
	for (i = 0; i < logo_n; i++) {
		const uint8 *p = logo + 2 + i * 8;
		int16 k;

		for (k = 0; k < 2; k++) {
			int16 x = (int16)(gets16(p) + (k ? gets16(p + 4) : 0));
			int16 y = (int16)(gets16(p + 2) + (k ? gets16(p + 6) : 0));

			if (x < minx) minx = x;
			if (x > maxx) maxx = x;
			if (y < miny) miny = y;
			if (y > maxy) maxy = y;
		}
	}
	w = maxx - minx + 1;
	h = maxy - miny + 1;
	scale = (int32)(cw - cw / 8) * 256 / w;
	if (h * scale > (int32)height * 256)
		scale = (int32)height * 256 / h;
	left = (int16)((cw - (int16)(w * scale / 256)) / 2);
	/* titlemain: a sweep goes left to right dsweep pixels a step; each line
	   it passes the middle of sparks for 7 steps, then stays. It starts again
	   every TITLE_REPEAT steps. */
	{
		int16 dsweep = (int16)(1 + ((cw - cw / 8) >> 6));
		int16 t = (int16)(frame % TITLE_REPEAT);

		for (i = 0; i < logo_n; i++) {
			const uint8 *p = logo + 2 + i * 8;
			int16 x = gets16(p), y = gets16(p + 2);
			int16 x1 = (int16)(left + (x - minx) * scale / 256);
			int16 y1 = (int16)(top + (y - miny) * scale / 256);
			int16 x2 = (int16)(left + (x + gets16(p + 4) - minx) * scale / 256);
			int16 y2 = (int16)(top + (y + gets16(p + 6) - miny) * scale / 256);
			int16 lt = (int16)(t - ((x1 + x2) / 2 + dsweep) / dsweep);

			if (lt < 0)
				continue;
			if (lt < 7) {
				VA.color = 1;
				fractal_line(x1, y1, x2, y2, (int16)(1 << (8 - ((6 - lt) >> 1))), 3);
			} else {
				VA.color = 0;
				VALine(x1, y1, x2, y2);
			}
		}
	}
}

/* The title's own random numbers, so the demos' FastRandom stream is left
   as it was. */
SEG_FRONT static int16 title_random(void)
{
	title_seed = title_seed * 1103515245UL + 12345UL;
	return (int16)(title_seed >> 16);
}

/* VAFractalLine from the VA Kit: each half of the line is pushed sideways
   by an amount from the jitter table, level times over. */
SEG_FRONT static void sizzle(int16 x1, int16 y1, int16 x2, int16 y2)
{
	int16 dx = (int16)(x2 - x1), dy = (int16)(y2 - y1), mx, my;
	int32 sz = jitter[(uint8)(jitter_point >> 8)];

	jitter_point = (int16)(jitter_point + jitter_step);
	mx = (int16)(x1 + (dx >> 1) + (int32)dy * sz / sizzle_factor);
	my = (int16)(y1 + (dy >> 1) - (int32)dx * sz / sizzle_factor);
	if (--sizzle_level > 0) {
		sizzle(x1, y1, mx, my);
		sizzle(mx, my, x2, y2);
	} else {
		VASafeLineTo(mx, my);
		VASafeLineTo(x2, y2);
	}
	sizzle_level++;
}

SEG_FRONT static void fractal_line(int16 x1, int16 y1, int16 x2, int16 y2, int16 factor, int16 level)
{
	if (!jitter_ready) {
		int16 i;

		/* The original's table is of chars, so the bigger ones wrap. */
		for (i = 0; i < 256; i++)
			jitter[i] = (int8)((((title_random() >> 4) & 15) *
			                    ((int32)Sins[(uint16)title_random() % ANGLES] * 32)) / 256);
		jitter_ready = 1;
	}
	sizzle_factor = factor;
	jitter_step = title_random();
	sizzle_level = level;
	VAMoveTo(x1, y1);
	sizzle(x1, y1, x2, y2);
}

/* DrawZoom from RectZoom.c: a box goes from one rectangle to another in
   ZOOMSTEPS steps with its corners turned a quarter round (the first of
   the original's RectMappings), sizzling at the start and
   the end. t counts the steps from 0. */
SEG_FRONT static void draw_zoom(int16 t, int16 x0, int16 y0, int16 x1, int16 y1)
{
	int16 fx[4], fy[4], tx[4], ty[4], mx[4], my[4], i, phaze;

	if (t < 0)
		return;
	phaze = (int16)(ZOOMSTEPS - t);
	if (phaze < 0)
		phaze = 0;
	/* From the play area inset by a twelfth, to x0,y0 x1,y1. */
	fx[0] = fx[1] = (int16)(VA.frame.right / 12);
	fx[2] = fx[3] = (int16)(VA.frame.right - VA.frame.right / 12);
	fy[0] = fy[3] = (int16)(VA.frame.bottom / 12);
	fy[1] = fy[2] = (int16)(VA.frame.bottom - VA.frame.bottom / 12);
	tx[0] = tx[1] = x0;
	tx[2] = tx[3] = x1;
	ty[0] = ty[3] = y0;
	ty[1] = ty[2] = y1;
	for (i = 0; i < 4; i++) {
		mx[i] = (int16)(tx[i] + (int32)phaze * (fx[(i + 1) & 3] - tx[i]) / ZOOMSTEPS);
		my[i] = (int16)(ty[i] + (int32)phaze * (fy[(i + 1) & 3] - ty[i]) / ZOOMSTEPS);
	}
	VA.color = 2;
	if (phaze > JITTERTIME && phaze < ZOOMSTEPS - JITTERTIME) {
		VAMoveTo(mx[3], my[3]);
		for (i = 0; i < 4; i++)
			VASafeLineTo(mx[i], my[i]);
	} else {
		int16 sf = (int16)((phaze >= ZOOMSTEPS - JITTERTIME ? ZOOMSTEPS - phaze : phaze) + 7);

		for (i = 0; i < 4; i++)
			fractal_line(mx[i], my[i], mx[(i + 1) & 3], my[(i + 1) & 3], (int16)(1 << sf), 5);
	}
}

SEG_FRONT static void draw_title(void)
{
	int16 cx = (int16)(VA.frame.right / 2), b = VA.frame.bottom;

	draw_logo((int16)(b / 8), (int16)(b / 2));
	/* Once the logo is in, a box zooms in round TAP TO PLAY. */
	{
		int16 tw = (int16)(11 * (ts * 3 + 3) / 2 + ts * 2 + 2);

		draw_zoom((int16)(frame % TITLE_REPEAT - TITLE_ZOOM), (int16)(cx - tw),
		          (int16)(b - lh * 3), (int16)(cx + tw), (int16)(b - lh * 2 + lh / 2));
	}
	text("TAP TO PLAY", cx, (int16)(b - lh * 2), (int16)((frame / 6) & 1 ? 3 : 2));
	text("ARASHI 1.1.1", cx, (int16)(b - lh / 2), 5);
}

static const char *const menu_names[MENU_ITEMS] = {
	"PLAY", "SELECT LEVEL", "HIGH SCORES", "OPTIONS", "ABOUT"
};

SEG_FRONT static int16 menu_y0(void)
{
	return (int16)(VA.frame.bottom / 3 + lh);
}

SEG_FRONT static void draw_menu(void)
{
	int16 i, cx = (int16)(VA.frame.right / 2), y = menu_y0();

	text("ARASHI", cx, (int16)(lh + lh / 2), 1);
	for (i = 0; i < MENU_ITEMS; i++)
		text(menu_names[i], cx, (int16)(y + i * lh), (int16)(i == sel ? 3 : 2));
}

SEG_FRONT static void load_choices(void)
{
	nchoices = game_level_choices(Saved.practice, Saved.highest, choices, MAX_CHOICES);
	if (nchoices < 1)
		nchoices = 0;
	if (start_choice >= nchoices)
		start_choice = 0;
}

/* DrawLevelPresentation: the level's shape, fitted to a box wide. */
SEG_FRONT static void draw_shape(int16 field, int16 cx, int16 cy, int16 box)
{
	uint32 bytes = 0;
	const uint8 *d = plat_resource(FOURCC('S', 'P', 'C', 'E'), field, &bytes);
	int16 n, i, a, ang[MAXSEGS];
	int32 x, y, xmin, xmax, ymin, ymax, ext;

	if (!d)
		return;
	n = bytes >= 8 ? gets16(d) : 0;
	if (n > MAXSEGS)
		n = MAXSEGS;
	if ((uint32)(8 + 2 * n) > bytes)
		n = (int16)((bytes - 8) / 2);
	for (i = 0; i < n; i++)
		ang[i] = gets16(d + 8 + 2 * i);
	plat_release(d);
	if (n < 2)
		return;
	x = y = xmin = xmax = ymin = ymax = 0;
	a = 0;
	for (i = 0; i < n; i++) {
		a = (int16)((a + ang[i]) % ANGLES);
		if (a < 0)
			a = (int16)(a + ANGLES);
		x += Cosins[a];
		y -= Sins[a];
		if (x > xmax) xmax = x;
		if (y > ymax) ymax = y;
		if (x < xmin) xmin = x;
		if (y < ymin) ymin = y;
	}
	/* The original's margin of half a unit (512 with sins of 1024). */
	xmax += 128; ymax += 128;
	xmin -= 128; ymin -= 128;
	x = -(xmin + xmax) / 2;
	y = -(ymin + ymax) / 2;
	ext = xmax - xmin;
	if (ymax - ymin > ext)
		ext = ymax - ymin;
	VAMoveTo((int16)(x * box / ext + cx), (int16)(y * box / ext + cy));
	a = 0;
	for (i = 0; i < n; i++) {
		a = (int16)((a + ang[i]) % ANGLES);
		if (a < 0)
			a = (int16)(a + ANGLES);
		x += Cosins[a];
		y -= Sins[a];
		VASafeLineTo((int16)(x * box / ext + cx), (int16)(y * box / ext + cy));
	}
}

/* Colours 1 to 6 are the levels round the one picked, each in its own
   level's colour 8 (the grey for invisible levels), as DoLevelSelect sets
   them. Level k uses colour k % 6 + 1. */
SEG_FRONT static void level_colours(void)
{
	uint8 buf[NUMCOLORS * 8];
	uint32 n = 0, i;
	const uint8 *c = plat_resource(FOURCC('C', 'L', 'O', 'T'), 1000, &n);
	int16 k;

	if (!c)
		return;
	if (n > sizeof(buf))
		n = sizeof(buf);
	for (i = 0; i < n; i++)
		buf[i] = c[i];
	plat_release(c);
	for (k = (int16)(start_choice - 2); k <= start_choice + 3; k++) {
		uint32 m = 0;
		int16 id, j, slot = (int16)((k + 6) % 6 + 1);
		const uint8 *lc;

		if (k < 0 || k >= nchoices || slot * 8 + 8 > (int16)n)
			continue;
		id = choices[k].color;
		if (id == 5)
			id = (int16)(id + 128);
		lc = plat_resource(FOURCC('C', 'L', 'O', 'T'), id, &m);
		if (!lc)
			continue;
		if (m >= 72)
			for (j = 2; j < 8; j++)
				buf[slot * 8 + j] = lc[64 + j];
		plat_release(lc);
	}
	VASetColors(buf, n);
	sel_coloured = start_choice;
}

SEG_FRONT static void draw_levels(void)
{
	char buf[16];
	int16 cx = (int16)(VA.frame.right / 2), b = VA.frame.bottom, k;
	int16 box = (int16)(VA.frame.right / 5), cy = (int16)(b * 5 / 8), x, y;

	if (box < 8)
		box = 8;
	text("SELECT LEVEL", cx, (int16)(lh + lh / 2), BG1);
	if (nchoices == 0)
		return;
	if (sel_coloured != start_choice)
		level_colours();
	for (k = (int16)(start_choice - 2); k <= start_choice + 3; k++) {
		if (k < 0 || k >= nchoices)
			continue;
		VA.color = (int16)(k % 6 + 1);
		draw_shape(choices[k].field, (int16)(cx + (k - start_choice) * box - sel_off), cy, box);
	}
	/* The box round the level picked. */
	x = (int16)(cx - box / 2);
	y = (int16)(cy + box / 2);
	VA.color = 0;
	VAMoveTo(x, y);
	VASafeLineTo((int16)(x + box), y);
	VASafeLineTo((int16)(x + box), (int16)(y - box));
	VASafeLineTo(x, (int16)(y - box));
	VASafeLineTo(x, y);
	fmt_num(buf, choices[start_choice].level);
	text(buf, cx, (int16)(y + lh), BG1);
	if (choices[start_choice].startBonus > 0) {
		fmt_num(buf, choices[start_choice].startBonus);
		text("STARTING BONUS", cx, (int16)(y - box - lh * 2), BG2);
		text(buf, cx, (int16)(y - box - lh / 2), BG1);
	}
}

/* Drag to turn the carousel, or < >; a tap on the middle or fire plays
   the level in the box, a tap to the side moves one. */
SEG_FRONT static void levels_step(const PlatInput *in)
{
	int16 box = (int16)(VA.frame.right / 5), step;

	if (box < 8)
		box = 8;
	if (in->pen_down && in->pen_seen) {
		if (!sel_drag) {
			sel_drag = 1;
			sel_px = in->pen_x;
			sel_moved = 0;
		} else {
			int16 d = (int16)(in->pen_x - sel_px);

			sel_px = in->pen_x;
			sel_moved = (int16)(sel_moved + (d < 0 ? -d : d));
			sel_off = (int16)(sel_off - d);
		}
	} else if (!in->pen_down) {
		sel_drag = 0;
	}
	if (pressed & P_PREV) {
		if (start_choice > 0) {
			start_choice--;
			sel_off = (int16)(sel_off + box);
		}
	} else if (pressed & P_NEXT) {
		if (start_choice + 1 < nchoices) {
			start_choice++;
			sel_off = (int16)(sel_off - box);
		}
	} else if (tap && sel_moved < 4 && tap_y > VA.frame.bottom / 4) {
		if (tap_x < VA.frame.right / 2 - box / 2) {
			if (start_choice > 0) {
				start_choice--;
				sel_off = (int16)(sel_off + box);
			}
		} else if (tap_x > VA.frame.right / 2 + box / 2) {
			if (start_choice + 1 < nchoices) {
				start_choice++;
				sel_off = (int16)(sel_off - box);
			}
		} else if (nchoices > 0) {
			SOUND(SND_PHAZERIN, 0);
			start_game(choices[start_choice].level);
			return;
		}
	} else if (nchoices > 0 && (pressed & P_OK)) {
		SOUND(SND_PHAZERIN, 0);
		start_game(choices[start_choice].level);
		return;
	}
	if (tap)
		sel_moved = 0;
	/* A drag picks the level nearest the box as it goes. */
	while (sel_drag && sel_off > box / 2 && start_choice + 1 < nchoices) {
		sel_off = (int16)(sel_off - box);
		start_choice++;
	}
	while (sel_drag && sel_off < -box / 2 && start_choice > 0) {
		sel_off = (int16)(sel_off + box);
		start_choice--;
	}
	if (!sel_drag) {
		step = (int16)(box / 4 > 1 ? box / 4 : 1);
		if (sel_off > step)
			sel_off = (int16)(sel_off - step);
		else if (sel_off < -step)
			sel_off = (int16)(sel_off + step);
		else
			sel_off = 0;
	}
	if (pressed & P_BACK) {
		set_colors();
		set_screen(FRONT_MENU);
	}
}

SEG_FRONT static int16 rows_per_page(void)
{
	int16 n = (int16)((VA.frame.bottom - lh * 3) / lh);

	return n < 1 ? 1 : n;
}

/* The attract loop's credit pages (Text in Flashmain.c): TEXT 128 to 131 in
   a box that zooms in, one line a row, drawn twice a pixel apart. */
SEG_FRONT static void draw_text_page(void)
{
	static const uint8 *t;
	static uint32 n;
	int16 inset = (int16)(VA.frame.right / 12), x0 = inset, y0 = (int16)(VA.frame.bottom / 12);
	int16 x1 = (int16)(VA.frame.right - inset), y1 = (int16)(VA.frame.bottom - VA.frame.bottom / 12);
	int16 save_ts = ts, save_lh = lh, s, y;
	uint32 i, start = 0;

	if (frame == 0) {
		if (t)
			plat_release(t);
		t = plat_resource(FOURCC('T', 'E', 'X', 'T'), (int16)(128 + text_page), &n);
	}
	/* 19 columns is the widest line. */
	s = (int16)(((x1 - x0 - 8) / 19 - 3) / 3);
	if (s > ts)
		s = ts;
	if (s < 1)
		s = 1;
	ts = s;
	lh = (int16)(s * 6 + 8);
	y = (int16)(y0 + lh);
	for (i = 0; t && i < n; i++) {
		if (t[i] == 13) {
			if (i > start) {
				char buf[40];
				int16 len = (int16)(i - start > 39 ? 39 : i - start);

				int16 k;

				for (k = 0; k < len; k++)
					buf[k] = (char)t[start + k];
				buf[len] = 0;
				text_left(buf, (int16)(x0 + s * 3), y, 2);
				if (s >= 2)
					text_left(buf, (int16)(x0 + s * 3 + 1), y, 2);
			}
			y = (int16)(y + lh);
			start = i + 1;
		}
	}
	ts = save_ts;
	lh = save_lh;
	draw_zoom((int16)frame, x0, y0, x1, y1);
}

/* Firework in Highmain.c: an explosion of size 2 at a random spot. */
SEG_FRONT static void firework(void)
{
	VAExplosion((int16)((uint16)title_random() % (uint16)VA.frame.right),
	            (int16)((uint16)title_random() % (uint16)VA.frame.bottom), 2,
	            (int16)((uint16)title_random() % 7));
}

SEG_FRONT static void draw_scores(cbool full)
{
	char buf[16];
	int16 t = Saved.practice, n = rows_per_page(), i, y, cx = (int16)(VA.frame.right / 2);
	int16 first;

	if (n > STORE_ENTRIES)
		n = STORE_ENTRIES;
	first = (int16)(page * n);
	text(t ? "PRACTICE" : "HIGH SCORES", cx, (int16)(lh + lh / 2), 1);
	y = (int16)(lh * 3);
	for (i = 0; i < n && first + i < STORE_ENTRIES; i++) {
		const ScoreEntry *e = &Saved.table[t][first + i];
		int16 c;

		fmt_num(buf, first + i + 1);
		c = (int16)((first + i == hl_rank && t == hl_table) ? (frame / 3) % 6 + 1 : 2);
		text_left(buf, 2, y, 5);
		{
			char nm[STORE_NAME + 1];
			int16 j;

			for (j = 0; j < STORE_NAME; j++)
				nm[j] = e->name[j];
			nm[STORE_NAME] = 0;
			text_left(nm, (int16)(2 + 3 * (ts * 3 + 3)), y, c);
		}
		fmt_num(buf, e->score);
		text_left(buf, (int16)(VA.frame.right - 2 - slen(buf) * (ts * 3 + 3)), y, c);
		y = (int16)(y + lh);
	}
	if (full)
		text("< >   BACK", cx, (int16)(VA.frame.bottom - lh / 2), 3);
}

SEG_FRONT static void draw_options(void)
{
	int16 cx = (int16)(VA.frame.right / 2), y = (int16)(VA.frame.bottom / 3 + lh);

	text("OPTIONS", cx, (int16)(lh + lh / 2), 1);
	text(Saved.control ? "CONTROL DRAG" : "CONTROL TOUCH", cx, y, (int16)(sel == 0 ? 3 : 2));
	text(Saved.practice ? "MODE PRACTICE" : "MODE ARCADE", cx, (int16)(y + lh), (int16)(sel == 1 ? 3 : 2));
	text(Saved.sound ? "SOUND ON" : "SOUND OFF", cx, (int16)(y + 2 * lh), (int16)(sel == 2 ? 3 : 2));
	text("BACK", cx, (int16)(y + 3 * lh), (int16)(sel == 3 ? 3 : 2));
}

SEG_FRONT static void draw_about(void)
{
	int16 cx = (int16)(VA.frame.right / 2), y = (int16)(lh * 3);

	text("ARASHI", cx, (int16)(lh + lh / 2), 1);
	text("BY JURI MUNKKI", cx, y, 2);
	text("PALM PORT", cx, (int16)(y + lh), 2);
	text("1.1.1 ORIGINAL", cx, (int16)(y + lh * 2), 5);
	text("TAP TO GO BACK", cx, (int16)(VA.frame.bottom - lh / 2), 3);
}

SEG_FRONT static void draw_pause(void)
{
	int16 cx = (int16)(VA.frame.right / 2), y = (int16)(VA.frame.bottom / 3 + lh);

	text("PAUSED", cx, (int16)(lh + lh / 2), 1);
	text("RESUME", cx, y, (int16)(sel == 0 ? 3 : 2));
	text(Saved.control ? "CONTROL DRAG" : "CONTROL TOUCH", cx, (int16)(y + lh), (int16)(sel == 1 ? 3 : 2));
	text("QUIT", cx, (int16)(y + 2 * lh), (int16)(sel == 2 ? 3 : 2));
}

SEG_FRONT static void draw_over(void)
{
	char buf[16];
	int16 cx = (int16)(VA.frame.right / 2), b = VA.frame.bottom;

	text("GAME OVER", cx, (int16)(b / 3), (int16)(1 + (frame / 3) % 6));
	text("SCORE", cx, (int16)(b / 2), 2);
	fmt_num(buf, Hero.score);
	text(buf, cx, (int16)(b / 2 + lh), 3);
}

SEG_FRONT static void name_begin(void)
{
	hl_table = Saved.practice;
	name_pos = 0;
	/* 'A' picked to start with. */
	wheel_big = (int16)((89 + 4) * SUBROTATION);
	wheel_hist = 0;
	wheel_dir = SUBROTATION;
	wheel_drag = 0;
}

/* One step of the wheel: delta in fifths of an angle, at most a letter a
   step, with the rest kept for the steps after. With nothing to do it turns
   on the way it last went until a letter is in place. */
SEG_FRONT static void wheel_turn(int16 delta)
{
	int16 rot;

	delta = (int16)(delta + wheel_hist);
	wheel_hist = delta;
	if (delta > MAXROTLETTER)
		delta = MAXROTLETTER;
	if (delta < -MAXROTLETTER)
		delta = -MAXROTLETTER;
	wheel_hist = (int16)(wheel_hist - delta);
	wheel_hist = (int16)(wheel_hist * 3 / 4);
	rot = (int16)(wheel_big / SUBROTATION);
	if (delta) {
		wheel_dir = (int16)(delta > 0 ? SUBROTATION : -SUBROTATION);
		wheel_big = (int16)(wheel_big + delta);
	} else if ((rot + ANGLES) % 4 != 1) {
		wheel_big = (int16)(wheel_big + wheel_dir);
	}
	wheel_big = (int16)(wheel_big % (SUBROTATION * ANGLES));
	if (wheel_big < 0)
		wheel_big = (int16)(wheel_big + SUBROTATION * ANGLES);
}

/* The letter at angles 88 to 91, or 0 while none is there. */
SEG_FRONT static char wheel_char(void)
{
	int16 rot = (int16)(wheel_big / SUBROTATION), c;

	for (c = 0; c < WHEEL_CHARS; c++) {
		int16 a = (int16)((rot + ANGLES * 2 + 4 * c - 8) % ANGLES);

		if (a > 87 && a < 92)
			return (char)('@' + c);
	}
	return 0;
}

SEG_FRONT static void draw_name(void)
{
	char buf[16];
	int16 cx = (int16)(VA.frame.right / 2), w = VA.frame.right, i, c;
	int16 rot = (int16)(wheel_big / SUBROTATION), s2 = (int16)(ts * 2), top, ys0;
	char shown[NAME_LEN + 1];

	text("NEW HIGH SCORE", cx, (int16)(lh + lh / 2), 1);
	fmt_num(buf, Hero.score);
	text(buf, cx, (int16)(lh * 3), 3);
	for (i = 0; i < NAME_LEN; i++)
		shown[i] = i < name_pos ? name[i] : '.';
	shown[NAME_LEN] = 0;
	text(shown, cx, (int16)(lh * 9 / 2), 3);
	/* The drum, seen from inside: x = cos, y = sin + 500 with the
	   original's sins of 8192. */
	{
		int16 hh = (int16)((s2 + 1) * 4 + 4);

		/* The picked letter's baseline goes at ys0. */
		ys0 = (int16)(VA.frame.bottom * 3 / 4 + hh / 2);
		top = (int16)(ys0 - 150L * w / (Sins[90] * 16L + 500));
	}
	for (c = 0; c < WHEEL_CHARS; c++) {
		int16 a = (int16)((rot + ANGLES * 2 + 4 * c - 8) % ANGLES), xs, ys, s;
		int32 x = (int32)Cosins[a] * 16, y = (int32)Sins[a] * 16 + 500;
		char ch = (char)('@' + c);

		if (y > -500)
			continue;
		s = (int16)(s2 + 1);
		xs = (int16)(cx + x * w / y);
		ys = (int16)(top + 150L * w / y);
		if (xs < 0 || xs > w)
			continue;
		VA.segmscale = s;
		VA.color = (int16)(a > 87 && a < 92 ? 1 : BG2);
		VAMoveTo((int16)(xs - (s * 3 + 3) / 2), ys);
		VADrawText(&ch, 0, 1);
	}
	/* The frame round the letter picked. */
	{
		int16 bw = (int16)((s2 + 1) * 3 + 3 + 8), bh = (int16)((s2 + 1) * 4 + 4 + 8);
		int16 bx = (int16)(cx - bw / 2), by = (int16)(ys0 - bh + 4);

		VA.color = 2;
		VAMoveTo(bx, (int16)(by + 2));
		VASafeLineTo(bx, by);
		VASafeLineTo((int16)(bx + bw), by);
		VASafeLineTo((int16)(bx + bw), (int16)(by + 2));
		VAMoveTo(bx, (int16)(by + bh - 2));
		VASafeLineTo(bx, (int16)(by + bh));
		VASafeLineTo((int16)(bx + bw), (int16)(by + bh));
		VASafeLineTo((int16)(bx + bw), (int16)(by + bh - 2));
	}
}

/* Name entry: drag or < > turn the wheel, a tap or fire takes the letter,
   back takes the last one off. Five letters, or a tap on the name, end it. */
SEG_FRONT static void name_step(const PlatInput *in)
{
	int16 w = VA.frame.right, delta = 0;
	char ch;
	cbool take = 0, done = 0;

	if (in->pen_down && in->pen_seen) {
		if (!wheel_drag) {
			wheel_drag = 1;
			wheel_px = in->pen_x;
			wheel_acc = 0;
			wheel_moved = 0;
		} else {
			int16 d = (int16)(in->pen_x - wheel_px);

			wheel_px = in->pen_x;
			wheel_moved = (int16)(wheel_moved + (d < 0 ? -d : d));
			/* A letter (20) for each quarter of the width. */
			wheel_acc = (int16)(wheel_acc + d * 80);
			delta = (int16)(wheel_acc / w);
			wheel_acc = (int16)(wheel_acc - delta * w);
		}
	} else if (!in->pen_down) {
		wheel_drag = 0;
	}
	if (pressed & P_PREV)
		delta = (int16)(delta + 4 * SUBROTATION);
	if (pressed & P_NEXT)
		delta = (int16)(delta - 4 * SUBROTATION);
	wheel_turn(delta);
	if (pressed & P_OK)
		take = 1;
	if (tap && wheel_moved < 4) {
		if (tap_y < lh * 5 && name_pos > 0)
			done = 1;
		else
			take = 1;
	}
	if (tap)
		wheel_moved = 0;
	ch = wheel_char();
	if (take && ch && name_pos < NAME_LEN)
		name[name_pos++] = ch;
	if ((pressed & P_BACK) && name_pos > 0)
		name_pos--;
	if (name_pos >= NAME_LEN || done) {
		name[name_pos] = 0;
		hl_rank = store_insert(hl_table, Hero.score, name);
		page = (int16)(hl_rank >= 0 ? hl_rank / rows_per_page() : 0);
		store_save();
		set_screen(FRONT_SCORES);
	}
}

SEG_FRONT static void start_game(int16 level)
{
	set_colors();
	game_seed();
	PlayControl = Saved.control;
	BeatTable = Saved.practice;
	/* A cut-short demo or a game leaves state the next demo doesn't expect,
	   so the attract loop starts again from the first recording. */
	attract_n = DEMO_FIRST;
	if (!game_start(level, 0)) {
		set_screen(FRONT_MENU);
		return;
	}
	spin_acc = 0;
	set_screen(FRONT_PLAY);
}

SEG_FRONT static void begin_demo(void)
{
	set_colors();
	PlayControl = 0;
	BeatTable = 0;
	if (!game_demo_start(attract_n)) {
		text_page = 0;
		set_screen(FRONT_TEXT);
		return;
	}
	attract_n = (int16)((attract_n + 1) % NUM_DEMOS);
	spin_acc = 0;
	set_screen(FRONT_DEMO);
}

/* The game has ended: the score goes in the table when it is high enough. */
SEG_FRONT static void end_game(void)
{
	store_reached(game_level());
	set_colors();
	set_screen(FRONT_OVER);
}

SEG_FRONT static void play_step(const PlatInput *in)
{
	PlatInput g = *in;

	g.spin = (int16)(g.spin + spin_acc);
	if (game_frame_in(&g))
		spin_acc = 0;
	else
		spin_acc = g.spin;
	if (game_over_now()) {
		if (screen == FRONT_DEMO) {
			game_demo_stop();
			set_colors();
			text_page = 0;
			set_screen(FRONT_TEXT);
		} else {
			end_game();
		}
	}
}

SEG_FRONT static void ui_step(const PlatInput *in)
{
	int16 r;

	(void)in;
	switch (screen) {
	case FRONT_TITLE:
		draw_title();
		if (tap || (pressed & (P_OK | P_BACK))) {
			SOUND(SND_BLAST, 100);
			set_screen(FRONT_MENU);
		} else if (frame >= TITLE_STEPS)
			begin_demo();
		break;
	case FRONT_TEXT:
		draw_text_page();
		if (tap || (pressed & (P_OK | P_BACK)))
			set_screen(FRONT_MENU);
		else if (frame >= TEXT_STEPS) {
			if (++text_page >= TEXT_PAGES)
				set_screen(FRONT_SCORES_PAGE);
			else
				set_screen(FRONT_TEXT);
		}
		break;
	case FRONT_SCORES_PAGE:
		draw_scores(0);
		if ((uint16)title_random() % 9 == 0)
			firework();
		if (tap || (pressed & (P_OK | P_BACK)))
			set_screen(FRONT_MENU);
		else if (frame >= SCORES_STEPS) {
			set_screen(FRONT_TITLE);
		}
		break;
	case FRONT_MENU:
		draw_menu();
		if (pressed & P_PREV)
			sel = (int16)((sel + MENU_ITEMS - 1) % MENU_ITEMS);
		if (pressed & P_NEXT)
			sel = (int16)((sel + 1) % MENU_ITEMS);
		r = -1;
		if (tap) {
			r = row_at(tap_y, menu_y0(), MENU_ITEMS);
			if (r >= 0)
				sel = r;
		}
		if (r >= 0 || (pressed & P_OK)) {
			switch (sel) {
			case 0:
				start_game(1);
				break;
			case 1:
				choices_ready = 0;
				sel_coloured = -1;
				sel_off = 0;
				set_screen(FRONT_LEVELS);
				break;
			case 2:
				page = 0;
				hl_rank = -1;
				set_screen(FRONT_SCORES);
				break;
			case 3:
				set_screen(FRONT_OPTIONS);
				break;
			default:
				set_screen(FRONT_ABOUT);
				break;
			}
		} else if (pressed & P_BACK) {
			set_screen(FRONT_TITLE);
		}
		break;
	case FRONT_LEVELS:
		/* The list comes from LSEL 128, so it is quick to make. */
		if (!choices_ready) {
			load_choices();
			choices_ready = 1;
			ui_next = plat_ticks();
		}
		levels_step(in);
		if (screen == FRONT_LEVELS)
			draw_levels();
		break;
	case FRONT_SCORES:
		draw_scores(1);
		if ((pressed & P_NEXT) || (tap && tap_x > VA.frame.right * 2 / 3)) {
			if ((page + 1) * rows_per_page() < STORE_ENTRIES)
				page++;
		} else if ((pressed & P_PREV) || (tap && tap_x < VA.frame.right / 3)) {
			if (page > 0)
				page--;
		} else if (tap || (pressed & (P_OK | P_BACK))) {
			hl_rank = -1;
			set_screen(FRONT_MENU);
		}
		break;
	case FRONT_OPTIONS:
		draw_options();
		if (pressed & P_PREV)
			sel = (int16)((sel + 3) % 4);
		if (pressed & P_NEXT)
			sel = (int16)((sel + 1) % 4);
		r = -1;
		if (tap) {
			r = row_at(tap_y, (int16)(VA.frame.bottom / 3 + lh), 4);
			if (r >= 0)
				sel = r;
		}
		if (r >= 0 || (pressed & P_OK)) {
			if (sel == 0)
				Saved.control = (uint8)!Saved.control;
			else if (sel == 1)
				Saved.practice = (uint8)!Saved.practice;
			else if (sel == 2) {
				Saved.sound = (uint8)!Saved.sound;
				SoundOn = Saved.sound;
				SOUND(SND_PHAZERIN, 0);
			} else {
				store_save();
				set_screen(FRONT_MENU);
			}
		} else if (pressed & P_BACK) {
			store_save();
			set_screen(FRONT_MENU);
		}
		break;
	case FRONT_ABOUT:
		draw_about();
		if (tap || (pressed & (P_OK | P_BACK)))
			set_screen(FRONT_MENU);
		break;
	case FRONT_PAUSE:
		draw_pause();
		if (pressed & P_PREV)
			sel = (int16)((sel + 2) % 3);
		if (pressed & P_NEXT)
			sel = (int16)((sel + 1) % 3);
		r = -1;
		if (tap) {
			r = row_at(tap_y, (int16)(VA.frame.bottom / 3 + lh), 3);
			if (r >= 0)
				sel = r;
		}
		if (r >= 0 || (pressed & P_OK)) {
			if (sel == 1) {
				Saved.control = (uint8)!Saved.control;
				PlayControl = Saved.control;
			} else if (sel == 0) {
				game_resume();
				set_colors();
				VALoadColors(ThisLevel.lvColor);
				set_screen(FRONT_PLAY);
			} else {
				store_save();
				set_colors();
				set_screen(FRONT_MENU);
			}
		} else if (pressed & P_BACK) {
			game_resume();
			VALoadColors(ThisLevel.lvColor);
			set_screen(FRONT_PLAY);
		}
		break;
	case FRONT_OVER:
		draw_over();
		if (frame >= OVER_STEPS || tap || (pressed & (P_OK | P_BACK))) {
			hl_table = Saved.practice;
			if (store_rank(hl_table, Hero.score) >= 0) {
				name_begin();
				set_screen(FRONT_NAME);
			} else {
				hl_rank = -1;
				page = 0;
				set_screen(FRONT_SCORES);
			}
		}
		break;
	case FRONT_NAME:
		name_step(in);
		if (screen == FRONT_NAME)
			draw_name();
		break;
	default:
		break;
	}
}

SEG_FRONT cbool front_init(void)
{
	logo = plat_resource(FOURCC('L', 'O', 'G', 'O'), 131, &logo_size);
	logo_n = (logo && logo_size >= 2) ? gets16(logo) : 0;
	if (logo_n < 0 || logo_size < (uint32)(2 + logo_n * 8))
		logo_n = 0;
	store_load();
	SoundOn = Saved.sound;
	metrics();
	prev_act = 0;
	prev_pen = 0;
	attract_n = DEMO_FIRST;
	game_seed();
	if (!game_init(1))
		return 0;
	set_colors();
	set_screen(FRONT_TITLE);
	ui_next = plat_ticks();
	return 1;
}

SEG_FRONT void front_close(void)
{
	game_demo_stop();
	store_save();
	game_close();
	if (logo)
		plat_release(logo);
	logo = 0;
}

SEG_FRONT int16 front_screen(void)
{
	return screen;
}

SEG_FRONT void front_goto(int16 s, int16 arg)
{
	if (s == FRONT_DEMO) {
		attract_n = arg;
		begin_demo();
		return;
	}
	if (s == FRONT_PLAY) {
		start_game(arg);
		return;
	}
	if (s == FRONT_TEXT)
		text_page = (int16)(arg & 3);
	if (s == FRONT_LEVELS) {
		choices_ready = 0;
		sel_coloured = -1;
	}
	if (s == FRONT_NAME)
		name_begin();
	page = 0;
	hl_rank = -1;
	set_screen(s);
}

SEG_FRONT void front_frame(void)
{
	PlatInput in;
	uint32 now = plat_ticks(), tps = plat_ticks_per_second();

	if (!plat_screen())
		return;
	if (screen == FRONT_PLAY || screen == FRONT_DEMO) {
		read_input(&in);
		if (screen == FRONT_DEMO && (tap || pressed)) {
			game_demo_stop();
			attract_n = DEMO_FIRST;
			set_colors();
			set_screen(FRONT_MENU);
			return;
		}
		/* A tap on the status area pauses too; with no status area, a tap
		   anywhere outside the play area does. */
		if (screen == FRONT_PLAY && tap && tap_pauses())
			pressed |= P_BACK;
		if (screen == FRONT_PLAY && (pressed & P_BACK)) {
			set_colors();
			set_screen(FRONT_PAUSE);
			return;
		}
		if (screen == FRONT_PLAY)
			pressed = 0;
		play_step(&in);
		return;
	}
	if (now - ui_next > tps || (int32)(now - ui_next) < -(int32)tps)
		ui_next = now;
	if ((int32)(now - ui_next) < 0)
		return;
	ui_next += tps / UI_HZ;
	read_input(&in);
	ui_step(&in);
	frame++;
	VAStep();
}

SEG_FRONT uint32 front_wait(void)
{
	uint32 now;

	if (screen == FRONT_PLAY || screen == FRONT_DEMO)
		return game_wait();
	now = plat_ticks();
	return (int32)(ui_next - now) > 0 ? ui_next - now : 0;
}

SEG_FRONT void front_pause(void)
{
	if (screen == FRONT_PLAY) {
		set_colors();
		set_screen(FRONT_PAUSE);
	} else if (screen == FRONT_DEMO) {
		game_demo_stop();
		attract_n = DEMO_FIRST;
		set_colors();
		set_screen(FRONT_TITLE);
	}
	store_save();
}

SEG_FRONT void front_resume(void)
{
	ui_next = plat_ticks();
	prev_act = 0;
	prev_pen = 0;
}

SEG_FRONT void front_screen_changed(void)
{
	metrics();
	if (screen == FRONT_PLAY || screen == FRONT_DEMO)
		game_screen_changed();
	else
		set_colors();
}
