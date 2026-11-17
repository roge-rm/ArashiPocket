/* PalmOS backend for plat.h: memory from the dynamic heap, resources from
   the PRC and ticks. The screen is in screen.c and input in input.c. */
#include "palm.h"

#define SAVE_CREATOR 'ArPk'

#ifdef DEBUG_HEAP
UInt32 heap_least = 0xFFFFFFFFUL, heap_fails, fail_size, fail_free, fail_chunk;

void *plat_alloc(uint32 size)
{
	void *p = MemPtrNew(size ? size : 1);
	UInt32 freeBytes, maxChunk;

	MemHeapFreeBytes(0, &freeBytes, &maxChunk);
	if (!p && !heap_fails++) {
		fail_size = size;
		fail_free = freeBytes;
		fail_chunk = maxChunk;
	}
	if (freeBytes < heap_least)
		heap_least = freeBytes;
	return p;
}
#else
void *plat_alloc(uint32 size)
{
	return MemPtrNew(size ? size : 1);
}
#endif

void plat_free(void *p)
{
	MemPtrFree(p);
}

/* The keep buffer: in the dynamic heap when it has the room to spare,
   else the one record of the database ArashiKeep, made for it and deleted
   when it is freed (and at the next alloc, if the app stopped first). */
#define KEEP_NAME  "ArashiKeep"
#define KEEP_TYPE  'Keep'
#define KEEP_SPARE 65536UL
#define KEEP_MAX   65000UL

static DmOpenRef gKeepDb;
static MemHandle gKeepH;

static void keep_delete(void)
{
	LocalID id = DmFindDatabase(0, KEEP_NAME);

	if (id)
		DmDeleteDatabase(0, id);
}

const uint8 *plat_keep_alloc(uint32 size)
{
	UInt32 freeBytes, maxChunk;
	LocalID id;
	UInt16 at = 0;

	if (size == 0 || size > KEEP_MAX)
		return NULL;
	MemHeapFreeBytes(0, &freeBytes, &maxChunk);
	if (maxChunk > size + KEEP_SPARE)
		return (const uint8 *)MemPtrNew(size);
	if (gKeepDb)
		return NULL;
	keep_delete();
	if (DmCreateDatabase(0, KEEP_NAME, SAVE_CREATOR, KEEP_TYPE, false))
		return NULL;
	id = DmFindDatabase(0, KEEP_NAME);
	gKeepDb = id ? DmOpenDatabase(0, id, dmModeReadWrite) : NULL;
	if (!gKeepDb) {
		keep_delete();
		return NULL;
	}
	gKeepH = DmNewRecord(gKeepDb, &at, size);
	if (!gKeepH) {
		DmCloseDatabase(gKeepDb);
		gKeepDb = NULL;
		keep_delete();
		return NULL;
	}
	return (const uint8 *)MemHandleLock(gKeepH);
}

void plat_keep_write(const uint8 *keep, uint32 at, const void *src, uint32 n)
{
	if (gKeepH && MemPtrRecoverHandle((MemPtr)keep) == gKeepH)
		DmWrite((void *)keep, at, (void *)src, n);
	else
		MemMove((UInt8 *)keep + at, src, (Int32)n);
}

void plat_keep_free(const uint8 *keep)
{
	if (!keep)
		return;
	if (gKeepH && MemPtrRecoverHandle((MemPtr)keep) == gKeepH) {
		MemHandleUnlock(gKeepH);
		DmReleaseRecord(gKeepDb, 0, false);
		DmCloseDatabase(gKeepDb);
		gKeepDb = NULL;
		gKeepH = NULL;
		keep_delete();
	} else {
		MemPtrFree((MemPtr)keep);
	}
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
