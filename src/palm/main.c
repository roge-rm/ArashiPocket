/* Arashi for PalmOS 3.1: application shell. Sets up the screen and runs
   the renderer demo (a spinning level tube with its frame rate) until
   appStopEvent. Built with TEST_PATTERN it shows the M2 screen test
   pattern with the device details instead. Built with DEBUG_SCRIPT, it
   first dumps the Arashi Script levels 1 to 99 to a database. */
#include "palm.h"
#include "resource.h"
#include "../core/testpat.h"
#include "../core/va.h"
#include "../core/vademo.h"

#define DemoLevel 1

static Boolean gDemo;       /* the renderer demo is running */
static Boolean gPaused;     /* a menu or dialog is over the form */

#ifdef DEBUG_SCRIPT
#include "../core/parser.h"
#include "../core/scriptdump.h"
#include "../core/qdrandom.h"

#define DumpDBName "ArashiScriptDump"

static char dumpStatus[40] = "";

static void DumpRecord(DmOpenRef db, const void *data, UInt32 len)
{
	UInt16 at = dmMaxRecordIndex;
	MemHandle h = DmNewRecord(db, &at, len);

	if (h) {
		DmWrite(MemHandleLock(h), 0, (void *)data, len);
		MemHandleUnlock(h);
		DmReleaseRecord(db, at, true);
	}
}

/* One record per level, as the host test's dump, then a "#" line with the
   time taken and heap free at the start (left out of comparisons). Seed 1
   for random, as InitGraf sets it. */
static void ScriptDump(void)
{
	LocalID id;
	DmOpenRef db;
	char *buf;
	char line[80];
	Int16 level;
	UInt32 start, ticks, freeBytes, maxChunk;
	Boolean ok;

	id = DmFindDatabase(0, DumpDBName);
	if (id)
		DmDeleteDatabase(0, id);
	if (DmCreateDatabase(0, DumpDBName, 'ArPk', 'Dump', false))
		return;
	id = DmFindDatabase(0, DumpDBName);
	db = DmOpenDatabase(0, id, dmModeReadWrite);
	if (!db)
		return;
	MemHeapFreeBytes(0, &freeBytes, &maxChunk);
	buf = MemPtrNew(SCRIPTDUMP_LEVEL_BYTES);
	qd_seed(1);
	start = TimGetTicks();
	ok = buf && parser_alloc();
	if (ok) {
		for (level = 1; level <= 99; level++)
			DumpRecord(db, buf, script_dump_level(level, buf, SCRIPTDUMP_LEVEL_BYTES));
		parser_free();
	}
	ticks = TimGetTicks() - start;
	StrPrintF(line, "# levels %s ticks %lu per second %u heap free %lu\n",
	          ok ? "done" : "FAILED", ticks, SysTicksPerSecond(), freeBytes);
	DumpRecord(db, line, StrLen(line));
	if (buf) {
		UInt32 mathsStart = TimGetTicks();
		DumpRecord(db, buf, script_dump_maths(SCRIPT_MATHS_N, buf, SCRIPTDUMP_LEVEL_BYTES));
		StrPrintF(line, "# maths ticks %lu\n", TimGetTicks() - mathsStart);
		DumpRecord(db, line, StrLen(line));
	}
	StrPrintF(dumpStatus, "Script dump %s, %lu ticks", ok ? "done" : "FAILED", ticks);
	if (buf)
		MemPtrFree(buf);
	DmCloseDatabase(db);
}
#endif

/* Device details, drawn after each present in the lower part of the play
   area. */
static void DrawInfo(void)
{
	const PlatScreen *s = plat_screen();
	Char line[40];
	Char flags[40];
	Coord x, y, step;

	if (!s)
		return;
	step = screen_font_height();
	x = (Coord)(s->play.x + s->play.w / 20 + 3);
	y = (Coord)(s->play.y + s->play.w * 5 / 8);
	StrPrintF(line, "OS %d.%d %s", sysGetROMVerMajor(gDev.rom), sysGetROMVerMinor(gDev.rom),
	          sysFtrNumProcessorIs68K(gDev.cpu) ? "68k" : "ARM");
	screen_text(line, x, y);
	y += step;
	StrPrintF(line, "%dx%d %d bit %s", s->width, s->height, s->depth,
	          gDev.hasColor ? "colour" : "grey");
	screen_text(line, x, y);
	y += step;
	StrPrintF(line, "dens %ld%s play %d", gDev.density,
	          screen_pen_scale() > 1 && gDev.density < 144 ? " HR" : "", s->play.w);
	screen_text(line, x, y);
	y += step;
	flags[0] = 0;
	if (gDev.pinsVersion)
		StrCat(flags, "PINS ");
	if (gDev.nav)
		StrCat(flags, "NAV ");
	if (gDev.jog)
		StrCat(flags, "JOG ");
	if (gDev.zodiac)
		StrCat(flags, "ZOD ");
	if (gDev.rotation)
		StrCat(flags, "ROT ");
	if (gDev.landscape)
		StrCat(flags, "LAND ");
	if (gDev.sampled)
		StrCat(flags, "SND ");
	screen_text(flags, x, y);
}

