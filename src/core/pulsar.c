/* The pulsars, from STPulsar.c by Juri Munkki. A pulsar climbs the tube
   hanging from a corner of the lanes like a flipper, but each time it
   reaches a corner it stops turning and pulses for puTime steps. While it
   pulses in the upper part of the tube it electrifies the lane it hangs in
   (Hero.lanestat, shown by DrawField) for the middle of the pulse, and on
   the rim it catches the claw like a flipper. PulsarPower is the
   original's FrameCounter & 31 (game_steps, one a step as there) folded to 0..15. The level is 16.16.
   Update moves and tests hits, Draw queues the picture. A killed pulsar
   breaks into a piece (STCrack). */
#include "storm.h"
#include "qdrandom.h"
#include "game.h"

#define MAXPULSARS 20
#define PULSARCOLOR (ThisLevel.puColor)
#define PULSARROTSPEED (ThisLevel.puRot)
#define PULSARUPSPEED (ThisLevel.puSpeed)
#define PULSINGTIME (ThisLevel.puTime)
#define NERVOUSNESS (PULSINGTIME >> 2)

#define LEVELI(p) ((int16)((p)->level >> 16))

typedef struct {
	int16 state;        /* star 0 (waiting, then -1 minus the arrival), inactive 1, active 2 */
	int16 pulscounter;  /* while above 0 the pulsar is pulsing */
	int16 lane;
	int32 level;        /* 16.16 */
	int16 vertex;
	int16 mirror;
	int16 rotation;
	int16 direction;
} Pulsar;

enum { star, inactive, active };

static Pulsar Pulsars[MAXPULSARS];
static int16 ActivePulsars;
static int16 PulsarPower;

SEG_ENEMY static void PulsarSegSkip(Pulsar *thepulsar);
SEG_ENEMY static void UpdateActivePulsar(Pulsar *pulsarp);
SEG_ENEMY static void PulsarGeometry(const Pulsar *p, int16 *x, int16 *y, int16 *dx, int16 *dy);
SEG_ENEMY static Pulsar *FreePulsar(void);

/* A pulsar shape from the connecting point (cx, cy) along (dx, dy); power
   0 to 15 squeezes it and bends the zigzag. */
SEG_ENEMY void DrawPulsar(int16 cx, int16 cy, int16 dx, int16 dy, int16 power)
{
	int16 normx, normy, ex, ey, bx, by;

	dx = (int16)(dx >> 1);
	dy = (int16)(dy >> 1);
	ex = (int16)(cx + dx);
	ey = (int16)(cy + dy);
	dx = (int16)(dx - ((power * dx) >> 5));
	dy = (int16)(dy - ((power * dy) >> 5));
	bx = (int16)(dx / 3);
	by = (int16)(dy / 3);
	normx = (int16)((-dy * power) >> 4);
	normy = (int16)((dx * power) >> 4);

	VA.color = PULSARCOLOR;
	VAMoveTo((int16)(ex + dx), (int16)(ey + dy));
	VASafeLineTo((int16)(ex + dx - bx + normx), (int16)(ey + dy - by + normy));
	VASafeLineTo((int16)(ex + bx - normx), (int16)(ey + by - normy));
	VASafeLineTo((int16)(ex + normx), (int16)(ey + normy));
	VASafeLineTo((int16)(ex - bx - normx), (int16)(ey - by - normy));
	VASafeLineTo((int16)(ex - dx + bx + normx), (int16)(ey - dy + by + normy));
	VASafeLineTo((int16)(ex - dx), (int16)(ey - dy));
}

SEG_ENEMY static Pulsar *FreePulsar(void)
{
	Pulsar *p = Pulsars;

	while (p->state != inactive)
		p++;
	return p;
}

/* A newborn pulsar: a star at the centre first. */
SEG_ENEMY void CreateNewPulsar(void)
{
	Pulsar *thepulsar;

	if (ActivePulsars < MAXPULSARS) {
		thepulsar = FreePulsar();
		thepulsar->state = star;
		thepulsar->lane = 0;
		thepulsar->level = (int32)DEPTH << 16;
		thepulsar->mirror = 0;
		thepulsar->pulscounter = 0;
		thepulsar->direction = (int16)(va_random() < 0 ? PULSARROTSPEED : -PULSARROTSPEED);
		StarApproach(&thepulsar->state, PULSARCOLOR);
		ActivePulsars++;
	}
}

