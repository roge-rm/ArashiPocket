/* The game loop, from GameLoop.c and STLoadLevel in STResource.c by Juri
   Munkki. The original steps the game every 3 Mac ticks (20 a second) and
   skips drawing when late; here the steps due since the last frame run
   (a few at most) and the picture is drawn once. */
#include "game.h"
#include "parser.h"
#include "internalvars.h"
#include "cmath.h"
#include "qdrandom.h"

#define LASTLEVEL 99

LevelInfo ThisLevel;
uint32 game_steps;

static uint32 last_ticks;
static uint32 acc;          /* time owed to the game, in plat ticks * GAME_HZ */
static cbool ready;
static uint16 prev_zap;

SEG_GAME static int16 eval_int(int32 token);
SEG_GAME static int32 eval_long(int32 token);
SEG_GAME static void hero_died(void);
#ifdef GAME_PROFILE
SEG_GAME static void profile(void);
SEG_GAME static int16 put_num(char *s, int32 v);

static uint32 prof_t, prof_frames, prof_steps;
static int32 prof_fps10, prof_sps10, prof_draw10, prof_queue10, prof_present10;
static uint32 prof_draw_start, prof_present_start, prof_queue, prof_queue_start, prof_qt;

/* v with one decimal (v is tenths), returns the length. */
SEG_GAME static int16 put_num(char *s, int32 v)
{
	char tmp[12];
	int16 n = 0, len = 0;

	if (v < 0)
		v = 0;
	do {
		tmp[n++] = (char)('0' + (int16)(v % 10));
		v /= 10;
		if (n == 1)
			tmp[n++] = '.';
	} while (v != 0 || n < 3);
	while (n)
		s[len++] = tmp[--n];
	return len;
}

/* Frames drawn, steps run and time spent drawing per second, over 20
   frames, written at the bottom: F frames a second, S steps a second, D ms
   drawing a frame. */
SEG_GAME static void profile(void)
{
	uint32 now = plat_ticks(), tps = plat_ticks_per_second();
	char line[40];
	int16 len = 0;

	prof_frames++;
	if (prof_frames >= 20) {
		uint32 dt = now - prof_t;

		if (dt > 0) {
			prof_fps10 = (int32)(prof_frames * 10 * tps / dt);
			prof_sps10 = (int32)((game_steps - prof_steps) * 10 * tps / dt);
			prof_draw10 = (int32)((VA.DrawTicks - prof_draw_start) * 10000 / tps / prof_frames);
			prof_present10 = (int32)((VA.PresentTicks - prof_present_start) * 10000 / tps / prof_frames);
			prof_queue10 = (int32)((prof_queue - prof_queue_start) * 10000 / tps / prof_frames);
		}
		prof_present_start = VA.PresentTicks;
		prof_queue_start = prof_queue;
		prof_t = now;
		prof_frames = 0;
		prof_steps = game_steps;
		prof_draw_start = VA.DrawTicks;
	}
	VA.segmscale = 1;
	VA.color = 0;
	line[len++] = 'F'; line[len++] = ' ';
	len = (int16)(len + put_num(line + len, prof_fps10));
	line[len++] = ' '; line[len++] = 'S'; line[len++] = ' ';
	len = (int16)(len + put_num(line + len, prof_sps10));
	line[len++] = ' '; line[len++] = 'D'; line[len++] = ' ';
	len = (int16)(len + put_num(line + len, prof_draw10));
	VAMoveTo(2, (int16)(VA.frame.bottom - 2));
	VADrawText(line, 0, len);
	len = 0;
	line[len++] = 'Q'; line[len++] = ' ';
	len = (int16)(len + put_num(line + len, prof_queue10));
	line[len++] = ' '; line[len++] = 'P'; line[len++] = ' ';
	len = (int16)(len + put_num(line + len, prof_present10));
	VAMoveTo(2, (int16)(VA.frame.bottom - 12));
	VADrawText(line, 0, len);
}
#endif

/* The script's number as the original's C conversion made it (down to the
   whole number). A NaN is 0: pow(-1, 0.7) in the script (tanker count on
   levels 1, 17, 33, ...) gives NaN, and a count of 0 is the only value that
   lets those levels play and end (see handoff.md). */
SEG_GAME static int32 eval_long(int32 token)
{
	double d = parser_eval(token);

	if (cm_isnan(d))
		return 0;
	if (cm_lt(d, -2147483647.0))
		return -2147483647L;
	if (cm_lt(2147483647.0, d))
		return 2147483647L;
	return (int32)d;
}

SEG_GAME static int16 eval_int(int32 token)
{
	return (int16)eval_long(token);
}

