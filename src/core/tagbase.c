/* See tagbase.h. Same tag hash as the original. */
#include "tagbase.h"
#include "plat.h"

#define TAG_CLUMP 32

SEG_SCRIPT static int16 hash_tag(int32 tag)
{
	uint32 t = (uint32)tag;
	t += (uint32)((int32)t >> 16);
	t += (uint32)((int32)t >> 8);
	return (int16)(t & (TAGBASE_HASH - 1));
}

SEG_SCRIPT static void copy_bytes(void *to, const void *from, uint32 n)
{
	uint8 *t = (uint8 *)to;
	const uint8 *f = (const uint8 *)from;
	while (n--)
		*t++ = *f++;
}

SEG_SCRIPT void tagbase_init(TagBase *b)
{
	int16 i;

	b->entry = NULL;
	b->count = 0;
	b->cap = 0;
	for (i = 0; i < TAGBASE_HASH; i++)
		b->hash[i] = -1;
}

SEG_SCRIPT void tagbase_free(TagBase *b)
{
	int16 i;

	for (i = 0; i < b->count; i++)
		if (b->entry[i].data)
			plat_free(b->entry[i].data);
	if (b->entry)
		plat_free(b->entry);
	tagbase_init(b);
}

SEG_SCRIPT static int16 lookup(const TagBase *b, int32 tag)
{
	int16 i;

	for (i = b->hash[hash_tag(tag)]; i >= 0; i = b->entry[i].link)
		if (b->entry[i].tag == tag)
			break;
	return i;
}

SEG_SCRIPT cbool tagbase_write(TagBase *b, int32 tag, uint32 len, const void *data)
{
	int16 i = lookup(b, tag);
	TagEntry *e;
	void *p;

	if (i >= 0 && b->entry[i].len == len) {
		if (len)
			copy_bytes(b->entry[i].data, data, len);
		return 1;
	}
	p = plat_alloc(len ? len : 1);
	if (!p)
		return 0;
	if (len)
		copy_bytes(p, data, len);
	if (i >= 0) {
		plat_free(b->entry[i].data);
		b->entry[i].data = p;
		b->entry[i].len = len;
		return 1;
	}
	if (b->count == b->cap) {
		int16 cap = (int16)(b->cap + TAG_CLUMP);
		TagEntry *n = (TagEntry *)plat_alloc((uint32)cap * sizeof(TagEntry));
		if (!n) {
			plat_free(p);
			return 0;
		}
		if (b->count) {
			copy_bytes(n, b->entry, (uint32)b->count * sizeof(TagEntry));
			plat_free(b->entry);
		}
		b->entry = n;
		b->cap = cap;
	}
	i = b->count++;
	e = &b->entry[i];
	e->tag = tag;
	e->len = len;
	e->data = p;
	e->link = b->hash[hash_tag(tag)];
	b->hash[hash_tag(tag)] = i;
	return 1;
}

SEG_SCRIPT void *tagbase_get(const TagBase *b, int32 tag)
{
	int16 i = lookup(b, tag);
	return i >= 0 ? b->entry[i].data : NULL;
}
