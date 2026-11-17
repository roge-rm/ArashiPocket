/* Host test for the game loop and field (game.c, grid.c, player.c): loads
   levels, runs scripted input, checks the claw's lane is the same on every
   run, checks the step rate and writes PNGs of levels and claw positions.
   Usage: test_game DATADIR OUTDIR */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/plat.h"
#include "../core/game.h"
#include "../core/qdrandom.h"
#include "../core/testpat.h"
#include "plat_host.h"
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

/* The scripted input for step n. Held right, then left, a Jog Dial turn,
   then the stylus at the rim at 3 and 9 o'clock. */
static void script(int n, PlatInput *in)
{
	memset(in, 0, sizeof(*in));
	in->rim = -1;
	if (n >= 20 && n < 50)
		in->actions = PLAT_ACT_RIGHT;
	else if (n >= 60 && n < 72)
		in->actions = PLAT_ACT_LEFT;
	else if (n == 80)
		in->spin = 3;
	else if (n == 82)
		in->actions = PLAT_ACT_FIRE | PLAT_ACT_ZAP;
	else if (n >= 100 && n < 130) {
		in->pen_down = 1;
		in->rim = 64;
	} else if (n >= 130 && n < 160) {
		in->pen_down = 1;
		in->rim = 192;
	}
}

#define SCRIPT_STEPS 170

/* Runs the script and leaves lane and rawpos after each step in trace. */
static void run_script(int16 level, int16 *trace)
{
	PlatInput in;
	int n;

	CHECK(game_init(level), "game_init %d", level);
	for (n = 0; n < SCRIPT_STEPS; n++) {
		script(n, &in);
		game_tick(&in);
		trace[2 * n] = Hero.lane;
		trace[2 * n + 1] = Hero.rawpos;
	}
}

static int16 rim_distance(int16 a, int16 b)
{
	int16 d = (int16)(((a - b + 128) & 255) - 128);

	return (int16)(d < 0 ? -d : d);
}

static void check_script(const char *dir, int16 level)
{
	static int16 t1[2 * SCRIPT_STEPS], t2[2 * SCRIPT_STEPS];
	int16 lane_after_right, best, i;
	make_screen(160, 160, 2, 0);
	CHECK(VAInit(), "VAInit");
	run_script(level, t1);
	/* Right held from step 20 for 30 steps: the claw has gone round the
	   other way to LaneDir. */
	lane_after_right = t1[2 * 25];
	CHECK(lane_after_right == (LaneDir > 0 ? 1 : ww.numsegs - 1) ||
	      !ww.wraps, "first right press went to lane %d, dir %d", lane_after_right, LaneDir);
	/* At 3 o'clock (rim 64) at the end of that stylus run the claw is on the
	   lane nearest it; likewise at 9 o'clock at the end. */
	best = 0;
	for (i = 0; i < ww.numsegs; i++)
		if (rim_distance(LaneRim[i], 192) < rim_distance(LaneRim[best], 192))
			best = i;
	CHECK(t1[2 * (SCRIPT_STEPS - 11)] == best, "stylus at 192 gave lane %d, wanted %d",
	      t1[2 * (SCRIPT_STEPS - 11)], best);
	if (level == 1)
		CHECK(t1[2 * (SCRIPT_STEPS - 1)] == 3 && t1[2 * (SCRIPT_STEPS - 1) + 1] == 28,
		      "level 1 script ended on lane %d, rawpos %d, not 3 and 28",
		      t1[2 * (SCRIPT_STEPS - 1)], t1[2 * (SCRIPT_STEPS - 1) + 1]);
	game_close();
	run_script(level, t2);
	CHECK(memcmp(t1, t2, sizeof(t1)) == 0, "level %d: two runs of the script differ", level);
	game_close();
	VAClose();
	(void)dir;
}

