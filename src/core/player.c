/* The claw and the score line, from STPlayer.c and Shapes.c by Juri
   Munkki. DrawPlayer and DrawStaticPlayer are the original's. UpdatePlayer
   keeps its rawpos (8 steps to a lane), HeroShifts and the PLAYERMAXMOVE
   limit, but takes a target position from the input in place of the Mac
   mouse: buttons and the Jog Dial step the target a lane, the stylus sets
   it to the lane nearest the pen's rim position. */
#include "storm.h"
#include "game.h"

#define PLAYERMAXMOVE (ThisLevel.plMaxMove)
#define STEPS_PER_LANE 8
/* Held button: first repeat after 6 steps (0.3 s), then every 2. */
#define REPEAT_DELAY 6
#define REPEAT_EVERY 2
#define START_LIVES  2
#define FREELIFESCORE 20000L
#define MAXLIVES     5
#define DROPSPEED    5
#define NEXTBLOWTIMER   2
#define MAX_LEAD     (2 * STEPS_PER_LANE)

Player Hero;

static const int16 HeroShifts[8] = { 1, 2, 3, 4, 4, 5, 6, 7 };
static int16 target;            /* rawpos the claw is moving to */
static int16 held;              /* steps the left or right action has been held */
static uint16 prev_actions;
static cbool draw_hero = 1;
static const uint8 *demo_moves;  /* a recorded game being played back */
static int16 demo_n, demo_pos;
static int16 prev_rim = -1;
uint8 PlayControl;
static int16 ScoreSegmentScale;
static int16 ScoreX, ScoreY;

SEG_GAME void demo_start(const uint8 *moves, int16 n)
{
	demo_moves = moves;
	demo_n = n;
	demo_pos = 0;
	target = 0;
	Hero.rawpos = 0;
	Hero.lane = 0;
	Hero.oldlane = 0;
	Hero.shifter = 0;
}

SEG_GAME void demo_stop(void)
{
	demo_moves = 0;
}

SEG_GAME void ZeroScore(void)
{
	ScoreSegmentScale = (int16)(VA.frame.bottom >> 5);
	if (ScoreSegmentScale < 1)
		ScoreSegmentScale = 1;
	do {
		ScoreX = (int16)((ScoreSegmentScale + 7) * 8 + 10);
		ScoreY = (int16)(ScoreSegmentScale + ScoreSegmentScale + 4);
	} while ((ScoreX > VA.frame.right / 3) & (ScoreSegmentScale-- > 0));
	ScoreSegmentScale++;
	/* The 640x480 sizes shrink to nothing on a 160 pixel screen. */
	if (ScoreSegmentScale < 1)
		ScoreSegmentScale = 1;
	ScoreX = (int16)((ScoreSegmentScale + 7) * 8 + 10);
	ScoreY = (int16)(ScoreSegmentScale + ScoreSegmentScale + 4);

	if (!(Hero.Flags & HF_SAVEDGAMESTART)) {
		Hero.score = 0;
		Hero.lives = START_LIVES;
		Hero.freeLives = FREELIFESCORE;
		Hero.Flags = 0;
	}
	Hero.state = HeroPlaying;
}

SEG_GAME void AddLife(void)
{
	Hero.lives++;
}

SEG_GAME void RemoveLife(void)
{
	Hero.lives--;
}

/* A free life at 20000 points and each 20000 after, as long as the spare
   lives are under 5 (one more while the claw is flying to the next level,
   where the level's own life is already added). The colour flash is only for
   games that got their starting bonus. */
SEG_GAME void IncreaseScore(int32 points)
{
	Hero.score += points;
	Hero.drawscore = 1;
	if (Hero.score > Hero.freeLives) {
		if (Hero.lives < MAXLIVES || (Hero.lives <= MAXLIVES && Hero.state == HeroFlying)) {
			if (Hero.Flags & HF_STBONUSGIVEN)
				Hero.Flags |= 0x0001;
			AddLife();
		}
		Hero.freeLives = Hero.score - (Hero.score % FREELIFESCORE) + FREELIFESCORE;
	}
}

