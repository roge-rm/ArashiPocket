/* The fuseballs, from STFuseBalls.c by Juri Munkki (and DrawFuseBall from
   Shapes.c). A fuseball is a five armed spark. It warps up and down a lane,
   wanders sideways between the lane edges (sublane, 256 to a lane) and, when
   most of the level is dead, settles on the rim where it kills a claw that
   stands on it. It can be shot only while it is in the middle half of a
   lane. Level is in 1/16 of a depth step. Update moves and tests hits, Draw
   queues the picture. A killed ball breaks into its lines (BlowFuseBall). */
#include "storm.h"
#include "qdrandom.h"

#define MAXFUSEBALLS 20
#define FUSESTARCOLOR 0
#define WARPRANGE (((DEPTH + DEPTH / 5)) << 4)
#define MAXWARP (DEPTH << 3)
#define WARPSPEED 50
#define WARPPROB (ThisLevel.fuWarpP)
#define WARPANGER (ThisLevel.fuPlayerPlus)
#define MAXLEVELSPEED 3
#define LANEDIVISIONS 256
#define MAXLANESPEED (LANEDIVISIONS / 8)
#define ROTATIONSPEED 2
#define FUSEDANGERLEVEL 1
#define FUSESEGMENTS 3

typedef struct {
	int16 activeflag;
	int16 mode;
	int16 lane;
	int16 sublane;
	int16 level;
	int16 warplevel;
	int16 lanespeed;
	int16 levelspeed;
	int16 rotation;
} FuseBall;

enum { Waiting = 0, Brownian, Warp, Edge };

static FuseBall Balls[MAXFUSEBALLS];
static int16 NumBalls;
static const int16 fusedivs[FUSESEGMENTS] = { 1500, 800, 600 };
static int16 fuseoff[5][FUSESEGMENTS];

SEG_ENEMY static void UpdateFuseOff(void);
SEG_ENEMY static void DrawFuseBall(int16 cx, int16 cy, int16 size, int16 angle);
SEG_ENEMY static void BallPosition(const FuseBall *fbp, int16 *x, int16 *y);
SEG_ENEMY static void UpdateBall(FuseBall *fbp);
SEG_ENEMY static void BlowFuseBall(int16 cx, int16 cy, int16 size, int16 angle);

/* The jitter of the arms: each step's offsets move one place along. */
SEG_ENEMY static void UpdateFuseOff(void)
{
	int16 i, j;

	for (j = FUSESEGMENTS - 1; j > 0; j--)
		for (i = 0; i < 5; i++)
			fuseoff[i][j] = fuseoff[i][j - 1];
	for (i = 0; i < 5; i++)
		fuseoff[i][0] = (int16)((qd_random() & 15) - 7);
}

SEG_ENEMY static void DrawFuseBall(int16 cx, int16 cy, int16 size, int16 angle)
{
	int16 dx, dy, i, j, a;

	for (i = 0; i < 5; i++) {
		VA.color = ThisLevel.fuColor[i];
		VAMoveTo(cx, cy);
		for (j = 0; j < FUSESEGMENTS; j++) {
			a = (int16)(angle + fuseoff[i][j]);
			if (a < 0)
				a = (int16)(a + ANGLES);
			if (a >= ANGLES)
				a = (int16)(a - ANGLES);
			dx = (int16)((int32)size * Cosins[a] / fusedivs[j]);
			dy = (int16)((int32)size * Sins[a] / fusedivs[j]);
			VALineTo((int16)(cx + dx), (int16)(cy + dy));
		}
		angle = (int16)(angle + ANGLES / 5);
		if (angle >= ANGLES)
			angle = (int16)(angle - ANGLES);
	}
}

