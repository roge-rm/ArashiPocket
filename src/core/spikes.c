/* The spikers and their spikes, from STSpikes.c by Juri Munkki. A spiker
   climbs a lane from the far end, leaving a spike behind it that reaches
   from the highest point it got to down to the far end, turns at spTop and
   goes back down. It can fire plasma up its lane that catches the claw on
   the rim. A shot that reaches a spike wears it down by shPower levels and
   scores spSpikePoints. Levels are 16.16 in ball and plasma (the original's
   IFixed: the level is the top word). Update moves and tests, Draw queues
   the spikes, the spikers and the plasma. The original draws the spikes into
   the background layer; here they are lines in every frame. FlySpikes and
   DrawFlySpikes are the fly-through's part. */
#include "storm.h"
#include "qdrandom.h"

#define SPIKERSPEED     (ThisLevel.spSpeed)
#define SPIKERTOP       (ThisLevel.spTop)
#define SPIKERCOLOR     (ThisLevel.spColor[0])
#define PLASMASPEED     (ThisLevel.spPlSpeed)
#define PLASMAPRIMECOLOR (ThisLevel.spColor[1])
#define PLASMASECCOLOR  (ThisLevel.spColor[2])
#define PLASMARANGE     5
#define SPIKEHITDROP    (ThisLevel.shPower)

#define LEVELI(f) ((int16)((f) >> 16))
/* f with its level replaced by v, the fraction kept. */
#define SETLEVEL(f, v) (((f) & 0xFFFFL) | ((int32)(v) << 16))

typedef struct {
	int16 top;          /* where the spike starts; DEPTH or more is none */
	int32 ball;         /* the spiker's level */
	int32 direction;    /* 0 no spiker, negative up, positive down */
	int32 plasma;       /* level, above DEPTH when none is flying */
} Spiker;

static Spiker Spikers[MAXSEGS];
static int16 SpikeRotAngle;

SEG_ENEMY static void DrawSpiker(int16 cx, int16 cy, int16 size, int16 angle);
SEG_ENEMY static void DrawPlasma(int16 lane, int16 level);
SEG_ENEMY static void DropSpike(int16 lane);
SEG_ENEMY static void KillSpiker(int16 lane);
SEG_ENEMY static void BlowPlasma(int16 lane);
SEG_ENEMY static void SpikeShotTests(void);

/* A five sided star. */
SEG_ENEMY static void DrawSpiker(int16 cx, int16 cy, int16 size, int16 angle)
{
	int16 dx, dy, i;

	VA.color = SPIKERCOLOR;
	size = (int16)(size >> 2);
	dx = (int16)((int32)size * Cosins[angle] / 256);
	dy = (int16)((int32)size * Sins[angle] / 256);

	for (i = 0; i < 5; i++) {
		angle = (int16)(angle + ANGLES / 5);
		if (angle >= ANGLES)
			angle = (int16)(angle - ANGLES);
		VAMoveTo((int16)(cx + dx), (int16)(cy + dy));
		dx = (int16)((int32)size * Cosins[angle] / 256);
		dy = (int16)((int32)size * Sins[angle] / 256);
		VALineTo((int16)(cx + dx), (int16)(cy + dy));
	}
}

/* A triangle turning as it climbs, with a spot at the middle of each edge
   of the inner one. */
SEG_ENEMY static void DrawPlasma(int16 lane, int16 level)
{
	int16 dx, dy, x, y, angle, i, size;

	size = ww.unitlen[level];
	x = ww.xc[lane][level];
	y = ww.yc[lane][level];
	angle = (int16)((level * 21 + ANGLES) % ANGLES);

	VAMoveTo(x, y);
	for (i = 0; i < 3; i++) {
		angle = (int16)(angle + ANGLES / 3);
		if (angle >= ANGLES)
			angle = (int16)(angle - ANGLES);
		dx = (int16)(((int32)Cosins[angle] * size) >> 10);
		dy = (int16)(((int32)Sins[angle] * size) >> 10);
		VA.color = PLASMAPRIMECOLOR;
		VASafeLineTo((int16)(x + dx), (int16)(y + dy));
		VA.color = PLASMASECCOLOR;
		VASafeSpot((int16)(x - (dx >> 1)), (int16)(y - (dy >> 1)));
	}
}

