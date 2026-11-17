/* The tankers, from STTanker.c by Juri Munkki. A tanker climbs a lane and
   splits into two flippers (or fuseballs or pulsars, by its type) when
   it is shot or reaches the rim. A superzap kills it without a split. The
   level is 16.16, as the flippers'. Update moves and tests hits, Draw
   queues the picture. */
#include "storm.h"
#include "qdrandom.h"

#define MAXTANKERS 10

typedef struct {
	int16 tanktype;     /* FlipTank, FuseTank, PulsTank */
	int16 state;        /* star 0, inactive 1, active 2 */
	int32 level;        /* 16.16 */
	int16 lane;
} Tanker;

enum { star, inactive, active };

static Tanker Tankers[MAXTANKERS];
static int16 ActiveTankers;
static int16 ActiveTypes[TANKTYPES];

SEG_ENEMY static void DrawTanker(int16 cx, int16 cy, int16 dx, int16 dy, int16 tanktype);
SEG_ENEMY static void UpdateActiveTanker(Tanker *thetank);

SEG_ENEMY static void DrawTanker(int16 cx, int16 cy, int16 dx, int16 dy, int16 tanktype)
{
	int16 x1, y1, x2, y2;

	VA.color = ThisLevel.tk[tanktype].color;
	x1 = (int16)(dx >> 1);
	y1 = (int16)(dy >> 1);
	x2 = (int16)(dx >> 2);
	y2 = (int16)(dy >> 2);

	VAMoveTo(cx, cy);
	VALineTo((int16)(cx + x1 + y1), (int16)(cy + y1 - x1));
	VALineTo((int16)(cx + dx), (int16)(cy + dy));
	VALineTo((int16)(cx + x1 - y1), (int16)(cy + y1 + x1));
	VALineTo(cx, cy);
	VALineTo((int16)(cx + x2), (int16)(cy + y2));
	VALineTo((int16)(cx + x1 - y2), (int16)(cy + y1 + x2));
	VALineTo((int16)(cx + dx - x2), (int16)(cy + dy - y2));
	VALineTo((int16)(cx + x1 + y2), (int16)(cy + y1 - x2));
	VALineTo((int16)(cx + x2), (int16)(cy + y2));
	VAMoveTo((int16)(cx + dx - x2), (int16)(cy + dy - y2));
	VALineTo((int16)(cx + dx), (int16)(cy + dy));
}

/* A newborn tanker: a star at the centre first. */
SEG_ENEMY void CreateNewTanker(int16 tanktype)
{
	Tanker *thetank;

	if (ActiveTankers < MAXTANKERS) {
		thetank = Tankers;
		while (thetank->state != inactive)
			thetank++;
		thetank->tanktype = tanktype;
		thetank->state = star;
		thetank->lane = 0;
		thetank->level = (int32)DEPTH << 16;
		StarApproach(&thetank->state, ThisLevel.tk[tanktype].color);
		ActiveTankers++;
		ActiveTypes[tanktype]++;
	}
}

