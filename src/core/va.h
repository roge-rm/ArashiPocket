/* Vector renderer: the VA Kit calls the game uses (VALine, VAPixel, VASpot,
   VADrawNumber, VADrawText, VAExplosion, VAStep), drawing into the plat.h
   framebuffer at its native size and depth.

   Coordinates are play-area pixels: VA.frame is 0, 0, size, size, the
   square plat_layout picks, and the game scales its geometry to it as it
   did to the Mac window. Drawing calls queue into a display list; VAStep
   erases the previous frame by drawing its list again in the background
   colour, draws the new list and presents. Everything is clipped to the
   frame.

   Colours are the original's: 0 to 6 are the game colours, BG1, BG2 and
   BGC (or -1) the three background colours, all from a CLOT resource. */
#ifndef CORE_VA_H
#define CORE_VA_H

#include "plat.h"

#define ANGLES      120         /* one turn, as Sins and Cosins use */
#define NUMCOLORS   10
#define BGC         255
#define BG2         (255 - 64)
#define BG1         (255 - 128)

/* Display list sizes for each of the two frames. */
#define VA_MAXLINES 700
#define VA_MAXPIX   1000
#define VA_MAXSPOTS 200

/* Size of the frame the explosion tables (GNRL 1000 to 1003) were made
   for: 480 rows on a 640x480 Mac. They are scaled from this to the play
   area. */
#define VA_REF_SIZE 480

typedef struct {
	int16 left, top, right, bottom;
} VARect;

/* How VAStep erases the last frame: whichever is cheaper, always by
   drawing it again in the background, or always by filling the box round
   it. */
enum { VA_ERASE_AUTO, VA_ERASE_REDRAW, VA_ERASE_BOX };

typedef struct {
	VARect frame;           /* 0, 0, play size, play size */
	int16  color;           /* colour for the next drawing call */
	int16  segmscale;       /* text and number size */
	int16  CurrentX, CurrentY;
	uint32 FrameCounter;
	uint32 DrawTicks;       /* plat_ticks spent erasing and drawing, and */
	uint32 PresentTicks;    /* in plat_present, summed over all VASteps */
	uint32 EraseTicks;      /* parts of DrawTicks: erasing, and */
	uint32 GlyphTicks;      /* drawing this frame's glyphs */
	cbool  overflow;        /* a list filled up this frame */
	int16  EraseMode;       /* VA_ERASE_*, for tests */
} VAGlobalsRecord;

extern VAGlobalsRecord VA;

/* sin and cos of a * 2 pi / ANGLES, times 256 (0 <= a < ANGLES). */
extern int16 Sins[ANGLES];
extern int16 Cosins[ANGLES];

#define VAMoveTo(x, y) { VA.CurrentX = (x); VA.CurrentY = (y); }

/* Allocates the lists, loads CLOT 1000 and the explosions, and clears the
   screen. Needs plat_screen. Returns 0 when out of memory or no screen. */
SEG_VA cbool VAInit(void);
SEG_VA void  VAClose(void);
/* After the backend remade its screen (display change, form back on top):
   picks up the new size and depth, drops both lists and clears the screen
   to the background. Returns 0 if there's no screen. */
SEG_VA cbool VAScreenChanged(void);

/* Sets the 10 colours from a CLOT resource (10 ColorSpecs of 8 bytes). */
SEG_VA void  VALoadColors(int16 clot_id);
SEG_VA void  VASetColors(const uint8 *clot, uint32 size);
/* The pixel value colour c draws with on this screen (for tests). */
SEG_VA uint16 VAPixelValue(int16 c);

SEG_VA void VAEraseBuffer(void);
/* After VASetColors changed the background: fills the screen with it and
   draws the frame on screen again. */
SEG_VA void VARepaint(void);
/* Drawing goes to the status area (VA.frame is its size) until
   VAViewPlay. VAViewStatus does nothing if there is no status area. */
SEG_VA void VAViewStatus(void);
SEG_VA void VAViewPlay(void);
/* Drawing goes to x, y, w, h of the framebuffer (VA.frame is w by h) until
   VAViewPlay. */
SEG_VA void VAViewRect(int16 x, int16 y, int16 w, int16 h);
/* Draws what this frame has queued so far into the framebuffer and drops
   it, so it stays as a still picture that no VAStep erases. */
SEG_VA void VABake(void);
/* The still picture: lines that stay the same from frame to frame (the
   field in play). Each frame that has one calls VAStillBegin first, before
   queueing anything, with a key that changes when the picture does. When
   it returns 1 the caller queues the picture and calls VAStillEnd; when 0
   the picture is already on screen and isn't queued. The first time it is
   drawn into a copy of the screen kept with plat_keep_alloc and erasing
   puts that copy back, so it is never drawn or erased again. A frame
   without VAStillBegin drops it. Anything that clears or paints the screen
   (VAEraseBuffer, VARepaint, VAForget) makes the next frame draw it anew.
   With no room for the copy it is drawn every frame as before. */
SEG_VA cbool VAStillBegin(uint16 key);
SEG_VA void  VAStillEnd(void);
/* Forgets the frame on screen, after something painted over it, so the
   next VAStep doesn't erase it. */
SEG_VA void VAForget(void);
/* The box of the frame on screen, in framebuffer pixels. 0 if it is empty. */
SEG_VA cbool VAShown(PlatRect *r);
SEG_VA void VALine(int16 x1, int16 y1, int16 x2, int16 y2);
SEG_VA void VALineTo(int16 x, int16 y);
SEG_VA void VASafeLineTo(int16 x, int16 y);
SEG_VA void VAPixel(int16 x, int16 y);
SEG_VA void VASafePixel(int16 x, int16 y);
SEG_VA void VASpot(int16 x, int16 y);
SEG_VA void VASafeSpot(int16 x, int16 y);
/* Right-aligned: the last digit at x, bottom at y, going left. */
SEG_VA void VADrawNumber(int32 num, int16 x, int16 y);
SEG_VA void VADrawPadNumber(int32 num, int16 x, int16 y, int16 width);
/* Characters a to b - 1 of p at VA.CurrentX, CurrentY (the bottom),
   moving CurrentX on. */
SEG_VA void VADrawText(const char *p, int32 a, int32 b);
/* size 0 to 3. */
SEG_VA void VAExplosion(int16 x, int16 y, int16 size, int16 color);

/* Advances explosions, erases the last frame, draws this one and
   presents. */
SEG_VA void VAStep(void);
/* Drops this frame's list without drawing; explosions still advance. */
SEG_VA void VACancelFrame(void);

/* Between va.c and vatext.c only. A glyph whose box is inside the frame
   goes in the display list as one item and is turned into lines when it
   is drawn. */
typedef struct {
	int16 x1, y1, x2, y2;
} VASeg;

#define VA_MAXGLYPHS 160
#define VA_GLYPH_SEGS 8     /* most strokes in a glyph */

/* c is 32 to 95, left and bottom in play-area pixels. */
SEG_VA void  va_queue_glyph(int16 c, int16 left, int16 bottom, int16 w, int16 h);
/* The strokes of glyph c, each running left to right, into out; returns
   how many. */
SEG_VA int16 va_glyph_segs(int16 c, int16 left, int16 bottom, int16 w, int16 h, VASeg *out);

#endif
