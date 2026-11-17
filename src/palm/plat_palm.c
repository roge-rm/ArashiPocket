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

/* Storage records for big buffers: each is the one record of its own
   database, made for it and deleted when it is freed (and at the next alloc,
   if the app stopped first). The keep buffer (still picture) uses ArashiKeep
   when the dynamic heap has no room to spare, the grids use ArashiGrid
   always. Reads are direct, writes go through DmWrite. */
#define KEEP_SPARE 65536UL
#define KEEP_MAX   65000UL

typedef struct {
	const char *name;
	UInt32 type;
	DmOpenRef db;
	MemHandle h;
} Slot;

static Slot gKeepSlot = { "ArashiKeep", 'Keep', 0, 0 };
static Slot gGridSlot = { "ArashiGrid", 'Grid', 0, 0 };

static void slot_delete(Slot *s)
{
	LocalID id = DmFindDatabase(0, s->name);

	if (id)
		DmDeleteDatabase(0, id);
}

static const uint8 *slot_alloc(Slot *s, uint32 size)
{
	LocalID id;
	UInt16 at = 0;

	if (size == 0 || size > KEEP_MAX || s->db)
		return NULL;
	slot_delete(s);
	if (DmCreateDatabase(0, s->name, SAVE_CREATOR, s->type, false))
		return NULL;
	id = DmFindDatabase(0, s->name);
	s->db = id ? DmOpenDatabase(0, id, dmModeReadWrite) : NULL;
	if (!s->db) {
		slot_delete(s);
		return NULL;
	}
	s->h = DmNewRecord(s->db, &at, size);
	if (!s->h) {
		DmCloseDatabase(s->db);
		s->db = NULL;
		slot_delete(s);
		return NULL;
	}
	return (const uint8 *)MemHandleLock(s->h);
}

static int slot_owns(const Slot *s, const uint8 *p)
{
	return s->h && MemPtrRecoverHandle((MemPtr)p) == s->h;
}

static void slot_free(Slot *s)
{
	MemHandleUnlock(s->h);
	DmReleaseRecord(s->db, 0, false);
	DmCloseDatabase(s->db);
	s->db = NULL;
	s->h = NULL;
	slot_delete(s);
}

const uint8 *plat_keep_alloc(uint32 size)
{
	UInt32 freeBytes, maxChunk;

	if (size == 0 || size > KEEP_MAX)
		return NULL;
	MemHeapFreeBytes(0, &freeBytes, &maxChunk);
	if (maxChunk > size + KEEP_SPARE)
		return (const uint8 *)MemPtrNew(size);
	return slot_alloc(&gKeepSlot, size);
}

void plat_keep_write(const uint8 *keep, uint32 at, const void *src, uint32 n)
{
	if (slot_owns(&gKeepSlot, keep))
		DmWrite((void *)keep, at, (void *)src, n);
	else
		MemMove((UInt8 *)keep + at, src, (Int32)n);
}

void plat_keep_free(const uint8 *keep)
{
	if (!keep)
		return;
	if (slot_owns(&gKeepSlot, keep))
		slot_free(&gKeepSlot);
	else
		MemPtrFree((MemPtr)keep);
}

const uint8 *plat_store_alloc(uint32 size)
{
	return slot_alloc(&gGridSlot, size);
}

void plat_store_write(const uint8 *p, uint32 at, const void *src, uint32 n)
{
	DmWrite((void *)p, at, (void *)src, n);
}

void plat_store_free(const uint8 *p)
{
	if (p && slot_owns(&gGridSlot, p))
		slot_free(&gGridSlot);
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
