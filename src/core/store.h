/* What is kept between runs: the options, the highest level reached and the
   two high score tables (arcade and practice, STORE_ENTRIES each as in the
   original). The saved form is big-endian with a magic, a version and a
   checksum, and is read byte by byte. */
#ifndef CORE_STORE_H
#define CORE_STORE_H

#include "types.h"

#define STORE_ENTRIES 50
#define STORE_NAME    6
#define STORE_VERSION 1
#define STORE_BYTES   (12 + 2 * STORE_ENTRIES * 10 + 2)

typedef struct {
	int32 score;
	char  name[STORE_NAME];     /* padded with spaces */
} ScoreEntry;

typedef struct {
	uint8      control;         /* 0 stylus sets the claw's place, 1 it drags it */
	uint8      practice;        /* the practice tables and level choices */
	int16      highest;         /* highest level reached */
	ScoreEntry table[2][STORE_ENTRIES];
} Store;

extern Store Saved;

/* The original's starting tables (SCOR 128 and 129) and no options. */
SEG_FRONT void store_defaults(void);
/* Writes Saved to out (STORE_BYTES). Returns the size. */
SEG_FRONT uint32 store_encode(uint8 *out);
/* Reads a saved form. On anything wrong (short, other magic or version, bad
   checksum) Saved is the defaults and 0 comes back. */
SEG_FRONT cbool store_decode(const uint8 *in, uint32 size);
/* plat_save / plat_load of the saved form under one name. */
SEG_FRONT void store_save(void);
SEG_FRONT void store_load(void);
/* Where a score would go in table (0 arcade, 1 practice), 0 to
   STORE_ENTRIES - 1, or -1 if it is too low. Equal scores go below. */
SEG_FRONT int16 store_rank(int16 table, int32 score);
/* Puts the score in at its rank and moves the rest down. Returns the rank
   or -1. */
SEG_FRONT int16 store_insert(int16 table, int32 score, const char *name);
/* The highest level reached counts only if higher. */
SEG_FRONT void store_reached(int16 level);

#endif
