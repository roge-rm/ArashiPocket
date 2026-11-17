/* Menu drawing: filled boxes, rounded buttons, the menu font and the logo,
   painted straight into the plat.h framebuffer in screen pixels (ui.c), and
   the front end's screens built from them (menus.c). Both in the ui
   segment. The art comes from resources made by tools/mkui.py: UIFT 128
   and 129 (the font at low and double density), UIBM 128 and 129 (the
   logo). Nothing here holds more than a few bytes of heap. */
#ifndef CORE_UI_H
#define CORE_UI_H

#include "plat.h"

#define ARASHI_VERSION "0.1"   /* as VERSION in src/palm/arashi.rcp */

/* Menu colours. Each has an RGB for colour screens and a grey picked by
   hand for 1, 2 and 4 bit screens, so the panels and the button states
   stay apart on every depth. */
enum {
	UIC_BG,         /* black, as the game's background */
	UIC_PANEL,      /* navy panel under the buttons and lists */
	UIC_STRIPE,     /* every other list row */
	UIC_BAR,        /* title and footer bars */
	UIC_ACCENT,     /* red line under the title bar */
	UIC_LINE,       /* cyan button outline */
	UIC_BTN,        /* inside of a button */
	UIC_SEL,        /* the selected button, claw yellow */
	UIC_TEXT,
	UIC_DIM,
	UIC_INK,        /* text on UIC_SEL */
	UIC_HEAD,       /* list header band */
	UIC_HEADTXT,    /* text on the header band */
	UIC_GOLD,       /* yellow text */
	UIC_COUNT
};

/* Screen size, scale (1 low density, 2 for screens 320 or more each way),
   line height and button height, set by ui_begin. */
extern int16 ui_w, ui_h, ui_s, ui_lh, ui_bh;

/* Starts a paint: reads the screen, picks the scale, loads the font and
   works out the colours. 0 when there is no screen or font. */
SEG_UI cbool ui_begin(void);
SEG_UI void  ui_end(void);
/* Puts the whole framebuffer on screen. */
SEG_UI void  ui_present(void);
/* Scale and size only (no font), for layouts worked out outside a paint. */
SEG_UI void  ui_metrics(void);

SEG_UI void  ui_fill(int16 x, int16 y, int16 w, int16 h, int16 c);
/* A rounded box filled with fill, with an outline ui_s wide in edge
   (edge < 0: none). */
SEG_UI void  ui_box(int16 x, int16 y, int16 w, int16 h, int16 fill, int16 edge);
/* Text with the top of the capitals at y. Returns the width. */
SEG_UI int16 ui_text(int16 x, int16 y, const char *s, int16 c);
SEG_UI int16 ui_text_w(const char *s);
SEG_UI void  ui_text_c(int16 cx, int16 y, const char *s, int16 c);
SEG_UI void  ui_text_r(int16 rx, int16 y, const char *s, int16 c);
/* Text centred in a box of height h at y. */
SEG_UI int16 ui_text_y(int16 y, int16 h);
/* The logo, centred on cx with its top at y. Returns its height. */
SEG_UI int16 ui_logo(int16 cx, int16 y);
SEG_UI void  ui_logo_size(int16 *w, int16 *h);

/* Tap zones, set as a screen is painted: id at x, y, w, h. */
#define UI_ZONES 10
SEG_UI void  ui_zones_clear(void);
SEG_UI void  ui_zone(int16 id, int16 x, int16 y, int16 w, int16 h);
/* The zone at screen point x, y, or -1. */
SEG_UI int16 ui_zone_at(int16 x, int16 y);
SEG_UI cbool ui_zone_rect(int16 id, PlatRect *r);

/* ---- the screens (menus.c) ---- */

/* Zone ids beside the button numbers 0 to 9. */
enum {
	UIZ_PREV = 20, UIZ_NEXT, UIZ_BACK, UIZ_OK, UIZ_AREA, UIZ_NAME, UIZ_DEL
};

/* Where each screen's moving part goes (screen pixels): the carousel, the
   name wheel, the GAME OVER band, the title's play area, the credit box. */
SEG_UI void menus_area(int16 screen, PlatRect *r);
/* Rows on a high score page (with the footer or the attract page). */
SEG_UI int16 menus_score_rows(cbool footer);

SEG_UI void menus_title(void);
SEG_UI void menus_main(const char *const *labels, int16 n, int16 sel);
SEG_UI void menus_options(int16 sel, cbool drag, cbool practice, cbool sound);
SEG_UI void menus_about(void);
/* field and clot: the level's shape and colours, for the picture. */
SEG_UI void menus_pause(int16 sel, cbool drag, int16 level, int16 field, int16 clot, int32 score);
SEG_UI void menus_levels(int16 level, int32 bonus, int16 sel);
/* page and the row to highlight (-1 for none). */
SEG_UI void menus_scores(cbool practice, int16 page, int16 hl, cbool footer);
SEG_UI void menus_over(int32 score, int16 level);
SEG_UI void menus_name(int32 score, const char *name, int16 len, int16 max);
/* A credit page: lines from TEXT, filled in once the box has zoomed in. */
SEG_UI void menus_text(const uint8 *t, uint32 n, cbool inside);

/* The level shape from SPCE field, fitted to a box wide at cx, cy, queued
   to the renderer in VA.color. */
SEG_UI void menus_shape(int16 field, int16 cx, int16 cy, int16 box);

#endif
