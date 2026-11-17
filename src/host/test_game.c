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
	ClearSpikes();
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


/* ---- Spikes and the crack pieces ---- */

static void wipe_marks(void)
{
	int16 i;

	for (i = 0; i <= MAXSEGS; i++) {
		if (i < MAXSEGS)
			Hero.lanestat[i] = 0;
		Hero.segmstat[i] = 0;
	}
}

/* One step with the claw sent to lane (or nowhere for -1), firing or not,
   then a picture. keep leaves the enemies' marks on the claw. */
static void aim_step(int16 lane, uint16 extra, int keep)
{
	PlatInput in;

	memset(&in, 0, sizeof(in));
	in.rim = -1;
	in.actions = extra;
	if (lane >= 0) {
		in.pen_down = 1;
		in.rim = LaneRim[lane];
	}
	game_tick(&in);
	if (!keep)
		wipe_marks();
	game_draw();
}

/* The first (last 0) or the last (last 1) level with spikers. */
static int spiker_level(int last)
{
	int lv, found = 0;

	for (lv = 2; lv <= 99; lv++) {
		if (!game_load_level((int16)lv))
			continue;
		if (ThisLevel.spProb > 0 && ThisLevel.spSpeed > 0 && ThisLevel.shPower > 0) {
			found = lv;
			if (!last)
				break;
		}
	}
	if (found)
		game_load_level((int16)found);
	return found;
}

static void no_enemies(void)
{
	ThisLevel.flCount = ThisLevel.fuCount = ThisLevel.puCount = 0;
	ThisLevel.tk[0].count = ThisLevel.tk[1].count = ThisLevel.tk[2].count = 0;
	ThisLevel.totalCount = 0;
}

static int16 spike_top(int16 lane)
{
	int16 t, b, p;

	SpikerInfo(lane, &t, &b, &p);
	return t;
}

static int16 live_ball(void)
{
	int16 i, t, b, p;

	for (i = 0; i < ww.numsegs; i++) {
		SpikerInfo(i, &t, &b, &p);
		if (b >= 0)
			return i;
	}
	return -1;
}

static int16 live_plasma(void)
{
	int16 i, t, b, p, best = -1, bl = DEPTH + 1;

	for (i = 0; i < ww.numsegs; i++) {
		SpikerInfo(i, &t, &b, &p);
		if (p >= 0 && p < bl) {
			bl = p;
			best = i;
		}
	}
	return best;
}

