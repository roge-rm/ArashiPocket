/* Host test for the front end and the saved data (front.c, store.c).
   Usage: test_front DATADIR OUTDIR */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/plat.h"
#include "../core/game.h"
#include "../core/front.h"
#include "../core/store.h"
#include "../core/qdrandom.h"
#include "../core/testpat.h"
#include "../core/ui.h"
#include "plat_host.h"

#define MAX_CHOICES_TEST 100
#include "png.h"

static int failures;
static PlatScreen screen;
static cbool have_screen;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; \
	printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

const PlatScreen *plat_screen(void)
{
	return have_screen ? &screen : NULL;
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

static uint32 now_ticks;
static PlatInput inp;
static int find_ctl(int id, PlatRect *r);

/* Runs the front end for n UI steps at 20 a second. */
static void run(int n)
{
	int i;

	for (i = 0; i < n; i++) {
		now_ticks += 50;
		host_set_clock(1, now_ticks);
		host_set_input(&inp);
		front_frame();
	}
}

static void press(uint16 actions)
{
	inp.actions = actions;
	run(1);
	inp.actions = 0;
	run(1);
}

/* From the menu: the item with the buttons. */
static void menu_item(int n)
{
	int i;

	for (i = 0; i < n; i++)
		press(PLAT_ACT_RIGHT);
	press(PLAT_ACT_FIRE);
}

static void tap_at(int x, int y)
{
	run(1);        /* a tap right after a screen change is dropped */
	inp.pen_down = 1;
	inp.pen_seen = 1;
	inp.pen_x = (int16)x;
	inp.pen_y = (int16)y;
	run(1);
	inp.pen_down = 0;
	run(1);
}

static void check_store(void)
{
	uint8 a[STORE_BYTES + 8], b[STORE_BYTES + 8];
	uint32 n;
	int i;
	int16 r;

	store_defaults();
	CHECK(Saved.table[0][0].score > 0 && Saved.table[0][0].score >= Saved.table[0][49].score,
	      "default table %ld .. %ld", (long)Saved.table[0][0].score, (long)Saved.table[0][49].score);
	CHECK(Saved.table[0][49].score >= 1000, "default last score %ld", (long)Saved.table[0][49].score);
	CHECK(Saved.table[1][0].name[0] == '-', "default name");

	/* Insert order: above, below, equal goes below, too low is out. */
	r = store_insert(0, 123456, "ABC");
	CHECK(r == 0, "top score ranked %d", r);
	r = store_insert(0, 123456, "DEF");
	CHECK(r == 1, "equal score ranked %d", r);
	r = store_insert(0, 123457, "GHI");
	CHECK(r == 0 && Saved.table[0][1].score == 123456 && Saved.table[0][2].score == 123456 &&
	      Saved.table[0][1].name[0] == 'A' && Saved.table[0][2].name[0] == 'D', "insert order");
	CHECK(Saved.table[0][1].name[3] == ' ' && Saved.table[0][1].name[5] == ' ', "name padded");
	CHECK(store_insert(0, 5, "LOW") == -1 && store_rank(0, 5) == -1, "low score kept out");
	r = store_insert(1, 31000, "PRA");
	CHECK(r >= 0 && Saved.table[1][r].score == 31000 && Saved.table[0][0].score == 123457,
	      "tables are separate");
	for (i = 1; i < STORE_ENTRIES; i++)
		CHECK(Saved.table[0][i - 1].score >= Saved.table[0][i].score, "table order at %d", i);

	/* Round trip. */
	Saved.control = 1;
	Saved.practice = 1;
	Saved.sound = 0;
	Saved.highest = 42;
	n = store_encode(a);
	CHECK(n == STORE_BYTES, "encoded %lu bytes", (unsigned long)n);
	store_defaults();
	CHECK(store_decode(a, n), "decode");
	CHECK(Saved.control == 1 && Saved.practice == 1 && Saved.sound == 0 && Saved.highest == 42 &&
	      Saved.table[0][0].score == 123457 && Saved.table[0][0].name[0] == 'G' &&
	      Saved.table[1][r].score == 31000, "round trip lost something");
	store_encode(b);
	CHECK(memcmp(a, b, STORE_BYTES) == 0, "encode again differs");
	CHECK(a[0] == 'A' && a[4] == 0 && a[5] == STORE_VERSION && a[8] == 0 && a[9] == 42,
	      "big-endian layout");

	/* Saved through the platform. */
	host_forget_save();
	store_load();
	CHECK(Saved.highest == 1 && Saved.control == 0 && Saved.sound == 1, "first run is the defaults");
	Saved.highest = 7;
	store_save();
	Saved.highest = 1;
	store_load();
	CHECK(Saved.highest == 7, "save then load gave %d", Saved.highest);

	/* Bad records give the defaults and say so. */
	memcpy(b, a, STORE_BYTES);
	b[300] ^= 0x10;
	CHECK(!store_decode(b, STORE_BYTES) && Saved.highest == 1 && Saved.table[0][0].score != 123457,
	      "corrupt record taken");
	memcpy(b, a, STORE_BYTES);
	b[5] = 0;                       /* an old version */
	CHECK(!store_decode(b, STORE_BYTES), "old version taken");
	b[5] = 2;                       /* a newer one */
	CHECK(!store_decode(b, STORE_BYTES), "newer version taken");
	memcpy(b, a, STORE_BYTES);
	b[0] = 'X';
	CHECK(!store_decode(b, STORE_BYTES), "other magic taken");
	CHECK(!store_decode(a, STORE_BYTES - 1), "short record taken");
	CHECK(!store_decode(a, 0) && !store_decode(NULL, STORE_BYTES), "empty record taken");
	CHECK(!store_decode(a, STORE_BYTES + 2), "long record taken");
	memset(b, 0, STORE_BYTES);
	CHECK(!store_decode(b, STORE_BYTES), "zeros taken");
	store_defaults();
	printf("store: round trips, corrupt, old and short records, insert order ok\n");
}

/* All 8 recorded games play in step with their recordings: the claw dies
   only when its moves run out, or, in a recording that ends before the
   400 move limit (the player died), within the last few moves. They play
   in the order they were recorded, from MYGA 133, as the attract loop does,
   since some need the state the one before left. Demos 130 and 135 still
   drift: they are reported, not failed, and fail the test
   once they come into step, so the list gets updated. */
/* The attract loop as it runs with no input, from the title: each demo
   still plays to the end of its moves. */
static void check_attract(void)
{
	int s, in_demo = 0, steps = 0, played = 0, left = -1, n = 0;
	int texts = 0, tsteps = 0, scores = 0, last = -1, order_ok = 1;

	make_screen(160, 160, 2, 0);
	CHECK(VAInit(), "VAInit");
	store_defaults();
	CHECK(front_init(), "front_init");
	memset(&inp, 0, sizeof(inp));
	inp.rim = -1;
	for (s = 0; s < 9000; s++) {
		int16 now_left = demo_moves_left();

		run(1);
		/* Stop after the sixth high score page, once the next bolt starts. */
		if (scores >= 6 && front_screen() != FRONT_SCORES_PAGE)
			break;
		if (front_screen() != last) {
			/* Demo, four credit pages, high scores, title. */
			if (front_screen() == FRONT_TEXT)
				texts++;
			if (front_screen() == FRONT_SCORES_PAGE) {
				scores++;
				if (texts != scores)
					order_ok = 0;
			}
			last = front_screen();
		}
		if (front_screen() == FRONT_TEXT)
			tsteps++;
		if (front_screen() == FRONT_DEMO) {
			if (!in_demo) {
				in_demo = 1;
				steps = 0;
				played = 0;
				left = -1;
			}
			steps++;
			if (Hero.state == HeroPlaying || Hero.state == HeroFlying)
				played = 1;
			else if (played && left < 0)
				left = now_left;
		} else if (in_demo) {
			in_demo = 0;
			printf("  attract demo %d: level %d, %d steps, %d moves left at the end\n", n++,
			       ThisLevel.lvNumber, steps, left < 0 ? now_left : left);
		}
	}
	printf("  attract: %d credit page steps (4 pages each) before %d high score pages\n", tsteps, scores);
	CHECK(scores > 0 && texts == scores && tsteps >= scores * 4 * 55 && tsteps <= scores * 4 * 66 && order_ok,
	      "attract loop: %d credit steps, %d score pages", tsteps, scores);
	front_close();
	VAClose();
}

/* The sounds of MYGA 134 (sounds-134.txt): a fixed recording, so a fixed
   list, checked by its length, a hash and how many of each. Then each way
   the claw dies has its sound, and the sound option silences them. */
static void check_demo_sounds(void)
{
	static const int want[SND_COUNT] = { 0, 2, 119, 4, 187, 0, 0, 0, 0, 0, 0, 0, 0, 15, 6, 4, 0, 1 };
	static const struct { uint16 cap; int16 snd; } deaths[] = {
		{ CapFlip, SND_FALLYELL }, { CapPuls, SND_FALLZAP }, { CapFuse, SND_FALLZAP },
		{ CapPlas, SND_PHAZEROUT }, { CapSpike, SND_FALLZAP }
	};
	int count[SND_COUNT], i, n = host_sounds;
	uint32 hash = 2166136261UL;

	memset(count, 0, sizeof(count));
	for (i = 0; i < n; i++) {
		count[host_sound[i]]++;
		hash = (hash ^ (uint32)host_sound[i]) * 16777619UL;
	}
	for (i = 0; i < SND_COUNT; i++)
		CHECK(count[i] == want[i], "demo 134 played %s %d times, not %d", host_sound_name((int16)i), count[i],
		      want[i]);
	CHECK(hash == 0x867ed911UL, "demo 134 sound list hash %08lx", (unsigned long)hash);
	for (i = 0; i < (int)(sizeof(deaths) / sizeof(deaths[0])); i++) {
		host_sounds = 0;
		Hero.state = HeroPlaying;
		PlayerCapture((int16)deaths[i].cap);
		CHECK(host_sounds == 1 && host_sound[0] == deaths[i].snd, "capture %x played %d sounds", deaths[i].cap,
		      host_sounds);
	}
	SoundOn = 0;
	host_sounds = 0;
	PlayerCapture(CapFlip);
	CHECK(host_sounds == 0, "sound off still played %d", host_sounds);
	SoundOn = 1;
	printf("sound: demo 134 plays %d sounds in the expected order, deaths and sound off ok\n", n);
}

static void check_demos(const char *dir, int w, int h)
{
	static const int drifts[8] = { 0, 0, 0, 0, 0, 0, 0, 1 };
	int id, k, s, synced = 0;
	char name[96];

	printf("demos at %dx%d:\n", w, h);
	make_screen(w, h, w > 160 ? 16 : 2, w > 160);
	CHECK(VAInit(), "VAInit");
	store_defaults();
	CHECK(front_init(), "front_init");
	for (k = 0; k < 8; k++) {
		int count, died = -1, left = -1, played = 0, ok;
		uint32 size;
		const uint8 *rec;

		id = (DEMO_FIRST + k) % 8;
		rec = plat_resource(FOURCC('M', 'Y', 'G', 'A'), (int16)(128 + id), &size);
		count = (int)get32(rec + 8);
		plat_release(rec);
		if (count > 400)
			count = 400;

		if (id == DEMO_FIRST + 1) {
			sprintf(name, "%s/sounds-%d.txt", dir, 128 + id);
			host_sound_log(name);
			host_sounds = 0;
		}
		front_goto(FRONT_DEMO, (int16)id);
		CHECK(front_screen() == FRONT_DEMO, "demo %d did not start", id);
		for (s = 0; s < 2000 && front_screen() == FRONT_DEMO; s++) {
			int16 now_left = demo_moves_left();

			run(1);
			if (Hero.state == HeroPlaying || Hero.state == HeroFlying)
				played = 1;
			else if (played && died < 0) {
				/* The moves left before the step the claw stopped playing. */
				died = s + 1;
				left = now_left;
			}
			if (s == 60 && id == 0) {
				sprintf(name, "front-demo-%d.png", id);
				save_png(dir, name);
			}
		}
		ok = died > 0 && (count >= 400 ? left <= 0 : left <= 3);
		printf("  demo %d (MYGA %d): level %d, %d moves, claw died at step %d with %d moves left, score %ld%s\n",
		       id, 128 + id, ThisLevel.lvNumber, count, died, left, (long)Hero.score,
		       ok ? "" : drifts[id] ? " (known drift)" : "");
		if (drifts[id])
			CHECK(!ok, "demo %d is in step now: take it off the drift list", id);
		else
			CHECK(ok, "demo %d: the claw died with %d of %d moves left", id, left, count);
		synced += ok;
		CHECK(front_screen() == FRONT_TEXT, "demo %d ended on screen %d", id, front_screen());
		if (id == DEMO_FIRST + 1) {
			host_sound_log(NULL);
			check_demo_sounds();
		}
	}
	/* A tap leaves a demo. */
	front_goto(FRONT_DEMO, 1);
	run(30);
	tap_at(80, 80);
	CHECK(front_screen() == FRONT_MENU, "tap in a demo went to screen %d", front_screen());
	front_close();
	VAClose();
	printf("demos: %d of 8 recorded games play to the end of their moves, a tap leaves them\n", synced);
}

/* The level select from the LSEL table matches running the script. */
static void check_lsel(void)
{
	static const int16 highs[] = { 1, 9, 20, 40, 60, 80, 99, 120, 200, 2000 };
	static LevelChoice a[MAX_CHOICES_TEST], b[MAX_CHOICES_TEST];
	int i, p, k, na, nb;

	for (p = 0; p < 2; p++)
		for (i = 0; i < (int)(sizeof(highs) / sizeof(highs[0])); i++) {
			na = game_level_choices_from(1, (cbool)p, highs[i], a, MAX_CHOICES_TEST);
			nb = game_level_choices_from(0, (cbool)p, highs[i], b, MAX_CHOICES_TEST);
			CHECK(na == nb && na > 0, "LSEL: %d choices, the script %d (practice %d, highest %d)",
			      na, nb, p, highs[i]);
			for (k = 0; k < na && k < nb; k++)
				CHECK(a[k].level == b[k].level && a[k].field == b[k].field &&
				      a[k].color == b[k].color && a[k].startBonus == b[k].startBonus,
				      "LSEL: choice %d differs (practice %d, highest %d)", k, p, highs[i]);
			if (p)
				break;      /* practice doesn't look at the highest level */
		}
	printf("lsel: the level select from LSEL 128 matches the script\n");
}

static void shot(const char *dir, int w, int h, const char *what)
{
	char name[96];

	sprintf(name, "front-%dx%d-%s.png", w, h, what);
	save_png(dir, name);
}

static void check_screens(const char *dir, int16 w, int16 h, uint8 depth, uint8 le16)
{
	int pw = w, ph = h, i;

	make_screen(w, h, depth, le16);
	CHECK(VAInit(), "VAInit");
	host_forget_save();
	store_defaults();
	front_init();
	memset(&inp, 0, sizeof(inp));
	inp.rim = -1;
	CHECK(front_screen() == FRONT_TITLE, "starts on the title");
	run(12);
	shot(dir, pw, ph, "title-spark");
	run(60);
	shot(dir, pw, ph, "title-zoom");
	run(20);
	shot(dir, pw, ph, "title");
	/* After the title the bolt comes down, flashes and fades, then a demo. */
	for (i = 0; i < 20 && front_screen() != FRONT_LIGHTNING; i++)
		run(1);
	CHECK(front_screen() == FRONT_LIGHTNING, "the title went to screen %d, not the lightning", front_screen());
	run(2);
	shot(dir, pw, ph, "lightning-1");
	run(3);
	shot(dir, pw, ph, "lightning-2");
	for (i = 0; i < 60 && front_screen() == FRONT_LIGHTNING; i++)
		run(1);
	CHECK(front_screen() == FRONT_DEMO, "the lightning went to screen %d, not a demo", front_screen());
	tap_at(20, 20);
	CHECK(front_screen() == FRONT_MENU, "tap on the title went to %d", front_screen());
	run(4);
	shot(dir, pw, ph, "menu");
	menu_item(1);
	CHECK(front_screen() == FRONT_LEVELS, "menu item 1 went to %d", front_screen());
	run(6);
	shot(dir, pw, ph, "levels");
	inp.actions = PLAT_ACT_RIGHT;
	run(1);
	inp.actions = 0;
	run(1);
	shot(dir, pw, ph, "levels-turn");
	run(6);
	shot(dir, pw, ph, "levels-next");
	press(PLAT_ACT_LEFT);
	run(6);
	press(PLAT_ACT_MENU);
	CHECK(front_screen() == FRONT_MENU, "back to the menu");
	/* The 5-way's down and up (which also set ZAP and FIRE) only move the
	   selection; the centre picks it. */
	press(PLAT_ACT_DOWN | PLAT_ACT_ZAP);
	press(PLAT_ACT_UP | PLAT_ACT_FIRE);
	press(PLAT_ACT_DOWN | PLAT_ACT_ZAP);
	CHECK(front_screen() == FRONT_MENU, "5-way up and down opened screen %d", front_screen());
	press(PLAT_ACT_OK | PLAT_ACT_FIRE);
	CHECK(front_screen() == FRONT_LEVELS, "5-way centre went to %d, not the level select", front_screen());
	press(PLAT_ACT_MENU);
	CHECK(front_screen() == FRONT_MENU, "back to the menu again");
	menu_item(2);
	CHECK(front_screen() == FRONT_SCORES, "scores %d", front_screen());
	run(4);
	shot(dir, pw, ph, "scores");
	press(PLAT_ACT_FIRE);
	menu_item(3);
	CHECK(front_screen() == FRONT_OPTIONS, "options %d", front_screen());
	run(4);
	shot(dir, pw, ph, "options");
	press(PLAT_ACT_FIRE);
	CHECK(Saved.control == 1, "option toggled");
	press(PLAT_ACT_FIRE);
	CHECK(Saved.control == 0, "option toggled back");
	press(PLAT_ACT_MENU);
	menu_item(4);
	CHECK(front_screen() == FRONT_ABOUT, "about %d", front_screen());
	run(4);
	shot(dir, pw, ph, "about");
	for (i = 0; i < 4; i++) {
		char nm[16];

		front_goto(FRONT_TEXT, i);
		run(i == 0 ? 8 : 30);
		sprintf(nm, "text-%d", i);
		shot(dir, pw, ph, nm);
	}
	front_goto(FRONT_SCORES_PAGE, 0);
	run(70);
	shot(dir, pw, ph, "fireworks");
	front_goto(FRONT_MENU, 0);
	press(PLAT_ACT_FIRE);
	menu_item(0);
	CHECK(front_screen() == FRONT_PLAY, "play %d", front_screen());
	CHECK(intro_running(), "no level intro");
	run(10);
	shot(dir, pw, ph, "intro-stars");
	run(55);
	shot(dir, pw, ph, "intro-field");
	run(35);
	CHECK(!intro_running(), "level intro still running");
	CHECK(front_screen() == FRONT_PLAY && Hero.lives >= 1, "playing, lives %d", Hero.lives);
	IncreaseScore(5000);
	run(1);
	shot(dir, pw, ph, "hud");
	{
		const PlatScreen *sc = plat_screen();

		if (sc->status.w > 0 && sc->status.h > 0) {
			PlatRect pb;

			/* On a screen with a status area only a tap on its pause
			   button pauses. */
			tap_at(VA.frame.right / 2, VA.frame.bottom / 2);
			CHECK(front_screen() == FRONT_PLAY, "tap in the play area paused");
			tap_at(sc->status.x + 1 - sc->play.x, sc->status.y + sc->status.h - 2 - sc->play.y);
			CHECK(front_screen() == FRONT_PLAY, "tap in the status area paused");
			CHECK(find_ctl(PANEL_PAUSE, &pb), "no pause button");
			tap_at(pb.x + pb.w / 2 - sc->play.x, pb.y + pb.h / 2 - sc->play.y);
			CHECK(front_screen() == FRONT_PAUSE, "tap on the pause button didn't pause (%d)", front_screen());
			press(PLAT_ACT_PAUSE);
			CHECK(front_screen() == FRONT_PLAY, "resume after the status tap");
		}
	}
	press(PLAT_ACT_PAUSE);
	CHECK(front_screen() == FRONT_PAUSE, "pause %d", front_screen());
	run(4);
	shot(dir, pw, ph, "pause");
	press(PLAT_ACT_PAUSE);
	CHECK(front_screen() == FRONT_PLAY, "resume %d", front_screen());
	run(10);
	front_pause();
	CHECK(front_screen() == FRONT_PAUSE, "auto pause");
	/* Game over with a high score goes to name entry and into the table. */
	Hero.score = 99999;
	front_goto(FRONT_PLAY, 1);
	/* Past the level intro. */
	for (i = 0; i < 400 && intro_running(); i++)
		run(1);
	Hero.score = 99999;
	Hero.lives = 0;
	Hero.state = HeroDead;
	/* The field flies away first. */
	for (i = 0; i < 200 && !game_over_now(); i++) {
		run(1);
		if (i == 20)
			shot(dir, pw, ph, "flyout");
	}
	run(1);
	CHECK(game_over_now() && front_screen() == FRONT_OVER, "game over screen %d", front_screen());
	run(2);
	shot(dir, pw, ph, "over");
	tap_at(30, 30);
	CHECK(front_screen() == FRONT_NAME, "name entry %d", front_screen());
	run(4);
	shot(dir, pw, ph, "name");
	/* The wheel: right turns to B, a drag of 3/16 of the width to the
	   left turns a letter on to C, back drops it, and a tap takes the letter
	   picked. */
	press(PLAT_ACT_RIGHT);
	run(4);
	press(PLAT_ACT_FIRE);
	press(PLAT_ACT_FIRE);
	inp.pen_down = 1;
	inp.pen_seen = 1;
	for (i = 0; i <= 6; i++) {
		inp.pen_x = (int16)(VA.frame.right / 2 - i * VA.frame.right / 32);
		inp.pen_y = (int16)(ph * 3 / 4);
		run(1);
	}
	inp.pen_down = 0;
	run(10);
	shot(dir, pw, ph, "name-wheel");
	tap_at(pw / 2, ph * 3 / 4);
	press(PLAT_ACT_PAUSE);
	tap_at(pw / 2, ph * 3 / 4);
	press(PLAT_ACT_LEFT);
	run(4);
	press(PLAT_ACT_FIRE);
	press(PLAT_ACT_FIRE);
	CHECK(front_screen() == FRONT_SCORES, "scores after the name %d", front_screen());
	CHECK(Saved.table[0][0].score == 99999 && !memcmp(Saved.table[0][0].name, "BBCBB", 5),
	      "score entered: %ld %.5s", (long)Saved.table[0][0].score, Saved.table[0][0].name);
	run(4);
	shot(dir, pw, ph, "scores-new");
	front_close();
	VAClose();
}

/* Taps the middle of tap zone id on the screen showing. */
static void tap_zone_id(int id)
{
	PlatRect r;
	const PlatScreen *sc = plat_screen();

	if (!ui_zone_rect((int16)id, &r)) {
		CHECK(0, "no tap zone %d on screen %d", id, front_screen());
		return;
	}
	tap_at(r.x + r.w / 2 - sc->play.x, r.y + r.h / 2 - sc->play.y);
}

static void dshot(const char *dir, int w, int h, int depth, const char *what)
{
	char name[96];

	sprintf(name, "front-%dx%d-d%d-%s.png", w, h, depth, what);
	save_png(dir, name);
}

/* The painted screens at one size and depth, reached with taps on their
   buttons. */
static void check_ui(const char *dir, int16 w, int16 h, uint8 depth)
{
	int i, sound;

	make_screen(w, h, depth, 0);
	CHECK(VAInit(), "VAInit");
	host_forget_save();
	store_defaults();
	front_init();
	memset(&inp, 0, sizeof(inp));
	inp.rim = -1;
	run(92);
	dshot(dir, w, h, depth, "title");
	front_goto(FRONT_MENU, 0);
	run(2);
	dshot(dir, w, h, depth, "menu");
	tap_zone_id(2);
	CHECK(front_screen() == FRONT_SCORES, "tap on High scores went to %d", front_screen());
	run(2);
	tap_zone_id(UIZ_NEXT);
	run(2);
	dshot(dir, w, h, depth, "scores-2");
	tap_zone_id(UIZ_BACK);
	CHECK(front_screen() == FRONT_MENU, "Done on the scores went to %d", front_screen());
	run(2);
	tap_zone_id(3);
	CHECK(front_screen() == FRONT_OPTIONS, "tap on Options went to %d", front_screen());
	run(2);
	sound = Saved.sound;
	tap_zone_id(2);
	CHECK(Saved.sound != sound, "tap on Sound didn't toggle it");
	press(PLAT_ACT_RIGHT);
	run(2);
	dshot(dir, w, h, depth, "options");
	tap_zone_id(3);
	CHECK(front_screen() == FRONT_MENU, "Done on the options went to %d", front_screen());
	run(2);
	tap_zone_id(1);
	CHECK(front_screen() == FRONT_LEVELS, "tap on Select level went to %d", front_screen());
	run(6);
	dshot(dir, w, h, depth, "levels");
	tap_zone_id(UIZ_BACK);
	CHECK(front_screen() == FRONT_MENU, "Back on the level select went to %d", front_screen());
	run(2);
	tap_zone_id(4);
	CHECK(front_screen() == FRONT_ABOUT, "tap on About went to %d", front_screen());
	run(2);
	dshot(dir, w, h, depth, "about");
	tap_at(5, 5);
	CHECK(front_screen() == FRONT_MENU, "tap on About went to %d", front_screen());
	front_goto(FRONT_TEXT, 1);
	run(30);
	dshot(dir, w, h, depth, "text");
	front_goto(FRONT_SCORES_PAGE, 0);
	run(40);
	dshot(dir, w, h, depth, "fireworks");
	front_goto(FRONT_PLAY, 1);
	for (i = 0; i < 400 && intro_running(); i++)
		run(1);
	press(PLAT_ACT_PAUSE);
	CHECK(front_screen() == FRONT_PAUSE, "pause %d", front_screen());
	run(2);
	dshot(dir, w, h, depth, "pause");
	tap_zone_id(0);
	CHECK(front_screen() == FRONT_PLAY, "tap on Resume went to %d", front_screen());
	Hero.score = 12345;
	front_goto(FRONT_OVER, 0);
	run(3);
	dshot(dir, w, h, depth, "over");
	front_goto(FRONT_NAME, 0);
	press(PLAT_ACT_FIRE);
	run(4);
	dshot(dir, w, h, depth, "name");
	tap_zone_id(UIZ_DEL);
	tap_zone_id(UIZ_OK);
	CHECK(front_screen() == FRONT_NAME, "Done with no letters left name entry");
	press(PLAT_ACT_FIRE);
	tap_zone_id(UIZ_OK);
	CHECK(front_screen() == FRONT_SCORES, "Done went to %d", front_screen());
	front_close();
	VAClose();
}

/* The box of the status area points panel_hit gives id (screen pixels),
   its side part included. */
static int find_ctl(int id, PlatRect *r)
{
	int x, y, k, x0 = 32767, y0 = 32767, x1 = -1, y1 = -1;

	for (k = 0; k < 2; k++) {
		const PlatRect *a = k ? &screen.side : &screen.status;

		for (y = a->y; y < a->y + a->h; y++)
			for (x = a->x; x < a->x + a->w; x++)
				if (panel_hit((int16)x, (int16)y) == id) {
					if (x < x0) x0 = x;
					if (y < y0) y0 = y;
					if (x > x1) x1 = x;
					if (y > y1) y1 = y;
				}
	}
	r->x = (int16)x0;
	r->y = (int16)y0;
	r->w = (int16)(x1 - x0 + 1);
	r->h = (int16)(y1 - y0 + 1);
	return x1 >= 0;
}

/* The pen at screen point x, y (down) or lifted. */
static void pen_at(int down, int x, int y)
{
	inp.pen_down = (cbool)down;
	inp.pen_seen = 1;
	inp.pen_x = (int16)(x - screen.play.x);
	inp.pen_y = (int16)(y - screen.play.y);
	inp.rim = down ? plat_rim_from_point(inp.pen_x, inp.pen_y, screen.play.w) : -1;
}

/* Sounds of snd played since host_sounds was at from. */
static int sounds_since(int from, int16 snd)
{
	int i, n = 0;

	for (i = from; i < host_sounds && i < HOST_SOUNDS; i++)
		if (host_sound[i] == snd)
			n++;
	return n;
}

/* Lanes from a to b the short way round a closed tube of n lanes. */
static int lane_delta(int a, int b, int n)
{
	int d = ((b - a) % n + n) % n;

	return d > n / 2 ? d - n : d;
}

/* A long move along the strip in one read turns the claw the way a short
   one does, and a pen that lifted and touched again between two reads
   starts a new stroke. */
static void check_flick(int16 w, int16 h, uint8 depth)
{
	PlatRect drag;
	int i, y, x, l0, l1, l2, d_small, d_big;

	make_screen(w, h, depth, 0);
	CHECK(VAInit(), "VAInit");
	host_forget_save();
	store_defaults();
	front_init();
	memset(&inp, 0, sizeof(inp));
	inp.rim = -1;
	front_goto(FRONT_PLAY, 1);
	for (i = 0; i < 400 && intro_running(); i++)
		run(1);
	run(4);
	if (!find_ctl(PANEL_DRAG, &drag)) {
		CHECK(0, "%dx%d: no drag strip", w, h);
		front_close();
		VAClose();
		return;
	}
	y = drag.y + drag.h / 2;
	x = drag.x + 4;
	l0 = Hero.lane;
	pen_at(1, x, y);
	run(1);
	pen_at(1, x + 200, y);
	run(1);
	pen_at(0, x + 200, y);
	run(10);
	l1 = Hero.lane;
	d_big = lane_delta(l0, l1, ww.numsegs);
	pen_at(1, x, y);
	run(1);
	pen_at(1, x + 8, y);
	run(1);
	pen_at(0, x + 8, y);
	run(10);
	l2 = Hero.lane;
	d_small = lane_delta(l1, l2, ww.numsegs);
	/* Dragging right turns the lane number down; a flick clamped to half a
	   turn lands on the opposite lane, which reads as either way. */
	CHECK(d_small < 0 && d_big != 0 && (d_big < 0 || d_big == ww.numsegs / 2),
	      "%dx%d: a 200 px flick turned %d lanes, 8 px %d", w, h, d_big, d_small);
	/* Down, up and down again in one read, far along the strip. */
	l0 = Hero.lane;
	pen_at(1, x, y);
	run(1);
	pen_at(1, x + 150, y);
	inp.tapped = 1;
	inp.tap_x = (int16)(x + 150 - screen.play.x);
	inp.tap_y = (int16)(y - screen.play.y);
	run(1);
	inp.tapped = 0;
	pen_at(0, x + 150, y);
	run(10);
	CHECK(lane_delta(l0, Hero.lane, ww.numsegs) == 0, "%dx%d: tap with the pen down turned the claw %d lanes", w, h,
	      lane_delta(l0, Hero.lane, ww.numsegs));
	front_close();
	VAClose();
}

/* The status panel and touch controls at one layout: shots of each state,
   a drag along the strip turns the claw the way the right button does, Fire
   and Zap act while held, a tap on the strip doesn't pause and one on the
   pause button does. */
static void check_panel(const char *dir, int16 w, int16 h, uint8 depth)
{
	PlatRect drag, fire, zap, pause;
	int i, l0, l1, l2, n, from, x, y, step;

	make_screen(w, h, depth, 0);
	CHECK(VAInit(), "VAInit");
	host_forget_save();
	store_defaults();
	front_init();
	memset(&inp, 0, sizeof(inp));
	inp.rim = -1;
	front_goto(FRONT_PLAY, 1);
	for (i = 0; i < 400 && intro_running(); i++)
		run(1);
	run(4);
	dshot(dir, w, h, depth, "panel");
	if (!find_ctl(PANEL_DRAG, &drag) || !find_ctl(PANEL_FIRE, &fire) ||
	    !find_ctl(PANEL_ZAP, &zap) || !find_ctl(PANEL_PAUSE, &pause)) {
		CHECK(0, "%dx%d: a control is missing from the status area", w, h);
		front_close();
		VAClose();
		return;
	}
	n = ww.numsegs;
	/* Drag right along the strip, a few pixels a step. */
	l0 = Hero.lane;
	y = drag.y + drag.h / 2;
	x = drag.x + 4;
	step = drag.w / 40 > 1 ? drag.w / 40 : 1;
	pen_at(1, x, y);
	run(1);
	for (i = 0; i < 12; i++) {
		x += step;
		pen_at(1, x, y);
		run(1);
	}
	dshot(dir, w, h, depth, "panel-drag");
	pen_at(0, x, y);
	run(10);
	l1 = Hero.lane;
	CHECK(front_screen() == FRONT_PLAY, "%dx%d: a drag on the strip left play (%d)", w, h, front_screen());
	CHECK(l1 != l0, "%dx%d: a drag on the strip didn't move the claw (lane %d)", w, h, l0);
	press(PLAT_ACT_RIGHT);
	run(6);
	l2 = Hero.lane;
	/* The strip dragged right and the right button turn the same way. */
	CHECK(lane_delta(l0, l1, n) * lane_delta(l1, l2, n) > 0,
	      "%dx%d: the strip turned the claw %d lanes, the right button %d", w, h,
	      lane_delta(l0, l1, n), lane_delta(l1, l2, n));
	/* Fire held on the button. */
	from = host_sounds;
	pen_at(1, fire.x + fire.w / 2, fire.y + fire.h / 2);
	run(2);
	dshot(dir, w, h, depth, "panel-fire");
	run(8);
	pen_at(0, fire.x + fire.w / 2, fire.y + fire.h / 2);
	run(2);
	CHECK(sounds_since(from, SND_PHAZERIN) >= 2, "%dx%d: holding Fire fired %d shots", w, h,
	      sounds_since(from, SND_PHAZERIN));
	CHECK(front_screen() == FRONT_PLAY, "%dx%d: Fire left play (%d)", w, h, front_screen());
	/* Zap. */
	from = host_sounds;
	CHECK(ThisLevel.plSuperZaps > 0, "%dx%d: no superzap to start with", w, h);
	pen_at(1, zap.x + zap.w / 2, zap.y + zap.h / 2);
	run(2);
	dshot(dir, w, h, depth, "panel-zap");
	pen_at(0, zap.x + zap.w / 2, zap.y + zap.h / 2);
	run(4);
	CHECK(ThisLevel.plSuperZaps <= 0 && sounds_since(from, SND_ZZSUPERZAP) == 1,
	      "%dx%d: Zap didn't superzap (%d left)", w, h, ThisLevel.plSuperZaps);
	dshot(dir, w, h, depth, "panel-zapped");
	CHECK(front_screen() == FRONT_PLAY, "%dx%d: Zap left play (%d)", w, h, front_screen());
	/* A tap elsewhere in the status area keeps playing, the pause button pauses. */
	tap_at(drag.x + drag.w / 2 - screen.play.x, drag.y + drag.h / 2 - screen.play.y);
	tap_at(screen.status.x + 1 - screen.play.x, screen.status.y + screen.status.h - 2 - screen.play.y);
	CHECK(front_screen() == FRONT_PLAY, "%dx%d: a tap in the status area left play (%d)", w, h, front_screen());
	tap_at(pause.x + pause.w / 2 - screen.play.x, pause.y + pause.h / 2 - screen.play.y);
	CHECK(front_screen() == FRONT_PAUSE, "%dx%d: the pause button went to %d", w, h, front_screen());
	run(2);
	tap_zone_id(0);
	CHECK(front_screen() == FRONT_PLAY, "%dx%d: Resume went to %d", w, h, front_screen());
	run(4);
	dshot(dir, w, h, depth, "panel-resumed");
	if (depth == 16) {
		/* Frames where nothing in the panel changed leave its pixels
		   alone; moving the strip paints it again. */
		uint16 *px = (uint16 *)(screen.bits + (size_t)(drag.y + 4) * screen.row_bytes) + drag.x + 5;

		*px = 0x1234;
		run(3);
		CHECK(*px == 0x1234, "%dx%d: the panel was painted with nothing changed", w, h);
		pen_at(1, drag.x + drag.w / 2, drag.y + drag.h / 2);
		run(1);
		pen_at(1, drag.x + drag.w / 2 + 3, drag.y + drag.h / 2);
		run(1);
		pen_at(0, drag.x + drag.w / 2 + 3, drag.y + drag.h / 2);
		run(1);
		CHECK(*px != 0x1234, "%dx%d: the strip wasn't painted after a drag", w, h);
	}
	/* The score to beat, with a free life. */
	IncreaseScore(20500);
	run(2);
	dshot(dir, w, h, depth, "panel-beat");
	front_close();
	VAClose();
}

/* Plays one recorded demo and writes every step as a PNG, for making a GIF.
   Usage: test_front DATADIR OUTDIR --frames DEMO_ID WIDTH HEIGHT */
static void dump_demo(const char *dir, int id, int w, int h)
{
	int s;
	char name[96];

	make_screen((int16)w, (int16)h, 16, 1);
	VAInit();
	store_defaults();
	front_init();
	front_goto(FRONT_DEMO, (int16)id);
	for (s = 0; s < 2000 && front_screen() == FRONT_DEMO; s++) {
		run(1);
		sprintf(name, "f%04d.png", s);
		save_png(dir, name);
	}
	printf("%d frames\n", s);
	front_close();
	VAClose();
}

int main(int argc, char **argv)
{
	if (argc == 7 && !strcmp(argv[3], "--frames")) {
		host_set_data_dir(argv[1]);
		dump_demo(argv[2], atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
		return 0;
	}
	if (argc != 3) {
		fprintf(stderr, "usage: test_front DATADIR OUTDIR\n");
		return 2;
	}
	host_set_data_dir(argv[1]);
	check_store();
	check_lsel();
	check_demos(argv[2], 320, 320);
	check_demos(argv[2], 160, 160);
	check_attract();
	check_screens(argv[2], 160, 160, 2, 0);
	check_screens(argv[2], 320, 320, 16, 1);
	check_screens(argv[2], 320, 480, 16, 1);
	check_screens(argv[2], 480, 320, 16, 1);
	check_screens(argv[2], 560, 160, 16, 1);
	check_screens(argv[2], 160, 560, 16, 1);
	check_ui(argv[2], 160, 160, 1);
	check_ui(argv[2], 160, 160, 2);
	check_ui(argv[2], 160, 160, 4);
	check_ui(argv[2], 160, 160, 8);
	check_ui(argv[2], 160, 160, 16);
	check_ui(argv[2], 320, 320, 4);
	check_ui(argv[2], 320, 320, 16);
	check_ui(argv[2], 320, 480, 16);
	check_ui(argv[2], 480, 320, 16);
	check_ui(argv[2], 560, 160, 4);
	check_ui(argv[2], 160, 560, 16);
	check_panel(argv[2], 320, 480, 16);
	check_panel(argv[2], 320, 480, 4);
	check_panel(argv[2], 480, 320, 16);
	check_panel(argv[2], 480, 320, 2);
	check_panel(argv[2], 560, 160, 4);
	check_panel(argv[2], 560, 160, 2);
	check_panel(argv[2], 160, 560, 16);
	check_panel(argv[2], 160, 560, 1);
	check_flick(560, 160, 4);
	if (failures) {
		printf("front: %d failures\n", failures);
		return 1;
	}
	printf("front: store, demos, menus, HUD, pause, game over and name entry ok; PNGs in %s\n", argv[2]);
	return 0;
}
