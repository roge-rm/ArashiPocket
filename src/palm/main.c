/* Arashi for PalmOS 3.1: application shell. Opens one empty form and runs
   the standard event loop until appStopEvent. Built with DEBUG_SCRIPT, it
   first dumps the Arashi Script levels 1 to 99 to a database. */
#include <Pilot.h>
#include "resource.h"

#ifdef DEBUG_SCRIPT
#include "../core/parser.h"
#include "../core/scriptdump.h"
#include "../core/qdrandom.h"

#define DumpDBName "ArashiScriptDump"

static char dumpStatus[40] = "";

static void DumpRecord(DmOpenRef db, const void *data, ULong len)
{
	UInt at = dmMaxRecordIndex;
	VoidHand h = DmNewRecord(db, &at, len);

	if (h) {
		DmWrite(MemHandleLock(h), 0, (VoidPtr)data, len);
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
	Int level;
	ULong start, ticks, freeBytes, maxChunk;
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
		ULong mathsStart = TimGetTicks();
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

static Boolean MainFormHandleEvent(EventPtr event)
{
	switch (event->eType) {
	case frmOpenEvent:
		FrmDrawForm(FrmGetActiveForm());
#ifdef DEBUG_SCRIPT
		WinDrawChars(dumpStatus, StrLen(dumpStatus), 2, 70);
#endif
		return true;
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
		Word formId = event->data.frmLoad.formID;
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
	Word error;

	do {
		EvtGetEvent(&event, evtWaitForever);
		if (SysHandleEvent(&event))
			continue;
		if (MenuHandleEvent(0, &event, &error))
			continue;
		if (ApplicationHandleEvent(&event))
			continue;
		FrmDispatchEvent(&event);
	} while (event.eType != appStopEvent);
}

DWord PilotMain(Word cmd, Ptr cmdPBP, Word launchFlags)
{
	if (cmd == sysAppLaunchCmdNormalLaunch) {
#ifdef DEBUG_SCRIPT
		ScriptDump();
#endif
		FrmGotoForm(MainForm);
		EventLoop();
		FrmCloseAllForms();
	}
	return 0;
}
