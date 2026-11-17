/* Host test for the screen test pattern and layout: renders the pattern at
   1, 2, 4, 8 and 16 bit into PNGs and checks pixel values. Usage:
   test_testpat OUTDIR */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/plat.h"
#include "../core/testpat.h"
#include "png.h"

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; \
	printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* The RGB the host shows for a pixel value. Palette entries 0 to 15 are the
   greys, 16 to 22 the colours, as testpat expects of pal[]. */
static void host_rgb(const PlatScreen *s, uint16 v, uint8 *rgb)
{
	static const uint8 col[7][3] = {
		{ 255, 0, 0 }, { 0, 200, 0 }, { 0, 0, 255 }, { 255, 255, 0 },
		{ 0, 255, 255 }, { 255, 0, 255 }, { 255, 128, 0 }
	};

	if (s->format == PLAT_PIX_GREY) {
		int max = (1 << s->depth) - 1;

		rgb[0] = rgb[1] = rgb[2] = (uint8)(255 - v * 255 / max);
	} else if (s->format == PLAT_PIX_PALETTE) {
		if (v < 16)
			rgb[0] = rgb[1] = rgb[2] = (uint8)(v * 255 / 15);
		else if (v < 23)
			memcpy(rgb, col[v - 16], 3);
		else
			rgb[0] = rgb[1] = rgb[2] = 0;
	} else {
		rgb[0] = (uint8)(((v >> 11) & 31) * 255 / 31);
		rgb[1] = (uint8)(((v >> 5) & 63) * 255 / 63);
		rgb[2] = (uint8)((v & 31) * 255 / 31);
	}
}

static void make_screen(PlatScreen *s, int16 w, int16 h, uint8 depth)
{
	int i;

	memset(s, 0, sizeof(*s));
	s->width = w;
	s->height = h;
	s->depth = depth;
	s->format = depth == 16 ? PLAT_PIX_RGB565 : depth == 8 ? PLAT_PIX_PALETTE : PLAT_PIX_GREY;
	/* Rows are padded to 2 bytes, as Palm bitmaps are. */
	s->row_bytes = (int16)((((int32)w * depth + 15) / 16) * 2);
	s->bits = (uint8 *)calloc((size_t)s->row_bytes * h, 1);
	for (i = 0; i < 23; i++)
		s->pal[i] = (uint8)i;
	plat_layout(s);
}

static void save_png(const PlatScreen *s, const char *dir, const char *name)
{
	char path[512];
	uint8 *rgb = (uint8 *)malloc((size_t)s->width * s->height * 3);
	int x, y;

	for (y = 0; y < s->height; y++)
		for (x = 0; x < s->width; x++)
			host_rgb(s, testpat_get(s, (int16)x, (int16)y), rgb + ((size_t)y * s->width + x) * 3);
	sprintf(path, "%s/%s", dir, name);
	if (!png_write_rgb(path, rgb, s->width, s->height))
		printf("cannot write %s\n", path);
	free(rgb);
}

static void check_depth(const char *dir, uint8 depth)
{
	PlatScreen s;
	PlatRect r;
	int16 i, n, size;
	char name[64];
	uint16 black, white;

	make_screen(&s, 160, 160, depth);
	size = s.play.w;
	black = testpat_black(&s);
	white = testpat_white(&s);
	testpat_draw(&s, 1, 100, 100);
	sprintf(name, "testpat-%d.png", depth);
	save_png(&s, dir, name);

	CHECK(s.play.x == 0 && s.play.y == 0 && size == 160, "play area at %d", depth);
	CHECK(testpat_get(&s, 0, 0) == black, "corner black at %d", depth);
	CHECK(testpat_get(&s, 159, 159) == black, "far corner black at %d", depth);
	CHECK(testpat_get(&s, 80, 159) == black, "bottom edge black at %d", depth);
	CHECK(testpat_get(&s, 80, 140) == white, "inside white at %d", depth);
	n = testpat_levels(&s);
	CHECK(n == (depth <= 4 ? (1 << depth) : 16), "levels %d at %d", n, depth);
	for (i = 0; i < n; i++) {
		testpat_ramp_rect(&s, i, &r);
		CHECK(testpat_get(&s, (int16)(r.x + r.w / 2), (int16)(r.y + r.h / 2)) ==
		      testpat_grey(&s, i, n), "ramp %d at %d", i, depth);
	}
	testpat_ramp_rect(&s, 0, &r);
	CHECK(testpat_get(&s, (int16)(r.x + r.w / 2), (int16)(r.y + r.h / 2)) == black,
	      "ramp starts black at %d", depth);
	testpat_ramp_rect(&s, (int16)(n - 1), &r);
	CHECK(testpat_get(&s, (int16)(r.x + r.w / 2), (int16)(r.y + r.h / 2)) == white,
	      "ramp ends white at %d", depth);
	CHECK(testpat_get(&s, 100, 100) == black, "cross centre at %d", depth);
	CHECK(testpat_get(&s, 100, 106) == black && testpat_get(&s, 94, 100) == black,
	      "cross arms at %d", depth);
	CHECK(testpat_get(&s, 105, 105) == white, "cross is a cross at %d", depth);
	if (depth >= 8) {
		int16 y = (int16)(size / 2 + size / 16);

		for (i = 0; i < 7; i++) {
			int16 x = (int16)(size / 10 + (size - size / 5) * (2 * i + 1) / 14);

			CHECK(testpat_get(&s, x, y) == testpat_colour(&s, i), "colour %d at %d", i, depth);
		}
	}
	free(s.bits);
}

