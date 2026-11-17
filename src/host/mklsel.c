/* Writes LSEL 128: the facts the level select needs of each level it walks
   (game_level_choices), worked out by running the script here so the
   device doesn't have to. Walks the levels as the select does in practice
   mode and in arcade mode with no highest level, so any highest level is
   covered. Levels past the table still run the script on the device.

   usage: mklsel DATADIR OUT.bin */
#include <stdio.h>
#include <stdlib.h>
#include "../core/game.h"
#include "plat_host.h"

#define MAXENTRIES 1000
#define MAXLEVEL 32000

/* No screen: the script is all this runs. */
const PlatScreen *plat_screen(void)
{
	return NULL;
}

void plat_present(void)
{
}

void plat_present_rect(const PlatRect *r)
{
	(void)r;
}

uint8 plat_palette_index(uint8 r, uint8 g, uint8 b)
{
	return (uint8)(r ^ g ^ b);
}

static LevelRestart table[MAXENTRIES];
static int count;

static const LevelRestart *find(int16 level)
{
	int i;

	for (i = 0; i < count; i++)
		if (table[i].level == level)
			return &table[i];
	if (count >= MAXENTRIES) {
		fprintf(stderr, "mklsel: more than %d levels\n", MAXENTRIES);
		exit(1);
	}
	if (!game_level_restart(level, &table[count])) {
		fprintf(stderr, "mklsel: no script for level %d\n", level);
		exit(1);
	}
	return &table[count++];
}

/* game_level_choices' walk, with no limit in arcade mode. */
static void walk(int practice)
{
	int n = 0, level = 1, flag;

	do {
		do {
			const LevelRestart *r = find((int16)level);

			flag = practice ? r->practice : r->arcade;
			if (flag == 1)
				n++;
			level = r->next < 1 ? 1 : r->next;
		} while (flag == 0 && level < MAXLEVEL);
	} while (n < 95 && flag >= 0 && level < MAXLEVEL);
}

static void put16(FILE *f, int v)
{
	fputc((v >> 8) & 255, f);
	fputc(v & 255, f);
}

int main(int argc, char **argv)
{
	FILE *f;
	int i;

	if (argc != 3) {
		fprintf(stderr, "usage: mklsel DATADIR OUT.bin\n");
		return 2;
	}
	host_set_data_dir(argv[1]);
	walk(1);
	walk(0);
	f = fopen(argv[2], "wb");
	if (!f) {
		perror(argv[2]);
		return 1;
	}
	for (i = 0; i < count; i++) {
		const LevelRestart *r = &table[i];

		put16(f, r->level);
		put16(f, r->next);
		put16(f, r->arcade);
		put16(f, r->practice);
		put16(f, r->field);
		put16(f, r->color);
		put16(f, (int)((r->startBonus >> 16) & 0xFFFF));
		put16(f, (int)(r->startBonus & 0xFFFF));
	}
	fclose(f);
	printf("mklsel: %d levels, %d bytes in %s\n", count, count * LSEL_ENTRY, argv[2]);
	return 0;
}