static void check_spikes(const char *dir, int16 w, int16 h, uint8 depth, uint8 le16)
{
	char name[96];
	int lv, n, grew, sawball = 0, hits, bad, kills, i, plasma;
	int16 top0[MAXSEGS], lane, t, b, p, t1;
	int32 s0, d;

	make_screen(w, h, depth, le16);
	CHECK(VAInit(), "VAInit");
	qd_seed(12345);
	CHECK(game_init(1), "game_init");

	for (plasma = 0; plasma < 2; plasma++) {
		lv = spiker_level(plasma);
		CHECK(lv, "no level with spikers");
		if (!lv)
			continue;
		no_enemies();
		EnemySpawning = 1;
		printf("spikes: level %d: points %d, spike points %d, prob %u, speed %ld, start %d, top %d, "
		       "plasma %d (%ld, %d points), power %d\n", lv, ThisLevel.spPoints, ThisLevel.spSpikePoints,
		       ThisLevel.spProb, (long)ThisLevel.spSpeed, ThisLevel.spStart, ThisLevel.spTop,
		       ThisLevel.spPlasma, (long)ThisLevel.spPlSpeed, ThisLevel.spPlPoints, ThisLevel.shPower);
		for (i = 0; i < ww.numsegs; i++)
			top0[i] = spike_top((int16)i);

		/* The spikers climb and the spikes grow. */
		grew = 0;
		for (n = 0; n < 2500 && !(grew && sawball >= 3); n++) {
			aim_step(-1, 0, 0);
			for (i = 0; i < ww.numsegs; i++) {
				SpikerInfo((int16)i, &t, &b, &p);
				if (t < top0[i]) {
					grew = 1;
					top0[i] = t;
				}
				if (b >= 0 && b < DEPTH / 2 && sawball < 3) {
					sawball++;
					if (sawball == 3 && !plasma) {
						sprintf(name, "spikes-%dx%d-%d-grow.png", w, h, depth);
						save_png(dir, name);
					}
				}
			}
		}
		CHECK(grew, "level %d: the spikes never grew in %d steps", lv, n);
		CHECK(sawball >= 1, "level %d: no spiker seen", lv);

		/* Shots wear a spike down by shPower each and score for it. */
		EnemySpawning = 0;
		for (n = 0; n < 400 && live_ball() >= 0; n++)
			aim_step(-1, 0, 0);
		CHECK(live_ball() < 0, "a spiker never left");
		lane = -1;
		for (i = 0; i < ww.numsegs; i++)
			if (spike_top((int16)i) < DEPTH - 3 * ThisLevel.shPower &&
			    (lane < 0 || spike_top((int16)i) < spike_top(lane)))
				lane = (int16)i;
		CHECK(lane >= 0, "level %d: no spike to shoot", lv);
		if (lane >= 0) {
			for (n = 0; n < 200 && Hero.lane != lane; n++)
				aim_step(lane, 0, 0);
			CHECK(Hero.lane == lane, "claw in lane %d, wanted %d", Hero.lane, lane);
			hits = bad = 0;
			sprintf(name, "spikes-%dx%d-%d-shoot%d.png", w, h, depth, plasma);
			for (n = 0; n < 600 && spike_top(lane) < DEPTH; n++) {
				t = spike_top(lane);
				s0 = Hero.score;
				aim_step(lane, PLAT_ACT_FIRE, 0);
				t1 = spike_top(lane);
				d = Hero.score - s0;
				if (t1 != t) {
					hits++;
					if (!(t1 == t + ThisLevel.shPower || (t + ThisLevel.shPower >= DEPTH && t1 >= DEPTH)) ||
					    d != ThisLevel.spSpikePoints)
						bad++;
				} else if (d != 0) {
					bad++;
				}
				if (hits == 2 && plasma == 0 && n > 0) {
					save_png(dir, name);
					hits++;
				}
			}
			CHECK(bad == 0, "level %d: %d shots that changed a spike wrongly", lv, bad);
			CHECK(hits >= 2 && spike_top(lane) >= DEPTH, "level %d: spike worn after %d hits, top %d", lv,
			      hits, spike_top(lane));
			printf("spikes: level %d lane %d worn down in %d hits, %d points each\n", lv, lane, hits,
			       ThisLevel.spSpikePoints);
		}

		/* A spiker shot dead scores spPoints; a superzap kills all of them. */
		EnemySpawning = 1;
		kills = bad = 0;
		for (n = 0; n < 6000 && kills < 2; n++) {
			int16 bl = live_ball(), bt, bb, bp;

			if (bl >= 0) {
				SpikerInfo(bl, &bt, &bb, &bp);
				s0 = Hero.score;
				aim_step(bl, PLAT_ACT_FIRE, 0);
				SpikerInfo(bl, &bt, &bb, &bp);
				d = Hero.score - s0;
				if (bb < 0 && d >= ThisLevel.spPoints) {
					kills++;
					/* The same step can wear the spike (one or two shots) and blow plasma. */
					int32 rem = d - ThisLevel.spPoints;
					int ok = 0, x, y;

					for (x = 0; x <= 3; x++)
						for (y = 0; y <= 2; y++)
							if (rem == x * ThisLevel.spSpikePoints + y * ThisLevel.spPlPoints)
								ok = 1;
					if (!ok)
						bad++;
				}
			} else {
				aim_step(-1, 0, 0);
			}
		}
		CHECK(kills >= 2, "level %d: %d spikers shot in %d steps", lv, kills, n);
		CHECK(bad == 0, "level %d: %d spiker kills scored oddly", lv, bad);
		printf("spikes: level %d: %d spikers shot, %d points each\n", lv, kills, ThisLevel.spPoints);

		for (n = 0; n < 6000; n++) {
			aim_step(-1, 0, 0);
			p = 0;
			for (i = 0; i < ww.numsegs; i++) {
				SpikerInfo((int16)i, &t, &b, &t1);
				if (b >= 0)
					p++;
			}
			if (p >= 2)
				break;
		}
		CHECK(p >= 2, "level %d: no two spikers for the zap", lv);
		EnemySpawning = 0;      /* a new spiker could start in the zap's own step */
		ThisLevel.plSuperZaps = 2;
		s0 = Hero.score;
		aim_step(-1, PLAT_ACT_ZAP, 0);
		CHECK(live_ball() < 0 && Hero.score - s0 >= (int32)p * ThisLevel.spPoints,
		      "level %d zap: spiker left %d, +%ld for %d", lv, live_ball(), (long)(Hero.score - s0), p);

		if (ThisLevel.spPlasma) {
			/* Plasma flies up, a shot blows it for spPlPoints, and one that
			   gets to the rim kills the claw. */
			int sawp = 0, blown = 0, caught = 0;

			EnemySpawning = 1;
			for (n = 0; n < 8000 && blown < 2; n++) {
				int16 pl = live_plasma();

				if (pl >= 0) {
					SpikerInfo(pl, &t, &b, &p);
					s0 = Hero.score;
					aim_step(pl, PLAT_ACT_FIRE, 0);
					SpikerInfo(pl, &t, &b, &t1);
					if (!sawp && p < DEPTH / 2) {
						sawp = 1;
						sprintf(name, "spikes-%dx%d-%d-plasma.png", w, h, depth);
						save_png(dir, name);
					}
					if (t1 < 0 && p > 2 && Hero.score - s0 >= ThisLevel.spPlPoints)
						blown++;
				} else {
					aim_step(-1, 0, 0);
				}
			}
			CHECK(blown >= 2, "level %d: %d plasma shot", lv, blown);
			EnemySpawning = 1;
			for (n = 0; n < 12000 && !caught; n++) {
				aim_step(-1, 0, 1);
				if (Hero.state == HeroDisintegrating || Hero.state == HeroPostMortem)
					caught = 1;
			}
			CHECK(caught, "level %d: plasma never caught the claw in %d steps", lv, n);
			printf("spikes: level %d: plasma blown %d times, caught the claw after %d steps (state %d)\n",
			       lv, blown, n, Hero.state);
			if (caught) {
				for (i = 0; i < 4; i++)
					aim_step(-1, 0, 1);
				sprintf(name, "spikes-%dx%d-%d-plasma-caught.png", w, h, depth);
				save_png(dir, name);
			}
		}
		game_load_level((int16)lv);
	}
	EnemySpawning = 0;
	game_close();
	VAClose();
}