/* Level pictures at a size and depth. */
static void pictures(const char *dir, int16 w, int16 h, uint8 depth, uint8 le16)
{
	static const int16 levels[] = { 1, 2, 3, 5, 9, 17, 25, 33, 41, 49, 57, 65, 73, 81, 89, 97 };
	PlatInput in;
	char name[96];
	size_t k;
	int n;

	make_screen(w, h, depth, le16);
	CHECK(VAInit(), "VAInit %dx%d", w, h);
	CHECK(game_init(1), "game_init");
	for (k = 0; k < sizeof(levels) / sizeof(levels[0]); k++) {
		CHECK(game_load_level(levels[k]), "level %d loads", levels[k]);
		memset(&in, 0, sizeof(in));
		in.rim = -1;
		in.actions = PLAT_ACT_RIGHT;
		for (n = 0; n < 40; n++) {
			game_tick(&in);
			in.actions = 0;
		}
		game_draw();
		game_draw();
		if (k < 6 || (w == 160 && depth == 2)) {
			sprintf(name, "game-%dx%d-%d%s-l%d.png", w, h, depth, le16 ? "le" : "", levels[k]);
			save_png(dir, name);
		}
	}
	game_close();
	VAClose();
}

/* Every level loads and fits at every size; lane rims go round. */
static void check_levels(void)
{
	int16 l, i;
	int bad = 0;

	make_screen(160, 160, 2, 0);
	CHECK(VAInit(), "VAInit");
	CHECK(game_init(1), "game_init");
	for (l = 1; l <= 99; l++) {
		if (!game_load_level(l)) {
			bad++;
			continue;
		}
		for (i = 0; i < ww.numsegs; i++)
			if (LaneRim[i] < 0 || LaneRim[i] > 255)
				bad++;
		if (ww.x[0][0] < -200 || ww.x[0][0] > 400)
			bad++;
	}
	CHECK(bad == 0, "%d level problems", bad);
	game_close();
	VAClose();
}

/* 20 steps a second at any frame rate, and a cap when the device is slow. */
static void check_timing(void)
{
	static const uint32 frame_ms[] = { 10, 33, 50, 100, 250 };
	size_t k;
	PlatInput in;
	uint32 t, s0, draws;

	make_screen(160, 160, 2, 0);
	CHECK(VAInit(), "VAInit");
	CHECK(game_init(1), "game_init");
	memset(&in, 0, sizeof(in));
	in.rim = -1;
	host_set_input(&in);
	for (k = 0; k < sizeof(frame_ms) / sizeof(frame_ms[0]); k++) {
		host_set_clock(1, 1000);
		game_resume();
		game_frame();               /* the first frame's step */
		s0 = game_steps;
		draws = 0;
		for (t = 1000 + frame_ms[k]; t <= 11000; t += frame_ms[k])
			{
				host_set_clock(1, t);
				if (game_frame())
					draws++;
			}
		if (frame_ms[k] <= 50) {
			CHECK(game_steps - s0 >= 198 && game_steps - s0 <= 200,
			      "%u ms frames: %u steps in 10 s, wanted 200", (unsigned)frame_ms[k],
			      (unsigned)(game_steps - s0));
		} else {
			/* 100 ms is 2 steps a frame, 250 ms is 5 owed and capped at 4. */
			uint32 per = frame_ms[k] * GAME_HZ / 1000;

			if (per > GAME_MAXCATCHUP)
				per = GAME_MAXCATCHUP;
			CHECK(game_steps - s0 >= per * draws - 5 && game_steps - s0 <= per * draws + 5,
			      "%u ms frames: %u steps in %u frames", (unsigned)frame_ms[k],
			      (unsigned)(game_steps - s0), (unsigned)draws);
		}
	}
	host_set_clock(0, 0);
	game_close();
	VAClose();
}

/* One step with actions, then the picture. */
static void step_draw(uint16 actions)
{
	PlatInput in;

	memset(&in, 0, sizeof(in));
	in.rim = -1;
	in.actions = actions;
	game_tick(&in);
	game_draw();
}

/* Moves, fires, zaps and dies. Shots stay within MAXSHOTS, travel down the
   tube and leave at the far end; superzaps are counted per level. */
