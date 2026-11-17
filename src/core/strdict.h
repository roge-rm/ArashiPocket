/* String dictionary, after Juri Munkki's CStringDictionary: words get
   tokens 0, 1, 2, ... in the order they are added, found through a hash
   table. Comparison is byte for byte, case sensitive, up to 255 bytes. */
#ifndef CORE_STRDICT_H
#define CORE_STRDICT_H

#include "types.h"

#define STRDICT_HASH 128        /* power of 2 */

typedef struct {
	int16  count;
	int16  cap;                 /* entries allocated */
	uint32 *offset;             /* word i starts at words + offset[i] (length byte) */
	int16  *link;               /* next entry with the same hash, or -1 */
	uint8  *words;              /* length-prefixed words */
	uint32 words_used, words_cap;
	int16  hash[STRDICT_HASH];
} StrDict;

void  strdict_init(StrDict *d);
void  strdict_free(StrDict *d);
/* Returns the new token, or -1 when out of memory. */
int16 strdict_add(StrDict *d, const uint8 *word, int16 len);
/* Returns the token, or -1 if not there. */
int16 strdict_search(const StrDict *d, const uint8 *word, int16 len);
/* Finds the word, adding it if it isn't there. */
int16 strdict_find(StrDict *d, const uint8 *word, int16 len);
/* The word for a token, as length and bytes (not terminated). */
const uint8 *strdict_word(const StrDict *d, int16 token, int16 *len);

#endif
