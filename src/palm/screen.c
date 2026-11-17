/* Screen setup and the framebuffer: best depth, double density or Sony
   HiRes, the input area, and an offscreen bitmap at native size and depth
   that the core draws into. plat_present copies it to the display. */
#define ALLOW_ACCESS_TO_INTERNALS_OF_WINDOWS
#include "palm.h"
#include "sonyhr.h"

static PlatScreen gScreen;
static Boolean gHaveScreen;
static WinHandle gOff;
static UInt8 *gDisp;               /* the display's bits, when frames are copied straight in */
static Char gError[40];

static UInt32 gOldDepth;
static Boolean gDepthSet;
static UInt16 gOldCoord;
static Boolean gNative;            /* drawing in the native (double density) system */
static Boolean gHaveNativeMode;

/* Sony HiRes (OS 4 CLIEs). */
static UInt16 gSonyRef;
static Boolean gSony, gSonyLoaded, gSonyOpen, gSonyMode;
static UInt32 gSonyWidth, gSonyHeight, gSonyDepth;
static Boolean gSonyColour;

/* Input area (PINS). */
static Boolean gPinsSet;
static UInt16 gOldInputArea, gOldTrigger;
static UInt16 gOldOrientation;
static Boolean gStatHidden;

const Char *screen_error(void)
{
	return gError;
}

static void set_error(const Char *s)
{
	StrNCopy(gError, s, sizeof(gError) - 1);
	gError[sizeof(gError) - 1] = 0;
}

const PlatScreen *plat_screen(void)
{
	return gHaveScreen ? &gScreen : NULL;
}

UInt16 screen_pen_scale(void)
{
	return (gSony || gHaveNativeMode) ? 2 : 1;
}

UInt16 screen_font_height(void)
{
	return (UInt16)(FntLineHeight() * screen_pen_scale());
}

/* Sony HR library, loaded here if it isn't already; removed on exit only if
   loaded here. */
static void sony_close(void)
{
	Err err = errNone;

	if (gSonyMode)
		HRWinScreenMode(gSonyRef, winScreenModeSet, &gSonyWidth, &gSonyHeight,
		                &gSonyDepth, &gSonyColour);
	if (gSonyOpen)
		err = HRClose(gSonyRef);
	if (gSonyLoaded && err == errNone)
		SysLibRemove(gSonyRef);
	gSony = gSonyLoaded = gSonyOpen = gSonyMode = false;
}

static Boolean sony_init(void)
{
	UInt16 version = 0;
	UInt32 w = 320, h = 320;
	Err err;

	if (gDev.winVersion >= 4 || (!gDev.sony && !gDev.sonyHR))
		return false;
	err = SysLibFind("Sony HR Library", &gSonyRef);
	if (err == sysErrLibNotFound) {
		err = SysLibLoad('libr', 'SlHr', &gSonyRef);
		gSonyLoaded = err == errNone;
	}
	if (err != errNone)
		return false;
	if (HROpen(gSonyRef) != errNone) {
		sony_close();
		return false;
	}
	gSonyOpen = true;
	if (HRGetAPIVersion(gSonyRef, &version) != errNone || version == 0 ||
	    HRWinScreenMode(gSonyRef, winScreenModeGet, &gSonyWidth, &gSonyHeight,
	                    &gSonyDepth, &gSonyColour) != errNone) {
		sony_close();
		return false;
	}
	if (HRWinScreenMode(gSonyRef, winScreenModeSet, &w, &h, NULL, NULL) != errNone) {
		sony_close();
		return false;
	}
	gSonyMode = true;
	gSony = true;
	return true;
}

/* Deepest colour depth if the screen has colour (16, else 8), else the
   deepest grey up to 4. */
static UInt32 best_depth(void)
{
	UInt32 m = gDev.depthMask;

	if (gDev.hasColor) {
		if (m & (1UL << 15))
			return 16;
		if (m & (1UL << 7))
			return 8;
	}
	if (m & (1UL << 3))
		return 4;
	if (m & (1UL << 1))
		return 2;
	return 1;
}

