/* Host test for the renderer (va.c, vatext.c, vademo.c): draws a test
   scene at 1, 2, 4, 8 and 16 bit (and 16 bit low byte first) into PNGs and
   checks erase by redraw, clipping and line ends. Usage:
   test_va DATADIR OUTDIR */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/plat.h"
#include "../core/va.h"
#include "../core/vademo.h"
#include "../core/testpat.h"
#include "plat_host.h"
#include "png.h"

static int failures;
static PlatScreen screen;
static cbool have_screen;
static int presents;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; \
	printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

const PlatScreen *plat_screen(void)
{
	return have_screen ? &screen : NULL;
}

void plat_present(void)
{
	presents++;
}

void plat_present_rect(const PlatRect *r)
{
	if (r->x < 0 || r->y < 0 || r->w <= 0 || r->h <= 0 ||
	    r->x + r->w > screen.width || r->y + r->h > screen.height)
		failures++, printf("FAIL present rect %d,%d %dx%d\n", r->x, r->y, r->w, r->h);
	presents++;
}

/* The host's 8 bit palette is a 6 x 6 x 6 colour cube. */
uint8 plat_palette_index(uint8 r, uint8 g, uint8 b)
{
	return (uint8)(((r + 25) / 51) * 36 + ((g + 25) / 51) * 6 + (b + 25) / 51);
}

static void host_rgb(const PlatScreen *s, uint16 v, uint8 *rgb)
{
	if (s->format == PLAT_PIX_GREY) {
		int max = (1 << s->depth) - 1;

		rgb[0] = rgb[1] = rgb[2] = (uint8)(255 - v * 255 / max);
	} else if (s->format == PLAT_PIX_PALETTE) {
		rgb[0] = (uint8)((v / 36) * 51);
		rgb[1] = (uint8)(((v / 6) % 6) * 51);
		rgb[2] = (uint8)((v % 6) * 51);
	} else {
		rgb[0] = (uint8)(((v >> 11) & 31) * 255 / 31);
		rgb[1] = (uint8)(((v >> 5) & 63) * 255 / 63);
		rgb[2] = (uint8)((v & 31) * 255 / 31);
	}
}

static void make_screen(int16 w, int16 h, uint8 depth, uint8 le16)
{
	free(screen.bits);
	memset(&screen, 0, sizeof(screen));
	screen.width = w;
	screen.height = h;
	screen.depth = depth;
	screen.le16 = le16;
	screen.format = depth == 16 ? PLAT_PIX_RGB565 : depth == 8 ? PLAT_PIX_PALETTE : PLAT_PIX_GREY;
	screen.row_bytes = (int16)((((int32)w * depth + 15) / 16) * 2);
	screen.bits = (uint8 *)calloc((size_t)screen.row_bytes * h, 1);
	plat_layout(&screen);
	have_screen = 1;
}

static void save_png(const char *dir, const char *name)
{
	char path[512];
	uint8 *rgb = (uint8 *)malloc((size_t)screen.width * screen.height * 3);
	int x, y;

	for (y = 0; y < screen.height; y++)
		for (x = 0; x < screen.width; x++)
			host_rgb(&screen, testpat_get(&screen, (int16)x, (int16)y),
			         rgb + ((size_t)y * screen.width + x) * 3);
	sprintf(path, "%s/%s", dir, name);
	if (!png_write_rgb(path, rgb, screen.width, screen.height))
		printf("cannot write %s\n", path);
	free(rgb);
}

/* Native value of a pixel in play-area coordinates; the stored 16 bit
   word is turned to RGB565 as testpat_get reads it. */
static uint16 play_get(int16 x, int16 y)
{
	return testpat_get(&screen, (int16)(screen.play.x + x), (int16)(screen.play.y + y));
}

/* What testpat_get reads for colour c: VAPixelValue holds a grey level
   repeated across the byte or a word in framebuffer order. */
static uint16 read_value(int16 c)
{
	uint16 v = VAPixelValue(c);

	if (screen.format == PLAT_PIX_GREY)
		return (uint16)(v & ((1 << screen.depth) - 1));
	if (screen.format == PLAT_PIX_RGB565) {
		uint8 b[2];

		memcpy(b, &v, 2);
		return screen.le16 ? (uint16)(b[0] | (b[1] << 8)) : (uint16)((b[0] << 8) | b[1]);
	}
	return v;
}

static size_t buf_bytes(void)
{
	return (size_t)screen.row_bytes * screen.height;
}

static int count_not_bg(void)
{
	int16 x, y;
	int n = 0;
	uint16 bg = read_value(BGC);

	for (y = 0; y < screen.height; y++)
		for (x = 0; x < screen.width; x++)
			if (testpat_get(&screen, x, y) != bg)
				n++;
	return n;
}

/* The scene: a star of lines at every angle, lines running off every
   edge, a bar per colour, text and numbers, spots, pixels. Moves with
   frame so erase by redraw has something to do. */