SEG_GAME cbool game_load_level(int16 level)
{
	int16 i;

	if (level < 1 || level > LASTLEVEL)
		level = 1;
	if (!parser_alloc())
		return 0;
	parser_load_level(level);
	parser_write(kConstDEPTH, (double)DEPTH);
	ThisLevel.lvNumber = level;
	ThisLevel.lvField = eval_int(kVarLevelField);
	ThisLevel.lvColor = eval_int(kVarLevelColor);
	ThisLevel.lvBonus = eval_long(kVarLevelBonus);
	ThisLevel.lvStBonus = eval_long(kVarLevelSTBonus);
	ThisLevel.lvNext = eval_int(kVarLevelNext);
	ThisLevel.plMaxMove = eval_int(kVarPlayerMaxMove);
	ThisLevel.plSuperZaps = eval_int(kVarPlayerSuperZaps);
	for (i = 0; i < 5; i++)
		ThisLevel.shColor[i] = eval_int(kVarShotColor_0_ + i);
	ThisLevel.shSpeed = eval_int(kVarShotSpeed);
	ThisLevel.shPower = eval_int(kVarShotPower);

	ThisLevel.flPoints = eval_int(kVarFlipperPoints);
	ThisLevel.flCount = eval_int(kVarFlipperCount);
	ThisLevel.flProb = (uint16)eval_long(kVarFlipperProb);
	ThisLevel.flSpeed = eval_long(kVarFlipperSpeed);
	ThisLevel.flRot = eval_int(kVarFlipperRot);
	ThisLevel.flColor = eval_int(kVarFlipperColor);
	ThisLevel.fuPoints = eval_int(kVarFusePoints);
	ThisLevel.fuBullseye = eval_int(kVarFuseBullseye);
	ThisLevel.fuCount = eval_int(kVarFuseCount);
	ThisLevel.fuProb = (uint16)eval_long(kVarFuseProb);
	ThisLevel.fuWarpP = eval_int(kVarFuseWarpP);
	ThisLevel.fuPlayerPlus = eval_int(kVarFusePlayerPlus);
	for (i = 0; i < 5; i++)
		ThisLevel.fuColor[i] = eval_int(kVarFuseColor_0_ + i);
	ThisLevel.puPoints = eval_int(kVarPulsarPoints);
	ThisLevel.puCount = eval_int(kVarPulsarCount);
	ThisLevel.puProb = (uint16)eval_long(kVarPulsarProb);
	ThisLevel.puSpeed = eval_long(kVarPulsarSpeed);
	ThisLevel.puRot = eval_int(kVarPulsarRot);
	ThisLevel.puColor = eval_int(kVarPulsarColor);
	ThisLevel.puTime = eval_int(kVarPulsarTime);
	ThisLevel.puPulsDepth = eval_int(kVarPulsarPulsDepth);
	for (i = 0; i < TANKTYPES; i++) {
		int32 t = kVarTank_0_points + 5 * i;

		ThisLevel.tk[i].points = eval_int(t);
		ThisLevel.tk[i].count = eval_int(t + 1);
		ThisLevel.tk[i].prob = (uint16)eval_long(t + 2);
		ThisLevel.tk[i].speed = eval_long(t + 3);
		ThisLevel.tk[i].color = eval_int(t + 4);
	}
	ThisLevel.spPoints = eval_int(kVarSpikerPoints);
	ThisLevel.spProb = (uint16)eval_long(kVarSpikerProb);
	ThisLevel.spSpeed = eval_long(kVarSpikerSpeed);
	ThisLevel.spStart = eval_int(kVarSpikerStart);
	ThisLevel.spTop = eval_int(kVarSpikerTop);
	ThisLevel.spPlasma = eval_int(kVarSpikerPlasma);
	ThisLevel.spPlSpeed = eval_long(kVarSpikerPlSpeed);
	ThisLevel.spPlPoints = eval_int(kVarSpikerPlPoints);
	ThisLevel.spSpikePoints = eval_int(kVarSpikerSpikePoints);
	for (i = 0; i < 3; i++)
		ThisLevel.spColor[i] = eval_int(kVarSpikerColor_0_ + i);
	ThisLevel.boredomCount = eval_int(kVarBoredomCount);
	ThisLevel.boredProb = eval_long(kVarBoredProb);
	ThisLevel.endTimer = eval_int(kVarEndTimer);
	ThisLevel.ShowSpikesMsg = eval_int(kVarShowSpikesMsg);
	ThisLevel.Doomsday = eval_int(kVarDoomsday);
	parser_free();
	ThisLevel.totalCount = (int16)(ThisLevel.flCount + ThisLevel.fuCount + ThisLevel.puCount +
	                               ThisLevel.tk[0].count +
	                               ThisLevel.tk[1].count + ThisLevel.tk[2].count);
	ThisLevel.activeCount = 0;
	ThisLevel.edgeCount = 0;
	ThisLevel.starCount = 0;
	ThisLevel.probIncrease = 0;
	if (ThisLevel.plMaxMove < 1)
		ThisLevel.plMaxMove = 1;

	VALoadColors(ThisLevel.lvColor);
	if (!CreateFit())
		return 0;
	InitPlayer();
	InitShots();
	InitEnemies();
	game_resume();
	return 1;
}

