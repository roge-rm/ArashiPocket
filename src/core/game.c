/* The game loop, from GameLoop.c, STLoadLevel in STResource.c and
   STFlyThru.c by Juri Munkki. The original steps the game every 3 Mac ticks
   (20 a second) and skips drawing when late; here the steps due since the
   last frame run (a few at most) and the picture is drawn once.

   GameLoop's nested loops are a phase here: playing a level, the "Avoid
   Spikes" message, the fly-through, and the claw finishing its death after
   the fly-through. Each game_tick runs one pass of the loop that is
   running. When the level's enemies are gone the claw flies down the tube
   and the next level (the script's LevelNext) is loaded. */
#include "game.h"
#include "parser.h"
#include "internalvars.h"
#include "cmath.h"
#include "qdrandom.h"

#define MSGDELAY 40         /* 2 seconds of steps */
#define MAXLEVEL 32000

enum { PhasePlay, PhaseMsg, PhaseFly, PhaseFlyDie, PhaseIntro, PhaseFlyOut };
/* What follows the level intro: the claw set up for play, after taking
   the life the round gives (a new level after one was cleared) or not
   (the start of a game). */
enum { ThenStart, ThenRound };

LevelInfo ThisLevel;
cbool LevelEnds = 1;
cbool LevelIntro = 1;
cbool SoundOn = 1;
uint32 game_steps;

static uint32 last_ticks;
static uint32 acc;          /* time owed to the game, in plat ticks * GAME_HZ */
static cbool ready;
static const uint8 *demo_res;
static cbool over;              /* the last claw died: the front end takes over */
static uint16 prev_zap;

static int16 phase;
static int16 msg_timer;
static cbool fly_started;       /* the first fly step doesn't advance flydepth */
static cbool fly_hero;          /* the claw was updated, so it is drawn */
static int16 FieldColor, SpikeColor;
static int16 start_level;
/* GameLoop's locals: the bonus of the level just cleared, and whether the
   claw died on a doomsday level. */
static cbool levelComplete;
static int32 levelBonus, levelStBonus;
static cbool doomsday_death;
static int16 intro_then;

SEG_GAME static int16 eval_int(int32 token);
SEG_GAME static int32 eval_long(int32 token);
SEG_GAME static cbool st_load_level(int16 level);
SEG_GAME static int32 lv_long(int32 token);
SEG_GAME static int16 lv_int(int32 token);
SEG_GAME static void read_values(void);
SEG_GAME static void st_initialize(void);
SEG_GAME static cbool begin_level(int16 level);
SEG_GAME static void play_step(const PlatInput *in);
SEG_GAME static void msg_step(const PlatInput *in);
SEG_GAME static void fly_step(const PlatInput *in);
SEG_GAME static void flydie_step(const PlatInput *in);
SEG_GAME static void intro_done(void);
SEG_GAME static void begin_intro(int16 then);
SEG_GAME static void level_restart_script(int16 level, LevelRestart *r);
SEG_GAME static cbool level_restart_table(const uint8 *t, uint32 size, int16 level, LevelRestart *r);
SEG_GAME static void start_flythru(void);
SEG_GAME static void end_flythru(void);
SEG_GAME static void round_end(void);
SEG_GAME static void new_round(void);
SEG_GAME static void game_over(void);
SEG_GAME static void MapFlyColors(void);
SEG_GAME static void FlyGrid(void);
SEG_GAME static void DoWatchSpikesMsg(void);
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

/* LVAL 128 holds the script's values of levels 1 to LVAL_LEVELS, worked out
   at build time by build/host/mklval in the order read_values reads them
   (LVAL_ENTRY bytes a level, big-endian, int16 or int32 as below), so a game
   starts without running the script (about 3 s on a IIIx). Other levels run
   it. lv_out is for mklval: the values are written there as they are
   worked out. */
static const uint8 *lv_row;
static uint8 *lv_out;

