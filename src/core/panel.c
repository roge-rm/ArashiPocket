/* The status area during play: a panel at the top with the score, level,
   spare lives, the score to beat, the superzap and a pause button, and
   under it a strip the pen drags to turn the claw and Fire and Zap
   buttons. Painted with ui.c straight into the framebuffer; each part is
   painted and presented only when what it shows changes, or after the
   status area was wiped (VA.Wipes). Screen pixels throughout. */
#include "ui.h"
#include "va.h"

/* The parts, each painted on its own. BEAT2 is the score to beat on a line
   of its own where the panel is narrow (w 0 elsewhere). */
enum {
	C_SCORE, C_LEVEL, C_LIVES, C_BEAT, C_BEAT2, C_ZAP, C_PAUSE, C_DRAG, C_FIRE, C_ZAPB, C_N
};

static PlatRect cell[C_N];
static PlatRect pbox;           /* the panel */
static PlatRect laid_for;       /* the status area the layout is for */
static PlatRect laid_side;      /* and its right part */
static int16 laid_w, laid_h;
static cbool laid;
static uint16 wipes;
static cbool shown_ok;
static PanelInfo shown;
static int16 shown_held, shown_grip;

/* A spare life, 11 by 6. */
static const uint16 claw_icon[6] = {
	0x8020, 0xC060, 0xA0A0, 0x9120, 0x4A40, 0x3180
};

SEG_UI static void set(PlatRect *r, int16 x, int16 y, int16 w, int16 h);
SEG_UI static void layout(const PlatScreen *sc);
SEG_UI static cbool fresh_layout(void);
SEG_UI static cbool inside(const PlatRect *r, int16 x, int16 y);
SEG_UI static int16 num(char *out, int32 v);
SEG_UI static void paint_cell(int16 c, const PanelInfo *p, int16 held, int16 grip);
SEG_UI static void button(const PlatRect *r, const char *label, cbool down, cbool dim);
SEG_UI static void grow(PlatRect *u, const PlatRect *r);

SEG_UI static void set(PlatRect *r, int16 x, int16 y, int16 w, int16 h)
{
	r->x = x;
	r->y = y;
	r->w = w < 0 ? 0 : w;
	r->h = h < 0 ? 0 : h;
}

/* Wide panels hold everything on one row, middling ones (under the play
   area on a T|X, left of it on a Dana) on two, narrow ones (beside it on a
   Zodiac) on five. The controls go under the panel: side by side where the
   room left is much wider than high, else the strip over a row of buttons,
   or the strip over Fire over Zap where it is tall. With a side part (Dana)
   the strip fills the rest of the left and Fire over Zap the right. */