/* An active tanker has gone through the star phase and now climbs up. */
SEG_ENEMY static void UpdateActiveTanker(Tanker *thetank)
{
	int16 lv;
	int16 split = 0;
	int16 zap;

	thetank->level -= ThisLevel.tk[thetank->tanktype].speed;       /* move up */
	lv = (int16)(thetank->level >> 16);
	if (lv <= 0) {                                  /* don't move past the top */
		thetank->level &= 0xFFFFL;
		Hero.lanestat[thetank->lane] |= CapFlip;
		split = 1;
	} else {
		/* Has the player hit us? */
		zap = ShotHitTest(thetank->lane, lv);
		if (zap) {
			IncreaseScore(ThisLevel.tk[thetank->tanktype].points);
			if (zap != -2) {
				split = 1;
			} else {
				ThisLevel.totalCount--;
				thetank->state = inactive;
				ActiveTankers--;
				ActiveTypes[thetank->tanktype]--;
				ThisLevel.tk[thetank->tanktype].count--;
			}
		}
	}
	if (split) {
		ThisLevel.tk[thetank->tanktype].count--;
		switch (thetank->tanktype) {
		case FlipTank:
			ThisLevel.flCount = (int16)(ThisLevel.flCount + 2);
			CreateSplitFlippers(thetank->lane, (int16)(thetank->level >> 16));
			ThisLevel.totalCount++;
			break;
		case FuseTank:
			ThisLevel.fuCount = (int16)(ThisLevel.fuCount + 2);
			CreateSplitFuseBalls(thetank->lane, (int16)(thetank->level >> 16));
			ThisLevel.totalCount++;
			break;
		case PulsTank:
			ThisLevel.puCount = (int16)(ThisLevel.puCount + 2);
			CreateSplitPulsars(thetank->lane, (int16)(thetank->level >> 16));
			ThisLevel.totalCount++;
			break;
		}
		thetank->state = inactive;
		ActiveTankers--;
		ActiveTypes[thetank->tanktype]--;
	}
}

/* Promotes the stars that have reached the tube to tankers and moves the
   rest, then maybe starts another of each type. */
SEG_ENEMY void UpdateTankers(void)
{
	Tanker *tank = Tankers;
	int16 i;

	for (i = 0; i < MAXTANKERS; i++) {
		if (tank->state < 0) {
			tank->lane = (int16)(-(tank->state + 1) / STARDIVISION);
			tank->state = active;
			if (ww.wraps) {
				if (tank->lane >= ww.numsegs)
					tank->lane = (int16)(ww.numsegs - 1);
			} else {
				if (tank->lane > ww.numsegs - 2)
					tank->lane = (int16)(ww.numsegs - 2);
				else if (tank->lane <= 0)
					tank->lane = 1;
			}
		} else {
			if (tank->state == active)
				UpdateActiveTanker(tank);
		}
		tank++;
	}

	for (i = 0; i < TANKTYPES; i++) {
		if (EnemySpawning && ActiveTankers < MAXTANKERS && ThisLevel.tk[i].count > ActiveTypes[i]) {
			if ((int32)(uint16)va_random() < (int32)ThisLevel.tk[i].prob + ThisLevel.probIncrease)
				CreateNewTanker(i);
		}
		ThisLevel.starCount = (int16)(ThisLevel.starCount + ThisLevel.tk[i].count - ActiveTypes[i]);
		ThisLevel.activeCount = (int16)(ThisLevel.activeCount + ActiveTypes[i]);
	}
}

SEG_ENEMY void DrawTankers(void)
{
	Tanker *tank = Tankers;
	int16 i, lv, x, y, dx, dy;

	for (i = 0; i < MAXTANKERS; i++, tank++) {
		if (tank->state != active)
			continue;
		lv = (int16)(tank->level >> 16);
		if (lv <= 0)
			continue;
		x = ww.x[tank->lane][lv];
		y = ww.y[tank->lane][lv];
		dx = (int16)(ww.x[tank->lane + 1][lv] - x);
		dy = (int16)(ww.y[tank->lane + 1][lv] - y);
		DrawTanker(x, y, dx, dy, tank->tanktype);
	}
}

SEG_ENEMY void InitTankers(void)
{
	int16 i;

	for (i = 0; i < MAXTANKERS; i++)
		Tankers[i].state = inactive;
	ActiveTankers = 0;
	for (i = 0; i < TANKTYPES; i++)
		ActiveTypes[i] = 0;
}

SEG_ENEMY int16 ListTankers(EnemyInfo *out, int16 max)
{
	int16 i, n = 0;

	for (i = 0; i < MAXTANKERS && n < max; i++)
		if (Tankers[i].state == active) {
			out[n].kind = EnemyTanker;
			out[n].lane = Tankers[i].lane;
			out[n].level = (int16)(Tankers[i].level >> 16);
			n++;
		}
	return n;
}
