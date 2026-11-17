/* Input: hard buttons, stylus, 5-way and Jog Dial turned into the plat.h
   actions. Held buttons are polled with KeyCurrentState, which works on
   every OS version; the stylus and the Jog Dial come from events. */
#include "palm.h"

#define vchrNavChange       0x0503   /* palmOne 5-way */
#define vchrHardRockerCenter 0x013D  /* what the E2 and T|X send when the centre is held (keyCode 0x0136) */
#define vchrSonyJogUp       0x1700
#define vchrSonyJogDown     0x1701
#define vchrSonyJogPush     0x1705
#define vchrSonyJogRelease  0x1706
#define vchrSonyJogBack     0x1707
#define vchrSonyJogPushRep  0x1708   /* auto-repeat of a held push, if any */

/* KeyCurrentState bits of the 5-way: the Tungsten and T|X set the low three
   of these in the top byte, Treo style rockers use the 0x0001xxxx bits. */
#define kNavLeft     (0x01000000UL | 0x00040000UL)
#define kNavRight    (0x02000000UL | 0x00080000UL)
#define kNavSelect   (0x04000000UL | 0x00100000UL)
#define kNavUp       0x00010000UL
#define kNavDown     0x00020000UL

static Boolean gPenDown, gPenSeen, gJogPush;
static Int16 gPenX, gPenY;
static Int16 gRawX, gRawY, gPenEvents;
static UInt16 gKeyChr, gKeyCode, gKeyMods;
static Int16 gKeyEvents;
/* The last pen event's own coordinates, for the test pattern. */
static Int16 gSpin;
static Boolean gTapped;       /* a pen up since the last plat_input */
static Int16 gTapX, gTapY;
static UInt16 gLatch;
/* Keys that only come as events (the Zodiac stick, rockers without a
   KeyCurrentState bit) count as held for a moment after each key event, so
   the auto-repeat keeps them held. */
static UInt32 gLeftUntil, gRightUntil;
/* 5-way state from the navChange key events (navBit* in keyCode). */
static UInt16 gNavBits;
#define navBitUp     0x0001
#define navBitDown   0x0002
#define navBitLeft   0x0004
#define navBitRight  0x0008
#define navBitSelect 0x0010
#define kEventHoldMs 150

void input_init(void)
{
	gPenDown = gPenSeen = gJogPush = false;
	gSpin = 0;
	gTapped = false;
	gLatch = 0;
	gLeftUntil = gRightUntil = 0;
	gNavBits = 0;
}

void input_exit(void)
{
}

static Boolean is_game_key(WChar chr)
{
	switch (chr) {
	case vchrHard1: case vchrHard2: case vchrHard3: case vchrHard4:
	case vchrPageUp: case vchrPageDown:
	case vchrRockerUp: case vchrRockerDown: case vchrRockerLeft:
	case vchrRockerRight: case vchrRockerCenter: case vchrHardRockerCenter:
	case vchrNavChange:
	case vchrMenu:
	case vchrSonyJogUp: case vchrSonyJogDown: case vchrSonyJogPush:
	case vchrSonyJogRelease: case vchrSonyJogBack: case vchrSonyJogPushRep:
		return true;
	}
	return false;
}

/* Stylus position in play-area coordinates. Events are in standard
   (160 wide) pixels; the play area is in native ones. */
static void pen_point(const EventType *e)
{
	const PlatScreen *s = plat_screen();
	UInt16 scale = screen_pen_scale();

	if (!s)
		return;
	gPenEvents++;
	gRawX = e->screenX;
	gRawY = e->screenY;
	gPenX = (Int16)(e->screenX * scale - s->play.x);
	gPenY = (Int16)(e->screenY * scale - s->play.y);
	gPenSeen = true;
}

