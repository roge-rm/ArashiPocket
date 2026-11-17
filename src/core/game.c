/* The game loop, from GameLoop.c and STLoadLevel in STResource.c by Juri
   Munkki. The original steps the game every 3 Mac ticks (20 a second) and
   skips drawing when late; here the steps due since the last frame run
   (a few at most) and the picture is drawn once. */
#include "game.h"
#include "parser.h"
#include "internalvars.h"

#define LASTLEVEL 99

LevelInfo ThisLevel;
uint32 game_steps;

static uint32 last_ticks;
static uint32 acc;          /* time owed to the game, in plat ticks * GAME_HZ */
static cbool ready;
static uint16 prev_zap;

SEG_GAME static int16 eval_int(int32 token);
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

SEG_GAME static int16 eval_int(int32 token)
{
	return (int16)parser_eval(token);
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
	ThisLevel.lvBonus = (int32)parser_eval(kVarLevelBonus);
	ThisLevel.lvStBonus = (int32)parser_eval(kVarLevelSTBonus);
	ThisLevel.lvNext = eval_int(kVarLevelNext);
	ThisLevel.plMaxMove = eval_int(kVarPlayerMaxMove);
	ThisLevel.plSuperZaps = eval_int(kVarPlayerSuperZaps);
	for (i = 0; i < 5; i++)
		ThisLevel.shColor[i] = eval_int(kVarShotColor_0_ + i);
	ThisLevel.shSpeed = eval_int(kVarShotSpeed);
	ThisLevel.endTimer = eval_int(kVarEndTimer);
	ThisLevel.ShowSpikesMsg = eval_int(kVarShowSpikesMsg);
	ThisLevel.Doomsday = eval_int(kVarDoomsday);
	parser_free();
	if (ThisLevel.plMaxMove < 1)
		ThisLevel.plMaxMove = 1;

	VALoadColors(ThisLevel.lvColor);
	if (!CreateFit())
		return 0;
	InitPlayer();
	InitShots();
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

SEG_GAME cbool game_init(int16 level)
{
	ready = 0;
	if (!AllocGrids())
		return 0;
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
	UpdatePlayer(in);
	UpdateShots();
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