/* Two pulsars from a destroyed tanker. */
SEG_ENEMY void CreateSplitPulsars(int16 lane, int16 level)
{
	int16 i, delta, nextseg, prevseg;
	Pulsar *thepulsar;

	for (i = 0; i < 2; i++) {
		if (ActivePulsars < MAXPULSARS) {
			thepulsar = FreePulsar();
			thepulsar->vertex = (int16)(lane + i);
			if (thepulsar->vertex >= ww.numsegs)
				thepulsar->vertex = (int16)(thepulsar->vertex - ww.numsegs);
			thepulsar->state = active;
			thepulsar->level = (int32)level << 16;
			thepulsar->mirror = 0;
			thepulsar->pulscounter = 0;
			thepulsar->direction = (int16)(i ? PULSARROTSPEED : -PULSARROTSPEED);
			nextseg = NextSeg[thepulsar->vertex];
			prevseg = PrevSeg[thepulsar->vertex];
			delta = (int16)(nextseg - prevseg);
			if (delta < 0)
				delta = (int16)(delta + ANGLES);
			if (thepulsar->direction < 0)
				thepulsar->rotation = (int16)(NextSeg[thepulsar->vertex] - delta / 2);
			else
				thepulsar->rotation = (int16)(PrevSeg[thepulsar->vertex] + delta / 2);
			thepulsar->lane = LaneAt(thepulsar->vertex, thepulsar->rotation);
			ActivePulsars++;
		}
	}
}

/* The pulsar grabs the other corner of its arm: mirror says how to draw it. */
SEG_ENEMY static void PulsarSegSkip(Pulsar *thepulsar)
{
	thepulsar->rotation = (int16)(thepulsar->rotation + ANGLES / 2);
	if (thepulsar->rotation >= ANGLES)
		thepulsar->rotation = (int16)(thepulsar->rotation - ANGLES);
	thepulsar->mirror = !thepulsar->mirror;
	if (LEVELI(thepulsar) == 0) {
		int16 lane = LaneAt(thepulsar->vertex, thepulsar->rotation);

		if (lane >= 0)
			Hero.lanestat[lane] |= CapPuls;
	}
}

SEG_ENEMY static void PulsarGeometry(const Pulsar *p, int16 *x, int16 *y, int16 *dx, int16 *dy)
{
	int16 lv = LEVELI(p);
	int16 l = ww.unitlen[lv];
	int16 r = (int16)((p->rotation % ANGLES + ANGLES) % ANGLES);

	*dy = (int16)((l * (int32)-Sins[r]) >> 8);
	*dx = (int16)((l * (int32)Cosins[r]) >> 8);
	*x = ww.x[p->vertex][lv];
	*y = ww.y[p->vertex][lv];
}

