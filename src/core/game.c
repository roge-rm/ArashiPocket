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

enum { PhasePlay, PhaseMsg, PhaseFly, PhaseFlyDie };

LevelInfo ThisLevel;
cbool LevelEnds = 1;
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

SEG_GAME static int16 eval_int(int32 token);
SEG_GAME static int32 eval_long(int32 token);
SEG_GAME static cbool st_load_level(int16 level);
SEG_GAME static void st_initialize(void);
SEG_GAME static cbool begin_level(int16 level);
SEG_GAME static void play_step(const PlatInput *in);
SEG_GAME static void msg_step(const PlatInput *in);
SEG_GAME static void fly_step(const PlatInput *in);
SEG_GAME static void flydie_step(const PlatInput *in);
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

/* STLoadLevel: reads the script's values for level into ThisLevel. The
   level number, field, colour and the next level come from the script too,
   so the levels go on as the script says. A doomsday level picks its field
   with VARandom. */
SEG_GAME static cbool st_load_level(int16 level)
{
	int16 i;
	int16 doomzaps = ThisLevel.plSuperZaps;

	if (level < 1)
		level = 1;
	if (level > MAXLEVEL)
		level = MAXLEVEL;
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
	if (ThisLevel.Doomsday == 1) {
		ThisLevel.lvField = (int16)((uint16)va_random() % 16 + 1);
		if (Hero.Flags & HF_SAVEDGAMESTART) {
			Hero.Flags &= (uint16)~HF_SAVEDGAMESTART;
			ThisLevel.plSuperZaps = doomzaps;
		}
	}
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

/* STLoadLevel, CreateFit and STInitialize. */
SEG_GAME static cbool begin_level(int16 level)
{
	if (!st_load_level(level))
		return 0;
	VALoadColors(ThisLevel.lvColor);
	if (!CreateFit())
		return 0;
	st_initialize();
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
	ZeroScore();
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
	FreeGrids();
}

SEG_GAME cbool game_running(void)
{
	return ready;
}

/* The levels the level select offers, as CreateStable: from level 1 on,
   those the script allows a restart on (AllowArcadeRestart, or
   AllowPracticeRestart in practice mode), up to the highest level reached
   (at least 9) in arcade mode. */
SEG_GAME int16 game_level_choices(cbool practice, int16 highest, LevelChoice *out, int16 max)
{
	int16 n = 0, level = 1, flag, last = 0;
	int16 limit = highest < 9 ? 9 : highest;

	if (!parser_alloc())
		return 0;
	do {
		do {
			parser_load_level(level);
			parser_write(kConstDEPTH, (double)DEPTH);
			flag = eval_int(practice ? kVarAllowPracticeRestart : kVarAllowArcadeRestart);
			last = level;
			if (flag == 1 && n < max) {
				out[n].level = level;
				out[n].field = eval_int(kVarLevelField);
				out[n].color = eval_int(kVarLevelColor);
				out[n].startBonus = eval_long(kVarLevelSTBonus);
				n++;
			}
			level = eval_int(kVarLevelNext);
			if (level < 1)
				level = 1;
		} while (flag == 0 && level < MAXLEVEL);
	} while (n < 95 && n < max && flag >= 0 && (last <= limit || practice));
	parser_free();
	if (!practice && n > 0 && limit < out[n - 1].level)
		n--;
	return n;
}

/* The claw is dead for good: a new game on the level the last one started
   on, until the front end takes the game over. */
SEG_GAME static void game_over(void)
{
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
	}
}

SEG_GAME static void msg_step(const PlatInput *in)
{
	UpdatePlayer(in);
	UpdateShots();
	UpdateSpikes();
	UpdateCracks();
	if (--msg_timer <= 0)
		phase = PhaseFly;
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
		Hero.Flags |= HF_SHOWZAPMSG;
		doomsday_death = 0;
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
	/* GameLoop seeds the random numbers before the level loads and the
	   claw gets the one life that its first pass gives. */
	va_seed((int32)seed);
	if (!game_start(level, 0)) {
		plat_release(rec);
		return 0;
	}
	va_seed((int32)seed);
	game_steps = frames;
	Hero.lives = 1;
	demo_start(rec + 14, (int16)(get32(rec + 8) > 400 ? 400 : get32(rec + 8)));
	demo_res = rec;
	return 1;
}

SEG_GAME void game_demo_stop(void)
{
	demo_stop();
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
