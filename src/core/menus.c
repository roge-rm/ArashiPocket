/* The front end's screens, painted with ui.c: a dark panel under a title
   bar with a red line, cyan outlined buttons with the selected one in claw
   yellow, list rows on alternate stripes and a footer bar. The main menu
   and the pause menu have a picture band over the panel: the logo and a
   tube drawn once with the renderer. Every screen also sets its tap
   zones. Screen pixels throughout. */
#include "ui.h"
#include "front.h"
#include "va.h"
#include "store.h"

typedef struct {
	int16 m;            /* side margin */
	int16 bar;          /* title bar height, red line included */
	int16 foot;         /* footer top (ui_h without one) */
	int16 cw, cx;       /* content width, and its centre */
} Form;

SEG_UI static void form_metrics(Form *f, cbool footer);
SEG_UI static void form(Form *f, const char *title, cbool footer);
SEG_UI static void button(int16 id, int16 x, int16 y, int16 w, int16 h, const char *label, cbool sel);
SEG_UI static void footer2(const Form *f, const char *a, int16 ida, const char *b, int16 idb, int16 sel);
SEG_UI static void split(int16 top, int16 n, int16 extra, PlatRect *band, PlatRect *panel);
SEG_UI static void hero(const PlatRect *r, int16 clear);
SEG_UI static void square(PlatRect *r);
SEG_UI static int16 fmt(char *out, int32 v);
SEG_UI static void cat(char *out, const char *a, const char *b);
SEG_UI static void buttons(const PlatRect *panel, int16 y, const char *const *labels, int16 n, int16 sel);
SEG_UI static void option(int16 id, int16 x, int16 y, int16 w, const char *label, const char *value, cbool sel);
SEG_UI static void form_score_rows(const Form *f, cbool footer, int16 *y0, int16 *n);

SEG_UI static int16 fmt(char *out, int32 v)
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

/* a then b into out. */
SEG_UI static void cat(char *out, const char *a, const char *b)
{
	while (*a)
		*out++ = *a++;
	while (*b)
		*out++ = *b++;
	*out = 0;
}

SEG_UI static void form_metrics(Form *f, cbool footer)
{
	int16 s = ui_s;

	f->m = (int16)(6 * s);
	f->bar = (int16)(15 * s);
	f->foot = (int16)(footer ? ui_h - ui_bh - 6 * s : ui_h);
	f->cw = (int16)(ui_w - 2 * f->m);
	if (f->cw > 150 * s)
		f->cw = (int16)(150 * s);
	f->cx = (int16)(ui_w / 2);
}

/* The panel, the title bar with its red line and the footer bar. */
SEG_UI static void form(Form *f, const char *title, cbool footer)
{
	int16 s = ui_s;

	form_metrics(f, footer);
	ui_zones_clear();
	ui_fill(0, 0, ui_w, ui_h, UIC_PANEL);
	ui_fill(0, 0, ui_w, (int16)(f->bar - s), UIC_BAR);
	ui_fill(0, (int16)(f->bar - s), ui_w, s, UIC_ACCENT);
	ui_text(f->m, ui_text_y(0, (int16)(f->bar - s)), title, UIC_TEXT);
	if (footer)
		ui_fill(0, f->foot, ui_w, (int16)(ui_h - f->foot), UIC_BAR);
}

SEG_UI static void button(int16 id, int16 x, int16 y, int16 w, int16 h, const char *label, cbool sel)
{
	ui_box(x, y, w, h, sel ? UIC_SEL : UIC_BTN, sel ? UIC_SEL : UIC_LINE);
	ui_text_c((int16)(x + w / 2), ui_text_y(y, h), label, sel ? UIC_INK : UIC_TEXT);
	ui_zone(id, x, y, w, h);
}

/* Two buttons in the footer, a on the left, b on the right; sel is the id
   drawn selected. */
SEG_UI static void footer2(const Form *f, const char *a, int16 ida, const char *b, int16 idb, int16 sel)
{
	int16 s = ui_s, w = (int16)(60 * s), y = (int16)(f->foot + 3 * s);
	int16 l = (int16)(f->cx - f->cw / 2), r = (int16)(f->cx + f->cw / 2);

	if (a)
		button(ida, l, y, w, ui_bh, a, (cbool)(sel == ida));
	if (b)
		button(idb, (int16)(r - w), y, w, ui_bh, b, (cbool)(sel == idb));
}

