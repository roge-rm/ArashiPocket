/* PalmOS backend for plat.h. So far: memory from the dynamic heap,
   resources from the PRC, and ticks. */
#include <Pilot.h>
#include "../core/plat.h"

void *plat_alloc(uint32 size)
{
	return MemPtrNew(size ? size : 1);
}

void plat_free(void *p)
{
	MemPtrFree(p);
}

/* Locks the resource and returns its bytes; plat_release unlocks and
   releases it. */
const uint8 *plat_resource(uint32 type, int16 id, uint32 *size)
{
	VoidHand h = DmGetResource(type, id);

	if (!h)
		return NULL;
	if (size)
		*size = MemHandleSize(h);
	return (const uint8 *)MemHandleLock(h);
}

void plat_release(const uint8 *data)
{
	VoidHand h = MemPtrRecoverHandle((VoidPtr)data);

	MemHandleUnlock(h);
	DmReleaseResource(h);
}

uint32 plat_ticks(void)
{
	return TimGetTicks();
}

uint32 plat_ticks_per_second(void)
{
	return SysTicksPerSecond();
}
