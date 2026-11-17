/* Writes LVAL 128: the script's values of levels 1 to LVAL_LEVELS, so the
   device loads a level without running the script. Fails if the script
   draws a random number, since then the values aren't fixed.

   usage: mklval DATADIR OUT.bin */
#include <stdio.h>
#include <stdlib.h>
#include "../core/game.h"
#include "../core/qdrandom.h"
#include "plat_host.h"

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

int main(int argc, char **argv)
{
	static uint8 buf[LVAL_LEVELS * LVAL_ENTRY];
	FILE *f;
	int level;

	if (argc != 3) {
		fprintf(stderr, "usage: mklval DATADIR OUT.bin\n");
		return 2;
	}
	host_set_data_dir(argv[1]);
	for (level = 1; level <= LVAL_LEVELS; level++) {
		int32 seed = qd_get_seed();

		if (!game_level_values((int16)level, buf + (level - 1) * LVAL_ENTRY)) {
			fprintf(stderr, "mklval: no script for level %d\n", level);
			return 1;
		}
		if (qd_get_seed() != seed) {
			fprintf(stderr, "mklval: level %d's script draws random numbers\n", level);
			return 1;
		}
	}
	f = fopen(argv[2], "wb");
	if (!f || fwrite(buf, 1, sizeof(buf), f) != sizeof(buf)) {
		perror(argv[2]);
		return 1;
	}
	fclose(f);
	printf("mklval: %d levels, %d bytes in %s\n", LVAL_LEVELS, (int)sizeof(buf), argv[2]);
	return 0;
}
