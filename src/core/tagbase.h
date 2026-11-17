/* Tagged blob store, after Juri Munkki's CTagBase, cut down to what the
   script parser uses: write a blob under a 32-bit tag, get a pointer to it
   back. Each blob is its own allocation, so pointers stay valid until that
   tag is written again with a different size or the base is freed. */
#ifndef CORE_TAGBASE_H
#define CORE_TAGBASE_H

#include "types.h"

#define TAGBASE_HASH 64         /* power of 2 */

typedef struct {
	int32  tag;
	uint32 len;
	void  *data;
	int16  link;                /* next entry with the same hash, or -1 */
} TagEntry;

typedef struct {
	TagEntry *entry;
	int16     count, cap;
	int16     hash[TAGBASE_HASH];
} TagBase;

SEG_SCRIPT void  tagbase_init(TagBase *b);
SEG_SCRIPT void  tagbase_free(TagBase *b);
/* Stores a copy of len bytes under tag, replacing what was there.
   Returns 0 when out of memory. */
SEG_SCRIPT cbool tagbase_write(TagBase *b, int32 tag, uint32 len, const void *data);
/* The blob stored under tag, or NULL. */
SEG_SCRIPT void *tagbase_get(const TagBase *b, int32 tag);

#endif