/* The fly-through's collision: a spike that reaches the claw sets CapSpike
   on its lane and kills it with the pieces of the claw. */
static void check_spike_fly(const char *dir, int16 w, int16 h, uint8 depth, uint8 le16)
{
	int lv, i;
	int16 lane = -1, t, b, p;
	int32 s0;
	char name[96];

	make_screen(w, h, depth, le16);
	CHECK(VAInit(), "VAInit");
	CHECK(game_init(1), "game_init");
	lv = spiker_level(1);
	if (!lv) {
		game_close();
		VAClose();
		return;
	}
	no_enemies();
	EnemySpawning = 0;
	for (i = 0; i < ww.numsegs; i++) {
		SpikerInfo((int16)i, &t, &b, &p);
		if (t < DEPTH && (lane < 0 || t < spike_top(lane)))
			lane = (int16)i;
	}
	if (lane >= 0) {
		/* Flying down 'depth' levels: the spike is reached when its top is
		   at the claw. */
		Hero.flydepth = (int16)spike_top(lane);
		Hero.lanestat[lane] = 0;
		FlySpikes();
		CHECK(Hero.lanestat[lane] & CapSpike, "flying onto a spike in lane %d didn't set CapSpike", lane);
		Hero.flydepth = (int16)(spike_top(lane) - 5);
		for (i = 0; i < MAXSEGS; i++)
			Hero.lanestat[i] = 0;
		FlySpikes();
		CHECK(!(Hero.lanestat[lane] & CapSpike), "a spike 5 levels ahead is a hit");
		Hero.flydepth = 0;
		Hero.lane = lane;
		s0 = CrackCount();
		PlayerCapture(CapSpike);
		CHECK(Hero.state == HeroPostMortem && CrackCount() == s0 + 2, "spike death: state %d, %d pieces",
		      Hero.state, CrackCount());
		for (i = 0; i < 6; i++)
			aim_step(-1, 0, 1);
		sprintf(name, "spikes-%dx%d-%d-claw-cracked.png", w, h, depth);
		save_png(dir, name);
	}
	game_close();
	VAClose();
}

/* Pieces: a flipper breaks into 2, a pulsar into 1, a fuseball into 15
   lines with its points above it, the claw into 2 halves and 16 lines. */