static void scene(int frame)
{
	int16 size = VA.frame.right;
	int16 c = (int16)(size / 2);
	int16 r = (int16)(size * 3 / 8);
	int16 a, i, s = VA.segmscale;
	char text[48];

	for (a = 0; a < ANGLES; a += 2) {
		int16 an = (int16)((a + frame) % ANGLES);

		VA.color = (int16)(a % 7);
		VALine(c, c, (int16)(c + (int32)Cosins[an] * r / 256), (int16)(c - (int32)Sins[an] * r / 256));
	}
	/* Off every edge and corner, and wholly outside. */
	VA.color = 0;
	VALine((int16)(-size), (int16)(size / 5 + frame), (int16)(2 * size), (int16)(size / 4));
	VALine((int16)(size / 6 + frame), -1000, (int16)(size / 5), 1000);
	VALine(-30, -30, (int16)(size + 30), (int16)(size + 30));
	VALine((int16)(size + 40), -20, -40, (int16)(size + 20));
	VALine(-100, -100, -10, -50);
	VALine((int16)(size + 5), 0, (int16)(size + 5), size);
	/* Colour bars, 0 to 6 then BG1, BG2 (BGC draws nothing). */
	for (i = 0; i < 9; i++) {
		int16 y0 = (int16)(size - size / 12 - 1);
		int16 x0 = (int16)(size / 20 + i * (size / 10));
		int16 k;

		VA.color = i < 7 ? i : i == 7 ? BG1 : BG2;
		for (k = 0; k < size / 14; k++)
			VALine((int16)(x0 + k), y0, (int16)(x0 + k), (int16)(y0 - size / 20));
	}
	VA.color = 0;
	VAMoveTo(1, (int16)(s * 4 + 4));
	VADrawText("ABCDEFGHIJKLM", 0, 13);
	VAMoveTo(1, (int16)(2 * (s * 4 + 4) + 2));
	VA.color = 1;
	VADrawText("NOPQRSTUVWXYZ", 0, 13);
	VAMoveTo(1, (int16)(3 * (s * 4 + 4) + 4));
	VA.color = 6;
	sprintf(text, "0123456789 .,:;-+/!?()*=<>%%$#&@'");
	VADrawText(text, 0, (int32)strlen(text));
	VA.color = 2;
	VADrawNumber(1234567890L + frame, (int16)(size - s - 9), (int16)(size / 2 + 2 * s + 2));
	VA.color = 4;
	VADrawNumber(-42, (int16)(size - s - 9), (int16)(size / 2 + 5 * s + 6));
	VADrawPadNumber(7, (int16)(size / 3), (int16)(size / 2 + 5 * s + 6), 3);
	VA.color = 5;
	for (i = 0; i < 8; i++)
		VASpot((int16)(size / 8 + i * size / 10), (int16)(size * 2 / 3 + (frame & 3)));
	VASpot(0, 0);
	VASpot((int16)(size - 1), (int16)(size / 2));
	VA.color = 3;
	for (i = 0; i < 20; i++)
		VAPixel((int16)(i * 7 % size), (int16)((i * 13 + frame) % size));
	VAPixel(-1, 5);
	VAPixel(size, 5);
}

/* Erase by redraw: after several moving frames the screen must match a
   fresh one with only the last frame on it. */
static void check_erase(const char *dir, const char *tag, int16 mode)
{
	uint8 *after;
	int f;
	char name[64];

	VA.EraseMode = mode;
	VAScreenChanged();
	for (f = 0; f < 6; f++) {
		scene(f);
		VAStep();
	}
	after = (uint8 *)malloc(buf_bytes());
	memcpy(after, screen.bits, buf_bytes());
	sprintf(name, "va-%s.png", tag);
	save_png(dir, name);

	memset(screen.bits, 0x5A, buf_bytes());
	VAScreenChanged();
	scene(5);
	VAStep();
	CHECK(memcmp(after, screen.bits, buf_bytes()) == 0, "%s: erase mode %d left pixels behind", tag, mode);
	free(after);

	/* Two empty frames erase everything. */
	VAStep();
	VAStep();
	CHECK(count_not_bg() == 0, "%s: %d pixels left after empty frames", tag, count_not_bg());
	VA.EraseMode = VA_ERASE_AUTO;
}