/* The ball's lines fly off as pieces. */
SEG_ENEMY static void BlowFuseBall(int16 cx, int16 cy, int16 size, int16 angle)
{
	int16 dx, dy, i, j, a;

	BlowSetup(cx, cy, 4, 10);
	for (i = 0; i < 5; i++) {
		VA.color = ThisLevel.fuColor[i];
		VAMoveTo(cx, cy);
		for (j = 0; j < FUSESEGMENTS; j++) {
			a = (int16)(angle + fuseoff[i][j]);
			if (a < 0)
				a = (int16)(a + ANGLES);
			if (a >= ANGLES)
				a = (int16)(a - ANGLES);
			dx = (int16)((int32)size * Cosins[a] / fusedivs[j]);
			dy = (int16)((int32)size * Sins[a] / fusedivs[j]);
			BlowUpLine((int16)(cx + dx), (int16)(cy + dy));
		}
		angle = (int16)(angle + ANGLES / 5);
		if (angle >= ANGLES)
			angle = (int16)(angle - ANGLES);
	}
}

SEG_ENEMY void CreateNewBall(void)
{
	FuseBall *fbp;

	if (NumBalls < MAXFUSEBALLS) {
		fbp = Balls;
		NumBalls++;
		while (fbp->activeflag)
			fbp++;
		fbp->mode = Waiting;
		fbp->level = (int16)(DEPTH << 4);
		fbp->rotation = 0;
		fbp->activeflag = -1;
		StarApproach(&fbp->mode, FUSESTARCOLOR);
	}
}

/* Two fuseballs from a destroyed tanker. */
SEG_ENEMY void CreateSplitFuseBalls(int16 lane, int16 level)
{
	int16 i;
	FuseBall *theball;

	for (i = 0; i < 2; i++) {
		if (NumBalls < MAXFUSEBALLS) {
			theball = Balls;
			while (theball->activeflag)
				theball++;
			theball->mode = Brownian;
			theball->lane = lane;
			theball->activeflag = 1;
			theball->level = (int16)(level << 4);
			theball->sublane = (int16)(i ? (LANEDIVISIONS - LANEDIVISIONS / 4) : (LANEDIVISIONS / 4));
			theball->lanespeed = (int16)(i ? MAXLANESPEED : -MAXLANESPEED);
			theball->levelspeed = 0;
			theball->rotation = (int16)(i ? 0 : (ANGLES / 3));
			NumBalls++;
		}
	}
}

/* Where the ball is on screen, between the two edges of its lane. */
SEG_ENEMY static void BallPosition(const FuseBall *fbp, int16 *x, int16 *y)
{
	int16 l = (int16)(fbp->level >> 4);
	int16 x0 = ww.x[fbp->lane][l], y0 = ww.y[fbp->lane][l];

	*x = (int16)(x0 + ((int32)(ww.x[fbp->lane + 1][l] - x0) * fbp->sublane) / LANEDIVISIONS);
	*y = (int16)(y0 + ((int32)(ww.y[fbp->lane + 1][l] - y0) * fbp->sublane) / LANEDIVISIONS);
}

