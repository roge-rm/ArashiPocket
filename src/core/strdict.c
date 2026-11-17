/* See strdict.h. Same hash as the original (first + last + middle byte). */
#include "strdict.h"
#include "plat.h"

#define DICT_CLUMP  16
#define WORD_CLUMP  256

static int16 hash_word(const uint8 *s, int16 len)
{
	if (len > 0)
		return (int16)((s[0] + s[len - 1] + s[len >> 1]) & (STRDICT_HASH - 1));
	return 0;
}

static void copy_bytes(void *to, const void *from, uint32 n)
{
	uint8 *t = (uint8 *)to;
	const uint8 *f = (const uint8 *)from;
	while (n--)
		*t++ = *f++;
}

void strdict_init(StrDict *d)
{
	int16 i;

	d->count = 0;
	d->cap = 0;
	d->offset = NULL;
	d->link = NULL;
	d->words = NULL;
	d->words_used = 0;
	d->words_cap = 0;
	for (i = 0; i < STRDICT_HASH; i++)
		d->hash[i] = -1;
}

void strdict_free(StrDict *d)
{
	if (d->offset)
		plat_free(d->offset);
	if (d->link)
		plat_free(d->link);
	if (d->words)
		plat_free(d->words);
	strdict_init(d);
}

int16 strdict_add(StrDict *d, const uint8 *word, int16 len)
{
	int16 h, token;

	if (len < 0)
		len = 0;
	if (len > 255)
		len = 255;
	if (d->count == d->cap) {
		int16 cap = (int16)(d->cap + DICT_CLUMP);
		uint32 *off = (uint32 *)plat_alloc((uint32)cap * sizeof(uint32));
		int16 *link = (int16 *)plat_alloc((uint32)cap * sizeof(int16));
		if (!off || !link) {
			if (off)
				plat_free(off);
			if (link)
				plat_free(link);
			return -1;
		}
		if (d->count) {
			copy_bytes(off, d->offset, (uint32)d->count * sizeof(uint32));
			copy_bytes(link, d->link, (uint32)d->count * sizeof(int16));
			plat_free(d->offset);
			plat_free(d->link);
		}
		d->offset = off;
		d->link = link;
		d->cap = cap;
	}
	if (d->words_used + (uint32)len + 1 > d->words_cap) {
		uint32 cap = d->words_cap + WORD_CLUMP;
		uint8 *w;
		while (d->words_used + (uint32)len + 1 > cap)
			cap += WORD_CLUMP;
		w = (uint8 *)plat_alloc(cap);
		if (!w)
			return -1;
		if (d->words) {
			copy_bytes(w, d->words, d->words_used);
			plat_free(d->words);
		}
		d->words = w;
		d->words_cap = cap;
	}
	d->words[d->words_used] = (uint8)len;
	copy_bytes(d->words + d->words_used + 1, word, (uint32)len);
	token = d->count++;
	h = hash_word(word, len);
	d->offset[token] = d->words_used;
	d->link[token] = d->hash[h];
	d->hash[h] = token;
	d->words_used += (uint32)len + 1;
	return token;
}

int16 strdict_search(const StrDict *d, const uint8 *word, int16 len)
{
	int16 walk, i;
	const uint8 *p;

	if (len < 0)
		len = 0;
	if (len > 255)
		len = 255;
	for (walk = d->hash[hash_word(word, len)]; walk >= 0; walk = d->link[walk]) {
		p = d->words + d->offset[walk];
		if (*p++ != len)
			continue;
		for (i = 0; i < len && p[i] == word[i]; i++)
			;
		if (i == len)
			return walk;
	}
	return -1;
}

int16 strdict_find(StrDict *d, const uint8 *word, int16 len)
{
	int16 token = strdict_search(d, word, len);
	return token >= 0 ? token : strdict_add(d, word, len);
}

const uint8 *strdict_word(const StrDict *d, int16 token, int16 *len)
{
	const uint8 *p;

	if (token < 0 || token >= d->count) {
		*len = 0;
		return NULL;
	}
	p = d->words + d->offset[token];
	*len = p[0];
	return p + 1;
}