/* A shot has reached the spike: it loses shPower levels. */
SEG_ENEMY static void DropSpike(int16 lane)
{
	int16 l = (int16)(Spikers[lane].top - Hero.flydepth);

	if (l < ZOOMDEPTH)
		l = ZOOMDEPTH;
	VAExplosion(ww.xc[lane][l], ww.yc[lane][l], (int16)(Spikers[lane].top > DEPTH / 2 ? 0 : 1), 5);
	IncreaseScore(ThisLevel.spSpikePoints);

	Spikers[lane].top = (int16)(Spikers[lane].top + SPIKEHITDROP);
	if (Spikers[lane].top >= DEPTH)
		Spikers[lane].top = (int16)(DEPTH + 999);
}

SEG_ENEMY static void KillSpiker(int16 lane)
{
	int16 l = LEVELI(Spikers[lane].ball);

	if (Spikers[lane].direction && l < DEPTH) {
		VAExplosion(ww.xc[lane][l], ww.yc[lane][l], (int16)(l > DEPTH / 2 ? 1 : 2), 0);
		IncreaseScore(ThisLevel.spPoints);
		Spikers[lane].direction = 0;
	}
}

SEG_ENEMY static void BlowPlasma(int16 lane)
{
	int16 l = LEVELI(Spikers[lane].plasma);

	VAExplosion((int16)(ww.xc[lane][l] + 1), (int16)(ww.yc[lane][l] + 1), 1, PLASMASECCOLOR);
	IncreaseScore(ThisLevel.spPlPoints);
	Spikers[lane].plasma = SETLEVEL(Spikers[lane].plasma, DEPTH + 1);
}

/* A superzap kills every spiker and plasma. A shot is stopped by the spike,
   and kills the spiker if it is past it, or stops at the plasma. */
SEG_ENEMY static void SpikeShotTests(void)
{
	int16 i, shotlevel, lane;

	if (Hero.superzapping == 1 && Hero.flydepth == 0) {
		for (i = 0; i < ww.numsegs; i++) {
			if (LEVELI(Spikers[i].ball) < DEPTH)
				KillSpiker(i);
			if (LEVELI(Spikers[i].plasma) < DEPTH)
				BlowPlasma(i);
		}
	}

	for (i = 0; i < shotcount; i++) {
		lane = shots[i].lane;
		shotlevel = (int16)(shots[i].level + Hero.flydepth);
		if (shotlevel > Spikers[lane].top) {
			if (shotlevel <= DEPTH)
				DropSpike(lane);
			if (shotlevel > LEVELI(Spikers[lane].ball))
				KillSpiker(lane);
			shots[i] = shots[--shotcount];
			i--;
		} else if (Hero.flydepth == 0) {
			if (shotlevel > LEVELI(Spikers[lane].plasma)) {
				VA.color = PLASMASECCOLOR;
				ExplodeShot(lane, shotlevel);
				BlowPlasma(lane);
				shots[i] = shots[--shotcount];
				i--;
			}
		}
	}
}

SEG_ENEMY void UpdateSpikes(void)
{
	int16 i, k;

	SpikeRotAngle = (int16)(SpikeRotAngle + 5);
	if (SpikeRotAngle >= ANGLES)
		SpikeRotAngle = (int16)(SpikeRotAngle - ANGLES);

	for (i = 0; i < ww.numsegs; i++) {
		Spiker *sp = &Spikers[i];

		if (sp->direction) {
			sp->ball += sp->direction;
			k = LEVELI(sp->ball);

			if (k <= SPIKERTOP)
				sp->direction = SPIKERSPEED;        /* at the top, turning round */
			else if (k >= DEPTH)
				sp->direction = 0;
			if (k < sp->top)
				sp->top = k;
		} else if (EnemySpawning && (uint16)va_random() < ThisLevel.spProb) {
			sp->direction = -SPIKERSPEED;
			sp->ball = SETLEVEL(sp->ball, DEPTH);
			if (LEVELI(sp->plasma) > DEPTH && ThisLevel.spPlasma)
				sp->plasma = SETLEVEL(sp->plasma, DEPTH);
		}

		if (LEVELI(sp->plasma) <= DEPTH) {
			sp->plasma -= PLASMASPEED;
			if (LEVELI(sp->plasma) < 0) {
				sp->plasma = SETLEVEL(sp->plasma, DEPTH + 1);
				Hero.lanestat[i] |= CapPlas;
			} else if (LEVELI(sp->plasma) < PLASMARANGE) {
				Hero.lanestat[i] |= CapPlas;
			}
		}
	}
	SpikeShotTests();
}