SEG_GAME static int32 lv_long(int32 token)
{
	int32 v;

	if (lv_row) {
		v = gets32(lv_row);
		lv_row += 4;
		return v;
	}
	v = eval_long(token);
	if (lv_out) {
		lv_out[0] = (uint8)((uint32)v >> 24);
		lv_out[1] = (uint8)((uint32)v >> 16);
		lv_out[2] = (uint8)((uint32)v >> 8);
		lv_out[3] = (uint8)v;
		lv_out += 4;
	}
	return v;
}

SEG_GAME static int16 lv_int(int32 token)
{
	int16 v;

	if (lv_row) {
		v = gets16(lv_row);
		lv_row += 2;
		return v;
	}
	v = eval_int(token);
	if (lv_out) {
		lv_out[0] = (uint8)((uint16)v >> 8);
		lv_out[1] = (uint8)v;
		lv_out += 2;
	}
	return v;
}

/* The level's values into ThisLevel, from lv_row or the script (which must
   be loaded for the level). */
SEG_GAME static void read_values(void)
{
	int16 i;

	ThisLevel.lvField = lv_int(kVarLevelField);
	ThisLevel.lvColor = lv_int(kVarLevelColor);
	ThisLevel.lvBonus = lv_long(kVarLevelBonus);
	ThisLevel.lvStBonus = lv_long(kVarLevelSTBonus);
	ThisLevel.lvNext = lv_int(kVarLevelNext);
	ThisLevel.plMaxMove = lv_int(kVarPlayerMaxMove);
	ThisLevel.plSuperZaps = lv_int(kVarPlayerSuperZaps);
	for (i = 0; i < 5; i++)
		ThisLevel.shColor[i] = lv_int(kVarShotColor_0_ + i);
	ThisLevel.shSpeed = lv_int(kVarShotSpeed);
	ThisLevel.shPower = lv_int(kVarShotPower);

	ThisLevel.flPoints = lv_int(kVarFlipperPoints);
	ThisLevel.flCount = lv_int(kVarFlipperCount);
	ThisLevel.flProb = (uint16)lv_long(kVarFlipperProb);
	ThisLevel.flSpeed = lv_long(kVarFlipperSpeed);
	ThisLevel.flRot = lv_int(kVarFlipperRot);
	ThisLevel.flColor = lv_int(kVarFlipperColor);
	ThisLevel.fuPoints = lv_int(kVarFusePoints);
	ThisLevel.fuBullseye = lv_int(kVarFuseBullseye);
	ThisLevel.fuCount = lv_int(kVarFuseCount);
	ThisLevel.fuProb = (uint16)lv_long(kVarFuseProb);
	ThisLevel.fuWarpP = lv_int(kVarFuseWarpP);
	ThisLevel.fuPlayerPlus = lv_int(kVarFusePlayerPlus);
	for (i = 0; i < 5; i++)
		ThisLevel.fuColor[i] = lv_int(kVarFuseColor_0_ + i);
	ThisLevel.puPoints = lv_int(kVarPulsarPoints);
	ThisLevel.puCount = lv_int(kVarPulsarCount);
	ThisLevel.puProb = (uint16)lv_long(kVarPulsarProb);
	ThisLevel.puSpeed = lv_long(kVarPulsarSpeed);
	ThisLevel.puRot = lv_int(kVarPulsarRot);
	ThisLevel.puColor = lv_int(kVarPulsarColor);
	ThisLevel.puTime = lv_int(kVarPulsarTime);
	ThisLevel.puPulsDepth = lv_int(kVarPulsarPulsDepth);
	for (i = 0; i < TANKTYPES; i++) {
		int32 t = kVarTank_0_points + 5 * i;

		ThisLevel.tk[i].points = lv_int(t);
		ThisLevel.tk[i].count = lv_int(t + 1);
		ThisLevel.tk[i].prob = (uint16)lv_long(t + 2);
		ThisLevel.tk[i].speed = lv_long(t + 3);
		ThisLevel.tk[i].color = lv_int(t + 4);
	}
	ThisLevel.spPoints = lv_int(kVarSpikerPoints);
	ThisLevel.spProb = (uint16)lv_long(kVarSpikerProb);
	ThisLevel.spSpeed = lv_long(kVarSpikerSpeed);
	ThisLevel.spStart = lv_int(kVarSpikerStart);
	ThisLevel.spTop = lv_int(kVarSpikerTop);
	ThisLevel.spPlasma = lv_int(kVarSpikerPlasma);
	ThisLevel.spPlSpeed = lv_long(kVarSpikerPlSpeed);
	ThisLevel.spPlPoints = lv_int(kVarSpikerPlPoints);
	ThisLevel.spSpikePoints = lv_int(kVarSpikerSpikePoints);
	for (i = 0; i < 3; i++)
		ThisLevel.spColor[i] = lv_int(kVarSpikerColor_0_ + i);
	ThisLevel.boredomCount = lv_int(kVarBoredomCount);
	ThisLevel.boredProb = lv_long(kVarBoredProb);
	ThisLevel.endTimer = lv_int(kVarEndTimer);
	ThisLevel.ShowSpikesMsg = lv_int(kVarShowSpikesMsg);
	ThisLevel.Doomsday = lv_int(kVarDoomsday);
}