static void check_cracks(const char *dir, int16 w, int16 h, uint8 depth, uint8 le16)
{
	static const char *const names[3] = { "flipper", "fuseball", "pulsar" };
	static const int want[3] = { 2, 15, 1 };
	char name[96];
	int kind, lv, n, i, got, shown;
	int32 s0;

	make_screen(w, h, depth, le16);
	CHECK(VAInit(), "VAInit");
	qd_seed(12345);
	CHECK(game_init(1), "game_init");
	EnemySpawning = 1;
	ShowFloatingScores = 1;

	for (kind = 0; kind < 3; kind++) {
		lv = kind == 0 ? 1 : find_level(kind == 1 ? 0 : 1);
		CHECK(lv, "no level with %s", names[kind]);
		if (!lv)
			continue;
		if (kind == 0)
			game_load_level(1);
		else
			isolate(kind == 1 ? 0 : 1);
		ThisLevel.spProb = 0;
		got = shown = 0;
		for (n = 0; n < 6000 && !got; n++) {
			int16 c0 = ThisLevel.flCount, f0 = ThisLevel.fuCount, p0 = ThisLevel.puCount;
			int hit;

			bot_step();
			hit = kind == 0 ? ThisLevel.flCount < c0 : kind == 1 ? ThisLevel.fuCount < f0 :
			      ThisLevel.puCount < p0;
			if (hit) {
				CHECK(CrackCount() >= want[kind], "%s killed: %d pieces, wanted %d", names[kind],
				      CrackCount(), want[kind]);
				if (kind == 1)
					CHECK(FloatingScoreCount() == 1, "fuseball killed: %d floating scores",
					      FloatingScoreCount());
				for (i = 0; i < 4; i++)
					bot_step();
				sprintf(name, "crack-%dx%d-%d-%s.png", w, h, depth, names[kind]);
				save_png(dir, name);
				got = 1;
				/* The pieces are gone after their time. */
				for (i = 0; i < 80; i++)
					bot_step();
			}
		}
		CHECK(got, "%s never killed", names[kind]);
		printf("cracks: %s breaks into %d pieces\n", names[kind], want[kind]);
	}

	/* A floating score goes after 40 steps. */
	AddFLScore(10, 10, 500);
	CHECK(FloatingScoreCount() == 1, "floating score added: %d", FloatingScoreCount());
	for (i = 0; i < 40; i++)
		UpdateFloatingScores();
	CHECK(FloatingScoreCount() == 0, "floating score stays: %d", FloatingScoreCount());

	/* The claw: a pulsar on the rim gives the 2 halves and 16 lines. */
	game_load_level(1);
	no_enemies();
	EnemySpawning = 0;
	Hero.lane = 3;
	s0 = CrackCount();
	PlayerCapture(CapPuls);
	CHECK(CrackCount() - s0 == 2 + 16, "claw killed by a pulsar: %d pieces", CrackCount() - s0);
	for (i = 0; i < 5; i++)
		aim_step(-1, 0, 1);
	sprintf(name, "crack-%dx%d-%d-claw.png", w, h, depth);
	save_png(dir, name);
	game_close();
	VAClose();
}


/* The order of the random calls, against a reading of the original on level
   1 with nothing active. Level setup: InitCStars asks for an index and a
   direction from VARandom for each of the 10 stars' two paths. A step then
   runs UpdateFuseBalls (UpdateFuseOff: 5 FastRandom), UpdateFlippers (4 to
   come, so one VARandom for the new flipper's chance), UpdateSpikes (one
   VARandom for each lane without a spiker, the spike chance, spProb is 0
   here but the call is made) and no others (no fuseballs, pulsars or
   tankers). The script is not asked for a Random on any level. */
extern void (*rand_trace)(int stream, int16 value);
static int trace_n;
static unsigned char trace_s[4096];
static int16 trace_v[4096];

static void trace_hook(int stream, int16 value)
{
	if (trace_n < 4096) {
		trace_s[trace_n] = (unsigned char)stream;
		trace_v[trace_n] = value;
		trace_n++;
	}
}

static uint32 hash_trace(void)
{
	uint32 h = 2166136261UL;
	int i;

	for (i = 0; i < trace_n; i++)
		h = (h ^ (uint32)trace_s[i] ^ ((uint32)(uint16)trace_v[i] << 8)) * 16777619UL;
	return h;
}