SEG_ENEMY void DrawSpikes(void)
{
	int16 i, k, top, p;

	for (i = 0; i < ww.numsegs; i++) {
		const Spiker *sp = &Spikers[i];

		top = sp->top;
		if (top < DEPTH) {
			if (top < 0)
				top = 0;
			VA.color = 0;
			VALine(ww.xc[i][top], ww.yc[i][top], ww.xc[i][DEPTH], ww.yc[i][DEPTH]);
		}
		if (sp->direction) {
			k = LEVELI(sp->ball);
			if (k >= 0 && k < DEPTH)
				DrawSpiker(ww.xc[i][k], ww.yc[i][k], ww.unitlen[k], SpikeRotAngle);
		}
		p = LEVELI(sp->plasma);
		if (p >= 0 && p <= DEPTH)
			DrawPlasma(i, p);
		if (top < DEPTH) {
			VA.color = ThisLevel.spColor[1];
			VAPixel(ww.xc[i][top], ww.yc[i][top]);
		}
	}
}

SEG_ENEMY void InitSpikes(void)
{
	int16 i;

	SpikeRotAngle = 0;
	for (i = 0; i < ww.numsegs; i++) {
		Spikers[i].direction = 0;
		Spikers[i].plasma = SETLEVEL(Spikers[i].plasma, DEPTH + 1);     /* no plasma flying */
		if (ThisLevel.spStart >= 0)
			Spikers[i].top = ThisLevel.spStart;
	}
	ThisLevel.spStart = -1;
}

SEG_ENEMY void RedrawSpikes(void)
{
	int16 i;

	for (i = 0; i < ww.numsegs; i++)
		Spikers[i].direction = 0;
}

SEG_ENEMY void FlySpikes(void)
{
	int16 toplevel, botlevel, i;

	SpikeShotTests();

	botlevel = (int16)(DEPTH - Hero.flydepth);
	for (i = 0; i < ww.numsegs; i++) {
		toplevel = (int16)(Spikers[i].top - Hero.flydepth);
		if (toplevel <= ZOOMDEPTH)
			toplevel = (int16)(ZOOMDEPTH + 1);
		if (toplevel < botlevel && toplevel <= 0)
			Hero.lanestat[i] |= CapSpike;
	}
}

SEG_ENEMY void DrawFlySpikes(int16 color)
{
	int16 toplevel, botlevel, i;

	botlevel = (int16)(DEPTH - Hero.flydepth);
	for (i = 0; i < ww.numsegs; i++) {
		toplevel = (int16)(Spikers[i].top - Hero.flydepth);
		if (toplevel <= ZOOMDEPTH)
			toplevel = (int16)(ZOOMDEPTH + 1);
		if (toplevel < botlevel) {
			VA.color = ThisLevel.spColor[1];
			VASafePixel(ww.xc[i][toplevel], ww.yc[i][toplevel]);
			VA.color = color;
			VALine(ww.xc[i][botlevel], ww.yc[i][botlevel], ww.xc[i][toplevel], ww.yc[i][toplevel]);
		}
	}
}

SEG_ENEMY void SpikerInfo(int16 lane, int16 *top, int16 *ball, int16 *plasma)
{
	*top = Spikers[lane].top;
	*ball = (int16)(Spikers[lane].direction ? LEVELI(Spikers[lane].ball) : -1);
	*plasma = (int16)(LEVELI(Spikers[lane].plasma) <= DEPTH ? LEVELI(Spikers[lane].plasma) : -1);
}

SEG_ENEMY void ClearSpikes(void)
{
	int16 i;

	for (i = 0; i < ww.numsegs; i++) {
		Spikers[i].top = (int16)(DEPTH + 999);
		Spikers[i].direction = 0;
		Spikers[i].plasma = SETLEVEL(Spikers[i].plasma, DEPTH + 1);
	}
	ThisLevel.spProb = 0;
}