/* The picture band and the button panel below it (beside it on a wide
   screen), for n buttons and extra rows of panel. top is where the band
   starts. */
SEG_UI static void split(int16 top, int16 n, int16 extra, PlatRect *band, PlatRect *panel)
{
	int16 s = ui_s, p = (int16)(12 * s + n * ui_bh + (n - 1) * 3 * s + extra);

	if (ui_w * 3 >= ui_h * 4) {
		panel->w = (int16)(ui_w * 2 / 5);
		panel->x = (int16)(ui_w - panel->w);
		panel->y = top;
		panel->h = (int16)(ui_h - top);
		band->x = 0;
		band->y = top;
		band->w = panel->x;
		band->h = panel->h;
	} else {
		if (p > ui_h - top)
			p = (int16)(ui_h - top);
		panel->x = 0;
		panel->w = ui_w;
		panel->y = (int16)(ui_h - p);
		panel->h = p;
		band->x = 0;
		band->y = top;
		band->w = ui_w;
		band->h = (int16)(panel->y - top);
	}
}

/* Buttons down a panel from y, as wide as it allows. */
SEG_UI static void buttons(const PlatRect *panel, int16 y, const char *const *labels, int16 n, int16 sel)
{
	int16 s = ui_s, i, w = (int16)(panel->w - 24 * s), x;

	if (w > 150 * s)
		w = (int16)(150 * s);
	x = (int16)(panel->x + (panel->w - w) / 2);
	for (i = 0; i < n; i++)
		button(i, x, (int16)(y + i * (ui_bh + 3 * s)), w, ui_bh, labels[i], (cbool)(i == sel));
}

/* The centred square the attract pages use. */
SEG_UI static void square(PlatRect *r)
{
	int16 n = ui_w < ui_h ? ui_w : ui_h;

	r->x = (int16)((ui_w - n) / 2);
	r->y = (int16)((ui_h - n) / 2);
	r->w = r->h = n;
}

/* The hero picture's colours, as a CLOT. */
static const uint8 hero_cols[NUMCOLORS][3] = {
	{ 255, 255, 255 }, { 255, 220, 0 }, { 255, 40, 40 }, { 0, 230, 60 },
	{ 220, 0, 220 }, { 0, 200, 255 }, { 30, 90, 255 }, { 90, 110, 170 },
	{ 0, 0, 200 }, { 0, 0, 0 }
};

/* An open field seen from its near edge: 12 lanes running off to a
   horizon under the logo, the claw on the near rim with its shots going
   down its lane, a flipper, a tanker, a spike and a few stars. Drawn once
   with the renderer into r; the top clear rows are under the logo. */