static void native_on(void)
{
	if (gHaveNativeMode && !gNative) {
		gOldCoord = WinSetCoordinateSystem(kCoordinatesNative);
		gNative = true;
	}
}

static void native_off(void)
{
	if (gHaveNativeMode && gNative) {
		WinSetCoordinateSystem(gOldCoord);
		gNative = false;
	}
}

/* Native display size, in pixels. */
static void display_extent(Coord *w, Coord *h)
{
	if (gSony)
		HRWinGetDisplayExtent(gSonyRef, w, h);
	else
		WinGetDisplayExtent(w, h);
}

/* Colour table for the palette depth: indices of 16 greys and 7 colours. */
static void make_palette(PlatScreen *s)
{
	static const UInt8 col[7][3] = {
		{ 255, 0, 0 }, { 0, 200, 0 }, { 0, 0, 255 }, { 255, 255, 0 },
		{ 0, 255, 255 }, { 255, 0, 255 }, { 255, 128, 0 }
	};
	RGBColorType c;
	UInt16 i;

	if (s->format != PLAT_PIX_PALETTE)
		return;
	c.index = 0;
	for (i = 0; i < 16; i++) {
		c.r = c.g = c.b = (UInt8)(i * 17);
		s->pal[i] = WinRGBToIndex(&c);
	}
	for (i = 0; i < 7; i++) {
		c.r = col[i][0];
		c.g = col[i][1];
		c.b = col[i][2];
		s->pal[16 + i] = WinRGBToIndex(&c);
	}
}

/* OS 3.0 to 3.3: no bitmap calls. The offscreen window's pixels are found
   through its WindowType and checked by drawing a pixel with the OS and
   reading it back. */
static Boolean bits_old(WinHandle w, Coord width, Coord height, UInt16 depth, PlatScreen *s)
{
	WindowType *wp = (WindowType *)w;
	UInt8 *p;
	UInt16 rb;
	WinHandle old;
	RectangleType probe;
	UInt8 want;
	Boolean ok;

	if (!wp->displayAddrV20 || wp->displayWidthV20 < width || wp->displayHeightV20 < height)
		return false;
	rb = (UInt16)((((UInt16)wp->displayWidthV20 * depth + 15) >> 4) << 1);
	p = (UInt8 *)wp->displayAddrV20;
	old = WinSetDrawWindow(w);
	probe.topLeft.x = 0;
	probe.topLeft.y = 0;
	probe.extent.x = 16;
	probe.extent.y = 2;
	WinEraseRectangle(&probe, 0);
	ok = p[0] == 0 && p[1] == 0 && p[rb] == 0 && p[rb + 1] == 0;
	WinDrawLine(1, 1, 1, 1);
	/* Pixel 1 of row 1: bit 6 at 1 bit, bits 5 and 4 at 2 bits, black is all ones. */
	want = depth == 1 ? 0x40 : 0x30;
	ok = ok && p[0] == 0 && p[rb] == want && p[rb + 1] == 0;
	WinEraseLine(1, 1, 1, 1);
	WinSetDrawWindow(old);
	if (!ok)
		return false;
	s->bits = p;
	s->row_bytes = (Int16)rb;
	return true;
}

/* On 68k devices before OS 5 the display bitmap is the screen itself, so
   frames are copied into it by copy_rows when it has the offscreen's size,
   depth and row bytes. Sony HiRes, double density and OS 5 keep
   WinCopyRectangle. */
