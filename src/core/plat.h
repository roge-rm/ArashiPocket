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

/* Data: the original resource bytes, as extracted by tools/rsrc.py (same
   type and ID as the Mac resource fork). Returns NULL if missing. The bytes
   stay valid until plat_release. */
const uint8 *plat_resource(uint32 type, int16 id, uint32 *size);
void plat_release(const uint8 *data);

/* Time: a free-running tick counter and its rate. */
uint32 plat_ticks(void);
uint32 plat_ticks_per_second(void);

/* Screen (later). The backend owns the framebuffer; the core draws into it
   and calls plat_present to put it on screen. */
enum {
	PLAT_PIX_GREY,      /* 1, 2 or 4 bit grey, 0 is white */
	PLAT_PIX_PALETTE,   /* 8 bit palette index */
	PLAT_PIX_RGB565     /* 16 bit */
};

typedef struct {
	uint8 *bits;
	int16  width, height;
	int16  row_bytes;
	uint8  depth;       /* bits per pixel: 1, 2, 4, 8 or 16 */
	uint8  format;      /* PLAT_PIX_* */
	int16  play_x, play_y, play_size;  /* square play area */
} PlatScreen;

const PlatScreen *plat_screen(void);
void plat_present(void);

/* Input (later): game actions plus an optional absolute rim position. */
enum {
	PLAT_ACT_LEFT   = 0x0001,
	PLAT_ACT_RIGHT  = 0x0002,
	PLAT_ACT_FIRE   = 0x0004,
	PLAT_ACT_ZAP    = 0x0008,
	PLAT_ACT_PAUSE  = 0x0010,
	PLAT_ACT_MENU   = 0x0020
};

typedef struct {
	uint16 actions;     /* PLAT_ACT_* held down */
	int16  rim;         /* rim position 0..255 from stylus or stick, -1 if none */
	int16  spin;        /* relative steps from a Jog Dial or similar */
} PlatInput;

void plat_input(PlatInput *in);

/* Saves (later): a named blob. plat_load returns the bytes read, or -1. */
cbool plat_save(const char *name, const void *data, uint32 size);
int32 plat_load(const char *name, void *data, uint32 size);

/* Sound (later): the backend picks tones or samples for each event. */
enum {
	PLAT_SND_SHOT,
	PLAT_SND_EXPLODE,
	PLAT_SND_ZAP,
	PLAT_SND_FLYTHRU,
	PLAT_SND_FREELIFE,
	PLAT_SND_DEATH
};

void plat_sound(int16 event);

#endif
