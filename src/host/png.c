#include <stdio.h>
#include <stdlib.h>
#include "png.h"

static unsigned long crc_table[256];

static void make_crc(void)
{
	unsigned long c;
	int n, k;

	for (n = 0; n < 256; n++) {
		c = (unsigned long)n;
		for (k = 0; k < 8; k++)
			c = (c & 1) ? 0xEDB88320UL ^ (c >> 1) : c >> 1;
		crc_table[n] = c;
	}
}

static unsigned long crc_update(unsigned long crc, const unsigned char *p, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		crc = crc_table[(crc ^ p[i]) & 255] ^ (crc >> 8);
	return crc;
}

static void put32(unsigned char *p, unsigned long v)
{
	p[0] = (unsigned char)(v >> 24);
	p[1] = (unsigned char)(v >> 16);
	p[2] = (unsigned char)(v >> 8);
	p[3] = (unsigned char)v;
}

static void chunk(FILE *f, const char *type, const unsigned char *data, size_t n)
{
	unsigned char head[8], tail[4];
	unsigned long crc;

	put32(head, (unsigned long)n);
	head[4] = (unsigned char)type[0];
	head[5] = (unsigned char)type[1];
	head[6] = (unsigned char)type[2];
	head[7] = (unsigned char)type[3];
	fwrite(head, 1, 8, f);
	if (n)
		fwrite(data, 1, n, f);
	crc = crc_update(0xFFFFFFFFUL, head + 4, 4);
	if (n)
		crc = crc_update(crc, data, n);
	put32(tail, crc ^ 0xFFFFFFFFUL);
	fwrite(tail, 1, 4, f);
}

int png_write_rgb(const char *path, const unsigned char *rgb, int width, int height)
{
	static const unsigned char sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	size_t rowlen = (size_t)width * 3 + 1;
	size_t rawlen = rowlen * (size_t)height;
	unsigned char *raw = (unsigned char *)malloc(rawlen);
	unsigned char *z;
	size_t zlen, pos = 0, at, blocks;
	unsigned char ihdr[13];
	unsigned long a = 1, b = 0;
	FILE *f;
	int y;
	size_t i;

	if (!raw)
		return 0;
	make_crc();
	for (y = 0; y < height; y++) {
		raw[(size_t)y * rowlen] = 0;
		for (i = 0; i < (size_t)width * 3; i++)
			raw[(size_t)y * rowlen + 1 + i] = rgb[(size_t)y * width * 3 + i];
	}
	for (i = 0; i < rawlen; i++) {
		a = (a + raw[i]) % 65521UL;
		b = (b + a) % 65521UL;
	}
	blocks = (rawlen + 65534) / 65535;
	zlen = 2 + rawlen + blocks * 5 + 4;
	z = (unsigned char *)malloc(zlen);
	if (!z) {
		free(raw);
		return 0;
	}
	z[pos++] = 0x78;
	z[pos++] = 0x01;
	for (at = 0; at < rawlen; at += 65535) {
		size_t n = rawlen - at < 65535 ? rawlen - at : 65535;

		z[pos++] = (unsigned char)(at + n >= rawlen);
		z[pos++] = (unsigned char)(n & 255);
		z[pos++] = (unsigned char)(n >> 8);
		z[pos++] = (unsigned char)(~n & 255);
		z[pos++] = (unsigned char)((~n >> 8) & 255);
		for (i = 0; i < n; i++)
			z[pos++] = raw[at + i];
	}
	put32(z + pos, (b << 16) | a);
	pos += 4;
	put32(ihdr, (unsigned long)width);
	put32(ihdr + 4, (unsigned long)height);
	ihdr[8] = 8;
	ihdr[9] = 2;
	ihdr[10] = ihdr[11] = ihdr[12] = 0;
	f = fopen(path, "wb");
	if (!f) {
		free(raw);
		free(z);
		return 0;
	}
	fwrite(sig, 1, 8, f);
	chunk(f, "IHDR", ihdr, 13);
	chunk(f, "IDAT", z, pos);
	chunk(f, "IEND", NULL, 0);
	fclose(f);
	free(raw);
	free(z);
	return 1;
}