/* Builds LVAL 128 for mklval: runs the level's script and writes its values
   (LVAL_ENTRY bytes) to out. Returns 0 if the script can't be loaded. */
SEG_GAME cbool game_level_values(int16 level, uint8 *out)
{
	if (!parser_alloc())
		return 0;
	parser_load_level(level);
	parser_write(kConstDEPTH, (double)DEPTH);
	lv_row = NULL;
	lv_out = out;
	read_values();
	lv_out = NULL;
	parser_free();
	return 1;
}

/* STLoadLevel: reads the script's values for level into ThisLevel. The
   level number, field, colour and the next level come from the script too,
   so the levels go on as the script says. A doomsday level picks its field
   with VARandom. */
SEG_GAME static cbool st_load_level(int16 level)
{
	int16 doomzaps = ThisLevel.plSuperZaps;
	uint32 size = 0;
	const uint8 *t = NULL;
	cbool parsed = 0;

	if (level < 1)
		level = 1;
	if (level > MAXLEVEL)
		level = MAXLEVEL;
	if (level <= LVAL_LEVELS)
		t = plat_resource(FOURCC('L', 'V', 'A', 'L'), 128, &size);
	if (t && size >= (uint32)level * LVAL_ENTRY) {
		lv_row = t + (uint32)(level - 1) * LVAL_ENTRY;
		read_values();
		lv_row = NULL;
	} else {
		/* The script needs about 15 KB of dynamic heap for a moment, more
		   than a IIIx has free in a game. The grids (about 40 KB) are made
		   again by CreateFit after it, so they go while it runs. */
		FreeGrids();
		if (!parser_alloc()) {
			if (t)
				plat_release(t);
			AllocGrids();
			return 0;
		}
		parser_load_level(level);
		parser_write(kConstDEPTH, (double)DEPTH);
		read_values();
		parsed = 1;
	}
	if (t)
		plat_release(t);
	ThisLevel.lvNumber = level;
	if (ThisLevel.Doomsday == 1) {
		ThisLevel.lvField = (int16)((uint16)va_random() % 16 + 1);
		if (Hero.Flags & HF_SAVEDGAMESTART) {
			Hero.Flags &= (uint16)~HF_SAVEDGAMESTART;
			ThisLevel.plSuperZaps = doomzaps;
		}
	}
	if (parsed)
		parser_free();
	if (!AllocGrids())
		return 0;
	ThisLevel.totalCount = (int16)(ThisLevel.flCount + ThisLevel.fuCount + ThisLevel.puCount +
	                               ThisLevel.tk[0].count +
	                               ThisLevel.tk[1].count + ThisLevel.tk[2].count);
	/* activeCount carries from the last step of the previous level, as the
	   original's STLoadLevel left it. */
	ThisLevel.edgeCount = 0;
	ThisLevel.starCount = 0;
	ThisLevel.probIncrease = 0;
	if (ThisLevel.plMaxMove < 1)
		ThisLevel.plMaxMove = 1;
	return 1;
}