/* Last stylus point drawn as a cross. */
static void Redraw(void)
{
	const PlatScreen *s = plat_screen();
	PlatInput in;

	if (!s)
		return;
	plat_input(&in);
	testpat_draw(s, in.pen_seen, in.pen_x, in.pen_y);
	plat_present();
	DrawInfo();
}

static Boolean MainFormHandleEvent(EventPtr event)
{
	switch (event->eType) {
	case frmOpenEvent: {
		FormPtr frm = FrmGetActiveForm();

		screen_form(frm);
		FrmDrawForm(frm);
		if (plat_screen()) {
#ifdef TEST_PATTERN
			Redraw();
#else
			gDemo = VAInit();
			if (gDemo)
				vademo_init(DemoLevel);
			else
				WinDrawChars("No memory for the renderer", 26, 2, 70);
#endif
		} else {
			Char msg[48];

			StrPrintF(msg, "No screen: %s", screen_error());
			WinDrawChars(msg, StrLen(msg), 2, 70);
		}
#ifdef DEBUG_SCRIPT
		WinDrawChars(dumpStatus, StrLen(dumpStatus), 2, 90);
#endif
		return true;
	}
	case frmUpdateEvent:
		if (!gDemo)
			Redraw();
		return false;
	case winEnterEvent:
		if (gDemo) {
			if (event->data.winEnter.enterWindow == (WinHandle)FrmGetWindowHandle(FrmGetActiveForm()) &&
			    FrmGetActiveFormID() == MainForm) {
				gPaused = false;
				VAScreenChanged();
			}
		} else {
			Redraw();
		}
		return false;
	case winExitEvent:
		if (gDemo && FrmGetActiveForm() &&
		    event->data.winExit.exitWindow == (WinHandle)FrmGetWindowHandle(FrmGetActiveForm()))
			gPaused = true;
		return false;
	case menuEvent:
		if (event->data.menu.itemID == MainMenuAbout) {
			FrmAlert(AboutAlert);
			return true;
		}
		return false;
	default:
		return false;
	}
}

static Boolean ApplicationHandleEvent(EventPtr event)
{
	if (event->eType == frmLoadEvent) {
		UInt16 formId = event->data.frmLoad.formID;
		FormPtr frm = FrmInitForm(formId);

		FrmSetActiveForm(frm);
		if (formId == MainForm)
			FrmSetEventHandler(frm, MainFormHandleEvent);
		return true;
	}
	return false;
}

static void EventLoop(void)
{
	EventType event;
	UInt16 error;

	do {
		EvtGetEvent(&event, gDemo && !gPaused ? 0 : evtWaitForever);
		if (input_event(&event)) {
			continue;
		}
		if (SysHandleEvent(&event))
			continue;
		if (MenuHandleEvent(0, &event, &error))
			continue;
		if (ApplicationHandleEvent(&event))
			continue;
		FrmDispatchEvent(&event);
		if (event.eType == winDisplayChangedEvent) {
			screen_rebuild();
			screen_form(FrmGetActiveForm());
			if (gDemo) {
				if (VAScreenChanged())
					vademo_init(DemoLevel);
			} else {
				Redraw();
			}
		} else if (gDemo) {
			if (event.eType == nilEvent && !gPaused && plat_screen()) {
				vademo_frame();
				EvtResetAutoOffTimer();
			}
		} else if (event.eType == penDownEvent || event.eType == penMoveEvent ||
		           event.eType == penUpEvent) {
			Redraw();
		}
	} while (event.eType != appStopEvent);
}

UInt32 PilotMain(UInt16 cmd, void *cmdPBP, UInt16 launchFlags)
{
	if (cmd == sysAppLaunchCmdNormalLaunch) {
#ifdef DEBUG_SCRIPT
		ScriptDump();
#endif
		screen_init();
		input_init();
		FrmGotoForm(MainForm);
		EventLoop();
		FrmCloseAllForms();
		VAClose();
		gDemo = false;
		input_exit();
		screen_exit();
	}
	return 0;
}