SEG_UI static void hero(const PlatRect *r, int16 clear)
{
	uint8 clot[NUMCOLORS * 8];
	int16 w = r->w, h = r->h, s = ui_s, i, cx, rx, yn, yf, lx, ly, rx2, ry2, mx, my, ux, uy;

	for (i = 0; i < NUMCOLORS * 8; i++)
		clot[i] = 0;
	for (i = 0; i < NUMCOLORS; i++) {
		clot[i * 8 + 2] = hero_cols[i][0];
		clot[i * 8 + 4] = hero_cols[i][1];
		clot[i * 8 + 6] = hero_cols[i][2];
	}
	VASetColors(clot, sizeof(clot));
	VAViewRect(r->x, r->y, w, h);
	cx = (int16)(w / 2);
	rx = (int16)(w * 47 / 100);
	yn = (int16)(h - 3 * s);
	yf = (int16)(clear * 3 / 4);
	if (yf > yn - 12 * s)
		yf = (int16)(yn - 12 * s);
	/* Stars over the horizon, the same every time. */
	{
		uint16 seed = 12345;

		VA.color = 7;
		for (i = 0; i < 24; i++) {
			int16 x, y;

			seed = (uint16)(seed * 25173U + 13849U);
			x = (int16)((uint32)seed * w >> 16);
			seed = (uint16)(seed * 25173U + 13849U);
			y = (int16)((uint32)seed * (yf > 0 ? yf : 1) >> 16);
			VAPixel(x, y);
		}
	}
	/* Lane edge k (0 to 12) at depth z (16 the near rim, 64 the far one):
	   x = cx + X / z, y = yv + (yn - yv) / z with the far rim on yf. */
#define AT(k, z, ox, oy) { \
	ox = (int16)(cx + (int32)rx * ((k) * 2 - 12) * 16 / (12 * (int32)(z))); \
	oy = (int16)(yf - (int32)(yn - yf) / 3 + (int32)(yn - yf) * 4 * 16 / (3 * (int32)(z))); }
	VA.color = 6;
	for (i = 0; i <= 12; i++) {
		AT(i, 16, lx, ly);
		AT(i, 64, rx2, ry2);
		VALine(lx, ly, rx2, ry2);
	}
	AT(0, 16, lx, ly);
	AT(12, 16, rx2, ry2);
	VALine(lx, ly, rx2, ry2);
	AT(0, 64, lx, ly);
	AT(12, 64, rx2, ry2);
	VALine(lx, ly, rx2, ry2);
	/* The claw over lane 5 on the near rim, pointing down the lane. */
	{
		int16 ax, ay, bx, by, qx, qy, tx, ty;

		AT(5, 16, ax, ay);
		AT(6, 16, bx, by);
		AT(5, 21, qx, qy);
		AT(6, 21, tx, ty);
		mx = (int16)((ax + bx) / 2);
		my = (int16)((ay + by) / 2);
		ux = (int16)((qx + tx) / 2 - mx);
		uy = (int16)((qy + ty) / 2 - my);
		VA.color = 1;
		VAMoveTo(ax, ay);
		VALineTo((int16)(ax + ux * 3 / 4 + (mx - ax) / 3), (int16)(ay + uy * 3 / 4 + (my - ay) / 3));
		VALineTo((int16)(mx + ux), (int16)(my + uy));
		VALineTo((int16)(bx + ux * 3 / 4 + (mx - bx) / 3), (int16)(by + uy * 3 / 4 + (my - by) / 3));
		VALineTo(bx, by);
		VALineTo((int16)(mx + ux / 3), (int16)(my + uy / 3));
		VALineTo(ax, ay);
	}
	/* Its shots going down the lane. */
	for (i = 0; i < 3; i++) {
		AT(5, 24 + i * 9, lx, ly);
		AT(6, 24 + i * 9, rx2, ry2);
		if (s > 1)
			VASpot((int16)((lx + rx2) / 2), (int16)((ly + ry2) / 2));
		else
			VAPixel((int16)((lx + rx2) / 2), (int16)((ly + ry2) / 2));
	}
	/* A flipper on lane 8, a bow tie standing across the lane. */
	AT(8, 26, lx, ly);
	AT(9, 26, rx2, ry2);
	ux = (int16)((rx2 - lx) / 3);
	VA.color = 2;
	VAMoveTo(lx, (int16)(ly - ux));
	VALineTo(rx2, (int16)(ry2 + ux / 2));
	VALineTo(rx2, (int16)(ry2 - ux));
	VALineTo(lx, (int16)(ly + ux / 2));
	VALineTo(lx, (int16)(ly - ux));
	/* A tanker on lane 2, a diamond with a cross. */
	AT(2, 34, lx, ly);
	AT(3, 34, rx2, ry2);
	mx = (int16)((lx + rx2) / 2);
	my = (int16)((ly + ry2) / 2 - (rx2 - lx) / 3);
	ux = (int16)((rx2 - lx) * 2 / 5);
	VA.color = 4;
	VAMoveTo((int16)(mx - ux), my);
	VALineTo(mx, (int16)(my - ux));
	VALineTo((int16)(mx + ux), my);
	VALineTo(mx, (int16)(my + ux));
	VALineTo((int16)(mx - ux), my);
	VALine(mx, (int16)(my - ux), mx, (int16)(my + ux));
	VALine((int16)(mx - ux), my, (int16)(mx + ux), my);
	/* A spike growing up lane 10 from the far rim. */
	AT(10, 64, lx, ly);
	AT(11, 64, rx2, ry2);
	AT(10, 30, mx, my);
	AT(11, 30, ux, uy);
	VA.color = 3;
	VALine((int16)((lx + rx2) / 2), (int16)((ly + ry2) / 2), (int16)((mx + ux) / 2), (int16)((my + uy) / 2));
#undef AT
	VABake();
	VAViewPlay();
	VALoadColors(1000);
}

