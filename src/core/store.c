/* Options, high scores and the highest level, and their saved form. */
#include "store.h"
#include "plat.h"

#define SAVE_NAME "ArashiSave"
#define MAGIC FOURCC('A', 'R', 'S', 'H')

Store Saved;

SEG_FRONT static uint16 checksum(const uint8 *p, uint32 n);
SEG_FRONT static void put16(uint8 *p, uint16 v);
SEG_FRONT static void put32(uint8 *p, uint32 v);

SEG_FRONT static uint16 checksum(const uint8 *p, uint32 n)
{
	uint16 a = 1, b = 0;

	while (n--) {
		a = (uint16)(a + *p++);
		b = (uint16)(b + a);
	}
	return (uint16)(a ^ (uint16)(b << 3) ^ b);
}

SEG_FRONT static void put16(uint8 *p, uint16 v)
{
	p[0] = (uint8)(v >> 8);
	p[1] = (uint8)v;
}

SEG_FRONT static void put32(uint8 *p, uint32 v)
{
	put16(p, (uint16)(v >> 16));
	put16(p + 2, (uint16)v);
}

SEG_FRONT void store_defaults(void)
{
	int16 t, i, k;
	uint32 size;

	Saved.control = 0;
	Saved.practice = 0;
	Saved.highest = 1;
	for (t = 0; t < 2; t++) {
		const uint8 *scor = plat_resource(FOURCC('S', 'C', 'O', 'R'), (int16)(128 + t), &size);

		for (i = 0; i < STORE_ENTRIES; i++) {
			ScoreEntry *e = &Saved.table[t][i];

			e->score = 0;
			for (k = 0; k < STORE_NAME; k++)
				e->name[k] = '-';
			/* Entry 0 of the resource is the original's spare one. */
			if (scor && size >= (uint32)((i + 2) * 10)) {
				e->score = (int32)get32(scor + (i + 1) * 10);
				for (k = 0; k < STORE_NAME; k++)
					e->name[k] = (char)scor[(i + 1) * 10 + 4 + k];
			}
		}
		if (scor)
			plat_release(scor);
	}
}

SEG_FRONT uint32 store_encode(uint8 *out)
{
	uint8 *p = out;
	int16 t, i, k;

	put32(p, MAGIC);
	put16(p + 4, STORE_VERSION);
	put16(p + 6, (uint16)(Saved.control | (Saved.practice << 8)));
	put16(p + 8, (uint16)Saved.highest);
	put16(p + 10, STORE_ENTRIES);
	p += 12;
	for (t = 0; t < 2; t++)
		for (i = 0; i < STORE_ENTRIES; i++) {
			put32(p, (uint32)Saved.table[t][i].score);
			for (k = 0; k < STORE_NAME; k++)
				p[4 + k] = (uint8)Saved.table[t][i].name[k];
			p += 10;
		}
	put16(p, checksum(out, (uint32)(p - out)));
	return (uint32)(p + 2 - out);
}

SEG_FRONT cbool store_decode(const uint8 *in, uint32 size)
{
	const uint8 *p;
	int16 t, i, k;
	uint16 w;

	store_defaults();
	if (!in || size != STORE_BYTES || get32(in) != MAGIC || get16(in + 4) != STORE_VERSION ||
	    get16(in + 10) != STORE_ENTRIES ||
	    get16(in + STORE_BYTES - 2) != checksum(in, STORE_BYTES - 2))
		return 0;
	w = get16(in + 6);
	if ((w & 0xFE) != 0 || (w >> 8) > 1)
		return 0;
	Saved.control = (uint8)(w & 0xFF);
	Saved.practice = (uint8)(w >> 8);
	Saved.highest = gets16(in + 8);
	if (Saved.highest < 1)
		Saved.highest = 1;
	p = in + 12;
	for (t = 0; t < 2; t++)
		for (i = 0; i < STORE_ENTRIES; i++) {
			Saved.table[t][i].score = gets32(p);
			for (k = 0; k < STORE_NAME; k++) {
				uint8 c = p[4 + k];

				Saved.table[t][i].name[k] = (char)(c >= 32 && c < 127 ? c : '-');
			}
			p += 10;
		}
	return 1;
}

SEG_FRONT void store_save(void)
{
	uint8 buf[STORE_BYTES];
	uint32 n = store_encode(buf);

	plat_save(SAVE_NAME, buf, n);
}

SEG_FRONT void store_load(void)
{
	uint8 buf[STORE_BYTES];
	int32 n = plat_load(SAVE_NAME, buf, STORE_BYTES);

	if (n != STORE_BYTES || !store_decode(buf, STORE_BYTES))
		store_defaults();
}

SEG_FRONT int16 store_rank(int16 table, int32 score)
{
	int16 i;

	for (i = 0; i < STORE_ENTRIES; i++)
		if (score > Saved.table[table][i].score)
			return i;
	return -1;
}

SEG_FRONT int16 store_insert(int16 table, int32 score, const char *name)
{
	int16 r = store_rank(table, score), i, k;

	if (r < 0)
		return -1;
	for (i = STORE_ENTRIES - 1; i > r; i--)
		Saved.table[table][i] = Saved.table[table][i - 1];
	Saved.table[table][r].score = score;
	for (k = 0; k < STORE_NAME; k++) {
		if (*name)
			Saved.table[table][r].name[k] = *name++;
		else
			Saved.table[table][r].name[k] = ' ';
	}
	return r;
}

SEG_FRONT void store_reached(int16 level)
{
	if (level > Saved.highest)
		Saved.highest = level;
}