/* STInitialize: the claw, shots and enemies back at the start. The spikes
   keep their length (InitSpikes sets it only when a level is loaded). */
SEG_GAME static void st_initialize(void)
{
	InitPlayer();
	InitShots();
	InitEnemies();
	phase = PhasePlay;
}

/* CreateFit's intro runs as steps of its own; STInitialize follows it. */
SEG_GAME static void begin_intro(int16 then)
{
	intro_then = then;
	if (!LevelIntro) {
		intro_done();
		return;
	}
	intro_begin();
	phase = PhaseIntro;
}

SEG_GAME static void intro_done(void)
{
	if (intro_then == ThenRound)
		RemoveLife();
	st_initialize();
}

/* STLoadLevel and CreateFit; the intro, then STInitialize. */
SEG_GAME static cbool begin_level(int16 level)
{
	if (!st_load_level(level))
		return 0;
	VALoadColors(ThisLevel.lvColor);
	if (!CreateFit())
		return 0;
	begin_intro(ThenStart);
	game_resume();
	return 1;
}

SEG_GAME cbool game_load_level(int16 level)
{
	return begin_level(level);
}

/* The script's next level. */
SEG_GAME void game_next_level(void)
{
	game_load_level(ThisLevel.lvNext);
}

SEG_GAME cbool game_screen_changed(void)
{
	if (!ready)
		return 0;
	ScoreLayout();
	return game_load_level(ThisLevel.lvNumber);
}

SEG_GAME void game_seed(void)
{
	int32 t = (int32)((plat_ticks() & 0x3FFFFFFFUL) + 1);

	qd_seed(t);
	va_seed(t);
}

SEG_GAME cbool game_start(int16 level, cbool saved)
{
	ready = 0;
	over = 0;
	game_demo_stop();
	start_level = level;
	if (saved)
		Hero.Flags |= HF_SAVEDGAMESTART;
	else
		Hero.Flags &= (uint16)~HF_SAVEDGAMESTART;
	ZeroScore();
	if (saved)
		Hero.Flags &= (uint16)~HF_SAVEDFIRSTDONE;
	levelComplete = 0;
	levelBonus = 0;
	levelStBonus = 0;
	doomsday_death = 0;
	if (!begin_level(level))
		return 0;
	prev_zap = 0;
	ready = 1;
	return 1;
}

SEG_GAME cbool game_init(int16 level)
{
	ready = 0;
	if (!AllocGrids())
		return 0;
	AllocCStars();
	return game_start(level, 0);
}

SEG_GAME void game_close(void)
{
	ready = 0;
	intro_stop();
	FreeGrids();
}

SEG_GAME cbool game_running(void)
{
	return ready;
}

/* What the level select needs of one level, from its script. The parser
   must be allocated. */
SEG_GAME static void level_restart_script(int16 level, LevelRestart *r)
{
	parser_load_level(level);
	parser_write(kConstDEPTH, (double)DEPTH);
	r->level = level;
	r->arcade = eval_int(kVarAllowArcadeRestart);
	r->practice = eval_int(kVarAllowPracticeRestart);
	r->field = eval_int(kVarLevelField);
	r->color = eval_int(kVarLevelColor);
	r->startBonus = eval_long(kVarLevelSTBonus);
	r->next = eval_int(kVarLevelNext);
}

SEG_GAME cbool game_level_restart(int16 level, LevelRestart *r)
{
	if (!parser_alloc())
		return 0;
	level_restart_script(level, r);
	parser_free();
	return 1;
}

/* The same from the LSEL table worked out at build time (LSEL_ENTRY bytes
   an entry, in the order the script runs them), or 0 if it isn't there. */