SEG_UI static void layout(const PlatScreen *sc)
{
	PlatRect S = sc->status, D = sc->side, C;
	int16 s, m, rh, ix, iw, y, rows, u, i, bw, h;

	ui_metrics();
	s = ui_s;
	m = (int16)(2 * s);
	rh = ui_lh;
	for (i = 0; i < C_N; i++)
		set(&cell[i], 0, 0, 0, 0);
	pbox.x = (int16)(S.x + m);
	pbox.y = (int16)(S.y + m);
	pbox.w = (int16)(S.w - 2 * m);
	ix = (int16)(pbox.x + 2 * s);
	iw = (int16)(pbox.w - 4 * s);
	u = (int16)(pbox.w / s);
	y = (int16)(pbox.y + s);
	if (u >= 300) {
		rows = 1;
		set(&cell[C_SCORE], ix, y, (int16)(48 * s), rh);
		set(&cell[C_LEVEL], (int16)(ix + 52 * s), y, (int16)(32 * s), rh);
		set(&cell[C_LIVES], (int16)(ix + 88 * s), y, (int16)(78 * s), rh);
		set(&cell[C_PAUSE], (int16)(ix + iw - rh), y, rh, rh);
		set(&cell[C_ZAP], (int16)(cell[C_PAUSE].x - 26 * s), y, (int16)(22 * s), rh);
		set(&cell[C_BEAT], (int16)(ix + 170 * s), y, (int16)(cell[C_ZAP].x - 4 * s - (ix + 170 * s)), rh);
	} else if (u >= 140) {
		rows = 2;
		set(&cell[C_SCORE], ix, y, (int16)(48 * s), rh);
		set(&cell[C_LEVEL], (int16)(ix + 52 * s), y, (int16)(32 * s), rh);
		bw = (int16)(iw - 88 * s);
		if (bw > 78 * s)
			bw = (int16)(78 * s);
		set(&cell[C_LIVES], (int16)(ix + iw - bw), y, bw, rh);
		y = (int16)(y + rh);
		set(&cell[C_PAUSE], (int16)(ix + iw - rh), y, rh, rh);
		set(&cell[C_ZAP], (int16)(cell[C_PAUSE].x - 26 * s), y, (int16)(22 * s), rh);
		set(&cell[C_BEAT], ix, y, (int16)(cell[C_ZAP].x - 4 * s - ix), rh);
	} else {
		rows = 5;
		set(&cell[C_PAUSE], (int16)(ix + iw - rh), y, rh, rh);
		set(&cell[C_SCORE], ix, y, (int16)(iw - rh - 2 * s), rh);
		y = (int16)(y + rh);
		set(&cell[C_ZAP], (int16)(ix + iw - 22 * s), y, (int16)(22 * s), rh);
		set(&cell[C_LEVEL], ix, y, (int16)(iw - 26 * s), rh);
		y = (int16)(y + rh);
		set(&cell[C_LIVES], ix, y, iw, rh);
		y = (int16)(y + rh);
		set(&cell[C_BEAT], ix, y, iw, rh);
		y = (int16)(y + rh);
		set(&cell[C_BEAT2], ix, y, iw, rh);
	}
	pbox.h = (int16)(rows * rh + 2 * s);

	C.x = (int16)(S.x + m);
	C.y = (int16)(pbox.y + pbox.h + m);
	C.w = (int16)(S.w - 2 * m);
	C.h = (int16)(S.y + S.h - m - C.y);
	if (D.w > 0 && D.h > 0) {
		if (C.h >= ui_bh)
			set(&cell[C_DRAG], C.x, C.y, C.w, C.h);
		C.x = (int16)(D.x + m);
		C.y = (int16)(D.y + m);
		C.w = (int16)(D.w - 2 * m);
		C.h = (int16)(D.h - 2 * m);
		h = (int16)((C.h - m) * 3 / 5);
		set(&cell[C_FIRE], C.x, C.y, C.w, h);
		set(&cell[C_ZAPB], C.x, (int16)(C.y + h + m), C.w, (int16)(C.h - h - m));
	} else if (C.h < ui_bh) {
		/* No room for the controls. */
	} else if (C.w * 2 >= C.h * 5) {
		bw = (int16)(C.w / 3);
		h = (int16)((C.h - m) * 3 / 5);
		set(&cell[C_DRAG], C.x, C.y, (int16)(C.w - bw - m), C.h);
		set(&cell[C_FIRE], (int16)(C.x + C.w - bw), C.y, bw, h);
		set(&cell[C_ZAPB], (int16)(C.x + C.w - bw), (int16)(C.y + h + m), bw, (int16)(C.h - h - m));
	} else if (C.w >= C.h) {
		h = (int16)((C.h - m) * 11 / 20);
		bw = (int16)(C.w / 3);
		set(&cell[C_DRAG], C.x, C.y, C.w, h);
		set(&cell[C_ZAPB], C.x, (int16)(C.y + h + m), bw, (int16)(C.h - h - m));
		set(&cell[C_FIRE], (int16)(C.x + bw + m), (int16)(C.y + h + m), (int16)(C.w - bw - m),
		    (int16)(C.h - h - m));
	} else {
		int16 rest;

		h = (int16)(C.h * 2 / 5);
		rest = (int16)(C.h - h - 2 * m);
		set(&cell[C_DRAG], C.x, C.y, C.w, h);
		set(&cell[C_FIRE], C.x, (int16)(C.y + h + m), C.w, (int16)(rest * 3 / 5));
		set(&cell[C_ZAPB], C.x, (int16)(cell[C_FIRE].y + cell[C_FIRE].h + m), C.w,
		    (int16)(rest - rest * 3 / 5));
	}
	laid_for = S;
	laid_side = D;
	laid_w = sc->width;
	laid_h = sc->height;
	laid = 1;
}