static void check_lines(const char *tag)
{
	int16 size, a, x2, y2;
	int16 x, y;
	int n;
	uint16 bg, v;

	VAScreenChanged();
	size = VA.frame.right;
	bg = read_value(BGC);
	VA.color = 0;
	v = read_value(0);
	/* Every angle from the centre: both ends lit, max(dx, dy) + 1 pixels. */
	for (a = 0; a < ANGLES; a++) {
		int16 c = (int16)(size / 2);

		x2 = (int16)(c + Cosins[a] * (size / 3) / 256);
		y2 = (int16)(c - Sins[a] * (size / 3) / 256);
		VALine(c, c, x2, y2);
		VAStep();
		CHECK(play_get(c, c) == v && play_get(x2, y2) == v, "%s: angle %d ends", tag, a);
		n = count_not_bg();
		{
			int16 dx = (int16)abs(x2 - c), dy = (int16)abs(y2 - c);

			CHECK(n == (dx > dy ? dx : dy) + 1, "%s: angle %d has %d pixels, want %d",
			      tag, a, n, (dx > dy ? dx : dy) + 1);
		}
	}
	VAStep();
	/* Clipping: lines far outside the play area light nothing outside it,
	   and edge lines reach the edge. */
	VALine(-500, -400, (int16)(size + 600), (int16)(size + 300));
	VALine((int16)(size / 2), -32000, (int16)(size / 2), 32000);
	VALine(-32000, (int16)(size / 3), 32000, (int16)(size / 3));
	VAStep();
	for (y = 0; y < screen.height; y++)
		for (x = 0; x < screen.width; x++) {
			cbool in = x >= screen.play.x && x < screen.play.x + screen.play.w &&
			           y >= screen.play.y && y < screen.play.y + screen.play.h;

			if (!in && testpat_get(&screen, x, y) != bg) {
				CHECK(0, "%s: pixel outside the play area at %d,%d", tag, x, y);
				x = screen.width;
				y = screen.height;
			}
		}
	CHECK(play_get((int16)(size / 2), 0) == v && play_get((int16)(size / 2), (int16)(size - 1)) == v,
	      "%s: vertical line doesn't reach both edges", tag);
	CHECK(play_get(0, (int16)(size / 3)) == v && play_get((int16)(size - 1), (int16)(size / 3)) == v,
	      "%s: horizontal line doesn't reach both edges", tag);
	VAStep();
	CHECK(count_not_bg() == 0, "%s: clip lines not erased", tag);
}

/* An explosion runs its frames and leaves nothing behind. */
static void check_explosion(const char *dir, const char *tag)
{
	int f, lit = 0;
	char name[64];
	int16 size;

	VAScreenChanged();
	size = VA.frame.right;
	VAExplosion((int16)(size / 2), (int16)(size / 2), 3, 1);
	VAExplosion((int16)(size / 4), (int16)(size / 4), 1, 5);
	VAExplosion(2, (int16)(size - 3), 2, 0);
	for (f = 0; f < 200; f++) {
		VAStep();
		if (f == 6) {
			lit = count_not_bg();
			sprintf(name, "va-%s-explosion.png", tag);
			save_png(dir, name);
		}
	}
	CHECK(lit > 50, "%s: explosion lit only %d pixels", tag, lit);
	CHECK(count_not_bg() == 0, "%s: explosion left %d pixels", tag, count_not_bg());
}

static void check_demo(const char *dir, const char *tag)
{
	int f;
	char name[64];

	VAScreenChanged();
	CHECK(vademo_init(1), "%s: demo found no shape for level 1", tag);
	for (f = 0; f < 40; f++)
		vademo_frame();
	sprintf(name, "va-%s-demo.png", tag);
	save_png(dir, name);
	vademo_init(5);
	for (f = 0; f < 10; f++)
		vademo_frame();
	sprintf(name, "va-%s-demo5.png", tag);
	save_png(dir, name);
	VALoadColors(1000);
}

static void run(const char *dir, int16 w, int16 h, uint8 depth, uint8 le16)
{
	char tag[32];

	sprintf(tag, "%dx%d-%d%s", w, h, depth, le16 ? "le" : "");
	make_screen(w, h, depth, le16);
	if (!VAInit()) {
		CHECK(0, "%s: VAInit failed", tag);
		return;
	}
	check_lines(tag);
	check_erase(dir, tag, VA_ERASE_REDRAW);
	check_erase(dir, tag, VA_ERASE_BOX);
	check_erase(dir, tag, VA_ERASE_AUTO);
	check_explosion(dir, tag);
	check_demo(dir, tag);
	VAClose();
}

int main(int argc, char **argv)
{
	static const uint8 depths[] = { 1, 2, 4, 8, 16 };
	size_t i;

	if (argc != 3) {
		fprintf(stderr, "usage: test_va DATADIR OUTDIR\n");
		return 2;
	}
	host_set_data_dir(argv[1]);
	for (i = 0; i < sizeof(depths); i++)
		run(argv[2], 160, 160, depths[i], 0);
	run(argv[2], 160, 160, 16, 1);
	run(argv[2], 320, 320, 16, 1);
	run(argv[2], 320, 480, 2, 0);
	run(argv[2], 480, 320, 8, 0);
	free(screen.bits);
	if (failures) {
		printf("renderer: %d failures\n", failures);
		return 1;
	}
	printf("renderer: lines, clipping, erase by redraw, explosions and demo ok at 1, 2, 4, 8, 16 bit; PNGs in %s\n", argv[2]);
	return 0;
}