static void check_shots(const char *dir, int16 w, int16 h, uint8 depth, uint8 le16)
{
	char name[96];
	int n, maxshots = 0, fired_steps = 0, seen_far = 0, before;
	int16 lane0, lane1;
	uint16 wb;

	make_screen(w, h, depth, le16);
	CHECK(VAInit(), "VAInit");
	CHECK(game_init(1), "game_init");
	CHECK(ThisLevel.shSpeed == 4, "shot speed %d", ThisLevel.shSpeed);
	CHECK(ThisLevel.plSuperZaps == 1, "zaps %d", ThisLevel.plSuperZaps);

	/* Fire held for 60 steps: one shot every other step until nine are out. */
	for (n = 0; n < 60; n++) {
		before = shotcount;
		step_draw(PLAT_ACT_FIRE);
		if (shotcount > maxshots)
			maxshots = shotcount;
		CHECK(shotcount <= MAXSHOTS, "%d shots", shotcount);
		if (shotcount > before)
			fired_steps++;
		if (n == 8) {
			CHECK(shotcount == 4 || shotcount == 5, "after 9 steps of fire there are %d shots", shotcount);
			sprintf(name, "shots-%dx%d-%d-a.png", w, h, depth);
			save_png(dir, name);
		}
		if (n == 20) {
			sprintf(name, "shots-%dx%d-%d-b.png", w, h, depth);
			save_png(dir, name);
		}
	}
	CHECK(maxshots == MAXSHOTS, "most shots at once was %d", maxshots);
	/* A shot is out for 31 steps, so at one every two steps 15 would be out:
	   the limit holds it at 9. */
	CHECK(fired_steps >= 9, "only %d steps fired", fired_steps);
	for (n = 0; n < MAXSHOTS; n++)
		seen_far += shots[n].level > DEPTH / 2;
	CHECK(seen_far > 0, "no shot got past half way");
	CHECK(shotcount == 9, "9 shots at the end of 60 steps, got %d", shotcount);
	for (n = 0; n < 40; n++)
		step_draw(0);
	CHECK(shotcount == 0, "%d shots left after the tube is clear", shotcount);

	/* Moving: a press fires at once on the new lane. */
	lane0 = Hero.lane;
	for (n = 0; n < 30; n++)
		step_draw(n < 12 ? (PLAT_ACT_RIGHT | PLAT_ACT_FIRE) : PLAT_ACT_FIRE);
	lane1 = Hero.lane;
	CHECK(lane0 != lane1, "claw stayed on lane %d", lane0);
	CHECK(shotcount > 0 && shotcount <= MAXSHOTS, "%d shots after moving", shotcount);
	sprintf(name, "shots-%dx%d-%d-moved.png", w, h, depth);
	save_png(dir, name);
	for (n = 0; n < 40; n++)
		step_draw(0);

	/* Superzap: the first white-flashes BG2 for one step, the second is
	   the single enemy zap, the third does nothing. */
	wb = VAPixelValue(BG2);
	step_draw(PLAT_ACT_ZAP);
	CHECK(Hero.superzapping == 1 && ThisLevel.plSuperZaps == 0, "zap 1: %d, %d left",
	      Hero.superzapping, ThisLevel.plSuperZaps);
	/* On grey screens BG2 may already be the brightest. */
	CHECK(depth < 8 || VAPixelValue(BG2) != wb, "BG2 did not change in the flash");
	sprintf(name, "zap-%dx%d-%d.png", w, h, depth);
	save_png(dir, name);
	CHECK(ShotHitTest(3, 50) == -2, "zap hit test");
	step_draw(0);
	CHECK(Hero.superzapping == 0 && VAPixelValue(BG2) == wb, "flash did not end");
	step_draw(PLAT_ACT_ZAP);
	CHECK(Hero.superzapping == 2 && ThisLevel.plSuperZaps == -1, "zap 2: %d, %d left",
	      Hero.superzapping, ThisLevel.plSuperZaps);
	CHECK(ShotHitTest(3, 50) == -2 && Hero.superzapping == 0, "single zap hit test");
	step_draw(0);
	step_draw(PLAT_ACT_ZAP);
	CHECK(Hero.superzapping == 0, "a third zap did something");

	/* A shot hits what is at its level and is removed. */
	InitShots();
	AddShot(2);
	shots[0].level = 40;
	CHECK(ShotHitTest(3, 40) == 0, "wrong lane hit");
	CHECK(ShotHitTest(2, 30) == 0, "shot at 40 hit level 30");
	CHECK(ShotHitTest(2, 44) == 0, "shot at 40 hit level 44");
	CHECK(ShotHitTest(2, 40) == -1 && shotcount == 0, "shot did not hit level 40");

	/* Death: a flipper drops the claw, a fuseball blows it up; the game
	   gives back a life and a new claw. */
	game_load_level(1);
	CHECK(Hero.lives == 2, "lives %d", Hero.lives);
	Hero.lanestat[Hero.oldlane] = CapFlip;
	step_draw(0);
	CHECK(Hero.state == HeroDropping, "state %d after a flipper", Hero.state);
	for (n = 0; n < 12 && Hero.state == HeroDropping; n++) {
		step_draw(PLAT_ACT_RIGHT);
		if (n == 3) {
			sprintf(name, "drop-%dx%d-%d.png", w, h, depth);
			save_png(dir, name);
		}
	}
	CHECK(Hero.state == HeroPostMortem && Hero.dropdepth == DEPTH, "state %d depth %d",
	      Hero.state, Hero.dropdepth);
	step_draw(0);
	sprintf(name, "boom-%dx%d-%d.png", w, h, depth);
	save_png(dir, name);
	for (n = 0; n < 62 && Hero.lives == 2; n++)
		step_draw(0);
	CHECK(Hero.lives == 1 && Hero.state == HeroPlaying && Hero.dropdepth == 0,
	      "after death: lives %d state %d", Hero.lives, Hero.state);
	Hero.lanestat[Hero.oldlane] = CapFuse;
	step_draw(0);
	CHECK(Hero.state == HeroDisintegrating, "state %d after a fuseball", Hero.state);
	for (n = 0; n < 70 && Hero.lives == 1; n++)
		step_draw(0);
	CHECK(Hero.lives == 0 && Hero.state == HeroPlaying, "after 2nd death: lives %d state %d",
	      Hero.lives, Hero.state);

	game_close();
	VAClose();
}