SEG_UI void menus_area(int16 scr, PlatRect *r)
{
	Form f;
	int16 s;

	ui_metrics();
	s = ui_s;
	switch (scr) {
	case FRONT_LEVELS:
		form_metrics(&f, 1);
		r->x = 0;
		r->y = f.bar;
		r->w = ui_w;
		r->h = (int16)(f.foot - f.bar - 2 * ui_lh - 8 * s);
		break;
	case FRONT_NAME:
		form_metrics(&f, 1);
		r->x = 0;
		r->y = (int16)(f.bar + 6 * s + ui_lh + 4 * s + 16 * s + 6 * s);
		r->w = ui_w;
		r->h = (int16)(f.foot - r->y);
		break;
	case FRONT_OVER:
		r->x = 0;
		r->y = 0;
		r->w = ui_w;
		r->h = (int16)(ui_h * 2 / 5);
		break;
	case FRONT_TITLE:
		/* Above the line with the versions. */
		square(r);
		if (r->y + r->h > ui_h - 11 * s)
			r->h = (int16)(ui_h - 11 * s - r->y);
		break;
	default:
		square(r);
		break;
	}
}

SEG_UI void menus_title(void)
{
	int16 y = (int16)(ui_h - 10 * ui_s);

	ui_zones_clear();
	ui_fill(0, 0, ui_w, ui_h, UIC_BG);
	ui_text((int16)(3 * ui_s), y, "Arashi 1.1.1", UIC_DIM);
	ui_text_r((int16)(ui_w - 3 * ui_s), y, "v" ARASHI_VERSION, UIC_DIM);
}

SEG_UI void menus_main(const char *const *labels, int16 n, int16 sel)
{
	PlatRect band, panel;
	int16 s = ui_s, lw, lh, y;

	ui_zones_clear();
	split(0, n, (int16)(10 * s), &band, &panel);
	ui_fill(0, 0, ui_w, ui_h, UIC_BG);
	ui_logo_size(&lw, &lh);
	hero(&band, (int16)(lh + 3 * s));
	ui_logo((int16)(band.x + band.w / 2), (int16)(band.y + 3 * s));
	ui_fill(panel.x, panel.y, panel.w, panel.h, UIC_PANEL);
	if (panel.x > 0)
		ui_fill(panel.x, panel.y, s, panel.h, UIC_ACCENT);
	else
		ui_fill(0, panel.y, ui_w, s, UIC_ACCENT);
	y = (int16)(panel.y + 6 * s);
	if (panel.x > 0)
		y = (int16)(panel.y + (panel.h - 10 * s - (n * ui_bh + (n - 1) * 3 * s)) / 2);
	buttons(&panel, y, labels, n, sel);
	ui_text_r((int16)(ui_w - 4 * s), (int16)(ui_h - 9 * s), "v" ARASHI_VERSION, UIC_DIM);
}

/* A button with a label on the left and its value on the right. */
SEG_UI static void option(int16 id, int16 x, int16 y, int16 w, const char *label, const char *value, cbool sel)
{
	int16 ty = ui_text_y(y, ui_bh);

	ui_box(x, y, w, ui_bh, sel ? UIC_SEL : UIC_BTN, sel ? UIC_SEL : UIC_LINE);
	ui_text((int16)(x + 6 * ui_s), ty, label, sel ? UIC_INK : UIC_TEXT);
	ui_text_r((int16)(x + w - 6 * ui_s), ty, value, sel ? UIC_INK : UIC_GOLD);
	ui_zone(id, x, y, w, ui_bh);
}

SEG_UI void menus_options(int16 sel, cbool drag, cbool practice, cbool sound)
{
	Form f;
	int16 s, x, y, step;

	form(&f, "Options", 1);
	s = ui_s;
	x = (int16)(f.cx - f.cw / 2);
	step = (int16)(ui_bh + 5 * s);
	y = (int16)(f.bar + 8 * s);
	option(0, x, y, f.cw, "Control", drag ? "Drag" : "Touch", (cbool)(sel == 0));
	option(1, x, (int16)(y + step), f.cw, "Mode", practice ? "Practice" : "Arcade", (cbool)(sel == 1));
	option(2, x, (int16)(y + 2 * step), f.cw, "Sound", sound ? "On" : "Off", (cbool)(sel == 2));
	footer2(&f, NULL, 0, "Done", 3, sel);
}