SEG_GAME static cbool level_restart_table(const uint8 *t, uint32 size, int16 level, LevelRestart *r)
{
	uint32 i;

	for (i = 0; i + LSEL_ENTRY <= size; i += LSEL_ENTRY)
		if (gets16(t + i) == level) {
			r->level = level;
			r->next = gets16(t + i + 2);
			r->arcade = gets16(t + i + 4);
			r->practice = gets16(t + i + 6);
			r->field = gets16(t + i + 8);
			r->color = gets16(t + i + 10);
			r->startBonus = (int32)get32(t + i + 12);
			return 1;
		}
	return 0;
}

/* The levels the level select offers, as CreateStable: from level 1 on,
   those the script allows a restart on (AllowArcadeRestart, or
   AllowPracticeRestart in practice mode), up to the highest level reached
   (at least 9) in arcade mode. Levels in the LSEL table don't run the
   script; with use_table 0 or past the table they do (about 0.4 s a level
   on a IIIx). */
SEG_GAME int16 game_level_choices_from(cbool use_table, cbool practice, int16 highest,
                                       LevelChoice *out, int16 max)
{
	int16 n = 0, level = 1, flag, last = 0;
	int16 limit = highest < 9 ? 9 : highest;
	uint32 size = 0;
	const uint8 *t = use_table ? plat_resource(FOURCC('L', 'S', 'E', 'L'), 128, &size) : NULL;
	cbool parsing = 0;
	LevelRestart r;

	do {
		do {
			if (!t || !level_restart_table(t, size, level, &r)) {
				if (!parsing && !parser_alloc()) {
					n = 0;
					flag = -1;
					break;
				}
				parsing = 1;
				level_restart_script(level, &r);
			}
			flag = practice ? r.practice : r.arcade;
			last = level;
			if (flag == 1 && n < max) {
				out[n].level = level;
				out[n].field = r.field;
				out[n].color = r.color;
				out[n].startBonus = r.startBonus;
				n++;
			}
			level = r.next;
			if (level < 1)
				level = 1;
		} while (flag == 0 && level < MAXLEVEL);
	} while (n < 95 && n < max && flag >= 0 && (last <= limit || practice));
	if (!practice && n > 0 && limit < out[n - 1].level)
		n--;
	if (parsing)
		parser_free();
	if (t)
		plat_release(t);
	return n;
}

SEG_GAME int16 game_level_choices(cbool practice, int16 highest, LevelChoice *out, int16 max)
{
	return game_level_choices_from(1, practice, highest, out, max);
}

/* The claw is dead for good: a new game on the level the last one started
   on, until the front end takes the game over. */
SEG_GAME static void game_over(void)
{
	/* A game played to the end flies the field away first (not a recorded
	   one). */
	if (!demo_res && LevelIntro && phase != PhaseFlyOut) {
		flyout_begin();
		phase = PhaseFlyOut;
		return;
	}
	Hero.Flags = 0;
	ready = 0;
	over = 1;
}

/* MapFlyColors: the level's colours with two of the six moving colours
   (not the shot colours) given to the field and the spikes. */
SEG_GAME static void MapFlyColors(void)
{
	uint8 buf[NUMCOLORS * 8];
	uint32 n = 0, i;
	int16 colorbits = 0, id = ThisLevel.lvColor;
	const uint8 *clot;

	for (i = 0; i < 5; i++)
		colorbits |= (int16)(1 << ThisLevel.shColor[i]);
	SpikeColor = 5;
	FieldColor = 6;
	for (i = 0; i < 6; i++)
		if ((colorbits & (1 << i)) == 0) {
			FieldColor = (int16)i;
			colorbits |= (int16)(1 << i);
			break;
		}
	for (i = 0; i < 6; i++)
		if ((colorbits & (1 << i)) == 0) {
			SpikeColor = (int16)i;
			colorbits |= (int16)(1 << i);
			break;
		}
	if (id == 5)
		id = (int16)(id + 128);
	clot = plat_resource(FOURCC('C', 'L', 'O', 'T'), id, &n);
	if (!clot)
		return;
	if (n > sizeof(buf))
		n = sizeof(buf);
	for (i = 0; i < n; i++)
		buf[i] = clot[i];
	plat_release(clot);
	if (n >= 72) {
		for (i = 0; i < 8; i++) {
			buf[SpikeColor * 8 + i] = buf[7 * 8 + i];
			buf[FieldColor * 8 + i] = buf[8 * 8 + i];
		}
	}
	VASetColors(buf, n);
}