SEG_GAME void DisplayLives(void)
{
	int16 i, x, y, dx, sx;

	dx = (int16)(ScoreSegmentScale + ScoreSegmentScale);
	sx = (int16)((dx + dx + dx) >> 2);
	x = ScoreSegmentScale;
	y = (int16)(ScoreY + ScoreSegmentScale);
	VA.color = 0;
	for (i = 0; i < Hero.lives; i++) {
		DrawStaticPlayer(x, y, sx, 0);
		x = (int16)(x + dx);
	}
}

SEG_GAME void DisplayScore(void)
{
	VA.color = BG1;
	VA.segmscale = ScoreSegmentScale;
	VADrawNumber(Hero.score, ScoreX, ScoreY);
}

/* The score, lives and level in the status area (VAViewStatus is on):
   score on top, the lives under it and the level on the right when it is
   wide, else on a third line. */
SEG_GAME void DrawStatusHud(void)
{
	int16 w = VA.frame.right, h = VA.frame.bottom;
	int16 sc, x, y, dx, sx, i, lvx, lvy;
	cbool wide;

	sc = (int16)(h / 8);
	if (sc > 12)
		sc = 12;
	while (sc > 1 && (sc + 7) * 8 + 8 > w)
		sc--;
	if (sc < 1)
		sc = 1;
	wide = (cbool)(w >= 2 * ((sc + 7) * 8 + 8));
	VA.color = BG1;
	VA.segmscale = sc;
	y = (int16)(2 * sc + 6);
	{
		int32 v = Hero.score;
		int16 digits = 1;

		while (v >= 10) {
			v /= 10;
			digits++;
		}
		VADrawNumber(Hero.score, (int16)(4 + digits * (sc + 7)), y);
	}
	dx = (int16)(sc + sc);
	if (dx < 6)
		dx = 6;
	sx = (int16)((dx + dx + dx) >> 2);
	x = (int16)(dx / 2 + 2);
	y = (int16)(y + 2 * sc + 2 + dx);
	VA.color = 0;
	for (i = 0; i < Hero.lives && x + dx < w; i++) {
		DrawStaticPlayer(x, y, sx, 0);
		x = (int16)(x + dx + 2);
	}
	VA.segmscale = sc;
	VA.color = BG2;
	if (wide) {
		lvx = (int16)(w - sc - 10);
		lvy = (int16)(2 * sc + 6);
	} else {
		lvx = (int16)((sc + 7) * 3 + 4);
		lvy = (int16)(y + 2 * sc + 2 + dx);
	}
	if (lvy + 2 < h)
		VADrawNumber(ThisLevel.lvNumber, lvx, lvy);
}

/* The level number at the top middle, as CreateFit draws it. */
SEG_GAME void DrawLevelNumber(void)
{
	VA.segmscale = (int16)(VA.frame.bottom >> 6);
	if (VA.segmscale < 1)
		VA.segmscale = 1;
	VA.color = BG2;
	if (ThisLevel.lvColor == 5)     /* BG2 is invisible on these */
		VA.color = BG1;
	VADrawNumber(ThisLevel.lvNumber, (int16)(VA.frame.right / 2 - VA.segmscale - 3),
	             (int16)(VA.segmscale * 2 + 4));
}

SEG_GAME void DrawPlayer(int16 cx, int16 cy, int16 dx, int16 dy, int16 slant)
{
	int16 x2, y2, dxs, dys, x1, y1, x4, y4, x8, y8;

	if (slant < 1)
		slant = 1;
	if (slant > 7)
		slant = 7;

	x1 = (int16)(cx + dx);          y1 = (int16)(cy + dy);
	x4 = (int16)(dx >> 2);          y4 = (int16)(dy >> 2);
	x8 = (int16)(dx >> 3);          y8 = (int16)(dy >> 3);
	dxs = (int16)(dx * slant);      dys = (int16)(dy * slant);

	VAMoveTo(cx, cy);
	x2 = (int16)((dxs >> 3) + y4);  y2 = (int16)((dys >> 3) - x4);
	VASafeLineTo((int16)(cx + x2), (int16)(cy + y2));
	VASafeLineTo(x1, y1);

	x2 = (int16)(-(dxs >> 4) - y4); y2 = (int16)(-(dys >> 4) + x4);
	VASafeLineTo((int16)(x1 + x2), (int16)(y1 + y2));
	VASafeLineTo((int16)(x1 - x8), (int16)(y1 - y8));

	x2 = (int16)((dxs >> 3) + y8);  y2 = (int16)((dys >> 3) - x8);
	VASafeLineTo((int16)(cx + x2), (int16)(cy + y2));
	VASafeLineTo((int16)(cx + x8), (int16)(cy + y8));

	x2 = (int16)((((8 - slant) * dx) >> 4) - y4);
	y2 = (int16)((((8 - slant) * dy) >> 4) + x4);
	VASafeLineTo((int16)(cx + x2), (int16)(cy + y2));
	VASafeLineTo(cx, cy);
}

