/* Stand-ins for the Mac Toolbox calls the jam sources make. Fixed-point
   maths is real. Everything marked STUB does nothing yet and gets a real
   Palm version in the milestone named beside it (M2 rendering, M1 data and
   resources, M7 front end, M8 sound). */
#include "macshim.h"

/* Real fixed-point maths (16.16) */
Fixed FixMul(Fixed a, Fixed b)
{
	return (Fixed)(((long long)a * (long long)b) >> 16);
}

Fixed FixDiv(Fixed a, Fixed b)
{
	if (b == 0)
		return a < 0 ? 0x80000000L : 0x7FFFFFFFL;
	return (Fixed)((((long long)a) << 16) / b);
}

Fixed FixRatio(short numer, short denom)
{
	return FixDiv(Long2Fix(numer), Long2Fix(denom));
}

long FixMulLong(Fixed a, long b)
{
	return (long)(((long long)a * (long long)b) >> 16);
}

Boolean BitTst(const void *base, long bit)
{
	/* Mac bit numbering: bit 0 is the high bit of the first byte. */
	return (((const unsigned char *)base)[bit >> 3] >> (7 - (bit & 7))) & 1;
}

void BitSet(void *base, long bit)
{
	((unsigned char *)base)[bit >> 3] |= 0x80 >> (bit & 7);
}

/* STUB (M3): time. Becomes TimGetTicks. */
unsigned long TickCount(void) { return 0; }
void Delay(long ticks, long *finalTicks) { if (finalTicks) *finalTicks = 0; }

/* STUB (M3/M9): input. Becomes Palm pen and hard buttons. */
void GetKeys(KeyMap keys) { }
void GetMouse(Point *pt) { pt->h = pt->v = 0; }
Boolean Button(void) { return 0; }
Boolean StillDown(void) { return 0; }
void FlushEvents(short eventMask, short stopMask) { }
Boolean GetNextEvent(short mask, EventRecord *ev) { return 0; }
Boolean EventAvail(short mask, EventRecord *ev) { return 0; }

/* STUB (M8): sound */
void SysBeep(short duration) { }

/* STUB (M1): memory and resources */
Handle NewHandle(long size) { return 0; }
Ptr NewPtr(long size) { return 0; }
void DisposeHandle(Handle h) { }
void DisposePtr(Ptr p) { }
void HLock(Handle h) { }
void HUnlock(Handle h) { }
long GetHandleSize(Handle h) { return 0; }
void BlockMove(const void *src, void *dst, long n) { }
Handle GetResource(ResType type, short id) { return 0; }
void ReleaseResource(Handle h) { }
void DetachResource(Handle h) { }
short ResError(void) { return 0; }