/* The tube coming at the claw. flydepth levels of it have gone past. */
SEG_GAME static void FlyGrid(void)
{
	int16 i, j, upper, depper;

	depper = (int16)(DEPTH - Hero.flydepth);
	upper = (int16)((depper - DEPTH) / 3);
	if (upper < ZOOMDEPTH)
		upper = ZOOMDEPTH;

	j = ww.numsegs;
	if (ww.wraps)
		j--;
	i = j;
	do {
		if (Hero.lane == i || Hero.lane + 1 == i)
			VA.color = ThisLevel.shColor[0];
		else if (i == 0 && ww.wraps && Hero.lane == ww.numsegs - 1)
			VA.color = ThisLevel.shColor[0];
		else
			VA.color = FieldColor;
		VALine(ww.x[i][upper], ww.y[i][upper], ww.x[i][depper], ww.y[i][depper]);
	} while (i--);

	VA.color = FieldColor;
	i = j;
	VAMoveTo(ww.x[ww.numsegs][upper], ww.y[ww.numsegs][upper]);
	do {
		VASafeLineTo(ww.x[i][upper], ww.y[i][upper]);
	} while (i--);

	i = j;
	VAMoveTo(ww.x[ww.numsegs][depper], ww.y[ww.numsegs][depper]);
	do {
		VASafeLineTo(ww.x[i][depper], ww.y[i][depper]);
	} while (i--);
}

/* "Avoid Spikes", in the middle of the field for the message delay. */
SEG_GAME static void DoWatchSpikesMsg(void)
{
	int16 s = (int16)((VA.frame.right * 7 / 10 / 12 - 3) / 3);

	if (s < 1)
		s = 1;
	VA.segmscale = s;
	VAMoveTo((int16)((VA.frame.right - 12 * (s * 3 + 3)) / 2), (int16)(VA.frame.bottom / 2 - 2));
	VA.color = BG1;
	VADrawText("Avoid Spikes", 0, 12);
}

/* The start of STFlyThruLoop. */
SEG_GAME static void start_flythru(void)
{
	int16 i;

	MapFlyColors();
	ThisLevel.totalCount = 0;
	ThisLevel.flCount = 0;
	ThisLevel.fuCount = 0;
	ThisLevel.puCount = 0;
	for (i = 0; i < TANKTYPES; i++)
		ThisLevel.tk[i].count = 0;
	fly_started = 0;
	fly_hero = 0;
	if (ThisLevel.ShowSpikesMsg) {
		phase = PhaseMsg;
		msg_timer = MSGDELAY;
	} else {
		phase = PhaseFly;
		SOUND(SND_FLYTHRU, 999);
	}
}

SEG_GAME static void msg_step(const PlatInput *in)
{
	UpdatePlayer(in);
	UpdateShots();
	UpdateSpikes();
	UpdateCracks();
	if (--msg_timer <= 0) {
		phase = PhaseFly;
		SOUND(SND_FLYTHRU, 999);
	}
}

/* One pass of the fly-through loop. */
SEG_GAME static void fly_step(const PlatInput *in)
{
	if (fly_started)
		Hero.flydepth = (int16)(Hero.flydepth + 2);
	fly_started = 1;

	if (Hero.flydepth < DEPTH)
		UpdateShots();
	FlySpikes();
	fly_hero = (cbool)(Hero.flydepth < DEPTH);
	if (fly_hero)
		UpdatePlayer(in);
	UpdateCracks();
	if (Hero.flydepth == 0)
		RedrawSpikes();

	if (Hero.flydepth + 2 >= DEPTH - ZOOMDEPTH || Hero.state == HeroDead) {
		phase = PhaseFlyDie;
		if (Hero.state == HeroFlying || Hero.state == HeroDead)
			end_flythru();
	}
}