/* Lays out again when the screen changed. 1 when it did, 0 when it has no
   status area. */
SEG_UI static cbool fresh_layout(void)
{
	const PlatScreen *sc = plat_screen();

	if (!sc || sc->status.w <= 0 || sc->status.h <= 0) {
		laid = 0;
		return 0;
	}
	if (!laid || sc->width != laid_w || sc->height != laid_h ||
	    sc->status.x != laid_for.x || sc->status.y != laid_for.y ||
	    sc->status.w != laid_for.w || sc->status.h != laid_for.h ||
	    sc->side.x != laid_side.x || sc->side.w != laid_side.w || sc->side.h != laid_side.h) {
		layout(sc);
		shown_ok = 0;
	}
	return 1;
}

SEG_UI static cbool inside(const PlatRect *r, int16 x, int16 y)
{
	return (cbool)(r->w > 0 && x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h);
}

SEG_UI int16 panel_hit(int16 x, int16 y)
{
	const PlatScreen *sc = plat_screen();
	PlatRect r;

	if (!fresh_layout() || (!inside(&sc->status, x, y) && !inside(&sc->side, x, y)))
		return PANEL_NONE;
	/* The pause button takes taps a little round it too. */
	r = cell[C_PAUSE];
	r.x = (int16)(r.x - 2 * ui_s);
	r.y = (int16)(r.y - 2 * ui_s);
	r.w = (int16)(r.w + 4 * ui_s);
	r.h = (int16)(r.h + 4 * ui_s);
	if (inside(&r, x, y))
		return PANEL_PAUSE;
	if (inside(&cell[C_DRAG], x, y))
		return PANEL_DRAG;
	if (inside(&cell[C_FIRE], x, y))
		return PANEL_FIRE;
	if (inside(&cell[C_ZAPB], x, y))
		return PANEL_ZAP;
	return PANEL_OTHER;
}

SEG_UI int16 panel_turn_len(void)
{
	int16 n;

	if (!fresh_layout())
		return 256;
	n = (int16)(160 * ui_s);
	return cell[C_DRAG].w > n ? cell[C_DRAG].w : n;
}

/* v in decimal into out. Returns the length. */
SEG_UI static int16 num(char *out, int32 v)
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

/* A control as the menu buttons draw them, claw yellow while held. */
SEG_UI static void button(const PlatRect *r, const char *label, cbool down, cbool dim)
{
	ui_fill(r->x, r->y, r->w, r->h, UIC_BG);
	ui_box(r->x, r->y, r->w, r->h, down ? UIC_SEL : UIC_BTN, down ? UIC_SEL : UIC_LINE);
	ui_text_c((int16)(r->x + r->w / 2), ui_text_y(r->y, r->h), label,
	          down ? UIC_INK : (dim ? UIC_DIM : UIC_TEXT));
}