/* The claw as a life: the original draws it with static lines, here it is
   queued with the rest of the frame. */
SEG_GAME void DrawStaticPlayer(int16 cx, int16 cy, int16 dx, int16 dy)
{
	int16 x2, y2, x1, y1, x4, y4, x8, y8;

	x1 = (int16)(cx + dx);  y1 = (int16)(cy + dy);
	x4 = (int16)(dx >> 2);  y4 = (int16)(dy >> 2);
	x8 = (int16)(dx >> 3);  y8 = (int16)(dy >> 3);

	VAMoveTo(cx, cy);
	x2 = (int16)((dx >> 1) + y4);   y2 = (int16)((dy >> 1) - x4);
	VALineTo((int16)(cx + x2), (int16)(cy + y2));
	VALineTo(x1, y1);

	x2 = (int16)(-(dx >> 2) - y4);  y2 = (int16)(-(dy >> 2) + x4);
	VALineTo((int16)(x1 + x2), (int16)(y1 + y2));
	VALineTo((int16)(x1 - x8), (int16)(y1 - y8));

	x2 = (int16)((dx >> 1) + y8);   y2 = (int16)((dy >> 1) - x8);
	VALineTo((int16)(cx + x2), (int16)(cy + y2));
	VALineTo((int16)(cx + x8), (int16)(cy + y8));

	x2 = (int16)((dx >> 2) - y4);   y2 = (int16)((dy >> 2) + x4);
	VALineTo((int16)(cx + x2), (int16)(cy + y2));
	VALineTo(cx, cy);
}

SEG_GAME void InitPlayer(void)
{
	int16 i;

	Hero.flydepth = 0;
	Hero.dropdepth = 0;
	Hero.pendingshot = 0;
	Hero.couldshoot = 0;
	Hero.superzapping = 0;
	draw_hero = 1;
	Hero.state = HeroPlaying;
	Hero.timer = 0;
	for (i = 0; i < MAXSEGS; i++) {
		Hero.lanestat[i] = 0;
		Hero.segmstat[i] = 0;
	}
	Hero.segmstat[MAXSEGS] = 0;
	Hero.endtimer = ThisLevel.endTimer;
	/* Start in the middle of lane 0, where the claw sits square. */
	Hero.rawpos = 4;
	Hero.lane = 0;
	Hero.oldlane = 0;
	Hero.shifter = HeroShifts[4];
	target = 4;
	held = 0;
	prev_actions = 0;
}

/* Moves the target by d steps, round the tube or stopping at its ends. */
SEG_GAME static void step_target(int16 d);
SEG_GAME static int16 nearest_lane(int16 rim);

SEG_GAME static void step_target(int16 d)
{
	int16 steps = (int16)(ww.numsegs * STEPS_PER_LANE);
	int16 ahead = (int16)(target - Hero.rawpos);
	int32 t;

	/* The target stays within two lanes of the claw, so it stops soon
	   after the button is let go. */
	if (ww.wraps) {
		if (ahead > steps / 2)
			ahead = (int16)(ahead - steps);
		else if (ahead < -(steps / 2))
			ahead = (int16)(ahead + steps);
	}
	if ((d > 0 && ahead >= MAX_LEAD) || (d < 0 && ahead <= -MAX_LEAD))
		return;
	t = (int32)target + d;

	if (ww.wraps) {
		while (t < 0)
			t += steps;
		while (t >= steps)
			t -= steps;
	} else {
		if (t < 0)
			t = 0;
		if (t >= steps)
			t = steps - 1;
	}
	target = (int16)t;
}

