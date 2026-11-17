/* Shared declarations for the Palm backend. */
#ifndef PALM_H
#define PALM_H

#include <PalmOS.h>
#include "../core/plat.h"

#define kMinRomVersion  sysMakeROMVersion(3, 0, 0, sysROMStageRelease, 0)
#define kRom35          sysMakeROMVersion(3, 5, 0, sysROMStageRelease, 0)
#define kRom40          sysMakeROMVersion(4, 0, 0, sysROMStageRelease, 0)
#define kRom50          sysMakeROMVersion(5, 0, 0, sysROMStageRelease, 0)

/* What device_detect finds out. Never read from a device name. */
typedef struct {
	UInt32 rom;          /* sysFtrNumROMVersion */
	UInt32 cpu;          /* sysFtrNumProcessorID */
	UInt32 company;      /* sysFtrNumOEMCompanyID, 0 before OS 3.5 */
	UInt32 winVersion;   /* sysFtrNumWinVersion, 0 before OS 4 */
	UInt32 depthMask;    /* bit n-1 set when depth n is supported */
	UInt32 density;      /* screen density: 72, 144 or 0 if unknown */
	UInt32 pinsVersion;  /* pinFtrAPIVersion, 0 without PINS */
	UInt16 ticksPerSecond;
	Coord  width, height; /* display extent in native pixels (Sony HR counted) */
	Boolean hasColor;
	Boolean sony;        /* Sony feature present */
	Boolean sonyHR;      /* Sony HR library present */
	Boolean jog;         /* Sony Jog Dial (taken from the Sony feature) */
	Boolean nav;         /* palmOne 5-way navigator */
	Boolean zodiac;      /* Tapwave Zodiac */
	Boolean rotation;    /* the screen can turn (PINS 1.1 or later) */
	Boolean landscape;   /* it is wider than tall now */
	Boolean sampled;     /* SndStream: sampled sound */
	Boolean dana;        /* wider than 3:1 */
} PalmDevice;

extern PalmDevice gDev;

SEG_DEV void device_detect(PalmDevice *d);

/* Lets go of the sound resources (sound.c). */
void sound_exit(void);

/* Screen. screen_init sets the depth, density or Sony HiRes
   and builds the offscreen bitmap; screen_exit puts it all back.
   screen_rebuild runs again after a winDisplayChangedEvent. */
Boolean screen_init(void);
void screen_exit(void);
Boolean screen_rebuild(void);
/* Prepares a form for the full screen: closes the input area and hides the
   status bar the first time, then sets its bounds. */
void screen_form(FormPtr frm);
#ifdef TEST_TURN
Boolean screen_test_turn(EventPtr e);
#endif
/* Draws a line of text onto the screen after plat_present, at x, y in
   native pixels. */
void screen_text(const Char *s, Coord x, Coord y);
/* Native pixels per standard pixel (1 or 2) for fonts and pen events. */
UInt16 screen_pen_scale(void);
UInt16 screen_font_height(void);
const Char *screen_error(void);

/* Input. input_init masks the hard buttons, input_exit puts the mask back.
   input_event returns true if the event is used up. */
void input_init(void);
void input_exit(void);
Boolean input_event(EventPtr e);
/* The last pen event's coordinates as the OS gave them (for the test pattern). */
void input_last_key(UInt16 *chr, UInt16 *code, UInt16 *mods, Int16 *count);
void input_last_raw(Int16 *x, Int16 *y, Int16 *count);

#endif