/* An active pulsar climbs, turns until it reaches a corner, pulses there. */
SEG_ENEMY static void UpdateActivePulsar(Pulsar *pulsarp)
{
	int16 dx, dy, x, y, sc;
	int16 doskip;

	pulsarp->level -= PULSARUPSPEED;
	if (LEVELI(pulsarp) <= 0) {
		pulsarp->level &= 0xFFFFL;
		ThisLevel.edgeCount++;
	}

	y = pulsarp->direction;
	if (pulsarp->pulscounter == 0) {
		x = pulsarp->rotation;
		dx = (int16)(NextSeg[pulsarp->vertex] - x);
		dy = (int16)(PrevSeg[pulsarp->vertex] - x);
		if (dx < -ANGLES)
			dx = (int16)(y + y);
		if (dy < -ANGLES)
			dy = (int16)(y + y);
		if (y > 0) {                            /* clockwise */
			if (dx <= 0)
				dx = (int16)(dx + ANGLES);
			if (dy <= 0)
				dy = (int16)(dy + ANGLES);
			if (dx < dy) {
				if (y >= dx) {                  /* reached the next clockwise corner */
					y = 0;
					pulsarp->pulscounter = PULSINGTIME;
					pulsarp->rotation = (int16)(pulsarp->rotation + dx);
					pulsarp->vertex++;
					doskip = 1;
					if (ww.wraps) {
						if (pulsarp->vertex >= ww.numsegs)
							pulsarp->vertex = 0;
					} else if (pulsarp->vertex >= ww.numsegs) {
						pulsarp->vertex = (int16)(ww.numsegs - 1);
						pulsarp->direction = (int16)-pulsarp->direction;
						doskip = 0;
						if (LEVELI(pulsarp) == 0)
							Hero.lanestat[pulsarp->vertex] |= CapPuls;
					}
					if (doskip)
						PulsarSegSkip(pulsarp);
				} else {
					pulsarp->rotation = (int16)(pulsarp->rotation + y);
					y = 0;
				}
			} else {
				if (y >= dy) {
					y = 0;
					pulsarp->pulscounter = PULSINGTIME;
					pulsarp->rotation = (int16)(pulsarp->rotation + dy);
					pulsarp->vertex--;
					doskip = 1;
					if (ww.wraps) {
						if (pulsarp->vertex < 0)
							pulsarp->vertex = (int16)(ww.numsegs - 1);
					} else if (pulsarp->vertex == 0) {
						pulsarp->vertex = 1;
						pulsarp->direction = (int16)-pulsarp->direction;
						doskip = 0;
						if (LEVELI(pulsarp) == 0)
							Hero.lanestat[0] |= CapPuls;
					}
					if (doskip)
						PulsarSegSkip(pulsarp);
				} else {
					pulsarp->rotation = (int16)(pulsarp->rotation + y);
					y = 0;
				}
			}
			if (pulsarp->rotation >= ANGLES)
				pulsarp->rotation = (int16)(pulsarp->rotation - ANGLES);
		} else {                                /* counter-clockwise */
			if (dx >= 0)
				dx = (int16)(dx - ANGLES);
			if (dy >= 0)
				dy = (int16)(dy - ANGLES);
			if (dx > dy) {
				if (y <= dx) {
					y = 0;
					pulsarp->pulscounter = PULSINGTIME;
					pulsarp->rotation = (int16)(pulsarp->rotation + dx);
					pulsarp->vertex++;
					doskip = 1;
					if (ww.wraps) {
						if (pulsarp->vertex >= ww.numsegs)
							pulsarp->vertex = 0;
					} else if (pulsarp->vertex >= ww.numsegs) {
						pulsarp->vertex = (int16)(ww.numsegs - 1);
						pulsarp->direction = (int16)-pulsarp->direction;
						doskip = 0;
						if (LEVELI(pulsarp) == 0)
							Hero.lanestat[pulsarp->vertex] |= CapPuls;
					}
					if (doskip)
						PulsarSegSkip(pulsarp);
				} else {
					pulsarp->rotation = (int16)(pulsarp->rotation + y);
					y = 0;
				}
			} else {
				if (y <= dy) {
					y = 0;
					pulsarp->pulscounter = PULSINGTIME;
					pulsarp->rotation = (int16)(pulsarp->rotation + dy);
					pulsarp->vertex--;
					doskip = 1;
					if (ww.wraps) {
						if (pulsarp->vertex < 0)
							pulsarp->vertex = (int16)(ww.numsegs - 1);
					} else if (pulsarp->vertex == 0) {
						pulsarp->vertex = 1;
						pulsarp->direction = (int16)-pulsarp->direction;
						doskip = 0;
						if (LEVELI(pulsarp) == 0)
							Hero.lanestat[0] |= CapPuls;
					}
					if (doskip)
						PulsarSegSkip(pulsarp);
				} else {
					pulsarp->rotation = (int16)(pulsarp->rotation + y);
					y = 0;
				}
			}
			if (pulsarp->rotation < 0)
				pulsarp->rotation = (int16)(pulsarp->rotation + ANGLES);
		}
	}

	pulsarp->lane = LaneAt(pulsarp->vertex, pulsarp->rotation);

	/* Pulsing: in the middle of the pulse the lane is deadly. */
	if (pulsarp->pulscounter) {
		if (ThisLevel.puPulsDepth > LEVELI(pulsarp)) {
			pulsarp->pulscounter--;
			if (pulsarp->pulscounter < PULSINGTIME * 3 / 4 && pulsarp->pulscounter > PULSINGTIME * 3 / 10)
				Hero.lanestat[pulsarp->lane] |= CapPuls;
			if (PulsarPower >= 13 && pulsarp->pulscounter < NERVOUSNESS)
				pulsarp->pulscounter = 0;
		} else {
			pulsarp->pulscounter = 0;
		}
	}

	PulsarGeometry(pulsarp, &x, &y, &dx, &dy);

	/* A pulsar on the rim tells the claw. */
	if (LEVELI(pulsarp) == 0)
		Hero.segmstat[pulsarp->vertex] |= CapPuls;

	if (ShotHitTest(pulsarp->lane, LEVELI(pulsarp))) {
		sc = ww.unitlen[LEVELI(pulsarp)];       /* explode into two parts */
		AddCrack(x, y, (int16)(-dx >> 2), (int16)(-dy >> 2), PULSARCOLOR, sc, PulsarCrack,
		         pulsarp->rotation, (int16)(-2 * pulsarp->direction), 10);
		ThisLevel.puCount--;
		ThisLevel.totalCount--;
		SOUND(SND_SPLITTER, 13);
		IncreaseScore(ThisLevel.puPoints);
		pulsarp->state = inactive;
		ActivePulsars--;
	}
}