static void bot_step(void);

/* The bot: sits in the lane of the enemy nearest the rim and fires. */
static void bot_input(PlatInput *in, uint16 extra)
{
	EnemyInfo e[40];
	int16 n = ListEnemies(e, 40), i, best = -1;

	memset(in, 0, sizeof(*in));
	in->rim = -1;
	in->actions = (uint16)(PLAT_ACT_FIRE | extra);
	for (i = 0; i < n; i++)
		if (best < 0 || e[i].level < e[best].level)
			best = i;
	if (best >= 0) {
		in->pen_down = 1;
		in->rim = LaneRim[e[best].lane];
	}
}

/* Level 1 (flippers only, tanker count NaN in the script), a level with a
   tanker, a superzap, and a game with no input. */
static void check_enemies(const char *dir, int16 w, int16 h, uint8 depth, uint8 le16)
{
	PlatInput in;
	EnemyInfo e[40];
	char name[96];
	int n, seen = 0, shot1 = 0, shot2 = 0, flips0, split = 0, tanker_seen = 0, k;
	int32 sc0;
	int16 lv, prevfl, totalprev;

	make_screen(w, h, depth, le16);
	CHECK(VAInit(), "VAInit");
	qd_seed(12345);
	CHECK(game_init(1), "game_init");
	EnemySpawning = 1;

	/* Level 1: four flippers and no tankers (NaN count is 0). */
	CHECK(ThisLevel.flCount == 4 && ThisLevel.flPoints == 150 && ThisLevel.tk[0].count == 0 &&
	      ThisLevel.totalCount == 4, "level 1 counts: %d flippers, %d tankers, total %d",
	      ThisLevel.flCount, ThisLevel.tk[0].count, ThisLevel.totalCount);
	for (n = 0; n < 1500 && ThisLevel.flCount > 0; n++) {
		bot_input(&in, 0);
		game_tick(&in);
		game_draw();
		k = ListEnemies(e, 40);
		if (k > seen) {
			seen = k;
			if (k == 2 && !shot1) {
				sprintf(name, "enemy-%dx%d-%d-flippers.png", w, h, depth);
				save_png(dir, name);
				shot1 = 1;
			}
		}
		if (Hero.lives < 2)
			break;
	}
	CHECK(seen >= 2, "only %d flippers at once", seen);
	CHECK(ThisLevel.flCount == 0, "%d flippers left after %d steps", ThisLevel.flCount, n);
	CHECK(Hero.score == 600, "score %ld, wanted 4 x 150", (long)Hero.score);
	CHECK(ThisLevel.totalCount == 0, "total count %d", ThisLevel.totalCount);

	/* A level with a tanker: it splits into two flippers when hit. */
	for (lv = 2; lv <= 99 && !(game_load_level(lv) && ThisLevel.tk[0].count > 0); lv++)
		;
	CHECK(lv < 99, "no level with tankers");
	ThisLevel.fuCount = ThisLevel.puCount = 0;      /* the new enemies have their own check */
	ThisLevel.tk[1].count = ThisLevel.tk[2].count = 0;
	ThisLevel.totalCount = (int16)(ThisLevel.flCount + ThisLevel.tk[0].count);
	flips0 = ThisLevel.flCount;
	CHECK(ThisLevel.totalCount == flips0 + ThisLevel.tk[0].count + ThisLevel.tk[1].count +
	      ThisLevel.tk[2].count, "total count %d", ThisLevel.totalCount);
	sc0 = Hero.score;
	prevfl = ThisLevel.flCount;
	totalprev = ThisLevel.totalCount;
	/* First let the tanker climb unshot for a picture of it. */
	for (n = 0; n < 400 && !shot2; n++) {
		memset(&in, 0, sizeof(in));
		in.rim = -1;
		game_tick(&in);
		game_draw();
		k = ListEnemies(e, 40);
		while (k-- > 0)
			if (e[k].kind == EnemyTanker && e[k].level < 70) {
				sprintf(name, "enemy-%dx%d-%d-tanker.png", w, h, depth);
				save_png(dir, name);
				shot2 = 1;
			}
	}
	CHECK(shot2, "no tanker climbed to level 70");
	for (n = 0; n < 2500 && !split; n++) {
		bot_step();
		k = ListEnemies(e, 40);
		while (k-- > 0)
			if (e[k].kind == EnemyTanker)
				tanker_seen = 1;
		if (ThisLevel.flCount > prevfl) {       /* two more flippers: the tanker split */
			CHECK(ThisLevel.flCount == prevfl + 2, "split gave %d flippers", ThisLevel.flCount - prevfl);
			CHECK(ThisLevel.totalCount == totalprev + 1, "split changed the total by %d",
			      ThisLevel.totalCount - totalprev);
			split = 1;
		}
		prevfl = ThisLevel.flCount;
		totalprev = ThisLevel.totalCount;
	}
	CHECK(tanker_seen, "level %d: no tanker appeared", lv);
	CHECK(split && Hero.score > sc0, "level %d: tanker did not split (tankers %d, flippers %d)", lv,
	      ThisLevel.tk[0].count, ThisLevel.flCount);
	CHECK(Hero.score - sc0 >= ThisLevel.tk[0].points, "level %d: score %ld", lv, (long)(Hero.score - sc0));
	/* Two flippers from a tanker, then the level's own on top. */
	n = ListEnemies(e, 40);
	sprintf(name, "enemy-%dx%d-%d-split.png", w, h, depth);
	save_png(dir, name);

	/* Superzap: every enemy on the field dies at once. */
	game_load_level(1);
	for (n = 0; n < 300 && ListEnemies(e, 40) < 3; n++) {
		memset(&in, 0, sizeof(in));
		in.rim = -1;
		game_tick(&in);
		game_draw();
	}
	k = ListEnemies(e, 40);
	CHECK(k >= 2, "only %d flippers for the zap", k);
	sc0 = Hero.score;
	memset(&in, 0, sizeof(in));
	in.rim = -1;
	in.actions = PLAT_ACT_ZAP;
	game_tick(&in);
	game_draw();
	CHECK(ListEnemies(e, 40) == 0 && Hero.score == sc0 + 150L * k, "zap: %d left, score +%ld for %d",
	      ListEnemies(e, 40), (long)(Hero.score - sc0), k);

	/* No input: a flipper gets the claw. */
	game_load_level(1);
	{
		int dropped = 0;

		for (n = 0; n < 4000 && Hero.lives == 2; n++) {
			memset(&in, 0, sizeof(in));
			in.rim = -1;
			game_tick(&in);
			game_draw();
			if (Hero.state == HeroDropping && !dropped) {
				dropped = 1;
				sprintf(name, "enemy-%dx%d-%d-caught.png", w, h, depth);
				save_png(dir, name);
			}
		}
		CHECK(dropped && Hero.lives == 1, "no input: caught %d, lives %d after %d steps", dropped,
		      Hero.lives, n);
	}
	printf("enemies %dx%d x%d: caught after %d steps\n", w, h, depth, n);
	EnemySpawning = 0;
	game_close();
	VAClose();
}