static uint32 random_run(int steps, int check_order)
{
	PlatInput in;
	int i, n0, expect;
	int32 f;
	uint32 h;

	make_screen(160, 160, 2, 0);
	VAInit();
	qd_seed(5);
	va_seed(1);
	EnemySpawning = 1;
	trace_n = 0;
	rand_trace = trace_hook;
	game_init(1);
	rand_trace = NULL;
	if (check_order) {
		CHECK(trace_n == 40, "level setup made %d random calls, 40 expected", trace_n);
		for (i = 0; i < trace_n; i++)
			CHECK(trace_s[i] == 2, "setup call %d is on stream %d", i, trace_s[i]);
	}
	n0 = trace_n;
	memset(&in, 0, sizeof(in));
	in.rim = -1;
	rand_trace = trace_hook;
	game_tick(&in);
	rand_trace = NULL;
	if (check_order) {
		expect = 5 + 1 + ww.numsegs;
		CHECK(trace_n - n0 == expect, "first step made %d random calls, %d expected", trace_n - n0, expect);
		f = 5;      /* randSeed after the setup: FastRandom steps it from there */
		for (i = 0; i < 5 && n0 + i < trace_n; i++) {
			CHECK(trace_s[n0 + i] == 1, "step call %d is on stream %d, FastRandom expected", i, trace_s[n0 + i]);
			f = (int32)(((unsigned long long)f * 16807ULL) % 2147483647ULL);
			CHECK(trace_v[n0 + i] == (int16)(f & 0xFFFF), "FastRandom value %d", i);
		}
		for (i = 5; i < expect && n0 + i < trace_n; i++)
			CHECK(trace_s[n0 + i] == 2, "step call %d is on stream %d, VARandom expected", i, trace_s[n0 + i]);
		CHECK(qd_get_seed() == f, "randSeed is %ld after the step, %ld expected", (long)qd_get_seed(), (long)f);
	}
	rand_trace = trace_hook;
	for (i = 0; i < steps; i++) {
		bot_input(&in, 0);
		game_tick(&in);
	}
	rand_trace = NULL;
	h = hash_trace();
	game_close();
	VAClose();
	return h;
}

static void check_random_order(void)
{
	uint32 a = random_run(400, 1), b = random_run(400, 0);

	CHECK(a == b, "the same seeds gave different random calls (%08lx and %08lx)", (unsigned long)a, (unsigned long)b);
	printf("random: order of the calls on level 1 as the original, 400 steps repeat (trace hash %08lx)\n",
	       (unsigned long)a);
}


/* ---- level flow (M7) ---- */

static void flow_tick(uint16 act)
{
	PlatInput in;

	memset(&in, 0, sizeof(in));
	in.rim = -1;
	in.actions = act;
	game_tick(&in);
	game_draw();
}

/* Level 1 cleared by the bot: the end timer, the fly-through (70 steps, 69
   with the first), the bonus, and level 2 loaded as the script says. */
static void check_clear(const char *dir, int16 w, int16 h, uint8 depth, uint8 le16)
{
	PlatInput in;
	char name[96];
	int n, flying = 0, maxdepth = 0, shots = 0, wasflying = 0;

	make_screen(w, h, depth, le16);
	CHECK(VAInit(), "VAInit");
	CHECK(game_init(1), "game_init");
	EnemySpawning = 1;
	LevelEnds = 1;
	for (n = 0; n < 4000 && ThisLevel.lvNumber == 1; n++) {
		bot_input(&in, 0);
		game_tick(&in);
		game_draw();
		wasflying = Hero.state == HeroFlying;
		if (wasflying) {
			flying++;
			if (Hero.flydepth > maxdepth)
				maxdepth = Hero.flydepth;
			if ((Hero.flydepth >= 36 && shots == 0) || (Hero.flydepth >= 76 && shots == 1) ||
			    (Hero.flydepth >= 116 && shots == 2)) {
				sprintf(name, "flow-%dx%d-%d-fly%d.png", w, h, depth, shots);
				save_png(dir, name);
				shots++;
			}
		}
	}
	CHECK(ThisLevel.lvNumber == 2, "level 1 never gave way to level 2 (%d steps)", n);
	CHECK(maxdepth >= 136 && maxdepth <= 138, "the fly-through reached depth %d", maxdepth);
	CHECK(flying >= 68 && flying <= 72, "the fly-through took %d steps", flying);
	CHECK(Hero.state == HeroPlaying && Hero.flydepth == 0, "after the fly-through: state %d depth %d",
	      Hero.state, Hero.flydepth);
	CHECK(Hero.score == 700, "score %ld after level 1: 4 x 150 and the bonus 100", (long)Hero.score);
	CHECK(Hero.lives == 2, "lives %d after the level", Hero.lives);
	CHECK(ThisLevel.lvNext == 3 && ThisLevel.totalCount > 0, "level 2 loaded: next %d, total %d",
	      ThisLevel.lvNext, ThisLevel.totalCount);
	CHECK(Hero.Flags & HF_STBONUSGIVEN, "the starting bonus is marked paid");
	game_close();
	VAClose();
	printf("flow: level 1 cleared in %d steps, fly-through %d steps to depth %d, level 2 next, score %ld\n",
	       n, flying, maxdepth, (long)Hero.score);
}