static void find_display_bits(void)
{
	const PlatScreen *s = &gScreen;
	WinHandle d = WinGetDisplayWindow();
	UInt8 *bits = NULL;
	Coord w = 0, h = 0;
	UInt16 rb = 0, depth = 0;

	gDisp = NULL;
	if (!d || gDev.rom >= kRom50 || gSony || gHaveNativeMode || !sysFtrNumProcessorIs68K(gDev.cpu))
		return;
	if (gDev.rom >= kRom35) {
		BitmapType *bm = WinGetBitmap(d);

		if (!bm)
			return;
		if (gDev.rom >= kRom40) {
			BmpGetDimensions(bm, &w, &h, &rb);
			depth = BmpGetBitDepth(bm);
		} else {
			const UInt8 *hd = (const UInt8 *)bm;

			w = (Coord)((hd[0] << 8) | hd[1]);
			h = (Coord)((hd[2] << 8) | hd[3]);
			rb = (UInt16)((hd[4] << 8) | hd[5]);
			depth = hd[8] ? hd[8] : 1;
		}
		bits = BmpGetBits(bm);
	} else {
		WindowType *wp = (WindowType *)d;

		bits = (UInt8 *)wp->displayAddrV20;
		w = wp->displayWidthV20;
		h = wp->displayHeightV20;
		depth = s->depth;
		rb = (UInt16)((((UInt16)w * depth + 15) >> 4) << 1);
	}
	if (bits && w == s->width && h == s->height && depth == s->depth && (Int16)rb == s->row_bytes)
		gDisp = bits;
}

static void free_offscreen(void)
{
	gHaveScreen = false;
	gDisp = NULL;
	if (gOff) {
		WinDeleteWindow(gOff, false);
		gOff = NULL;
	}
}

/* Builds the offscreen window and fills gScreen. */
static Boolean make_offscreen(void)
{
	UInt16 err = errNone;
	UInt32 depth = 1;
	Coord w, h;
	PlatScreen *s = &gScreen;

	free_offscreen();
	native_on();
	display_extent(&w, &h);
	if (gDev.rom >= kMinRomVersion)
		WinScreenMode(winScreenModeGet, NULL, NULL, &depth, NULL);
	if (gSony)
		gOff = HRWinCreateOffscreenWindow(gSonyRef, w, h, screenFormat, &err);
	else
		gOff = WinCreateOffscreenWindow(w, h, gNative ? nativeFormat : screenFormat, &err);
	native_off();
	if (!gOff || err != errNone) {
		gOff = NULL;
		set_error("no offscreen window");
		return false;
	}
	MemSet(s, sizeof(*s), 0);
	s->width = w;
	s->height = h;
	s->depth = (UInt8)depth;
	if (gDev.rom >= kRom35) {
		BitmapType *bm = WinGetBitmap(gOff);
		Coord bw, bh;
		UInt16 rb;

		if (gDev.rom >= kRom40) {
			BmpGetDimensions(bm, &bw, &bh, &rb);
			s->depth = BmpGetBitDepth(bm);
		} else {
			/* OS 3.5 has no accessors: width, height, rowBytes, flags
			   (2 bytes each), then pixel size, at the start of the bitmap. */
			const UInt8 *hd = (const UInt8 *)bm;

			bw = (Coord)((hd[0] << 8) | hd[1]);
			bh = (Coord)((hd[2] << 8) | hd[3]);
			rb = (UInt16)((hd[4] << 8) | hd[5]);
			s->depth = hd[8] ? hd[8] : 1;
		}
		s->bits = BmpGetBits(bm);
		s->row_bytes = (Int16)rb;
		if (bw != w || bh != h || !s->bits) {
			set_error("offscreen size differs");
			free_offscreen();
			return false;
		}
	} else if (!bits_old(gOff, w, h, (UInt16)depth, s)) {
		set_error("no direct pixel access");
		free_offscreen();
		return false;
	}
	/* On the ARM devices 16 bit pixels are stored low byte first, even to PACE. */
	s->le16 = s->depth == 16 && sysFtrNumProcessorIsARM(gDev.cpu);
	s->format = s->depth == 16 ? PLAT_PIX_RGB565 : s->depth == 8 ? PLAT_PIX_PALETTE : PLAT_PIX_GREY;
	make_palette(s);
	plat_layout(s);
	find_display_bits();
	gHaveScreen = true;
	return true;
}