SEG_UI static void paint_cell(int16 c, const PanelInfo *p, int16 held, int16 grip)
{
	const PlatRect *r = &cell[c];
	char buf[24];
	int16 s = ui_s, ty = ui_text_y(r->y, r->h), x, i, n;

	if (r->w <= 0 || r->h <= 0)
		return;
	if (c < C_DRAG && c != C_PAUSE)
		ui_fill(r->x, r->y, r->w, r->h, UIC_PANEL);
	ui_clip(r->x, r->y, r->w, r->h);
	switch (c) {
	case C_SCORE:
		num(buf, p->score);
		ui_text(r->x, ty, buf, UIC_GOLD);
		break;
	case C_LEVEL:
		x = (int16)(r->x + ui_text(r->x, ty, "LV", UIC_DIM) + 3 * s);
		num(buf, p->level);
		ui_text(x, ty, buf, UIC_TEXT);
		break;
	case C_LIVES:
		n = (int16)(r->w / (13 * s));
		if (n > p->lives)
			n = p->lives;
		for (i = 0; i < n; i++)
			ui_icon((int16)(r->x + i * 13 * s), (int16)(r->y + (r->h - 6 * s) / 2), claw_icon, 11, 6, UIC_SEL);
		break;
	case C_BEAT:
	case C_BEAT2:
		if (p->beat_place < 0)
			break;
		if (p->beat_place == 0) {
			if (c == C_BEAT)
				ui_text(r->x, ty, "HI-SCORE", UIC_GOLD);
			break;
		}
		n = 0;
		if (c == C_BEAT) {
			n = num(buf, p->beat_place);
			buf[n++] = '-';
			for (i = 0; i < 5 && p->beat_name[i]; i++)
				buf[n++] = p->beat_name[i];
			buf[n] = 0;
			x = (int16)(r->x + ui_text(r->x, ty, buf, UIC_DIM) + 5 * s);
			if (cell[C_BEAT2].w > 0)
				break;
		} else {
			x = r->x;
		}
		num(buf, p->beat_score);
		ui_text(x, ty, buf, UIC_TEXT);
		break;
	case C_ZAP:
		if (p->zap) {
			ui_box(r->x, r->y, r->w, r->h, UIC_BTN, UIC_LINE);
			ui_text_c((int16)(r->x + r->w / 2), ty, "ZAP", UIC_GOLD);
		} else {
			ui_text_c((int16)(r->x + r->w / 2), ty, "ZAP", UIC_DIM);
		}
		break;
	case C_PAUSE:
		ui_fill(r->x, r->y, r->w, r->h, UIC_PANEL);
		ui_box(r->x, r->y, r->w, r->h, held == PANEL_PAUSE ? UIC_SEL : UIC_BTN,
		       held == PANEL_PAUSE ? UIC_SEL : UIC_LINE);
		n = (int16)(r->h / 2);
		x = (int16)(r->x + r->w / 2);
		i = (int16)(r->y + (r->h - n) / 2);
		ui_fill((int16)(x - 3 * s), i, (int16)(2 * s), n, held == PANEL_PAUSE ? UIC_INK : UIC_TEXT);
		ui_fill((int16)(x + s), i, (int16)(2 * s), n, held == PANEL_PAUSE ? UIC_INK : UIC_TEXT);
		break;
	case C_DRAG: {
		int16 sp = (int16)(8 * s), in = (int16)(3 * s), h, y0, off;
		cbool down = (cbool)(held == PANEL_DRAG);

		ui_fill(r->x, r->y, r->w, r->h, UIC_BG);
		ui_box(r->x, r->y, r->w, r->h, UIC_PANEL, down ? UIC_SEL : UIC_LINE);
		/* Ridges that move with the pen, like a spinner's knurling. */
		ui_clip((int16)(r->x + in), (int16)(r->y + in), (int16)(r->w - 2 * in), (int16)(r->h - 2 * in));
		h = (int16)(r->h / 2);
		y0 = (int16)(r->y + (r->h - h) / 2);
		off = (int16)(grip % sp);
		if (off < 0)
			off = (int16)(off + sp);
		for (x = (int16)(r->x + in + off - sp); x < r->x + r->w; x = (int16)(x + sp))
			ui_fill(x, y0, s, h, down ? UIC_SEL : UIC_DIM);
		break;
	}
	case C_FIRE:
		button(r, "FIRE", (cbool)(held == PANEL_FIRE), 0);
		break;
	case C_ZAPB:
		button(r, "ZAP", (cbool)(held == PANEL_ZAP), (cbool)!p->zap);
		break;
	}
	ui_clip_off();
}