/* Level 5 has a spike in every lane: the fly-through ends on one, the claw
   dies and the level comes again with the same spikes and no enemies; the
   last life goes and the game starts over. */
static void check_spike_death(const char *dir, int16 w, int16 h, uint8 depth, uint8 le16)
{
	char name[96];
	int16 tops[MAXSEGS], i, nseg;
	int n, deaths = 0, sawdeath = 0, msg = 0, same = 1, dd = -1;
	int prevstate;

	make_screen(w, h, depth, le16);
	CHECK(VAInit(), "VAInit");
	CHECK(game_init(1), "game_init");
	CHECK(game_load_level(5), "level 5");
	EnemySpawning = 0;
	LevelEnds = 1;
	no_enemies();
	nseg = ww.numsegs;
	for (i = 0; i < nseg; i++)
		tops[i] = spike_top(i);
	CHECK(tops[0] == 104, "level 5 spike top %d", tops[0]);
	Hero.lives = 2;
	prevstate = Hero.state;
	for (n = 0; n < 6000 && deaths < 3 && !game_over_now(); n++) {
		int lives0 = Hero.lives;

		flow_tick(0);
		if (Hero.state == HeroFlying && Hero.flydepth == 0 && !msg) {
			msg = 1;
		}
		if (Hero.state == HeroFlying && dd < 0 && Hero.flydepth >= 60) {
			dd = Hero.flydepth;
			sprintf(name, "flow-%dx%d-%d-spikes-fly.png", w, h, depth);
			save_png(dir, name);
		}
		if ((Hero.state == HeroPostMortem || Hero.state == HeroDropping) && prevstate == HeroFlying) {
			CHECK(Hero.flydepth >= 102 && Hero.flydepth <= 106, "spike hit at depth %d", Hero.flydepth);
			sawdeath = 1;
			sprintf(name, "flow-%dx%d-%d-spikes-hit%d.png", w, h, depth, deaths);
			if (deaths == 0)
				save_png(dir, name);
		}
		if (Hero.lives != lives0 || (Hero.state == HeroPlaying && prevstate == HeroDead)) {
			deaths++;
			if (Hero.lives >= 0 && deaths < 3) {
				for (i = 0; i < nseg; i++)
					if (spike_top(i) != tops[i])
						same = 0;
				CHECK(ThisLevel.lvNumber == 5, "death sent the game to level %d", ThisLevel.lvNumber);
				CHECK(ThisLevel.totalCount == 0, "death brought back %d enemies", ThisLevel.totalCount);
			}
		}
		prevstate = Hero.state;
	}
	CHECK(sawdeath, "the claw never died on a spike");
	CHECK(same, "the spikes changed across a death");
	CHECK(deaths >= 2, "only %d deaths", deaths);
	CHECK(game_over_now() && Hero.lives == 0, "the last death did not end the game: lives %d",
	      Hero.lives);
	game_close();
	VAClose();
	printf("flow: spike hit the claw in the fly-through %d times, spikes kept, game over stops the game\n", deaths);
}

/* A spike worn down by shots keeps its length when the claw dies in play. */
static void check_spike_stay(void)
{
	int n, lane = 3;
	int16 top0, top1;

	make_screen(160, 160, 2, 0);
	CHECK(VAInit(), "VAInit");
	CHECK(game_init(1), "game_init");
	CHECK(game_load_level(5), "level 5");
	EnemySpawning = 0;
	LevelEnds = 0;
	no_enemies();
	top0 = spike_top((int16)lane);
	for (n = 0; n < 60; n++) {
		PlatInput in;

		memset(&in, 0, sizeof(in));
		in.rim = LaneRim[lane];
		in.pen_down = 1;
		in.actions = PLAT_ACT_FIRE;
		game_tick(&in);
		game_draw();
	}
	top1 = spike_top((int16)lane);
	CHECK(top1 > top0, "shots didn't wear the spike (%d, %d)", top0, top1);
	Hero.lives = 2;
	PlayerCapture(CapFuse);
	for (n = 0; n < 200 && Hero.lives == 2; n++)
		flow_tick(0);
	CHECK(Hero.lives == 1, "lives %d after the death", Hero.lives);
	CHECK(spike_top((int16)lane) == top1, "the spike was %d and is %d after the death", top1,
	      spike_top((int16)lane));
	CHECK(ThisLevel.lvNumber == 5, "level %d", ThisLevel.lvNumber);
	game_close();
	VAClose();
	printf("flow: spike worn from %d to %d stays after a death\n", top0, top1);
}