SEG_UI void menus_about(void)
{
	Form f;
	int16 s, lw, lh, y, need;

	form(&f, "About", 1);
	s = ui_s;
	ui_logo_size(&lw, &lh);
	need = (int16)(lh + 4 * ui_lh + 8 * s);
	y = (int16)(f.bar + (f.foot - f.bar - need) / 2);
	if (y < f.bar + 2 * s)
		y = (int16)(f.bar + 2 * s);
	if (need <= f.foot - f.bar) {
		ui_fill(0, y, ui_w, (int16)(lh + 4 * s), UIC_BG);
		ui_logo(f.cx, (int16)(y + 2 * s));
		y = (int16)(y + lh + 8 * s);
	}
	ui_text_c(f.cx, y, "Arashi 1.1.1", UIC_GOLD);
	ui_text_c(f.cx, (int16)(y + ui_lh), "by Juri Munkki", UIC_TEXT);
	ui_text_c(f.cx, (int16)(y + 2 * ui_lh + 4 * s), "Palm version " ARASHI_VERSION, UIC_DIM);
	footer2(&f, NULL, 0, "OK", UIZ_OK, UIZ_OK);
}

SEG_UI void menus_pause(int16 sel, cbool drag, int16 level, int16 field, int16 clot, int32 score)
{
	const char *lab[3];
	char buf[24], num[12];
	PlatRect band, panel;
	Form f;
	int16 s, box, y;

	form(&f, "Paused", 0);
	s = ui_s;
	split(f.bar, 3, (int16)(ui_lh + 4 * s), &band, &panel);
	ui_fill(band.x, band.y, band.w, band.h, UIC_BG);
	/* The level's shape in its own colour. */
	box = (int16)(band.h * 3 / 4);
	if (box > band.w * 3 / 4)
		box = (int16)(band.w * 3 / 4);
	if (box > 8) {
		VALoadColors(clot);
		VAViewRect(band.x, band.y, band.w, band.h);
		VA.color = 0;
		menus_shape(field, (int16)(band.w / 2), (int16)(band.h / 2), box);
		VABake();
		VAViewPlay();
		VALoadColors(1000);
	}
	if (panel.x > 0)
		ui_fill(panel.x, panel.y, s, panel.h, UIC_ACCENT);
	y = (int16)(panel.y + 6 * s);
	if (panel.x > 0)
		y = (int16)(panel.y + (panel.h - ui_lh - 4 * s - (3 * ui_bh + 6 * s)) / 2);
	fmt(num, level);
	cat(buf, "Level ", num);
	ui_text((int16)(panel.x + 12 * s), y, buf, UIC_TEXT);
	fmt(num, score);
	ui_text_r((int16)(panel.x + panel.w - 12 * s), y, num, UIC_GOLD);
	lab[0] = "Resume";
	lab[1] = drag ? "Control: Drag" : "Control: Touch";
	lab[2] = "Quit";
	buttons(&panel, (int16)(y + ui_lh + 4 * s), lab, 3, sel);
}

SEG_UI void menus_levels(int16 level, int32 bonus, int16 sel)
{
	PlatRect a;
	Form f;
	char buf[28], num[12];
	int16 s, y, bw;

	form(&f, "Select level", 1);
	s = ui_s;
	menus_area(FRONT_LEVELS, &a);
	ui_fill(a.x, a.y, a.w, a.h, UIC_BG);
	ui_zone(UIZ_AREA, a.x, a.y, a.w, a.h);
	y = (int16)(a.y + a.h + 4 * s);
	if (level > 0) {
		fmt(num, level);
		cat(buf, "Level ", num);
		ui_text_c(f.cx, y, buf, UIC_GOLD);
		if (bonus > 0) {
			fmt(num, bonus);
			cat(buf, "Bonus ", num);
			ui_text_c(f.cx, (int16)(y + ui_lh), buf, UIC_TEXT);
		}
	}
	/* < and > beside the level number. */
	bw = (int16)(20 * s);
	y = (int16)(a.y + a.h + (f.foot - a.y - a.h - ui_bh) / 2);
	button(UIZ_PREV, (int16)(f.cx - f.cw / 2), y, bw, ui_bh, "<", 0);
	button(UIZ_NEXT, (int16)(f.cx + f.cw / 2 - bw), y, bw, ui_bh, ">", 0);
	footer2(&f, "Back", UIZ_BACK, "Play", UIZ_OK, sel ? UIZ_BACK : UIZ_OK);
}