SEG_UI static void grow(PlatRect *u, const PlatRect *r)
{
	int16 x1, y1;

	if (r->w <= 0 || r->h <= 0)
		return;
	if (u->w <= 0) {
		*u = *r;
		return;
	}
	x1 = (int16)(u->x + u->w > r->x + r->w ? u->x + u->w : r->x + r->w);
	y1 = (int16)(u->y + u->h > r->y + r->h ? u->y + u->h : r->y + r->h);
	if (r->x < u->x)
		u->x = r->x;
	if (r->y < u->y)
		u->y = r->y;
	u->w = (int16)(x1 - u->x);
	u->h = (int16)(y1 - u->y);
}

SEG_UI void panel_update(const PanelInfo *p, int16 held, int16 grip)
{
	const PlatScreen *sc;
	uint16 dirty = 0;
	cbool all, beat;
	int16 c, i;
	PlatRect u, v;

	if (!fresh_layout())
		return;
	sc = plat_screen();
	all = (cbool)(!shown_ok || VA.Wipes != wipes);
	if (all) {
		dirty = (uint16)((1 << C_N) - 1);
	} else {
		beat = (cbool)(p->beat_place != shown.beat_place || p->beat_score != shown.beat_score);
		for (i = 0; i < 5; i++)
			if (p->beat_name[i] != shown.beat_name[i])
				beat = 1;
		if (p->score != shown.score)
			dirty |= 1 << C_SCORE;
		if (p->level != shown.level)
			dirty |= 1 << C_LEVEL;
		if (p->lives != shown.lives)
			dirty |= 1 << C_LIVES;
		if (beat)
			dirty |= (1 << C_BEAT) | (1 << C_BEAT2);
		if (p->zap != shown.zap)
			dirty |= (1 << C_ZAP) | (1 << C_ZAPB);
		if ((held == PANEL_PAUSE) != (shown_held == PANEL_PAUSE))
			dirty |= 1 << C_PAUSE;
		if ((held == PANEL_DRAG) != (shown_held == PANEL_DRAG) || grip != shown_grip)
			dirty |= 1 << C_DRAG;
		if ((held == PANEL_FIRE) != (shown_held == PANEL_FIRE))
			dirty |= 1 << C_FIRE;
		if ((held == PANEL_ZAP) != (shown_held == PANEL_ZAP))
			dirty |= 1 << C_ZAPB;
	}
	if (!dirty || !ui_begin())
		return;
	/* u gathers what changed on the left, v on the side part, so the play
	   area between them isn't presented. */
	u.x = u.y = u.w = u.h = 0;
	v = u;
	if (all) {
		ui_fill(sc->status.x, sc->status.y, sc->status.w, sc->status.h, UIC_BG);
		ui_box(pbox.x, pbox.y, pbox.w, pbox.h, UIC_PANEL, -1);
		u = sc->status;
		if (sc->side.w > 0 && sc->side.h > 0) {
			ui_fill(sc->side.x, sc->side.y, sc->side.w, sc->side.h, UIC_BG);
			v = sc->side;
		}
	}
	for (c = 0; c < C_N; c++)
		if (dirty & (1 << c)) {
			paint_cell(c, p, held, grip);
			if (sc->side.w > 0 && cell[c].x >= sc->side.x)
				grow(&v, &cell[c]);
			else
				grow(&u, &cell[c]);
		}
	ui_end();
	if (u.w > 0)
		plat_present_rect(&u);
	if (v.w > 0)
		plat_present_rect(&v);
	shown = *p;
	shown_held = held;
	shown_grip = grip;
	shown_ok = 1;
	wipes = VA.Wipes;
}