/* Free lives at 20000, each 20000 after, never over 5 spare lives. */
static void check_free_lives(void)
{
	make_screen(160, 160, 2, 0);
	CHECK(VAInit(), "VAInit");
	CHECK(game_init(1), "game_init");
	Hero.lives = 2;
	IncreaseScore(19990);
	CHECK(Hero.lives == 2, "life at 19990");
	IncreaseScore(10);
	CHECK(Hero.lives == 2, "life at exactly 20000 (the original wants over)");
	IncreaseScore(1);
	CHECK(Hero.lives == 3 && Hero.freeLives == 40000, "no life at 20001: lives %d next %ld", Hero.lives,
	      (long)Hero.freeLives);
	CHECK(!(Hero.Flags & HF_FLASH), "a flash without the starting bonus");
	Hero.Flags |= HF_STBONUSGIVEN;
	IncreaseScore(20000);
	CHECK(Hero.lives == 4 && Hero.freeLives == 60000 && (Hero.Flags & HF_FLASH) == 1,
	      "life at 40001: lives %d next %ld flags %x", Hero.lives, (long)Hero.freeLives, Hero.Flags);
	Hero.lives = 5;
	IncreaseScore(20000);
	CHECK(Hero.lives == 5 && Hero.freeLives == 80000, "a sixth life in play: %d next %ld", Hero.lives,
	      (long)Hero.freeLives);
	Hero.state = HeroFlying;
	IncreaseScore(20000);
	CHECK(Hero.lives == 6 && Hero.freeLives == 100000, "none while flying with 5: %d", Hero.lives);
	IncreaseScore(65000);
	CHECK(Hero.lives == 6 && Hero.freeLives == 160000, "no life over 5 spare while flying: %d next %ld", Hero.lives,
	      (long)Hero.freeLives);
	game_close();
	VAClose();
	printf("flow: free lives at 20000 steps ok\n");
}

/* The starting bonus is paid once, on the first level cleared. */
static void check_bonus(void)
{
	PlatInput in;
	int n;
	int32 st;

	make_screen(160, 160, 2, 0);
	CHECK(VAInit(), "VAInit");
	CHECK(game_init(10), "game_init 10");
	st = ThisLevel.lvStBonus;
	CHECK(st == 15000 && ThisLevel.lvBonus == 100, "level 10 bonuses %ld and %ld", (long)st, (long)ThisLevel.lvBonus);
	EnemySpawning = 0;
	LevelEnds = 1;
	no_enemies();
	ClearSpikes();
	memset(&in, 0, sizeof(in));
	in.rim = -1;
	for (n = 0; n < 3000 && ThisLevel.lvNumber == 10; n++)
		flow_tick(0);
	CHECK(ThisLevel.lvNumber == 11 && Hero.score == 15100, "level 10 cleared: level %d score %ld",
	      ThisLevel.lvNumber, (long)Hero.score);
	no_enemies();
	ClearSpikes();
	for (n = 0; n < 3000 && ThisLevel.lvNumber == 11; n++)
		flow_tick(0);
	CHECK(ThisLevel.lvNumber == 12 && Hero.score == 15200, "level 11 cleared: level %d score %ld",
	      ThisLevel.lvNumber, (long)Hero.score);
	/* A saved game keeps its score and lives. */
	Hero.score = 5000;
	Hero.lives = 3;
	CHECK(game_start(20, 1), "saved game start");
	CHECK(Hero.score == 5000 && Hero.lives == 3 && ThisLevel.lvNumber == 20,
	      "saved game: score %ld lives %d level %d", (long)Hero.score, Hero.lives, ThisLevel.lvNumber);
	CHECK(game_start(20, 0), "new start");
	CHECK(Hero.score == 0 && Hero.lives == 2, "a new game keeps nothing: %ld, %d", (long)Hero.score, Hero.lives);
	game_close();
	VAClose();
	printf("flow: starting bonus %ld and level bonus paid on the first clear, then the level bonus\n", (long)st);
}

/* Doomsday levels pick a field with VARandom, keep the zaps on a death, and
   the levels go on past 99. */