/* Input area closed, status bar hidden, and a form that fills the display. */
static void pins_on(void)
{
	if (gDev.pinsVersion && !gPinsSet) {
		gOldInputArea = PINGetInputAreaState();
		gOldTrigger = PINGetInputTriggerState();
		PINSetInputTriggerState(pinInputTriggerDisabled);
		PINSetInputAreaState(pinInputAreaClosed);
		if (StatHide() == errNone)
			gStatHidden = true;
		if (gDev.rotation)
			gOldOrientation = SysGetOrientation();
		gPinsSet = true;
	}
}

static void pins_off(void)
{
	if (!gPinsSet)
		return;
	if (gStatHidden)
		StatShow();
	gStatHidden = false;
	if (gDev.rotation)
		SysSetOrientation(gOldOrientation);
	PINSetInputTriggerState(gOldTrigger);
	PINSetInputAreaState(gOldInputArea);
	gPinsSet = false;
}

#ifdef TEST_TURN
/* Test build: a pen down in the top left corner (8 standard pixels) turns
   the screen, in the top right corner it opens or closes the input area,
   as a user could on a T|X, mid-game or paused. */
Boolean screen_test_turn(EventPtr e)
{
	Coord w, h;

	if (e->eType != penDownEvent || !gDev.pinsVersion || e->screenY >= 8)
		return false;
	WinGetDisplayExtent(&w, &h);
	if (e->screenX < 8 && gDev.rotation) {
		SysSetOrientation(SysGetOrientation() == sysOrientationPortrait ?
		                  sysOrientationLandscape : sysOrientationPortrait);
		return true;
	}
	if (e->screenX >= w - 8) {
		PINSetInputAreaState(PINGetInputAreaState() == pinInputAreaOpen ?
		                     pinInputAreaClosed : pinInputAreaOpen);
		return true;
	}
	return false;
}
#endif

void screen_form(FormPtr frm)
{
	RectangleType r;

	if (gDev.rom < kRom35)
		return;
	/* The input area and status bar act on the active form, so they are
	   closed here, once the form is open, and the offscreen remade at the
	   new size. Before any form is open the T|X writes through NULL. */
	if (gDev.pinsVersion && frm) {
		FrmSetDIAPolicyAttr(frm, frmDIAPolicyCustom);
		if (!gPinsSet) {
			pins_on();
#ifdef TEST_ROTATE
			/* Test build: turn to landscape to try the display change path. */
			if (gDev.rotation)
				SysSetOrientation(sysOrientationLandscape);
#endif
			screen_rebuild();
		} else if (gStatHidden) {
			/* A turn of the screen brings the status bar back; hiding it
			   again sends another display change, which lands here too. */
			StatHide();
		}
	}
	r.topLeft.x = 0;
	r.topLeft.y = 0;
	native_off();
	WinGetDisplayExtent(&r.extent.x, &r.extent.y);
	WinSetBounds(FrmGetWindowHandle(frm), &r);
}

Boolean screen_init(void)
{
	UInt32 depth;

	device_detect(&gDev);
	gError[0] = 0;
	if (gDev.rom < kMinRomVersion) {
		set_error("needs OS 3.0");
		return false;
	}
	depth = best_depth();
	if (WinScreenMode(winScreenModeGet, NULL, NULL, &gOldDepth, NULL) == errNone &&
	    WinScreenMode(winScreenModeSet, NULL, NULL, &depth, NULL) == errNone)
		gDepthSet = true;
	/* Double density: draw in the native system. */
	gHaveNativeMode = gDev.winVersion >= 4 && gDev.density == kDensityDouble;
	if (!gHaveNativeMode)
		sony_init();
	if (!make_offscreen()) {
		screen_exit();
		return false;
	}
	/* The extent now counts the native pixels. */
	gDev.width = gScreen.width;
	gDev.height = gScreen.height;
	gDev.landscape = gDev.width > gDev.height;
	gDev.dana = (UInt32)gDev.width > 3UL * gDev.height;
	return true;
}

Boolean screen_rebuild(void)
{
	Boolean ok = make_offscreen();

	if (ok) {
		gDev.width = gScreen.width;
		gDev.height = gScreen.height;
		gDev.landscape = gDev.width > gDev.height;
	}
	return ok;
}

