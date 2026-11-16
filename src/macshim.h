/* Mac Toolbox types and calls used by the Arashi game sources, for
   m68k-palmos-gcc. Force-included (-include) when compiling the jam files,
   so they compile as written. Real implementations of the stubbed calls
   come in later milestones (see macstubs.c). int is 16 bit (-mshort). */
#ifndef MACSHIM_H
#define MACSHIM_H

typedef unsigned char Boolean;
typedef char *Ptr;
typedef Ptr *Handle;
typedef long Fixed;
typedef long Fract;
typedef long OSErr;
typedef unsigned char Str255[256];
typedef unsigned char *StringPtr;
typedef const unsigned char *ConstStr255Param;
typedef unsigned long OSType;
typedef unsigned long ResType;
typedef short INTEGER;

#define true 1
#define false 0
#define nil 0
#ifndef NULL
#define NULL 0
#endif


typedef struct { short v, h; } Point;
typedef struct { short top, left, bottom, right; } Rect;
typedef Rect *RectPtr;

#define pascal
#define TRUE 1
#define FALSE 0

typedef struct { short rgnSize; Rect rgnBBox; } Region;
typedef Region **RgnHandle;
typedef struct { short picSize; Rect picFrame; } Picture;
typedef Picture **PicHandle;
typedef struct { short polySize; Rect polyBBox; Point polyPoints[1]; } Polygon;
typedef Polygon **PolyHandle;
typedef struct { Ptr baseAddr; short rowBytes; Rect bounds; } BitMap;
typedef struct { short rowBytes; Rect bounds; short pixelSize; } PixMap;
typedef PixMap **PixMapHandle;
typedef struct { PixMapHandle gdPMap; Rect gdRect; } GDevice;
typedef GDevice **GDHandle;
typedef struct { unsigned short red, green, blue; } RGBColor;
typedef struct { short value; RGBColor rgb; } ColorSpec;
typedef struct { long ctSeed; short ctFlags; short ctSize; ColorSpec ctTable[1]; } ColorTable;
typedef ColorTable **CTabHandle;
typedef struct { short dummy; } Palette;
typedef Palette **PaletteHandle;
typedef struct { void *textProc, *lineProc, *rectProc, *rRectProc, *ovalProc,
	*arcProc, *polyProc, *rgnProc, *bitsProc, *commentProc, *txMeasProc,
	*getPicProc, *putPicProc; } QDProcs;
typedef struct { void *textProc, *lineProc, *rectProc, *rRectProc, *ovalProc,
	*arcProc, *polyProc, *rgnProc, *bitsProc, *commentProc, *txMeasProc,
	*getPicProc, *putPicProc, *opcodeProc, *newProc1, *glyphProc, *newProc3,
	*newProc4, *newProc5, *newProc6; } CQDProcs;
typedef struct
{
	BitMap portBits;
	Rect portRect;
	RgnHandle visRgn;
	RgnHandle clipRgn;
	QDProcs *grafProcs;
} GrafPort;
typedef GrafPort *GrafPtr;
typedef GrafPtr WindowPtr;
typedef GrafPtr CGrafPtr;
typedef GrafPtr DialogPtr;
typedef struct { short dummy; } ControlRecord;
typedef ControlRecord **ControlHandle;

typedef struct
{
	short what;
	long message;
	unsigned long when;
	Point where;
	short modifiers;
} EventRecord;

typedef struct { long dummy; } KeyMap[4];

typedef struct { long randSeed; } QDGlobals;
extern long randSeed;
extern long Ticks;
extern RgnHandle GrayRgn;

enum { noErr = 0 };
enum { btnState = 0x0080, optionKey = 0x0800, cmdKey = 0x0100, shiftKey = 0x0200 };
enum { updateEvt = 6, activateEvt = 8, nullEvent = 0 };
enum { blackColor = 33, whiteColor = 30 };
enum { plainDBox = 2, documentProc = 0, noGrowDocProc = 4 };
enum { gestaltQuickdrawVersion = 0x71642020L, gestalt8BitQD = 0x0100,
	gestaltSystemVersion = 0x73797376L };
OSErr Gestalt(OSType sel, long *response);
RgnHandle LMGetGrayRgn(void);
enum { keyDown = 3, autoKey = 5, mouseDown = 1, mouseUp = 2 };
enum { everyEvent = -1 };

/* Fixed point (16.16) */
#define fixed1 0x00010000L
#define FixRound(x) ((short)(((x) + 0x8000L) >> 16))
#define Long2Fix(x) ((Fixed)(x) << 16)
#define Fix2Long(x) ((long)((x) >> 16))
Fixed FixMul(Fixed a, Fixed b);
Fixed FixDiv(Fixed a, Fixed b);
Fixed FixRatio(short numer, short denom);
long FixMulLong(Fixed a, long b);

/* Mac calls */
unsigned long TickCount(void);
void Delay(long ticks, long *finalTicks);
void GetKeys(KeyMap keys);
void GetMouse(Point *pt);
Boolean Button(void);
Boolean StillDown(void);
void FlushEvents(short eventMask, short stopMask);
Boolean GetNextEvent(short mask, EventRecord *ev);
Boolean EventAvail(short mask, EventRecord *ev);
void SysBeep(short duration);
void ExitToShell(void);
void SetCursor(const void *crsr);
void HideCursor(void);
void ShowCursor(void);
void InitCursor(void);

Handle NewHandle(long size);
Ptr NewPtr(long size);
void DisposeHandle(Handle h);
void DisposePtr(Ptr p);
void HLock(Handle h);
void HUnlock(Handle h);
long GetHandleSize(Handle h);
void BlockMove(const void *src, void *dst, long n);
Handle GetResource(ResType type, short id);
Handle GetIndResource(ResType type, short index);
Handle GetString(short id);
PicHandle GetPicture(short id);
short OpenResFile(ConstStr255Param name);
void CloseResFile(short ref);
void SetHandleSize(Handle h, long size);
void WriteResource(Handle h);
void ChangedResource(Handle h);
char HGetState(Handle h);
void HSetState(Handle h, char state);
OSErr HandToHand(Handle *h);
Boolean BitTst(const void *base, long bit);
void BitSet(void *base, long bit);
short Random(void);
Fixed FixATan2(long x, long y);
Fract FracSqrt(Fract x);
GDHandle GetMainDevice(void);
RgnHandle NewRgn(void);
void DisposeRgn(RgnHandle r);
WindowPtr NewWindow(void *storage, const Rect *bounds, const unsigned char *title,
	Boolean visible, short proc, WindowPtr behind, Boolean goAway, long refCon);
DialogPtr GetNewDialog(short id, void *storage, WindowPtr behind);
short Alert(short id, void *filter);
void ModalDialog(void *filter, short *item);
Boolean IsDialogEvent(const EventRecord *ev);
Boolean DialogSelect(const EventRecord *ev, DialogPtr *dlg, short *item);
void GetPort(GrafPtr *port);
void SetPort(GrafPtr port);
void SetRect(Rect *r, short l, short t, short rr, short b);
void OffsetRect(Rect *r, short dh, short dv);
void InsetRect(Rect *r, short dh, short dv);
void PaintRect(const Rect *r);
void FrameRect(const Rect *r);
void EraseRect(const Rect *r);
void MoveTo(short h, short v);
void Line(short dh, short dv);
void DebugStr(const unsigned char *s);
void ReleaseResource(Handle h);
void DetachResource(Handle h);
short ResError(void);


#endif