SEG_ENEMY void UpdatePulsars(void)
{
	Pulsar *psp = Pulsars;
	int16 i;

	PulsarPower = (int16)(game_steps & 31);
	if (ActivePulsars && PulsarPower == 30)
		SOUND(SND_PHAZER2, 9);
	if (PulsarPower > 15)
		PulsarPower = (int16)(31 - PulsarPower);

	for (i = 0; i < MAXPULSARS; i++, psp++) {
		if (psp->state < 0) {
			psp->vertex = (int16)(-(psp->state + 1) / STARDIVISION);
			psp->state = active;
			if (!ww.wraps && psp->vertex == 0)
				psp->vertex = 1;
			if (psp->vertex > ww.numsegs)
				psp->vertex = ww.numsegs;
			if (psp->direction < 0)
				psp->rotation = NextSeg[psp->vertex];
			else
				psp->rotation = PrevSeg[psp->vertex];
		} else if (psp->state == active) {
			UpdateActivePulsar(psp);
		}
	}

	if (EnemySpawning && ActivePulsars < MAXPULSARS && ThisLevel.puCount > ActivePulsars) {
		if ((int32)(uint16)va_random() < (int32)ThisLevel.puProb + ThisLevel.probIncrease)
			CreateNewPulsar();
	}
	ThisLevel.starCount = (int16)(ThisLevel.starCount + ThisLevel.puCount - ActivePulsars);
	ThisLevel.activeCount = (int16)(ThisLevel.activeCount + ActivePulsars);
}

SEG_ENEMY void DrawPulsars(void)
{
	Pulsar *psp = Pulsars;
	int16 i, x, y, dx, dy;

	for (i = 0; i < MAXPULSARS; i++, psp++) {
		if (psp->state != active)
			continue;
		PulsarGeometry(psp, &x, &y, &dx, &dy);
		if (psp->mirror)
			DrawPulsar((int16)(x + dx), (int16)(y + dy), (int16)-dx, (int16)-dy, PulsarPower);
		else
			DrawPulsar(x, y, dx, dy, PulsarPower);
	}
}

SEG_ENEMY void InitPulsars(void)
{
	int16 i;

	for (i = 0; i < MAXPULSARS; i++)
		Pulsars[i].state = inactive;
	ActivePulsars = 0;
}

SEG_ENEMY int16 ListPulsars(EnemyInfo *out, int16 max)
{
	int16 i, n = 0;

	for (i = 0; i < MAXPULSARS && n < max; i++)
		if (Pulsars[i].state == active) {
			out[n].kind = EnemyPulsar;
			out[n].lane = Pulsars[i].lane;
			out[n].level = LEVELI(&Pulsars[i]);
			n++;
		}
	return n;
}
