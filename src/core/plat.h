/* The only interface between the core and a platform backend (src/palm,
   src/host, later src/wince). The backend implements these; the core calls
   nothing else outside itself. Functions marked "later" are declared for the
   milestone that needs them and have no implementation yet. */
#ifndef CORE_PLAT_H
#define CORE_PLAT_H

#include "types.h"

/* Memory, sized for the IIIx dynamic heap. plat_alloc returns NULL when out
   of memory. Blocks don't move. */
void *plat_alloc(uint32 size);
void  plat_free(void *p);

/* One buffer of size bytes kept out of the dynamic heap where that is short
   (Palm: a record in the storage heap). Read it through the pointer, write
   it only with plat_keep_write. NULL when there is no room. One at a time. */
const uint8 *plat_keep_alloc(uint32 size);
void plat_keep_write(const uint8 *keep, uint32 at, const void *src, uint32 n);
void plat_keep_free(const uint8 *keep);
/* A buffer for tables that are written seldom and read often (the grids):
   in storage on Palm so it costs no dynamic heap, written only with
   plat_store_write. NULL when there is no room. One at a time. */
const uint8 *plat_store_alloc(uint32 size);
void plat_store_write(const uint8 *p, uint32 at, const void *src, uint32 n);
void plat_store_free(const uint8 *p);

/* Data: the original resource bytes, as extracted by tools/rsrc.py (same
   type and ID as the Mac resource fork). Returns NULL if missing. The bytes
   stay valid until plat_release. */
const uint8 *plat_resource(uint32 type, int16 id, uint32 *size);
void plat_release(const uint8 *data);

/* Time: a free-running tick counter and its rate. */
uint32 plat_ticks(void);
uint32 plat_ticks_per_second(void);

/* Screen. The backend owns the framebuffer, at the screen's native size and
   depth; the core draws into it and calls plat_present to put it on screen.
   Pixels: PLAT_PIX_GREY holds 1, 2 or 4 bit values, leftmost pixel in the
   high bits, 0 is white. PLAT_PIX_PALETTE is one byte per pixel, an index
   into the backend's palette; pal[] holds indices for 16 greys from black
   (pal[0]) to white (pal[15]) and then red, green, blue, yellow, cyan,
   magenta and orange (pal[16] to pal[22]). PLAT_PIX_RGB565 is two bytes per
   pixel, high byte first unless le16 is set. */
enum {
	PLAT_PIX_GREY,
	PLAT_PIX_PALETTE,
	PLAT_PIX_RGB565
};

typedef struct {
	int16 x, y, w, h;
} PlatRect;

typedef struct {
	uint8 *bits;
	int16  width, height;
	int16  row_bytes;
	uint8  depth;       /* bits per pixel: 1, 2, 4, 8 or 16 */
	uint8  format;      /* PLAT_PIX_* */
	uint8  le16;        /* 16 bit pixels are stored low byte first */
	uint8  pal[23];
	PlatRect play;      /* square play area */
	PlatRect status;    /* score and status, w or h 0 if there's no room */
} PlatScreen;

/* NULL while the backend has no screen (before setup, or while it rebuilds
   one after a display change). */
const PlatScreen *plat_screen(void);
void plat_present(void);
/* The same for part of the framebuffer only (framebuffer pixels). */
void plat_present_rect(const PlatRect *r);

/* On PLAT_PIX_PALETTE screens, the index of the palette colour nearest to
   r, g, b. */
uint8 plat_palette_index(uint8 r, uint8 g, uint8 b);

/* Fills play and status from width and height (layout.c): the largest
   square, at the top or left with the spare space as the status area when
   at least a quarter of the square is left over, else centred. */
SEG_PAT void plat_layout(PlatScreen *s);

/* Rim position 0..255 (0 straight up, clockwise) of a point x, y in a play
   area of the given size, seen from its centre. -1 at the centre. */
SEG_PAT int16 plat_rim_from_point(int16 x, int16 y, int16 size);

/* Input: game actions held, plus an absolute stylus position and rim. The
   backend turns buttons, stylus, 5-way, Jog Dial or keyboard into these. */
enum {
	PLAT_ACT_LEFT   = 0x0001,
	PLAT_ACT_RIGHT  = 0x0002,
	PLAT_ACT_FIRE   = 0x0004,
	PLAT_ACT_ZAP    = 0x0008,   /* superzap */
	PLAT_ACT_PAUSE  = 0x0010,   /* set once per press */
	PLAT_ACT_MENU   = 0x0020    /* set once per press */
};

typedef struct {
	uint16 actions;     /* PLAT_ACT_* held down */
	int16  rim;         /* rim position 0..255 while the stylus is down, else -1 */
	int16  spin;        /* relative steps from a Jog Dial since the last call */
	cbool  pen_down;
	cbool  pen_seen;    /* pen_x and pen_y are valid */
	int16  pen_x, pen_y; /* last stylus point, relative to the play area
	                        (can be outside it) */
	cbool  tapped;      /* the stylus was lifted since the last call, even if it
	                       was down and up between two calls, at tap_x, tap_y
	                       (as pen_x, pen_y). A backend may leave this 0, the
	                       front end also notices pen_down going false. */
	int16  tap_x, tap_y;
} PlatInput;

void plat_input(PlatInput *in);

/* Saves (later): a named blob. plat_load returns the bytes read, or -1. */
cbool plat_save(const char *name, const void *data, uint32 size);
int32 plat_load(const char *name, void *data, uint32 size);

/* The original's 18 sounds, in the order of its sound table (SKIT 1001).
   plat_sound plays one: the backend picks a tone or the sample. priority is
   the original's (higher cuts in on lower, 999 for the claw's death). */
enum {
	SND_BLAST, SND_BLOW, SND_BONK, SND_DZIUNG, SND_PHAZERIN, SND_PHAZEROUT,
	SND_FALLYELL, SND_FALLZAP, SND_FLYTHRU, SND_PHAZER2, SND_SPLITTER,
	SND_SPRINGY, SND_SWISH, SND_SWOOSH, SND_WHIZ, SND_ZROOM, SND_ZZFREELIFE,
	SND_ZZSUPERZAP, SND_COUNT
};

void plat_sound(int16 snd, int16 priority);

#endif