/* Which enemies a level has, then keeps only one kind: 0 fuseballs, 1
   pulsars, 2 fuse tankers, 3 pulsar tankers. */
static void isolate(int kind)
{
	ThisLevel.flCount = 0;
	ThisLevel.fuCount = (int16)(kind == 0 ? ThisLevel.fuCount : 0);
	ThisLevel.puCount = (int16)(kind == 1 ? ThisLevel.puCount : 0);
	ThisLevel.tk[0].count = 0;
	ThisLevel.tk[1].count = (int16)(kind == 2 ? ThisLevel.tk[1].count : 0);
	ThisLevel.tk[2].count = (int16)(kind == 3 ? ThisLevel.tk[2].count : 0);
	ThisLevel.totalCount = (int16)(ThisLevel.fuCount + ThisLevel.puCount + ThisLevel.tk[1].count +
	                               ThisLevel.tk[2].count);
}

static int find_level(int kind)
{
	int lv;

	for (lv = 2; lv <= 99; lv++) {
		if (!game_load_level((int16)lv))
			continue;
		if ((kind == 0 && ThisLevel.fuCount > 1) || (kind == 1 && ThisLevel.puCount > 1) ||
		    (kind == 2 && ThisLevel.tk[1].count > 0) || (kind == 3 && ThisLevel.tk[2].count > 0))
			return lv;
	}
	return 0;
}