void screen_exit(void)
{
	free_offscreen();
	native_off();
	pins_off();
	if (gDepthSet) {
		UInt32 depth = gOldDepth;

		WinScreenMode(winScreenModeSet, NULL, NULL, &depth, NULL);
		gDepthSet = false;
	}
	sony_close();
	gHaveNativeMode = false;
}

void plat_present(void)
{
	RectangleType r;

	if (!gHaveScreen)
		return;
	r.topLeft.x = 0;
	r.topLeft.y = 0;
	r.extent.x = gScreen.width;
	r.extent.y = gScreen.height;
	if (gSony) {
		HRWinCopyRectangle(gSonyRef, gOff, WinGetDisplayWindow(), &r, 0, 0, winPaint);
		return;
	}
	native_on();
	WinCopyRectangle(gOff, WinGetDisplayWindow(), &r, 0, 0, winPaint);
	native_off();
}

/* Copies h rows of n bytes (n even, both pointers even) rb apart, as longs:
   MemMove on OS 3.1 takes about 20 ms for a 160x160 2 bit screen. */
static void copy_rows(UInt8 *d, const UInt8 *s, Int16 n, Int16 h, Int16 rb)
{
	Int16 y, i;

	if (n <= 0)
		return;
	for (y = 0; y < h; y++, d += rb, s += rb) {
		UInt32 *dl = (UInt32 *)d;
		const UInt32 *sl = (const UInt32 *)s;

		for (i = (Int16)(n >> 2); i >= 4; i -= 4) {
			dl[0] = sl[0]; dl[1] = sl[1]; dl[2] = sl[2]; dl[3] = sl[3];
			dl += 4;
			sl += 4;
		}
		while (i-- > 0)
			*dl++ = *sl++;
		if (n & 2)
			*(UInt16 *)dl = *(const UInt16 *)sl;
	}
}

void plat_present_rect(const PlatRect *pr)
{
	RectangleType r;

	if (!gHaveScreen)
		return;
	if (gDisp) {
		Int16 rb = gScreen.row_bytes;
		Int16 b0 = (Int16)(((Int32)pr->x * gScreen.depth) >> 3);
		Int16 b1 = (Int16)(((Int32)(pr->x + pr->w) * gScreen.depth + 7) >> 3);
		Int32 at = (Int32)pr->y * rb;

		if (b1 > rb)
			b1 = rb;
		/* Whole words, so the copy can go by longs (the 68k only needs words
		   aligned). */
		b0 &= ~1;
		b1 = (Int16)((b1 + 1) & ~1);
		copy_rows(gDisp + at + b0, gScreen.bits + at + b0, (Int16)(b1 - b0), pr->h, rb);
		return;
	}
	r.topLeft.x = pr->x;
	r.topLeft.y = pr->y;
	r.extent.x = pr->w;
	r.extent.y = pr->h;
	if (gSony) {
		HRWinCopyRectangle(gSonyRef, gOff, WinGetDisplayWindow(), &r, pr->x, pr->y, winPaint);
		return;
	}
	native_on();
	WinCopyRectangle(gOff, WinGetDisplayWindow(), &r, pr->x, pr->y, winPaint);
	native_off();
}

void screen_text(const Char *s, Coord x, Coord y)
{
	WinHandle old = WinSetDrawWindow(WinGetDisplayWindow());
	FontID font = FntGetFont();
	UInt16 len = StrLen(s);

	if (gSony) {
		HRFntSetFont(gSonyRef, hrStdFont);
		HRWinDrawChars(gSonyRef, s, len, x, y);
	} else {
		native_on();
		WinDrawChars(s, len, x, y);
		native_off();
	}
	FntSetFont(font);
	WinSetDrawWindow(old);
}

/* WinRGBToIndex is there from OS 3.5, as is every 8 bit screen. */
uint8 plat_palette_index(uint8 r, uint8 g, uint8 b)
{
	RGBColorType c;

	if (gDev.rom < kRom35)
		return 0;
	c.index = 0;
	c.r = r;
	c.g = g;
	c.b = b;
	return (uint8)WinRGBToIndex(&c);
}