static void check_doomsday(void)
{
	int n;
	int16 zaps, field0;
	int32 expect;

	make_screen(160, 160, 2, 0);
	CHECK(VAInit(), "VAInit");
	CHECK(game_init(1), "game_init");
	EnemySpawning = 0;
	LevelEnds = 0;
	va_seed(777);
	expect = (int32)((uint16)va_random() % 16 + 1);
	va_seed(777);
	trace_n = 0;
	rand_trace = trace_hook;
	CHECK(game_load_level(97), "level 97");
	rand_trace = NULL;
	CHECK(ThisLevel.Doomsday == 1 && ThisLevel.lvField == expect, "doomsday field %d, VARandom says %ld",
	      ThisLevel.lvField, (long)expect);
	CHECK(trace_n >= 1 && trace_s[0] == 2 && (uint16)trace_v[0] % 16 + 1 == expect,
	      "the field's random call isn't the first one (%d calls)", trace_n);
	field0 = ThisLevel.lvField;
	game_load_level(96);
	CHECK(ThisLevel.Doomsday == 0 && ThisLevel.lvField == 16, "level 96 field %d", ThisLevel.lvField);

	game_load_level(97);
	Hero.lives = 2;
	ThisLevel.plSuperZaps = 0;
	PlayerCapture(CapFuse);
	for (n = 0; n < 300 && Hero.lives == 2; n++)
		flow_tick(0);
	zaps = ThisLevel.plSuperZaps;
	CHECK(Hero.lives == 1 && zaps == 0 && ThisLevel.lvNumber == 97 && ThisLevel.lvNext == 98,
	      "doomsday death: lives %d zaps %d level %d next %d", Hero.lives, zaps, ThisLevel.lvNumber,
	      ThisLevel.lvNext);
	(void)field0;
	/* A normal level recharges nothing either (no reload). */
	game_load_level(30);
	ThisLevel.plSuperZaps = 0;
	Hero.lives = 2;
	PlayerCapture(CapFuse);
	for (n = 0; n < 300 && Hero.lives == 2; n++)
		flow_tick(0);
	CHECK(ThisLevel.plSuperZaps == 0 && ThisLevel.lvNumber == 30, "level 30 after a death: zaps %d level %d",
	      ThisLevel.plSuperZaps, ThisLevel.lvNumber);

	game_load_level(99);
	CHECK(ThisLevel.lvNext == 100, "level 99 goes to %d", ThisLevel.lvNext);
	game_load_level(ThisLevel.lvNext);
	CHECK(ThisLevel.lvNumber == 100 && ThisLevel.lvNext == 101 && ThisLevel.Doomsday == 1,
	      "level 100: %d next %d doomsday %d", ThisLevel.lvNumber, ThisLevel.lvNext, ThisLevel.Doomsday);
	game_close();
	VAClose();
	printf("flow: doomsday field from VARandom first, zaps kept, levels go on past 99\n");
}

/* The level select list. */
static void check_choices(void)
{
	static LevelChoice c[96];
	int16 n, p, i;

	make_screen(160, 160, 2, 0);
	CHECK(VAInit(), "VAInit");
	n = game_level_choices(0, 9, c, 96);
	CHECK(n > 3 && c[0].level == 1 && c[0].startBonus == 0, "arcade list: %d entries, first level %d", n,
	      c[0].level);
	for (i = 1; i < n; i++)
		CHECK(c[i].level > c[i - 1].level, "list isn't increasing at %d", i);
	CHECK(c[n - 1].level <= 9, "arcade list goes to level %d with 9 reached", c[n - 1].level);
	p = game_level_choices(1, 9, c, 96);
	CHECK(p >= n, "practice list shorter than arcade");
	printf("flow: level select lists %d levels in arcade mode (to %d), %d in practice\n", n,
	       game_level_choices(0, 9, c, 96) ? c[n - 1].level : 0, p);
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
	LevelEnds = 0;
	LevelIntro = 0;     /* the checks count steps from the start of play */
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
	check_spikes(argv[2], 160, 160, 2, 0);
	check_spikes(argv[2], 320, 320, 16, 1);
	check_spike_fly(argv[2], 320, 320, 16, 1);
	check_random_order();
	LevelEnds = 0;
	check_clear(argv[2], 160, 160, 2, 0);
	check_clear(argv[2], 320, 320, 16, 1);
	check_spike_death(argv[2], 160, 160, 2, 0);
	check_spike_death(argv[2], 320, 320, 16, 1);
	check_spike_stay();
	check_free_lives();
	check_bonus();
	check_doomsday();
	check_choices();
	LevelEnds = 0;
	EnemySpawning = 0;
	check_cracks(argv[2], 160, 160, 2, 0);
	check_cracks(argv[2], 320, 320, 16, 1);
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
