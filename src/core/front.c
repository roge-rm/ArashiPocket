/* The front end. One state machine; UI screens step 20 times a second like
   the game. The menus and forms are painted by menus.c when they change
   (ui_dirty) and stay on screen; the moving parts (the title's logo and
   lightning, the carousel, the name wheel, GAME OVER, fireworks) are drawn
   through the renderer over black. */
#include "front.h"
#include "ui.h"
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
static cbool fresh;             /* a screen began since the last input read */
static cbool drop_pen;          /* ignore the pen until it is lifted */
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
/* The lightning between attract pages (Lightning in Flashmain.c): points
   lx, ly, and segments from lstart to lend (indexes into them). */
#define LIGHT_MAX    48
#define LIGHT_FULL   65535L
static int16 lx[LIGHT_MAX], ly[LIGHT_MAX], lstart[LIGHT_MAX], lend[LIGHT_MAX];
static int16 light_segs, light_block, light_speed, light_discharge, light_then;
static cbool light_hit;
static int32 light_intensity;
/* The level select's list, allocated while that screen is up. */
static LevelChoice *choices;
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
static cbool ui_dirty;          /* the screen needs painting */
static int16 shown_choice;      /* the level the level select shows */
static const uint8 *text_res;   /* the credit page shown, TEXT 128 to 131 */
static uint32 text_n;

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
SEG_FRONT static void draw_logo(int16 top, int16 height);
SEG_FRONT static int16 title_random(void);
SEG_FRONT static void sizzle(int16 x1, int16 y1, int16 x2, int16 y2);
SEG_FRONT static void fractal_line(int16 x1, int16 y1, int16 x2, int16 y2, int16 factor, int16 level);
SEG_FRONT static void draw_zoom(int16 t, int16 x0, int16 y0, int16 x1, int16 y1);
SEG_FRONT static void draw_title(void);
SEG_FRONT static void lightning_init(void);
SEG_FRONT static void lightning_begin(int16 then);
SEG_FRONT static void lightning_step(void);
SEG_FRONT static void lightning_flash(void);
SEG_FRONT static void draw_levels(void);
SEG_FRONT static void level_colours(void);
SEG_FRONT static void levels_step(const PlatInput *in);
SEG_FRONT static void draw_text_page(void);
SEG_FRONT static void firework(void);
SEG_FRONT static void draw_over(void);
SEG_FRONT static void draw_name(void);
SEG_FRONT static void ui_step(const PlatInput *in);
SEG_FRONT static void start_game(int16 level);
SEG_FRONT static void load_choices(void);
SEG_FRONT static void play_step(const PlatInput *in);
SEG_FRONT static void end_game(void);
SEG_FRONT static void begin_demo(void);
SEG_FRONT static void set_colors(void);
SEG_FRONT static cbool painted(int16 s);
SEG_FRONT static void paint(const PlatRect *clip);
SEG_FRONT static int16 tap_zone(void);
SEG_FRONT static void view(int16 s);
SEG_FRONT static int16 levels_box(void);
SEG_FRONT static int16 rows_per_page(void);

SEG_FRONT static int16 slen(const char *s)
{
	int16 n = 0;

	while (s[n])
		n++;
	return n;
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
	if (s != FRONT_LEVELS && choices) {
		plat_free(choices);
		choices = NULL;
		nchoices = 0;
		choices_ready = 0;
	}
	screen = s;
	frame = 0;
	sel = 0;
	tap = 0;
	fresh = 1;
	ui_dirty = 1;
}

/* Screens menus.c paints (the rest are all renderer). */
SEG_FRONT static cbool painted(int16 s)
{
	return (cbool)(s != FRONT_PLAY && s != FRONT_DEMO && s != FRONT_LIGHTNING);
}