/* One step of an active ball: moves it, tells the claw, tests for a hit. */
SEG_ENEMY static void UpdateBall(FuseBall *fbp)
{
	int16 x, y, l, j, rd, zap, pts;

	rd = qd_random();
	if (fbp->mode == Edge)
		ThisLevel.edgeCount++;

	switch (fbp->mode) {
	case Waiting:
		break;
	case Warp:
		if (fbp->levelspeed < 0) {
			fbp->level = (int16)(fbp->level + fbp->levelspeed);
			if (fbp->level < fbp->warplevel) {
				fbp->level = fbp->warplevel;
				fbp->levelspeed = 0;
				fbp->mode = Brownian;
			}
		} else {
			fbp->level = (int16)(fbp->level + fbp->levelspeed);
			if (fbp->level > fbp->warplevel) {
				fbp->level = fbp->warplevel;
				fbp->levelspeed = 0;
				fbp->mode = Brownian;
			}
		}
		break;
	case Brownian:
	case Edge:
		if (fbp->mode == Brownian) {
			if (ThisLevel.fuCount + (ThisLevel.lvNumber >> 3) < ThisLevel.totalCount)
				fbp->level = (int16)(fbp->level + fbp->levelspeed);
			else    /* near the end the balls keep moving up */
				fbp->level = (int16)(fbp->level + (fbp->levelspeed > 0 ? -fbp->levelspeed : fbp->levelspeed));

			if (fbp->level > (DEPTH << 4)) {
				fbp->level = (int16)(DEPTH << 4);
				if (fbp->levelspeed > 0)
					fbp->levelspeed = (int16)-(fbp->levelspeed >> 1);
			} else if (fbp->level <= 0) {
				fbp->level = 0;
				fbp->levelspeed = 0;
				if (ThisLevel.fuCount + ThisLevel.lvNumber / 8 >= ThisLevel.totalCount)
					fbp->mode = Edge;   /* only edge balls once most of the rest is dead */
			}
			if (fbp->level > (DEPTH << 3))
				fbp->levelspeed = (int16)(fbp->levelspeed - (((rd & 0x3000) >> 12) - 1));
			else
				fbp->levelspeed = (int16)(fbp->levelspeed + (((rd & 0x3000) >> 12) - 1));
			if (fbp->levelspeed > MAXLEVELSPEED || fbp->levelspeed <= -MAXLEVELSPEED)
				fbp->levelspeed = (int16)(fbp->levelspeed / 3);

			j = (int16)(rd & 0xff);
			if (fbp->lane == Hero.lane)
				j = (int16)(j - WARPANGER);
			if (j < WARPPROB) {
				fbp->mode = Warp;
				j = (int16)(((int32)(uint16)qd_random() % WARPRANGE) - WARPRANGE + (DEPTH << 4));
				if (j < 0)
					j = 0;
				if (j > (DEPTH << 4))
					j = (int16)(DEPTH << 4);
				if (j > fbp->level + MAXWARP)
					j = (int16)(fbp->level + MAXWARP);
				else if (j < fbp->level - MAXWARP)
					j = (int16)(fbp->level - MAXWARP);
				fbp->warplevel = j;
				fbp->levelspeed = (int16)(j < fbp->level ? -WARPSPEED : WARPSPEED);
			}
		}
		/* Sideways: the lane and the place in it. */
		fbp->sublane = (int16)(fbp->sublane + fbp->lanespeed);
		while (fbp->sublane < 0) {
			fbp->lane--;
			fbp->sublane = (int16)(fbp->sublane + LANEDIVISIONS);
		}
		while (fbp->sublane >= LANEDIVISIONS) {
			fbp->sublane = (int16)(fbp->sublane - LANEDIVISIONS);
			fbp->lane++;
		}
		if (fbp->lane < 0) {
			if (ww.wraps) {
				fbp->lane = (int16)(fbp->lane + ww.numsegs);
			} else {
				fbp->lane = 0;
				fbp->sublane = 0;
				fbp->lanespeed = (int16)-fbp->lanespeed;
			}
		} else if (fbp->lane >= ww.numsegs) {
			if (ww.wraps) {
				fbp->lane = (int16)(fbp->lane - ww.numsegs);
			} else {
				fbp->lane--;
				fbp->sublane = (int16)(fbp->sublane + LANEDIVISIONS - fbp->lanespeed);
				fbp->lanespeed = (int16)-fbp->lanespeed;
			}
		}
		fbp->lanespeed = (int16)(fbp->lanespeed + ((rd & 8192) ? -1 : 1));
		if (fbp->lanespeed > MAXLANESPEED || fbp->lanespeed < -MAXLANESPEED)
			fbp->lanespeed = (int16)(fbp->lanespeed / 2);
		break;
	}
	fbp->rotation = (int16)(fbp->rotation + ROTATIONSPEED);
	if (fbp->rotation >= ANGLES)
		fbp->rotation = (int16)(fbp->rotation - ANGLES);

	l = (int16)(fbp->level >> 4);
	BallPosition(fbp, &x, &y);

	/* On the rim, away from the lane edges (8 sublanes each side are safe),
	   the ball kills a claw in the lane. */
	if (l < FUSEDANGERLEVEL && fbp->sublane > (LANEDIVISIONS >> 5) &&
	    fbp->sublane < LANEDIVISIONS - (LANEDIVISIONS >> 5))
		Hero.lanestat[fbp->lane] |= CapFuse;

	/* Only the middle half of the lane can be shot, a superzap gets all. */
	if ((fbp->sublane > (LANEDIVISIONS >> 2) && fbp->sublane < LANEDIVISIONS - (LANEDIVISIONS >> 2)) ||
	    Hero.superzapping) {
		zap = ShotHitTest(fbp->lane, l);
		if (zap) {
			fbp->activeflag = 0;
			NumBalls--;
			ThisLevel.fuCount--;
			ThisLevel.totalCount--;
			/* The deeper the kill, the more it scores, and the points float
			   above it unless a superzap did it. */
			if (DEPTH > l && l >= DEPTH * 2 / 3)
				pts = ThisLevel.fuBullseye;
			else if (DEPTH * 2 / 3 > l && l >= DEPTH / 3)
				pts = ThisLevel.fuPoints;
			else
				pts = (int16)(ThisLevel.fuPoints >> 1);
			IncreaseScore(pts);
			if (ShowFloatingScores && zap != -2)
				AddFLScore(x, y, pts);
			BlowFuseBall(x, y, ww.unitlen[l], fbp->rotation);
		}
	}
}

