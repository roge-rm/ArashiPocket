/* Arashi for PalmOS 3.1: application shell. Opens one empty form and runs
   the standard event loop until appStopEvent. */
#include <Pilot.h>
#include "resource.h"

static Boolean MainFormHandleEvent(EventPtr event)
{
	switch (event->eType) {
	case frmOpenEvent:
		FrmDrawForm(FrmGetActiveForm());
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
		FrmGotoForm(MainForm);
		EventLoop();
		FrmCloseAllForms();
	}
	return 0;
}