/* The lane whose middle is nearest rim round the tube. */
SEG_GAME static int16 nearest_lane(int16 rim)
{
	int16 i, best = 0, bestd = 256;

	for (i = 0; i < ww.numsegs; i++) {
		int16 d = (int16)(((rim - LaneRim[i] + 128) & 255) - 128);

		if (d < 0)
			d = (int16)-d;
		if (d < bestd) {
			bestd = d;
			best = i;
		}
	}
	return best;
}

/* The claw blows into flying lines at the depth it is at. */
SEG_GAME static void NukePlayer(void)
{
	int16 l = Hero.lane, d = Hero.dropdepth;

	VA.color = ThisLevel.shColor[0];    /* shot colour 0 is the claw's too */
	BlowUpPlayer(ww.x[l][d], ww.y[l][d], (int16)(ww.x[l + 1][d] - ww.x[l][d]),
	             (int16)(ww.y[l + 1][d] - ww.y[l][d]), Hero.shifter);
}

/* The claw has been hit: the original's PlayerCapture, less the sounds. */
SEG_GAME void PlayerCapture(int16 status)
{
	if (status & CapFlip) {
		Hero.state = HeroDropping;
	} else if (status & CapPuls) {
		Hero.state = HeroPostMortem;
		Hero.timer = POSTMORTEMTIMER;
		CrackPlayer();
		NukePlayer();
	} else if (status & CapFuse) {
		Hero.state = HeroDisintegrating;
		Hero.timer = NEXTBLOWTIMER;
		NukePlayer();
	} else if (status & CapPlas) {
		Hero.state = HeroDisintegrating;
		Hero.timer = NEXTBLOWTIMER;
		NukePlayer();
	} else if (status & CapSpike) {
		Hero.state = HeroPostMortem;
		Hero.timer = POSTMORTEMTIMER;
		CrackPlayer();
	}
}

/* The one-step white flash of the level number's colour (BG2) a superzap
   makes, and its undoing. The original sets entry 8 of the colour table. */
SEG_GAME static void zap_flash(cbool on)
{
	uint8 buf[NUMCOLORS * 8];
	uint32 n = 0, i;
	const uint8 *clot;

	if (!on) {
		VALoadColors(ThisLevel.lvColor);
		return;
	}
	clot = plat_resource(FOURCC('C', 'L', 'O', 'T'), ThisLevel.lvColor, &n);
	if (!clot)
		return;
	if (n > sizeof(buf))
		n = sizeof(buf);
	for (i = 0; i < n; i++)
		buf[i] = clot[i];
	plat_release(clot);
	if (n >= 72) {
		for (i = 2; i < 8; i++)
			buf[64 + i] = 255;
	}
	VASetColors(buf, n);
}