SEG_FRONT static void paint(const PlatRect *clip)
{
	const char *labels[MENU_ITEMS];

	if (!ui_begin())
		return;
	if (clip)
		ui_clip(clip->x, clip->y, clip->w, clip->h);
	switch (screen) {
	case FRONT_TITLE:
		menus_title();
		break;
	case FRONT_MENU:
		labels[0] = "Play";
		labels[1] = "Select level";
		labels[2] = "High scores";
		labels[3] = "Options";
		labels[4] = "About";
		menus_main(labels, MENU_ITEMS, sel);
		break;
	case FRONT_LEVELS:
		shown_choice = start_choice;
		if (nchoices > 0)
			menus_levels(choices[start_choice].level, choices[start_choice].startBonus, 0);
		else
			menus_levels(0, 0, 1);
		break;
	case FRONT_SCORES:
		menus_scores(Saved.practice, page, Saved.practice == hl_table ? hl_rank : -1, 1);
		break;
	case FRONT_SCORES_PAGE:
		menus_scores(Saved.practice, 0, -1, 0);
		break;
	case FRONT_OPTIONS:
		menus_options(sel, Saved.control, Saved.practice, Saved.sound);
		break;
	case FRONT_ABOUT:
		menus_about();
		break;
	case FRONT_PAUSE:
		menus_pause(sel, Saved.control, game_level(), ThisLevel.lvField, ThisLevel.lvColor, Hero.score);
		break;
	case FRONT_OVER:
		menus_over(Hero.score, game_level());
		break;
	case FRONT_NAME:
		menus_name(Hero.score, name, name_pos, NAME_LEN);
		break;
	case FRONT_TEXT:
		menus_text(text_res, text_n, (cbool)(frame >= ZOOMSTEPS));
		break;
	default:
		break;
	}
	ui_end();
	ui_dirty = 0;
}

/* The zone a tap this step hit, or -1. */
SEG_FRONT static int16 tap_zone(void)
{
	const PlatScreen *sc = plat_screen();

	if (!tap || !sc)
		return -1;
	return ui_zone_at((int16)(tap_x + sc->play.x), (int16)(tap_y + sc->play.y));
}

/* The renderer's view for a screen's moving parts: the attract pages in
   the middle square, the carousel, the name wheel and GAME OVER in their
   areas. */
SEG_FRONT static void view(int16 s)
{
	PlatRect r;

	if (s == FRONT_PLAY || s == FRONT_DEMO || s == FRONT_MENU || s == FRONT_OPTIONS ||
	    s == FRONT_ABOUT || s == FRONT_PAUSE || s == FRONT_SCORES) {
		VAViewPlay();
		return;
	}
	menus_area(s, &r);
	VAViewRect(r.x, r.y, r.w, r.h);
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
	/* A tap or a held pen from the screen before is not for this one. */
	if (fresh) {
		tap = 0;
		drop_pen = in->pen_down;
		fresh = 0;
	} else if (drop_pen) {
		tap = 0;
		if (!in->pen_down)
			drop_pen = 0;
	}
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
}

/* InitializeLightning: a bolt of random segments from the top edge down,
   forking now and then, until one passes the bottom. The original's steps
   were for a 342 line window, so they scale with the frame. */
SEG_FRONT static void lightning_init(void)
{
	int16 segment = 1, current = 0, hitground = 0, forkedlast = 1, forkprob = 15, i;
	int16 b = VA.frame.bottom, k = 342;

	for (i = 0; i < LIGHT_MAX; i++)
		lx[i] = ly[i] = lstart[i] = lend[i] = 0;
	lx[0] = (int16)(VA.frame.right / 2);
	ly[0] = (int16)(-20 * (int32)b / k);
	while (!hitground && segment < LIGHT_MAX - 1) {
		int16 dx, dy;

		lstart[segment] = current;
		lend[segment] = segment;
		if (title_random() % forkprob == 0 && !forkedlast) {
			current--;
			forkedlast = 1;
			forkprob = (int16)(forkprob + 150);
		} else {
			forkedlast = 0;
			forkprob--;
			if (forkprob < 5)
				forkprob = 5;
		}
		if (current < 0)
			current = 0;
		dx = (int16)((2 + (uint16)title_random() % 30) * (int32)b / k);
		dy = (int16)((20 + (uint16)title_random() % 10) * (int32)b / k);
		if (title_random() % 2) {
			lx[lstart[segment]] = (int16)(lx[lstart[current]] + dx);
			ly[lstart[segment]] = (int16)(ly[lstart[current]] + dy);
		} else {
			lx[lstart[segment]] = (int16)(lx[lstart[current]] - dx);
			ly[lstart[segment]] = (int16)(ly[lstart[current]] + dy);
		}
		if (ly[lstart[segment]] > b)
			hitground = 1;
		segment++;
		current++;
	}
	lstart[segment] = 0;
	lend[segment] = 0;
	light_segs = segment;
}