Boolean input_event(EventPtr e)
{
	switch (e->eType) {
	case penDownEvent:
		gPenDown = true;
		pen_point(e);
		return false;
	case penMoveEvent:
		pen_point(e);
		return false;
	case penUpEvent:
		pen_point(e);
		gPenDown = false;
		gTapped = true;
		gTapX = gPenX;
		gTapY = gPenY;
		return false;
	case keyDownEvent: {
		WChar c = e->data.keyDown.chr;

		gKeyChr = c;
		gKeyCode = e->data.keyDown.keyCode;
		gKeyMods = e->data.keyDown.modifiers;
		gKeyEvents++;

		if (c == vchrSonyJogUp)
			gSpin--;
		else if (c == vchrSonyJogDown)
			gSpin++;
		else if (c == vchrSonyJogPush)
			gJogPush = true;
		else if (c == vchrSonyJogRelease)
			gJogPush = false;
		else if (c == vchrNavChange)
			gNavBits = (UInt16)(e->data.keyDown.keyCode & 0x001F);
		else if (c == vchrRockerLeft)
			gLeftUntil = TimGetTicks() + SysTicksPerSecond() * kEventHoldMs / 1000 + 1;
		else if (c == vchrRockerRight)
			gRightUntil = TimGetTicks() + SysTicksPerSecond() * kEventHoldMs / 1000 + 1;
		else if (c == 'p' || c == 'P')
			gLatch |= PLAT_ACT_PAUSE;
		else if (c == vchrMenu)
			gLatch |= PLAT_ACT_MENU;
		return is_game_key(c);
	}
	case keyHoldEvent:
		/* Holding the T|X's 5-way centre opens the Alerts screen unless the
		   app takes the hold; the press itself already fires. */
		return is_game_key(e->data.keyHold.chr);
	default:
		return false;
	}
}

void plat_input(PlatInput *in)
{
	UInt32 k = KeyCurrentState();
	UInt16 a = gLatch;
	const PlatScreen *s = plat_screen();

	gLatch = 0;
	if (k & (keyBitHard1 | kNavLeft))
		a |= PLAT_ACT_LEFT;
	if (k & (keyBitHard2 | kNavRight))
		a |= PLAT_ACT_RIGHT;
	if ((k & (keyBitHard3 | keyBitPageUp | kNavSelect | kNavUp)) || gJogPush)
		a |= PLAT_ACT_FIRE;
	if (k & (keyBitHard4 | keyBitPageDown | kNavDown))
		a |= PLAT_ACT_ZAP;
	if (gNavBits & navBitLeft)
		a |= PLAT_ACT_LEFT;
	if (gNavBits & navBitRight)
		a |= PLAT_ACT_RIGHT;
	if (gNavBits & (navBitSelect | navBitUp))
		a |= PLAT_ACT_FIRE;
	if (gNavBits & navBitDown)
		a |= PLAT_ACT_ZAP;
	if ((Int32)(gLeftUntil - TimGetTicks()) > 0)
		a |= PLAT_ACT_LEFT;
	if ((Int32)(gRightUntil - TimGetTicks()) > 0)
		a |= PLAT_ACT_RIGHT;
	in->actions = a;
	in->spin = gSpin;
	gSpin = 0;
	in->pen_down = gPenDown;
	in->pen_seen = gPenSeen;
	in->pen_x = gPenX;
	in->pen_y = gPenY;
	in->tapped = gTapped;
	in->tap_x = gTapX;
	in->tap_y = gTapY;
	gTapped = false;
	in->rim = (gPenDown && s) ? plat_rim_from_point(gPenX, gPenY, s->play.w) : -1;
}

void input_last_key(UInt16 *chr, UInt16 *code, UInt16 *mods, Int16 *count)
{
	*chr = gKeyChr;
	*code = gKeyCode;
	*mods = gKeyMods;
	*count = gKeyEvents;
}

void input_last_raw(Int16 *x, Int16 *y, Int16 *count)
{
	*x = gRawX;
	*y = gRawY;
	*count = gPenEvents;
}