SEG_GAME void UpdatePlayer(const PlatInput *in)
{
	int16 steps = (int16)(ww.numsegs * STEPS_PER_LANE);
	int16 delta = 0, dir, lanedir, i, j;
	int16 shootenable = 0;
	uint16 act = in->actions;
	uint16 pressed = (uint16)(act & ~prev_actions);
	int16 lr = (int16)(act & (PLAT_ACT_LEFT | PLAT_ACT_RIGHT));

	prev_actions = act;
	draw_hero = 1;

	if (Hero.state == HeroPlaying || Hero.state == HeroFlying) {
		if (demo_moves) {
			/* A recorded game: the move and the buttons of this step. */
			act = 0;
			delta = 0;
			if (demo_pos < demo_n) {
				int16 rec = (int16)(int8)demo_moves[demo_pos++];

				if (rec & 1)
					act |= PLAT_ACT_FIRE;
				if (rec & 2)
					act |= PLAT_ACT_ZAP;
				delta = (int16)(rec >> 2);
			} else {
				Hero.state = HeroDead;
				Hero.lives = 0;
			}
			pressed = (uint16)(act & ~prev_actions);
			prev_actions = act;
			if (delta > PLAYERMAXMOVE)
				delta = PLAYERMAXMOVE;
			if (delta < -PLAYERMAXMOVE)
				delta = (int16)-PLAYERMAXMOVE;
		} else {
			/* Right is clockwise on the screen whichever way the lanes are numbered. */
			lanedir = (int16)(LaneDir * STEPS_PER_LANE);
			if (lr == PLAT_ACT_LEFT || lr == PLAT_ACT_RIGHT) {
				dir = (int16)(lr == PLAT_ACT_RIGHT ? lanedir : -lanedir);
				if (pressed & lr) {
					held = 0;
					step_target(dir);
				} else {
					held++;
					if (held >= REPEAT_DELAY && ((held - REPEAT_DELAY) % REPEAT_EVERY) == 0)
						step_target(dir);
				}
			} else {
				held = 0;
			}
			if (in->spin)
				step_target((int16)(in->spin * lanedir));
			if (in->pen_down && in->rim >= 0) {
				if (PlayControl == 1) {
					/* Dragging: the claw moves as far round as the stylus does. */
					if (prev_rim >= 0) {
						int16 d = (int16)(in->rim - prev_rim);

						if (d > 128)
							d = (int16)(d - 256);
						else if (d < -128)
							d = (int16)(d + 256);
						target = (int16)(target + LaneDir * (int16)((int32)d * steps / 256));
						if (ww.wraps) {
							while (target < 0)
								target = (int16)(target + steps);
							while (target >= steps)
								target = (int16)(target - steps);
						} else if (target < 0) {
							target = 0;
						} else if (target >= steps) {
							target = (int16)(steps - 1);
						}
					}
				} else {
					target = (int16)(nearest_lane(in->rim) * STEPS_PER_LANE + STEPS_PER_LANE / 2);
				}
				prev_rim = in->rim;
			} else {
				prev_rim = -1;
			}

			/* The way to the target, the short way round a closed tube. */
			delta = (int16)(target - Hero.rawpos);
			if (ww.wraps) {
				if (delta > steps / 2)
					delta = (int16)(delta - steps);
				else if (delta < -(steps / 2))
					delta = (int16)(delta + steps);
			}
			if (delta > PLAYERMAXMOVE)
				delta = PLAYERMAXMOVE;
			if (delta < -PLAYERMAXMOVE)
				delta = (int16)-PLAYERMAXMOVE;

		}
		Hero.rawpos = (int16)(Hero.rawpos + delta);
		if (Hero.rawpos < 0) {
			if (ww.wraps) {
				while (Hero.rawpos < 0)
					Hero.rawpos = (int16)(Hero.rawpos + steps);
			} else {
				Hero.rawpos = 0;
			}
		} else if (Hero.rawpos >= steps) {
			if (ww.wraps) {
				while (Hero.rawpos >= steps)
					Hero.rawpos = (int16)(Hero.rawpos - steps);
			} else {
				Hero.rawpos = (int16)(steps - 1);
			}
		}
		Hero.shifter = HeroShifts[Hero.rawpos & 7];
		Hero.lane = (int16)(Hero.rawpos / STEPS_PER_LANE);

		/* A shot every other step, or at once on a new lane. */
		shootenable = (int16)(Hero.oldlane != Hero.lane || (game_steps & 1));

		if (Hero.lanestat[Hero.oldlane]) {
			Hero.lane = Hero.oldlane;
			PlayerCapture(Hero.lanestat[Hero.oldlane]);
		}
	}

	switch (Hero.state) {
	case HeroDropping:
		Hero.dropdepth = (int16)(Hero.dropdepth + (Hero.dropdepth >> 3) + DROPSPEED);
		if (Hero.dropdepth >= DEPTH) {
			Hero.state = HeroPostMortem;
			Hero.timer = POSTMORTEMTIMER;
			Hero.dropdepth = DEPTH;
			NukePlayer();
		}
		break;
	case HeroDisintegrating:
		draw_hero = 0;
		Hero.timer--;
		if (Hero.timer <= 0) {
			Hero.state = HeroPostMortem;
			Hero.timer = POSTMORTEMTIMER;
			Hero.shifter = (int16)((Hero.shifter + 4) & 7);
			NukePlayer();
		}
		break;
	case HeroPostMortem:
		draw_hero = 0;
		Hero.timer--;
		if (Hero.timer <= 0)
			Hero.state = HeroDead;
		break;
	default:
		break;
	}

	/* With nothing in the way the old lane follows. */
	if (Hero.oldlane != Hero.lane) {
		i = (int16)(delta > 0 ? 1 : -1);
		j = (int16)(delta > 0 ? 1 : 0);
		while (Hero.oldlane != Hero.lane) {
			if (Hero.segmstat[Hero.oldlane + j] || Hero.lanestat[Hero.oldlane]) {
				if (Hero.segmstat[Hero.oldlane + j]) {
					Hero.lane = (int16)(Hero.oldlane + j);
					PlayerCapture(Hero.segmstat[Hero.lane]);
				}
				Hero.lane = Hero.oldlane;
				if (Hero.lanestat[Hero.oldlane])
					PlayerCapture(Hero.lanestat[Hero.oldlane]);
			} else {
				Hero.oldlane = (int16)(Hero.oldlane + i);
				if (Hero.oldlane < 0)
					Hero.oldlane = (int16)(ww.numsegs - 1);
				else if (Hero.oldlane >= ww.numsegs)
					Hero.oldlane = 0;
			}
		}
	}

	/* The superzap flash lasts one step. A free life runs the colour tables
	   1 to 6 for 11 steps (the flash counter in Flags), then the level's own. */
	if ((Hero.superzapping == 1 || (Hero.Flags & HF_FLASH)) && Hero.state != HeroFlying) {
		if (!(Hero.Flags & HF_FLASH) || (Hero.Flags & 0x000C) == 0x000C) {
			zap_flash(0);
			Hero.Flags &= 0x7FF0;
		} else {
			VALoadColors((int16)((Hero.Flags & HF_FLASH) % 6 + 1));
			Hero.Flags++;
		}
	}

	if (Hero.state == HeroPlaying) {
		Hero.superzapping = 0;
		if (pressed & PLAT_ACT_ZAP) {
			if (ThisLevel.plSuperZaps > 0) {
				ThisLevel.plSuperZaps--;
				Hero.superzapping = 1;
				zap_flash(1);
			} else if (ThisLevel.plSuperZaps == 0) {
				ThisLevel.plSuperZaps--;
				Hero.superzapping = 2;
			}
		}
	}

	/* Fire held shoots, and a press on a step that can't shoot is kept. */
	if (Hero.state == HeroPlaying || Hero.state == HeroFlying) {
		if (shootenable) {
			Hero.couldshoot = 1;
			if ((act & PLAT_ACT_FIRE) || Hero.pendingshot)
				AddShot(Hero.lane);
			Hero.pendingshot = 0;
		} else {
			Hero.couldshoot = 0;
			Hero.pendingshot = (int16)((pressed & PLAT_ACT_FIRE) != 0);
		}
	}

	for (i = 0; i < ww.numsegs; i++) {
		Hero.lanestat[i] = 0;
		Hero.segmstat[i] = 0;
	}
	Hero.segmstat[i] = 0;
}

SEG_GAME void DrawHero(void)
{
	int16 l = Hero.lane, d = Hero.dropdepth;

	if (!draw_hero)
		return;
	VA.color = ThisLevel.shColor[0];    /* shot colour 0 is the claw's too */
	DrawPlayer(ww.x[l][d], ww.y[l][d],
	           (int16)(ww.x[l + 1][d] - ww.x[l][d]), (int16)(ww.y[l + 1][d] - ww.y[l][d]),
	           Hero.shifter);
	if (Hero.flydepth == 0) {
		VALine(ww.x[l][d], ww.y[l][d], ww.x[l][DEPTH], ww.y[l][DEPTH]);
		VALine(ww.x[l + 1][d], ww.y[l + 1][d], ww.x[l + 1][DEPTH], ww.y[l + 1][DEPTH]);
	}
}