/* The claw was hit in the fly-through: its pieces fly until it is dead. */
SEG_GAME static void flydie_step(const PlatInput *in)
{
	UpdatePlayer(in);
	UpdateCracks();
	if (Hero.state == HeroFlying || Hero.state == HeroDead)
		end_flythru();
}

SEG_GAME static void end_flythru(void)
{
	if (Hero.state == HeroDead)
		VALoadColors(ThisLevel.lvColor);
	round_end();
}

/* The end of a pass of GameLoop's outer loop, after a level was played or
   flown: on to the next round, or the game is over. */
SEG_GAME static void round_end(void)
{
	if (ThisLevel.Doomsday == 1 && Hero.lives > 0)
		doomsday_death = 1;
	if (Hero.lives > 0 || Hero.state != HeroDead)
		new_round();
	else
		game_over();
}

/* The top of GameLoop's outer loop. A claw that is alive has cleared the
   level: its bonus is paid and the next level loads. A dead claw starts the
   same level again with what is left of it (a doomsday level loads afresh),
   and the spikes stay. */
SEG_GAME static void new_round(void)
{
	if (Hero.state != HeroDead) {
		AddLife();
		if (levelComplete) {
			/* The starting bonus is paid once, on the first level cleared. */
			if (!(Hero.Flags & HF_STBONUSGIVEN)) {
				Hero.Flags |= (uint16)(HF_SHOWZAPMSG | HF_STBONUSGIVEN);
				IncreaseScore(levelStBonus);
			}
			Hero.Flags |= HF_SHOWZAPMSG;
			IncreaseScore(levelBonus);
		} else if (!(Hero.Flags & HF_SAVEDFIRSTDONE)) {
			Hero.Flags &= (uint16)~HF_SHOWZAPMSG;
		}
		levelComplete = 0;
		levelBonus = 0;
		levelStBonus = 0;
		if (!st_load_level(ThisLevel.lvNext)) {
			ready = 0;
			return;
		}
		VALoadColors(ThisLevel.lvColor);
		if (!CreateFit()) {
			ready = 0;
			return;
		}
		begin_intro(ThenRound);
		game_resume();
		return;
	} else if (doomsday_death) {
		int16 numzaps = ThisLevel.plSuperZaps;
		int16 lvl = ThisLevel.lvNumber;

		if (!st_load_level(ThisLevel.lvNext)) {
			ready = 0;
			return;
		}
		ThisLevel.plSuperZaps = numzaps;        /* keeps the old number of zaps */
		ThisLevel.lvNumber = lvl;
		ThisLevel.lvNext = (int16)(lvl + 1);
		Hero.Flags &= (uint16)~HF_SHOWZAPMSG;
		VALoadColors(ThisLevel.lvColor);
		if (!CreateFit()) {
			ready = 0;
			return;
		}
		begin_intro(ThenRound);
		Hero.Flags |= HF_SHOWZAPMSG;
		doomsday_death = 0;
		game_resume();
		return;
	}
	RemoveLife();
	st_initialize();
	game_resume();
}

/* One step of the level being played (the inner loop of GameLoop). */
SEG_GAME static void play_step(const PlatInput *in)
{
#ifdef DEBUG_LEVELS
	/* Superzap steps to the next level until the superzap exists. */
	if ((in->actions & PLAT_ACT_ZAP) && !(prev_zap & PLAT_ACT_ZAP))
		game_next_level();
	prev_zap = in->actions;
#endif
#ifdef DEBUG_CLEAR
	/* The enemies are gone after 3 seconds, to reach the fly-through. */
	if (game_steps == 60) {
		InitEnemies();
		ThisLevel.flCount = ThisLevel.fuCount = ThisLevel.puCount = 0;
		ThisLevel.tk[0].count = ThisLevel.tk[1].count = ThisLevel.tk[2].count = 0;
		ThisLevel.totalCount = 0;
	}
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

	/* The level is over when only enemies waiting on the rim are left, once
	   the end timer has run down. */
	if (LevelEnds && Hero.state == HeroPlaying && ThisLevel.edgeCount == ThisLevel.totalCount) {
		Hero.endtimer--;
		if (Hero.endtimer <= 0) {
			Hero.state = HeroFlying;
			Hero.flydepth = 0;
			levelComplete = 1;
			levelStBonus = ThisLevel.lvStBonus;
			levelBonus = ThisLevel.lvBonus;
		}
	}
	if (Hero.state == HeroFlying)
		start_flythru();
	else if (Hero.state == HeroDead)
		round_end();
}