/* The first row's top and how many rows fit. */
SEG_UI static void form_score_rows(const Form *f, cbool footer, int16 *y0, int16 *n)
{
	int16 s = ui_s;

	*y0 = (int16)(f->bar + 3 * s + ui_lh + 2 * s);
	*n = (int16)((f->foot - *y0 - (footer ? 2 * s : 0)) / ui_lh);
	if (*n < 1)
		*n = 1;
}

SEG_UI int16 menus_score_rows(cbool footer)
{
	Form f;
	int16 y0, n;

	ui_metrics();
	form_metrics(&f, footer);
	form_score_rows(&f, footer, &y0, &n);
	return n;
}

SEG_UI void menus_scores(cbool practice, int16 page, int16 hl, cbool footer)
{
	Form f;
	char num[12], nm[STORE_NAME + 1];
	int16 s, y0, n, i, lw, lx, rk, first;

	form(&f, practice ? "Practice scores" : "High scores", footer);
	s = ui_s;
	form_score_rows(&f, footer, &y0, &n);
	if (n > STORE_ENTRIES)
		n = STORE_ENTRIES;
	first = (int16)(page * n);
	lw = (int16)(ui_w - 2 * f.m);
	if (lw > 220 * s)
		lw = (int16)(220 * s);
	lx = (int16)((ui_w - lw) / 2);
	rk = (int16)(lx + 16 * s);
	/* The header band. */
	ui_fill(lx, (int16)(f.bar + 3 * s), lw, ui_lh, UIC_HEAD);
	ui_text_r(rk, ui_text_y((int16)(f.bar + 3 * s), ui_lh), "#", UIC_HEADTXT);
	ui_text((int16)(rk + 8 * s), ui_text_y((int16)(f.bar + 3 * s), ui_lh), "Name", UIC_HEADTXT);
	ui_text_r((int16)(lx + lw - 4 * s), ui_text_y((int16)(f.bar + 3 * s), ui_lh), "Score", UIC_HEADTXT);
	for (i = 0; i < n && first + i < STORE_ENTRIES; i++) {
		const ScoreEntry *e = &Saved.table[practice ? 1 : 0][first + i];
		int16 y = (int16)(y0 + i * ui_lh), ty = ui_text_y(y, ui_lh), c = UIC_TEXT, j;

		if (first + i == hl) {
			ui_fill(lx, y, lw, ui_lh, UIC_SEL);
			c = UIC_INK;
		} else if (i & 1) {
			ui_fill(lx, y, lw, ui_lh, UIC_STRIPE);
		}
		fmt(num, first + i + 1);
		ui_text_r(rk, ty, num, c == UIC_TEXT ? UIC_DIM : c);
		for (j = 0; j < STORE_NAME; j++)
			nm[j] = e->name[j];
		nm[STORE_NAME] = 0;
		ui_text((int16)(rk + 8 * s), ty, nm, c);
		fmt(num, e->score);
		ui_text_r((int16)(lx + lw - 4 * s), ty, num, c == UIC_TEXT ? UIC_GOLD : c);
	}
	if (footer) {
		int16 bw = (int16)(20 * s), y = (int16)(f.foot + 3 * s), l = (int16)(f.cx - f.cw / 2);
		char buf[16];

		button(UIZ_PREV, l, y, bw, ui_bh, "<", 0);
		button(UIZ_NEXT, (int16)(l + bw + 3 * s), y, bw, ui_bh, ">", 0);
		i = (int16)(first + n > STORE_ENTRIES ? STORE_ENTRIES : first + n);
		fmt(num, i);
		cat(buf + fmt(buf, first + 1), "-", num);
		ui_text((int16)(l + 2 * bw + 9 * s), ui_text_y(y, ui_bh), buf, UIC_DIM);
		footer2(&f, NULL, 0, "Done", UIZ_BACK, UIZ_BACK);
	}
}

