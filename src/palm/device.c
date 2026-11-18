/* Device detection: everything the screen and input setup choose from. Each
   trap newer than OS 3.0 is behind a version or feature check. */
#include "palm.h"
#include "sonyhr.h"

#define kSonyCreator  'SoNy'
#define kNavCreator   'fway'

PalmDevice gDev;

static SEG_DEV UInt32 get_ftr(UInt32 creator, UInt16 num, UInt32 def)
{
	UInt32 v;

	if (FtrGet(creator, num, &v) != errNone)
		return def;
	return v;
}

/* True if the trap is there (OS 5: SndStream appears with OS 5.1 or so,
   the TG50's 5.0 doesn't have it). */
static SEG_DEV Boolean trap_present(UInt16 trap)
{
	return SysGetTrapAddress(trap) != SysGetTrapAddress(sysTrapSysUnimplemented);
}

SEG_DEV void device_detect(PalmDevice *d)
{
	UInt32 depths = 1, v;
	Boolean color = false;
	UInt16 ref;
	Coord w = 160, h = 160;

	MemSet(d, sizeof(*d), 0);
	d->rom = get_ftr(sysFtrCreator, sysFtrNumROMVersion, 0);
	d->cpu = get_ftr(sysFtrCreator, sysFtrNumProcessorID, 0);
	d->ticksPerSecond = SysTicksPerSecond();
	if (d->rom >= kRom35) {
		d->company = get_ftr(sysFtrCreator, sysFtrNumOEMCompanyID, 0);
		d->winVersion = get_ftr(sysFtrCreator, sysFtrNumWinVersion, 0);
	}
	/* WinScreenMode is ScrDisplayMode on OS 3.0 to 3.3, the same trap, with
	   1 and 2 bits on the grey Palms. */
	if (d->rom >= kMinRomVersion) {
		if (WinScreenMode(winScreenModeGetSupportedDepths, NULL, NULL, &depths, NULL) != errNone)
			depths = 1;
		if (WinScreenMode(winScreenModeGetSupportsColor, NULL, NULL, NULL, &color) != errNone)
			color = false;
	}
	d->depthMask = depths;
	d->hasColor = color;

	d->density = 72;
	if (d->winVersion >= 4 && WinScreenGetAttribute(winScreenDensity, &v) == errNone)
		d->density = v;

	d->sony = FtrGet(kSonyCreator, 1, &v) == errNone;
	d->sonyHR = SysLibFind("Sony HR Library", &ref) == errNone;
	d->jog = d->sony;
	/* palmOne's 5-way (T|X, LifeDrive, E2) sets system feature 32, Treos
	   set the 'hsEx' navigator version (14). */
	d->nav = FtrGet(sysFileCSystem, 32, &v) == errNone ||
	         FtrGet('hsEx', 14, &v) == errNone ||
	         FtrGet(kNavCreator, 0, &v) == errNone;
	/* Tapwave's company ID is 'Tpwv' or 'tpwv'; no Tapwave header here. */
	d->zodiac = d->company == 0x54707776UL || d->company == 0x74707776UL;
	d->pinsVersion = get_ftr(pinCreator, pinFtrAPIVersion, 0);
	d->rotation = d->pinsVersion >= pinAPIVersion1_1;
	d->sampled = d->rom >= kRom50 && trap_present(sysTrapSndStreamCreate);

	WinGetDisplayExtent(&w, &h);
	d->width = w;
	d->height = h;
	d->landscape = w > h;
	d->dana = (UInt32)w > 3UL * h;
}