/* then 0: the title follows, 1: a demo. */
SEG_FRONT static void lightning_begin(int16 then)
{
	lightning_init();
	light_then = then;
	light_discharge = 0;
	light_hit = 0;
	light_intensity = 0;
	light_block = 10;
	light_speed = (int16)(light_segs / 8 + 1);
	set_colors();
	set_screen(FRONT_LIGHTNING);
}

/* FlashEffect: the background goes to a pale violet and fades. */
SEG_FRONT static void lightning_flash(void)
{
	uint8 clot[NUMCOLORS * 8];
	uint32 n = 0;
	const uint8 *src = plat_resource(FOURCC('C', 'L', 'O', 'T'), 1000, &n);
	int16 i;

	for (i = 0; i < NUMCOLORS * 8; i++)
		clot[i] = (uint8)(src && (uint32)i < n ? src[i] : 0);
	if (src)
		plat_release(src);
	if (light_intensity > LIGHT_FULL)
		light_intensity = LIGHT_FULL;
	clot[9 * 8 + 2] = (uint8)((light_intensity / 2) >> 8);
	clot[9 * 8 + 4] = (uint8)((light_intensity / 2) >> 8);
	clot[9 * 8 + 6] = (uint8)(light_intensity >> 8);
	VASetColors(clot, sizeof(clot));
	VAEraseBuffer();
	light_intensity = light_intensity * 2 / 3;
}

/* DrawLightning and LightningEffect: the bolt grows to the ground, then
   its tail runs up it while the flash fades. */
SEG_FRONT static void lightning_step(void)
{
	int16 i;

	if (!(light_intensity || !light_hit)) {
		set_colors();
		SOUND(SND_BLAST, 999);
		if (light_then)
			begin_demo();
		else
			set_screen(FRONT_TITLE);
		return;
	}
	if (!light_hit)
		light_block = (int16)(light_block + light_speed);
	else if (light_discharge < light_segs)
		light_discharge = (int16)(light_discharge + light_speed);
	VA.color = 2;
	for (i = light_discharge; i < light_block; i++) {
		if (i < light_segs && lx[lend[i]] != 0 && lx[lstart[i]] != 10 && ly[lend[i]] != 0) {
			VAMoveTo(lx[lstart[i]], ly[lstart[i]]);
			VASafeLineTo(lx[lend[i]], ly[lend[i]]);
		} else {
			if (!light_hit)
				light_intensity += LIGHT_FULL;
			light_hit = 1;
		}
	}
	for (i = light_discharge; i < light_block && i < LIGHT_MAX; i++) {
		if (lx[lstart[i]] != 0) {
			lx[lstart[i]] = (int16)(lx[lstart[i]] + title_random() % 4);
			ly[lstart[i]] = (int16)(ly[lstart[i]] + title_random() % 4);
		}
	}
	lightning_flash();
}