static void check_layout(const char *dir)
{
	PlatScreen s;
	static const struct {
		int16 w, h, px, py, sx, sy, sw, sh, dx, dw;
	} t[] = {
		{ 160, 160, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 320, 320, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 320, 480, 0, 0, 0, 320, 320, 160, 0, 0 },
		{ 480, 320, 0, 0, 320, 0, 160, 320, 0, 0 },
		{ 560, 160, 200, 0, 0, 0, 200, 160, 360, 200 },
		{ 400, 160, 120, 0, 0, 0, 120, 160, 280, 120 },
		{ 390, 160, 0, 0, 160, 0, 230, 160, 0, 0 },
		{ 160, 170, 0, 5, 0, 0, 0, 0, 0, 0 },
		{ 170, 160, 5, 0, 0, 0, 0, 0, 0, 0 }
	};
	int i;

	for (i = 0; i < (int)(sizeof(t) / sizeof(t[0])); i++) {
		make_screen(&s, t[i].w, t[i].h, 1);
		CHECK(s.play.x == t[i].px && s.play.y == t[i].py &&
		      s.play.w == (t[i].w < t[i].h ? t[i].w : t[i].h) && s.play.h == s.play.w,
		      "play area of %dx%d: %d,%d %d", t[i].w, t[i].h, s.play.x, s.play.y, s.play.w);
		CHECK(s.status.x == t[i].sx && s.status.y == t[i].sy &&
		      s.status.w == t[i].sw && s.status.h == t[i].sh,
		      "status area of %dx%d: %d,%d %dx%d", t[i].w, t[i].h,
		      s.status.x, s.status.y, s.status.w, s.status.h);
		CHECK(s.side.x == t[i].dx && s.side.w == t[i].dw &&
		      (s.side.w == 0 || (s.side.y == 0 && s.side.h == s.play.h)),
		      "side part of %dx%d: %d,%d %dx%d", t[i].w, t[i].h,
		      s.side.x, s.side.y, s.side.w, s.side.h);
		free(s.bits);
	}
	/* The wide layouts, as PNGs. */
	make_screen(&s, 320, 480, 16);
	testpat_draw(&s, 1, 160, 100);
	save_png(&s, dir, "testpat-320x480-16.png");
	free(s.bits);
	make_screen(&s, 560, 160, 4);
	testpat_draw(&s, 0, 0, 0);
	save_png(&s, dir, "testpat-560x160-4.png");
	free(s.bits);
	make_screen(&s, 480, 320, 8);
	testpat_draw(&s, 1, 100, 100);
	save_png(&s, dir, "testpat-480x320-8.png");
	free(s.bits);
	make_screen(&s, 160, 160, 16);
	s.le16 = 1;
	testpat_draw(&s, 0, 0, 0);
	CHECK(testpat_get(&s, 0, 0) == 0 && s.bits[0] == 0 && s.bits[5 * s.row_bytes + 2] == 0xFF && testpat_get(&s, 1, 5) == 0xFFFF &&
	      testpat_get(&s, 80, 120) == 0xFFFF, "little endian pixels");
	free(s.bits);
}

static void check_rim(void)
{
	static const struct {
		int16 x, y, rim;
	} t[] = {
		{ 50, 0, 0 }, { 99, 50, 64 }, { 50, 99, 128 }, { 0, 50, 192 },
		{ 99, 0, 32 }, { 99, 99, 96 }, { 0, 99, 160 }, { 0, 0, 224 },
		{ 75, 0, 19 }, { 99, 25, 45 }
	};
	int i;

	for (i = 0; i < (int)(sizeof(t) / sizeof(t[0])); i++) {
		int16 r = plat_rim_from_point(t[i].x, t[i].y, 100);
		int d = r - t[i].rim;

		if (d > 128)
			d -= 256;
		if (d < -128)
			d += 256;
		CHECK(d >= -3 && d <= 3, "rim of %d,%d is %d, want %d", t[i].x, t[i].y, r, t[i].rim);
	}
	CHECK(plat_rim_from_point(0, 0, 1) == -1, "rim at the centre");
}

int main(int argc, char **argv)
{
	const char *dir = argc > 1 ? argv[1] : "build/host";
	static const uint8 depths[] = { 1, 2, 4, 8, 16 };
	int i;

	for (i = 0; i < 5; i++)
		check_depth(dir, depths[i]);
	check_layout(dir);
	check_rim();
	printf("testpat: %s\n", failures ? "FAILED" : "ok");
	return failures ? 1 : 0;
}