SEG_UI void menus_over(int32 score, int16 level)
{
	PlatRect a;
	Form f;
	char num[12], buf[20];
	int16 s, y;

	ui_zones_clear();
	form_metrics(&f, 1);
	s = ui_s;
	menus_area(FRONT_OVER, &a);
	ui_fill(0, 0, ui_w, ui_h, UIC_PANEL);
	ui_fill(a.x, a.y, a.w, a.h, UIC_BG);
	ui_fill(0, (int16)(a.y + a.h), ui_w, s, UIC_ACCENT);
	y = (int16)(a.y + a.h + (f.foot - a.y - a.h - 3 * ui_lh) / 2);
	ui_text_c(f.cx, y, "Score", UIC_DIM);
	fmt(num, score);
	ui_text_c(f.cx, (int16)(y + ui_lh), num, UIC_GOLD);
	fmt(num, level);
	cat(buf, "Level ", num);
	ui_text_c(f.cx, (int16)(y + 2 * ui_lh + 2 * s), buf, UIC_TEXT);
	ui_fill(0, f.foot, ui_w, (int16)(ui_h - f.foot), UIC_BAR);
	footer2(&f, NULL, 0, "Continue", UIZ_OK, UIZ_OK);
}

SEG_UI void menus_name(int32 score, const char *name, int16 len, int16 max)
{
	PlatRect a;
	Form f;
	char num[12], buf[20], ch[2];
	int16 s, y, i, sw, sh, gap, x0;

	form(&f, "New high score", 1);
	s = ui_s;
	y = (int16)(f.bar + 6 * s);
	fmt(num, score);
	cat(buf, "Score ", num);
	ui_text_c(f.cx, y, buf, UIC_GOLD);
	y = (int16)(y + ui_lh + 4 * s);
	sw = (int16)(14 * s);
	sh = (int16)(16 * s);
	gap = (int16)(4 * s);
	x0 = (int16)(f.cx - (max * sw + (max - 1) * gap) / 2);
	ch[1] = 0;
	for (i = 0; i < max; i++) {
		int16 x = (int16)(x0 + i * (sw + gap));

		ui_box(x, y, sw, sh, UIC_BTN, i == len ? UIC_SEL : UIC_LINE);
		if (i < len) {
			ch[0] = name[i];
			ui_text_c((int16)(x + sw / 2), ui_text_y(y, sh), ch, UIC_TEXT);
		}
	}
	ui_zone(UIZ_NAME, x0, y, (int16)(max * sw + (max - 1) * gap), sh);
	menus_area(FRONT_NAME, &a);
	ui_fill(a.x, a.y, a.w, a.h, UIC_BG);
	footer2(&f, "Delete", UIZ_DEL, "Done", UIZ_OK, len > 0 ? UIZ_OK : -1);
	ui_zone(UIZ_AREA, a.x, a.y, a.w, a.h);
}

SEG_UI void menus_text(const uint8 *t, uint32 n, cbool inside)
{
	PlatRect sq;
	int16 s = ui_s, x0, y0, x1, y1, lines = 0, y, pass;
	uint32 i, start;

	ui_zones_clear();
	ui_fill(0, 0, ui_w, ui_h, UIC_BG);
	if (!inside || !t)
		return;
	square(&sq);
	/* Inside the box draw_zoom ends on. */
	x0 = (int16)(sq.x + sq.w / 12 + 2);
	y0 = (int16)(sq.y + sq.h / 12 + 2);
	x1 = (int16)(sq.x + sq.w - sq.w / 12 - 2);
	y1 = (int16)(sq.y + sq.h - sq.h / 12 - 2);
	ui_fill(x0, y0, (int16)(x1 - x0 + 1), (int16)(y1 - y0 + 1), UIC_PANEL);
	for (i = 0; i < n; i++)
		if (t[i] == 13)
			lines++;
	y = (int16)((y0 + y1) / 2 - lines * ui_lh / 2 + 2 * s);
	pass = 0;
	start = 0;
	for (i = 0; i < n; i++) {
		if (t[i] == 13) {
			char buf[40];
			int16 len = 0;
			uint32 k = start;

			while (k < i && t[k] == ' ')
				k++;
			for (; k < i && len < 39; k++)
				buf[len++] = (char)t[k];
			buf[len] = 0;
			if (len) {
				ui_text_c((int16)((x0 + x1) / 2), y, buf, pass ? UIC_TEXT : UIC_GOLD);
				pass = 1;
			}
			y = (int16)(y + ui_lh);
			start = i + 1;
		}
	}
}

/* DrawLevelPresentation: the level's shape, fitted to a box wide. */
SEG_UI void menus_shape(int16 field, int16 cx, int16 cy, int16 box)
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