SEG_GAME void game_tick(const PlatInput *in)
{
	switch (phase) {
	case PhaseMsg:
		msg_step(in);
		break;
	case PhaseFly:
		fly_step(in);
		break;
	case PhaseFlyDie:
		flydie_step(in);
		break;
	case PhaseIntro:
		if (intro_step())
			intro_done();
		break;
	case PhaseFlyOut:
		if (intro_step())
			game_over();
		break;
	default:
		play_step(in);
		break;
	}
	game_steps++;
}

SEG_GAME void game_draw(void)
{
#ifdef GAME_PROFILE
	prof_qt = plat_ticks();
#endif
	switch (phase) {
	case PhaseMsg:
		DrawField();
		DrawHero();
		DrawShots();
		DrawSpikes();
		DrawCracks();
		DoWatchSpikesMsg();
		break;
	case PhaseFly:
		FlyGrid();
		DrawFlySpikes(SpikeColor);
		if (fly_hero)
			DrawHero();
		DrawShots();
		DrawCracks();
		break;
	case PhaseFlyDie:
		DrawCracks();
		break;
	case PhaseIntro:
	case PhaseFlyOut:
		intro_draw();
		break;
	default:
		DrawField();
		DrawHero();
		DrawEnemies();
		DrawShots();
		break;
	}
	{
		const PlatScreen *ps = plat_screen();

		if (ps && ps->status.w > 0 && ps->status.h > 0) {
			VAViewStatus();
			DrawStatusHud();
			VAViewPlay();
		} else {
			DrawLevelNumber();
			DisplayScore();
			DisplayLives();
			DrawScoreToBeat();
		}
	}
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

	plat_input(&in);
	return game_frame_in(&in);
}

SEG_GAME cbool game_over_now(void)
{
	return over;
}

SEG_GAME int16 game_level(void)
{
	return ThisLevel.lvNumber;
}

SEG_GAME cbool game_demo_start(int16 id)
{
	uint32 size;
	const uint8 *rec = plat_resource(FOURCC('M', 'Y', 'G', 'A'), (int16)(128 + (id & 7)), &size);
	int16 level;
	uint32 seed, frames;

	if (!rec || size < 14)
		return 0;
	seed = get32(rec);
	frames = get32(rec + 4);
	level = gets16(rec + 12);
	/* StartPlayback seeds VARandom and sets the frame counter before the
	   level loads; the intro and STInitialize then draw on both as they
	   did. The claw has no spare lives (the first pass adds one and takes
	   it again). */
	va_seed((int32)seed);
	game_steps = frames;
	/* The demos were recorded one after another from a fresh launch,
	   starting with MYGA 133, and some rely on the spike fractions and the
	   enemy count the one before left. The attract loop plays them in that
	   order, so 133 starts from the launch state. */
	if ((id & 7) == DEMO_FIRST) {
		ResetSpikes();
		ThisLevel.activeCount = 0;
	}
	if (!game_start(level, 0)) {
		plat_release(rec);
		return 0;
	}
	Hero.lives = 0;
	demo_start(rec + 14, (int16)(get32(rec + 8) > 400 ? 400 : get32(rec + 8)));
	demo_res = rec;
	return 1;
}

SEG_GAME void game_demo_stop(void)
{
	demo_stop();
	intro_stop();
	if (demo_res) {
		plat_release(demo_res);
		demo_res = 0;
	}
}

SEG_GAME cbool game_frame_in(const PlatInput *inp)
{
	PlatInput in = *inp;
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