/* The claw can't be caught (the enemies' marks are wiped) while the bot
   kills. */
static int saw_hazard;

static void bot_step(void)
{
	PlatInput in;
	int16 i;

	bot_input(&in, 0);
	game_tick(&in);
	for (i = 0; i < ww.numsegs; i++)
		if (Hero.lanestat[i] & CapPuls)
			saw_hazard = 1;
	for (i = 0; i <= MAXSEGS; i++) {
		if (i < MAXSEGS)
			Hero.lanestat[i] = 0;
		Hero.segmstat[i] = 0;
	}
	game_draw();
}

/* Fuseballs, pulsars and their tankers: they spawn, die for the original's
   points, tankers split into the right kind, and each kind can kill the
   claw. */
static void check_new_enemies(const char *dir, int16 w, int16 h, uint8 depth, uint8 le16)
{
	static const char *const names[4] = { "fuseball", "pulsar", "fusetanker", "pulsartanker" };
	char name[96];
	int kind, lv, n, k, sawpic, hazard = 0, kills, bad, splits, paid;
	EnemyInfo e[40];
	int32 sc, sc0;
	int16 cnt, cnt0, tot0;

	make_screen(w, h, depth, le16);
	CHECK(VAInit(), "VAInit");
	qd_seed(12345);
	CHECK(game_init(1), "game_init");
	EnemySpawning = 1;

	for (kind = 0; kind < 4; kind++) {
		lv = find_level(kind);
		CHECK(lv, "no level with %s", names[kind]);
		if (!lv)
			continue;
		isolate(kind);
		cnt0 = (int16)(kind == 0 ? ThisLevel.fuCount : kind == 1 ? ThisLevel.puCount :
		               ThisLevel.tk[kind - 1].count);
		tot0 = ThisLevel.totalCount;
		sc0 = Hero.score = 0;
		sawpic = kills = bad = splits = paid = 0;
		saw_hazard = 0;
		printf("%s: level %d, %d to kill, points %d/%d, tanker points %d/%d\n", names[kind], lv, cnt0,
		       kind == 0 ? ThisLevel.fuPoints : ThisLevel.puPoints,
		       ThisLevel.fuBullseye, ThisLevel.tk[1].points, ThisLevel.tk[2].points);
		sc = 0;
		for (n = 0; n < 6000 && ThisLevel.totalCount > 0; n++) {
			int16 fu0 = ThisLevel.fuCount, pu0 = ThisLevel.puCount, tot = ThisLevel.totalCount;
			int32 s0 = Hero.score, d;

			bot_step();
			d = Hero.score - s0;
			k = ListEnemies(e, 40);
			while (k-- > 0) {
				int ek = e[k].kind;

				if (!sawpic && ((kind == 0 && ek == EnemyFuseball) || (kind == 1 && ek == EnemyPulsar) ||
				                (kind >= 2 && ek == EnemyTanker)) && e[k].level < 70 && e[k].level > 20) {
					sprintf(name, "newenemy-%dx%d-%d-%s.png", w, h, depth, names[kind]);
					save_png(dir, name);
					sawpic = 1;
				}
			}
			if (kind == 0 && ThisLevel.fuCount == fu0 - 1 && ThisLevel.totalCount == tot - 1) {
				kills++;
				if (d != ThisLevel.fuBullseye && d != ThisLevel.fuPoints && d != (ThisLevel.fuPoints >> 1))
					bad++;
			}
			if (kind == 1 && ThisLevel.puCount == pu0 - 1 && ThisLevel.totalCount == tot - 1) {
				kills++;
				if (d != ThisLevel.puPoints)
					bad++;
			}
			if (kind >= 2) {
				cnt = (int16)(kind == 2 ? ThisLevel.fuCount - fu0 : ThisLevel.puCount - pu0);
				if (cnt == 2) {
					CHECK(ThisLevel.totalCount == tot + 1, "%s split changed the total by %d", names[kind],
					      ThisLevel.totalCount - tot);
					splits++;
					if (d >= ThisLevel.tk[kind - 1].points)
						paid++;     /* shot, not just up at the rim */
					sprintf(name, "newenemy-%dx%d-%d-%s-split.png", w, h, depth, names[kind]);
					if (splits == 1)
						save_png(dir, name);
				}
				if (ThisLevel.fuCount == fu0 - 1 && kind == 2 && ThisLevel.totalCount == tot - 1) {
					kills += 100;       /* a ball of the split */
					if (d != ThisLevel.fuBullseye && d != ThisLevel.fuPoints && d != (ThisLevel.fuPoints >> 1))
						bad++;
				}
				if (ThisLevel.puCount == pu0 - 1 && kind == 3 && ThisLevel.totalCount == tot - 1) {
					kills += 100;
					if (d != ThisLevel.puPoints)
						bad++;
				}
			}
			sc = Hero.score;
		}
		CHECK(ThisLevel.totalCount == 0, "%s: %d enemies left after %d steps", names[kind],
		      ThisLevel.totalCount, n);
		CHECK(bad == 0, "%s: %d kills scored other than the level's points", names[kind], bad);
		if (kind < 2) {
			CHECK(kills == cnt0, "%s: %d kills of %d", names[kind], kills, cnt0);
			CHECK(sc - sc0 > 0, "%s: score %ld", names[kind], (long)(sc - sc0));
		} else {
			CHECK(splits == cnt0 && kills / 100 == 2 * splits && kills % 100 == 0,
			      "%s: %d splits of %d, %d balls killed", names[kind], splits, cnt0, kills / 100);
			CHECK(paid >= 1, "%s: no tanker was shot", names[kind]);
		}
		hazard = saw_hazard;
		if (kind == 1 || kind == 3)
			CHECK(hazard, "%s never charged a lane", names[kind]);
		printf("%s: %d steps, score %ld\n", names[kind], n, (long)(sc - sc0));
		(void)tot0;
	}

	/* A superzap clears fuseballs and pulsars. */
	for (kind = 0; kind < 2; kind++) {
		lv = find_level(kind);
		if (!lv)
			continue;
		isolate(kind);
		for (n = 0; n < 600 && ListEnemies(e, 40) < 2; n++)
			step_draw(0);
		k = ListEnemies(e, 40);
		CHECK(k >= 2, "%s: only %d for the zap", names[kind], k);
		memset(&hazard, 0, sizeof(hazard));
		sc0 = Hero.score;
		Hero.lanestat[0] = 0;
		step_draw(PLAT_ACT_ZAP);
		CHECK(ListEnemies(e, 40) == 0 && Hero.score > sc0, "%s zap: %d left, +%ld", names[kind],
		      ListEnemies(e, 40), (long)(Hero.score - sc0));
	}

	/* No input: fuseballs and pulsars can kill the claw. */
	for (kind = 0; kind < 2; kind++) {
		int died = 0, stt = -1;

		lv = find_level(kind);
		if (!lv)
			continue;
		isolate(kind);
		for (n = 0; n < 8000 && Hero.lives == 2 && !died; n++) {
			step_draw(0);
			if (Hero.state != HeroPlaying && Hero.state != HeroDead) {
				stt = Hero.state;
				died = 1;
				sprintf(name, "newenemy-%dx%d-%d-%s-caught.png", w, h, depth, names[kind]);
				save_png(dir, name);
			}
		}
		CHECK(died, "%s did not kill the claw in %d steps", names[kind], n);
		CHECK(stt == (kind == 0 ? HeroDisintegrating : HeroPostMortem), "%s: claw state %d",
		      names[kind], stt);
		printf("%s killed the claw after %d steps (state %d)\n", names[kind], n, stt);
	}
	EnemySpawning = 0;
	game_close();
	VAClose();
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: test_game DATADIR OUTDIR\n");
		return 2;
	}
	host_set_data_dir(argv[1]);
	EnemySpawning = 0;      /* the older checks are about the claw and the shots */
	check_levels();
	check_script(argv[2], 1);
	check_script(argv[2], 2);
	check_script(argv[2], 33);
	check_timing();
	check_shots(argv[2], 160, 160, 2, 0);
	check_shots(argv[2], 320, 320, 16, 1);
	check_enemies(argv[2], 160, 160, 2, 0);
	check_enemies(argv[2], 320, 320, 16, 1);
	check_new_enemies(argv[2], 160, 160, 2, 0);
	check_new_enemies(argv[2], 320, 320, 16, 1);
	pictures(argv[2], 160, 160, 2, 0);
	pictures(argv[2], 320, 320, 16, 1);
	pictures(argv[2], 480, 320, 8, 0);
	free(screen.bits);
	if (failures) {
		printf("game: %d failures\n", failures);
		return 1;
	}
	printf("game: 99 levels, claw lanes repeat, 20 steps a second, shots, zaps, death and enemies ok; PNGs in %s\n", argv[2]);
	return 0;
}