SEG_GAME void game_next_level(void)
{
	int16 n = (int16)(ThisLevel.lvNumber + 1);

	game_load_level(n > LASTLEVEL ? 1 : n);
}

SEG_GAME cbool game_screen_changed(void)
{
	if (!ready)
		return 0;
	ZeroScore();
	return game_load_level(ThisLevel.lvNumber);
}

SEG_GAME void game_seed(void)
{
	qd_seed((int32)((plat_ticks() & 0x3FFFFFFFUL) + 1));
}

SEG_GAME cbool game_init(int16 level)
{
	ready = 0;
	if (!AllocGrids())
		return 0;
	AllocCStars();
	ZeroScore();
	if (!game_load_level(level))
		return 0;
	prev_zap = 0;
	ready = 1;
	return 1;
}

SEG_GAME void game_close(void)
{
	ready = 0;
	FreeGrids();
}

/* The claw is gone: takes a life and starts again on the same level, or
   from the first life when none are left. The enemies, the bonus and the
   game over screen come later. */
SEG_GAME static void hero_died(void)
{
	int16 zaps = ThisLevel.plSuperZaps;

	Hero.lives--;
	if (Hero.lives < 0) {
		ZeroScore();
		game_load_level(ThisLevel.lvNumber);
		return;
	}
	game_load_level(ThisLevel.lvNumber);
	if (ThisLevel.Doomsday == 1)
		ThisLevel.plSuperZaps = zaps;       /* no recharge on a doomsday level */
}

SEG_GAME void game_tick(const PlatInput *in)
{
#ifdef DEBUG_LEVELS
	/* Superzap steps to the next level until the superzap exists. */
	if ((in->actions & PLAT_ACT_ZAP) && !(prev_zap & PLAT_ACT_ZAP))
		game_next_level();
	prev_zap = in->actions;
#endif
	/* The original's order: the enemies that run before the shots move, then
	   the shots, then the ones after them. */
	if (ThisLevel.activeCount < ThisLevel.boredomCount)
		ThisLevel.probIncrease = ThisLevel.boredProb;
	else
		ThisLevel.probIncrease = 0;
	ThisLevel.activeCount = 0;
	UpdateCStars();
	ThisLevel.edgeCount = 0;
	ThisLevel.starCount = 0;
	UpdatePlayer(in);
	UpdateFuseBalls();
	UpdatePulsars();
	UpdateFlippers();
	UpdateShots();
	UpdateSpikes();
	UpdateTankers();
	UpdateCracks();
	UpdateFloatingScores();
	if (Hero.state == HeroDead)
		hero_died();
	game_steps++;
}

SEG_GAME void game_draw(void)
{
#ifdef GAME_PROFILE
	prof_qt = plat_ticks();
#endif
	DrawField();
	DrawHero();
	DrawEnemies();
	DrawShots();
	DrawLevelNumber();
	DisplayScore();
	DisplayLives();
#ifdef GAME_PROFILE
	profile();
	prof_queue += plat_ticks() - prof_qt;
#endif
	VAStep();
}

SEG_GAME void game_resume(void)
{
	last_ticks = plat_ticks();
	acc = plat_ticks_per_second();      /* the first frame has a step due */
}

SEG_GAME cbool game_frame(void)
{
	PlatInput in;
	uint32 now = plat_ticks();
	uint32 tps = plat_ticks_per_second();
	uint32 el = now - last_ticks;
	int16 n = 0, i;

	if (!ready)
		return 0;
	last_ticks = now;
	if (el > tps)
		el = tps;               /* a long stall counts as a second */
	acc += el * GAME_HZ;
	while (acc >= tps && n < GAME_MAXCATCHUP) {
		acc -= tps;
		n++;
	}
	if (acc >= tps)
		acc %= tps;             /* too far behind: drop the rest */
	if (n == 0)
		return 0;
	plat_input(&in);
	for (i = 0; i < n; i++) {
		game_tick(&in);
		in.spin = 0;
	}
	game_draw();
	return 1;
}

SEG_GAME uint32 game_wait(void)
{
	uint32 tps = plat_ticks_per_second();
	uint32 gone = plat_ticks() - last_ticks;
	uint32 need;

	if (acc >= tps)
		return 0;
	need = (tps - acc + GAME_HZ - 1) / GAME_HZ;
	return need > gone ? need - gone : 0;
}