SEG_ENEMY void UpdateFuseBalls(void)
{
	FuseBall *fbp = Balls;
	int16 i;

	UpdateFuseOff();
	for (i = 0; i < MAXFUSEBALLS; i++, fbp++) {
		if (!fbp->activeflag)
			continue;
		if (fbp->mode <= 0) {
			if (fbp->mode < 0) {                /* the star has landed */
				fbp->lane = (int16)(-(fbp->mode + 1) / STARDIVISION);
				fbp->sublane = 0;
				fbp->mode = Warp;
				fbp->warplevel = (int16)((DEPTH << 4) / 3);
				fbp->levelspeed = -WARPSPEED;
				fbp->lanespeed = 0;
			}
		} else {
			UpdateBall(fbp);
		}
	}

	if (EnemySpawning && NumBalls < MAXFUSEBALLS && ThisLevel.fuCount > NumBalls) {
		if ((int32)(uint16)qd_random() < (int32)ThisLevel.fuProb + ThisLevel.probIncrease)
			CreateNewBall();
	}
	ThisLevel.starCount = (int16)(ThisLevel.starCount + ThisLevel.fuCount - NumBalls);
	ThisLevel.activeCount = (int16)(ThisLevel.activeCount + NumBalls);
}

SEG_ENEMY void DrawFuseBalls(void)
{
	FuseBall *fbp = Balls;
	int16 i, x, y;

	for (i = 0; i < MAXFUSEBALLS; i++, fbp++) {
		if (!fbp->activeflag || fbp->mode <= 0)
			continue;
		BallPosition(fbp, &x, &y);
		DrawFuseBall(x, y, ww.unitlen[fbp->level >> 4], fbp->rotation);
	}
}

SEG_ENEMY void InitFuseBalls(void)
{
	int16 i;

	for (i = 0; i < MAXFUSEBALLS; i++)
		Balls[i].activeflag = 0;
	NumBalls = 0;
}

SEG_ENEMY int16 ListFuseBalls(EnemyInfo *out, int16 max)
{
	int16 i, n = 0;

	for (i = 0; i < MAXFUSEBALLS && n < max; i++)
		if (Balls[i].activeflag && Balls[i].mode > 0) {
			out[n].kind = EnemyFuseball;
			out[n].lane = Balls[i].lane;
			out[n].level = (int16)(Balls[i].level >> 4);
			n++;
		}
	return n;
}
