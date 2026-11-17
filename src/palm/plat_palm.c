/* PalmOS backend for plat.h: memory from the dynamic heap, resources from
   the PRC and ticks. The screen is in screen.c and input in input.c. */
#include "palm.h"

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
	MemHandle h = DmGetResource(type, (UInt16)id);

	if (!h)
		return NULL;
	if (size)
		*size = MemHandleSize(h);
	return (const uint8 *)MemHandleLock(h);
}

void plat_release(const uint8 *data)
{
	MemHandle h = MemPtrRecoverHandle((MemPtr)data);

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

/* A saved blob is one record in a database of its own, named like the blob
   (31 characters at most). */
#define SAVE_CREATOR 'ArPk'
#define SAVE_TYPE    'Data'

cbool plat_save(const char *name, const void *data, uint32 size)
{
	LocalID id = DmFindDatabase(0, (Char *)name);
	DmOpenRef db;
	MemHandle h;
	UInt16 at = 0;
	cbool ok = 0;

	if (!id) {
		if (DmCreateDatabase(0, (Char *)name, SAVE_CREATOR, SAVE_TYPE, false))
			return 0;
		id = DmFindDatabase(0, (Char *)name);
	}
	db = DmOpenDatabase(0, id, dmModeReadWrite);
	if (!db)
		return 0;
	if (DmNumRecords(db) > 0) {
		h = DmResizeRecord(db, 0, (UInt32)size);
	} else {
		h = DmNewRecord(db, &at, (UInt32)size);
	}
	if (h) {
		DmWrite(MemHandleLock(h), 0, (void *)data, (UInt32)size);
		MemHandleUnlock(h);
		DmReleaseRecord(db, 0, true);
		ok = 1;
	}
	DmCloseDatabase(db);
	return ok;
}

int32 plat_load(const char *name, void *data, uint32 size)
{
	LocalID id = DmFindDatabase(0, (Char *)name);
	DmOpenRef db;
	MemHandle h;
	int32 n = -1;

	if (!id)
		return -1;
	db = DmOpenDatabase(0, id, dmModeReadOnly);
	if (!db)
		return -1;
	h = DmQueryRecord(db, 0);
	if (h && MemHandleSize(h) <= size) {
		n = (int32)MemHandleSize(h);
		MemMove(data, MemHandleLock(h), (UInt32)n);
		MemHandleUnlock(h);
	}
	DmCloseDatabase(db);
	return n;
}