SEG_FRONT static void load_choices(void)
{
	nchoices = 0;
	if (!choices)
		choices = (LevelChoice *)plat_alloc(MAX_CHOICES * (uint32)sizeof(LevelChoice));
	if (!choices)
		return;
	nchoices = game_level_choices(Saved.practice, Saved.highest, choices, MAX_CHOICES);
	if (nchoices < 1)
		nchoices = 0;
	if (start_choice >= nchoices)
		start_choice = 0;
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

/* The carousel's box: a fifth of its area's width, at most 3/5 of its
   height. */
SEG_FRONT static int16 levels_box(void)
{
	PlatRect r;
	int16 box;

	menus_area(FRONT_LEVELS, &r);
	box = (int16)(r.w / 5);
	if (box > r.h * 3 / 5)
		box = (int16)(r.h * 3 / 5);
	return box < 8 ? 8 : box;
}

/* The shapes round the level picked and the box, in the carousel's view;
   menus.c shows the level number and bonus. */
SEG_FRONT static void draw_levels(void)
{
	int16 cx = (int16)(VA.frame.right / 2), k;
	int16 box = levels_box(), cy = (int16)(VA.frame.bottom / 2), x, y;

	if (nchoices == 0)
		return;
	if (sel_coloured != start_choice)
		level_colours();
	for (k = (int16)(start_choice - 2); k <= start_choice + 3; k++) {
		if (k < 0 || k >= nchoices)
			continue;
		VA.color = (int16)(k % 6 + 1);
		menus_shape(choices[k].field, (int16)(cx + (k - start_choice) * box - sel_off), cy, box);
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
}

/* Drag to turn the carousel, or < >; a tap on the middle or fire plays
   the level in the box, a tap to the side moves one. */
SEG_FRONT static void levels_step(const PlatInput *in)
{
	int16 box = levels_box(), step, z = tap_zone();
	PlatRect a;

	menus_area(FRONT_LEVELS, &a);
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
	} else if (z == UIZ_PREV || z == UIZ_NEXT) {
		if (z == UIZ_PREV && start_choice > 0) {
			start_choice--;
			sel_off = (int16)(sel_off + box);
		} else if (z == UIZ_NEXT && start_choice + 1 < nchoices) {
			start_choice++;
			sel_off = (int16)(sel_off - box);
		}
	} else if (z == UIZ_BACK) {
		pressed |= P_BACK;
	} else if (z == UIZ_OK && nchoices > 0) {
		SOUND(SND_PHAZERIN, 0);
		start_game(choices[start_choice].level);
		return;
	} else if (z == UIZ_AREA && sel_moved < 4) {
		int16 tx = (int16)(tap_x + plat_screen()->play.x - a.x);

		if (tx < a.w / 2 - box / 2) {
			if (start_choice > 0) {
				start_choice--;
				sel_off = (int16)(sel_off + box);
			}
		} else if (tx > a.w / 2 + box / 2) {
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
	return menus_score_rows(1);
}

/* The attract loop's credit pages (Text in Flashmain.c): TEXT 128 to 131 in
   a box that zooms in; once it is in, the box stays still (DrawBox) and
   menus.c fills it with the lines. In the square view. */
SEG_FRONT static void draw_text_page(void)
{
	int16 inset = (int16)(VA.frame.right / 12), x0 = inset, y0 = (int16)(VA.frame.bottom / 12);
	int16 x1 = (int16)(VA.frame.right - inset), y1 = (int16)(VA.frame.bottom - VA.frame.bottom / 12);

	if (frame == 0) {
		if (text_res)
			plat_release(text_res);
		text_res = plat_resource(FOURCC('T', 'E', 'X', 'T'), (int16)(128 + text_page), &text_n);
	}
	if (frame == ZOOMSTEPS)
		ui_dirty = 1;
	if (frame < ZOOMSTEPS) {
		draw_zoom((int16)frame, x0, y0, x1, y1);
	} else {
		VA.color = 2;
		VAMoveTo(x0, y0);
		VASafeLineTo(x1, y0);
		VASafeLineTo(x1, y1);
		VASafeLineTo(x0, y1);
		VASafeLineTo(x0, y0);
	}
}

/* Firework in Highmain.c: an explosion of size 2 at a random spot. */
SEG_FRONT static void firework(void)
{
	view(FRONT_SCORES_PAGE);
	VAExplosion((int16)((uint16)title_random() % (uint16)VA.frame.right),
	            (int16)((uint16)title_random() % (uint16)VA.frame.bottom), 2,
	            (int16)((uint16)title_random() % 7));
}

/* GAME OVER over the score, in the band's view. */
SEG_FRONT static void draw_over(void)
{
	text("GAME OVER", (int16)(VA.frame.right / 2), (int16)(VA.frame.bottom / 2 + ts * 3),
	     (int16)(1 + (frame / 3) % 6));
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

/* The drum, in the name wheel's view: seen from inside, x = cos,
   y = sin + 500 with the original's sins of 8192, scaled to dw (the
   view's width, at most twice its height). menus.c shows the score and
   the name. */
SEG_FRONT static void draw_name(void)
{
	int16 cx = (int16)(VA.frame.right / 2), w = VA.frame.right, c;
	int16 rot = (int16)(wheel_big / SUBROTATION), s2 = (int16)(ts * 2), top, ys0;

	if (w > VA.frame.bottom * 2)
		w = (int16)(VA.frame.bottom * 2);
	{
		int16 hh = (int16)((s2 + 1) * 4 + 4);

		/* The picked letter's baseline goes at ys0, the letters shown in
		   the middle of the view. */
		ys0 = (int16)((VA.frame.bottom + hh) / 2 + 4);
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
		if (xs < cx - w / 2 || xs > cx + w / 2)
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
	int16 w = plat_screen()->play.w, delta = 0, z = tap_zone(), was = name_pos;
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
		if (z == UIZ_NAME || z == UIZ_OK) {
			if (name_pos > 0)
				done = 1;
		} else if (z == UIZ_DEL) {
			pressed |= P_BACK;
		} else {
			take = 1;
		}
	}
	if (tap)
		wheel_moved = 0;
	ch = wheel_char();
	if (take && ch && name_pos < NAME_LEN)
		name[name_pos++] = ch;
	if ((pressed & P_BACK) && name_pos > 0)
		name_pos--;
	if (name_pos != was)
		ui_dirty = 1;
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

/* One UI step: input and the moving parts, in the screen's view. A screen
   change sets ui_dirty, so front_frame paints the new screen. */
SEG_FRONT static void ui_step(const PlatInput *in)
{
	int16 r, z = tap_zone(), was = sel;

	view(screen);
	switch (screen) {
	case FRONT_TITLE:
		draw_title();
		if (tap || (pressed & (P_OK | P_BACK))) {
			SOUND(SND_BLAST, 100);
			set_screen(FRONT_MENU);
		} else if (frame >= TITLE_STEPS)
			lightning_begin(1);
		break;
	case FRONT_LIGHTNING:
		lightning_step();
		if (screen == FRONT_LIGHTNING && (tap || (pressed & (P_OK | P_BACK)))) {
			set_colors();
			set_screen(FRONT_MENU);
		}
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
		if ((uint16)title_random() % 9 == 0)
			firework();
		if (tap || (pressed & (P_OK | P_BACK)))
			set_screen(FRONT_MENU);
		else if (frame >= SCORES_STEPS)
			lightning_begin(0);
		break;
	case FRONT_MENU:
		if (pressed & P_PREV)
			sel = (int16)((sel + MENU_ITEMS - 1) % MENU_ITEMS);
		if (pressed & P_NEXT)
			sel = (int16)((sel + 1) % MENU_ITEMS);
		r = (z >= 0 && z < MENU_ITEMS) ? z : -1;
		if (r >= 0)
			sel = r;
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
			ui_dirty = 1;
		}
		levels_step(in);
		if (screen == FRONT_LEVELS) {
			if (start_choice != shown_choice)
				ui_dirty = 1;
			draw_levels();
		}
		break;
	case FRONT_SCORES:
		r = page;
		if ((pressed & P_NEXT) || z == UIZ_NEXT || (z < 0 && tap && tap_x > VA.frame.right * 2 / 3)) {
			if ((page + 1) * rows_per_page() < STORE_ENTRIES)
				page++;
		} else if ((pressed & P_PREV) || z == UIZ_PREV || (z < 0 && tap && tap_x < VA.frame.right / 3)) {
			if (page > 0)
				page--;
		} else if (tap || (pressed & (P_OK | P_BACK))) {
			hl_rank = -1;
			set_screen(FRONT_MENU);
		}
		if (page != r)
			ui_dirty = 1;
		break;
	case FRONT_OPTIONS:
		if (pressed & P_PREV)
			sel = (int16)((sel + 3) % 4);
		if (pressed & P_NEXT)
			sel = (int16)((sel + 1) % 4);
		r = (z >= 0 && z < 4) ? z : -1;
		if (r >= 0)
			sel = r;
		if (r >= 0 || (pressed & P_OK)) {
			ui_dirty = 1;
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
		if (tap || (pressed & (P_OK | P_BACK)))
			set_screen(FRONT_MENU);
		break;
	case FRONT_PAUSE:
		if (pressed & P_PREV)
			sel = (int16)((sel + 2) % 3);
		if (pressed & P_NEXT)
			sel = (int16)((sel + 1) % 3);
		r = (z >= 0 && z < 3) ? z : -1;
		if (r >= 0)
			sel = r;
		if (r >= 0 || (pressed & P_OK)) {
			if (sel == 1) {
				Saved.control = (uint8)!Saved.control;
				PlayControl = Saved.control;
				ui_dirty = 1;
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
			set_colors();
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
		if (screen == FRONT_NAME) {
			view(FRONT_NAME);
			draw_name();
		}
		break;
	default:
		break;
	}
	if (sel != was && screen != FRONT_PLAY)
		ui_dirty = 1;
	VAViewPlay();
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
	if (text_res)
		plat_release(text_res);
	text_res = NULL;
	game_demo_stop();
	store_save();
	game_close();
	if (logo)
		plat_release(logo);
	logo = 0;
	if (choices)
		plat_free(choices);
	choices = NULL;
	nchoices = 0;
	choices_ready = 0;
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
	int16 was;
	cbool full = 0;
	PlatRect box;

	if (!plat_screen())
		return;
	if (screen == FRONT_PLAY || screen == FRONT_DEMO) {
		VA.EraseMode = VA_ERASE_AUTO;
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
	/* Painted screens stay on screen, so the renderer erases its own
	   lines by drawing them again, never by filling a box. */
	VA.EraseMode = VA_ERASE_REDRAW;
	read_input(&in);
	/* Paint first: the taps of this step go to the zones on screen. What
	   the renderer showed is under the paint now. */
	if (ui_dirty && painted(screen)) {
		paint(0);
		VAForget();
		full = 1;
	} else if (screen == FRONT_SCORES_PAGE && VAShown(&box)) {
		/* Fireworks go over the list: put back what they cover. */
		box.x = (int16)(box.x - 1);
		box.y = (int16)(box.y - 1);
		box.w = (int16)(box.w + 2);
		box.h = (int16)(box.h + 2);
		paint(&box);
		VAForget();
		plat_present_rect(&box);
	}
	was = screen;
	ui_step(&in);
	/* A new screen is painted next step; drop what this one queued for
	   it so nothing shows over the old one. */
	if (screen != was && painted(screen))
		VACancelFrame();
	frame++;
	VAStep();
	if (full)
		plat_present();
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
	if (screen == FRONT_PLAY || screen == FRONT_DEMO) {
		game_screen_changed();
	} else {
		set_colors();
		sel_coloured = -1;
		ui_dirty = 1;
	}
}
