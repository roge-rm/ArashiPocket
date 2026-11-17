/* The flippers, from STFlipper.c by Juri Munkki. A flipper climbs the tube
   hanging from one corner (vertex) of the lanes and turns round it, grabbing
   the next corner when its arm reaches the angle of a lane edge. On the rim
   it keeps flipping and catches the claw. Same numbers as the original; the
   level is 16.16 (the original's IFixed, whose int half is the top word).
   Update moves and tests hits, Draw queues the picture. The pieces a dead
   flipper breaks into (STCrack) are a bang for now. */
#include "storm.h"
#include "qdrandom.h"

#define MAXFLIPPERS 20
#define FLIPPERCOLOR (ThisLevel.flColor)
#define FLIPPERROTSPEED (ThisLevel.flRot)
#define FLIPPERUPSPEED (ThisLevel.flSpeed)

#define LEVELI(f) ((int16)((f)->level >> 16))

typedef struct {
	int16 state;        /* star 0 (waiting, then -1 minus the arrival), inactive 1, active 2 */
	int16 lane;         /* actual lane number */
	int32 level;        /* 16.16, 0 at the rim to DEPTH at the far end */
	int16 vertex;       /* vertex currently connected to */
	int16 mirror;       /* draw the mirror image? */
	int16 rotation;     /* current rotation of the flipper */
	int16 direction;    /* rotation direction */
} Flipper;

enum { star, inactive, active };

static Flipper Flippers[MAXFLIPPERS];
static int16 ActiveFlippers;

SEG_ENEMY static void DrawFlipper(int16 cx, int16 cy, int16 dx, int16 dy);
SEG_ENEMY static void SegSkip(Flipper *theflip);
SEG_ENEMY static void UpdateActiveFlipper(Flipper *theflip);
SEG_ENEMY static void FlipperGeometry(const Flipper *theflip, int16 *x, int16 *y, int16 *dx, int16 *dy);
SEG_ENEMY static Flipper *FreeFlipper(void);

/* Given a connecting point (cx, cy) and a vector (dx, dy) a flipper shape
   is drawn. */
SEG_ENEMY static void DrawFlipper(int16 cx, int16 cy, int16 dx, int16 dy)
{
	int16 x1, y1, x2, y2;
	int16 dx4, dx8, dy4, dy8;

	VA.color = FLIPPERCOLOR;
	dx4 = (int16)(dx >> 2);     dy4 = (int16)(dy >> 2);
	dx8 = (int16)(dx >> 3);     dy8 = (int16)(dy >> 3);
	x1 = (int16)(cx + dx);      y1 = (int16)(cy + dy);

	VAMoveTo(cx, cy);
	x2 = (int16)(dx8 - dy4);    y2 = (int16)(dy8 + dx4);
	VALineTo((int16)(x1 - x2), (int16)(y1 - y2));
	x2 = (int16)(dx4 - dy8);    y2 = (int16)(dy4 + dx8);
	VALineTo((int16)(x1 - x2), (int16)(y1 - y2));
	VALineTo(x1, y1);

	x2 = (int16)(dx8 + dy4);    y2 = (int16)(dy8 - dx4);
	VALineTo((int16)(cx + x2), (int16)(cy + y2));
	x2 = (int16)(dx4 + dy8);    y2 = (int16)(dy4 - dx8);
	VALineTo((int16)(cx + x2), (int16)(cy + y2));
	VALineTo(cx, cy);
}

SEG_ENEMY static Flipper *FreeFlipper(void)
{
	Flipper *theflip = Flippers;

	while (theflip->state != inactive)
		theflip++;
	return theflip;
}

/* A newborn flipper: a star at the centre first. The lane or vertex is
   decided when the star reaches the tube. */
SEG_ENEMY void CreateNewFlipper(void)
{
	Flipper *theflip;

	if (ActiveFlippers < MAXFLIPPERS) {
		theflip = FreeFlipper();
		theflip->state = star;
		theflip->lane = 0;
		theflip->level = (int32)DEPTH << 16;
		theflip->mirror = 0;
		theflip->direction = (int16)(qd_random() < 0 ? FLIPPERROTSPEED : -FLIPPERROTSPEED);
		StarApproach(&theflip->state, FLIPPERCOLOR);
		ActiveFlippers++;
	}
}

/* Two flippers from a destroyed tanker. */
SEG_ENEMY void CreateSplitFlippers(int16 lane, int16 level)
{
	int16 i, delta, nextseg, prevseg;
	Flipper *theflip;

	for (i = 0; i < 2; i++) {
		if (ActiveFlippers < MAXFLIPPERS) {
			theflip = FreeFlipper();
			theflip->vertex = (int16)(lane + i);
			if (theflip->vertex >= ww.numsegs)
				theflip->vertex = (int16)(theflip->vertex - ww.numsegs);
			theflip->state = active;
			theflip->level = (int32)level << 16;
			theflip->mirror = 0;
			theflip->direction = (int16)(i ? FLIPPERROTSPEED : -FLIPPERROTSPEED);

			nextseg = NextSeg[theflip->vertex];
			prevseg = PrevSeg[theflip->vertex];
			delta = (int16)(nextseg - prevseg);
			if (delta < 0)
				delta = (int16)(delta + ANGLES);

			if (theflip->direction < 0)
				theflip->rotation = (int16)(NextSeg[theflip->vertex] - delta / 2);
			else
				theflip->rotation = (int16)(PrevSeg[theflip->vertex] + delta / 2);
			theflip->lane = LaneSel[theflip->vertex][theflip->rotation];
			ActiveFlippers++;
		}
	}
}

/* A flipper rotates from one vertex to another. It grabs the vertex while
   rotating, so at times it changes the way it touches; mirror says how to
   draw it then. */
SEG_ENEMY static void SegSkip(Flipper *theflip)
{
	theflip->rotation = (int16)(theflip->rotation + ANGLES / 2);
	if (theflip->rotation >= ANGLES)
		theflip->rotation = (int16)(theflip->rotation - ANGLES);
	theflip->mirror = !theflip->mirror;
	if (LEVELI(theflip) == 0)
		Hero.lanestat[(int16)LaneSel[theflip->vertex][theflip->rotation]] |= CapFlip;
}

/* Position and arm of the flipper on screen. */
SEG_ENEMY static void FlipperGeometry(const Flipper *theflip, int16 *x, int16 *y, int16 *dx, int16 *dy)
{
	int16 lv = LEVELI(theflip);
	int16 l = ww.unitlen[lv];

	*dy = (int16)((l * (int32)-Sins[theflip->rotation]) >> 8);
	*dx = (int16)((l * (int32)Cosins[theflip->rotation]) >> 8);
	*x = ww.x[theflip->vertex][lv];
	*y = ww.y[theflip->vertex][lv];
}

/* An active flipper has gone through the star phase and now rotates and
   climbs up. */
SEG_ENEMY static void UpdateActiveFlipper(Flipper *theflip)
{
	int16 dx, dy, x, y;
	int16 doskip;

	theflip->level -= FLIPPERUPSPEED;           /* move up a bit */
	if (LEVELI(theflip) <= 0) {                 /* don't move past the top */
		theflip->level &= 0xFFFFL;
		ThisLevel.edgeCount++;                  /* on the edge */
	}

	y = theflip->direction;
	while (y) {                                 /* while there is rotation to do */
		x = theflip->rotation;
		dx = (int16)(NextSeg[theflip->vertex] - x);     /* next clockwise vertex angle */
		dy = (int16)(PrevSeg[theflip->vertex] - x);     /* next counter-cw vertex angle */
		/* These angles are relative. A negative one is a flag that there is
		   no next vertex. */
		if (dx < -ANGLES)
			dx = (int16)(y + y);
		if (dy < -ANGLES)
			dy = (int16)(y + y);
		if (y > 0) {                            /* clockwise */
			if (dx <= 0)
				dx = (int16)(dx + ANGLES);      /* force the angle between 1 and ANGLES */
			if (dy <= 0)
				dy = (int16)(dy + ANGLES);
			if (dx < dy) {                      /* which vertex is next? */
				if (y >= dx) {                  /* can we reach the next clockwise vertex? */
					y = (int16)(y - dx);
					theflip->rotation = (int16)(theflip->rotation + dx);
					theflip->vertex++;
					doskip = 1;
					if (ww.wraps) {
						if (theflip->vertex >= ww.numsegs)
							theflip->vertex = 0;
					} else {
						if (theflip->vertex >= ww.numsegs) {
							theflip->vertex = (int16)(ww.numsegs - 1);
							theflip->direction = (int16)-theflip->direction;
							y = (int16)-y;
							doskip = 0;
							if (LEVELI(theflip) == 0)
								Hero.lanestat[theflip->vertex] |= CapFlip;
						}
					}
					if (doskip)
						SegSkip(theflip);
				} else {                        /* no more rotating to do */
					theflip->rotation = (int16)(theflip->rotation + y);
					y = 0;
				}
			} else {                            /* the same the other way */
				if (y >= dy) {
					y = (int16)(y - dy);
					theflip->rotation = (int16)(theflip->rotation + dy);
					theflip->vertex--;
					doskip = 1;
					if (ww.wraps) {
						if (theflip->vertex < 0)
							theflip->vertex = (int16)(ww.numsegs - 1);
					} else {
						if (theflip->vertex == 0) {
							theflip->vertex = 1;
							theflip->direction = (int16)-theflip->direction;
							y = (int16)-y;
							doskip = 0;
							if (LEVELI(theflip) == 0)
								Hero.lanestat[0] |= CapFlip;
						}
					}
					if (doskip)
						SegSkip(theflip);
				} else {
					theflip->rotation = (int16)(theflip->rotation + y);
					y = 0;
				}
			}
			if (theflip->rotation >= ANGLES)
				theflip->rotation = (int16)(theflip->rotation - ANGLES);
		} else {                                /* counter-clockwise */
			if (dx >= 0)
				dx = (int16)(dx - ANGLES);
			if (dy >= 0)
				dy = (int16)(dy - ANGLES);
			if (dx > dy) {
				if (y <= dx) {
					y = (int16)(y - dx);
					theflip->rotation = (int16)(theflip->rotation + dx);
					theflip->vertex++;
					doskip = 1;
					if (ww.wraps) {
						if (theflip->vertex >= ww.numsegs)
							theflip->vertex = 0;
					} else {
						if (theflip->vertex >= ww.numsegs) {
							theflip->vertex = (int16)(ww.numsegs - 1);
							theflip->direction = (int16)-theflip->direction;
							y = (int16)-y;
							doskip = 0;
							if (LEVELI(theflip) == 0)
								Hero.lanestat[theflip->vertex] |= CapFlip;
						}
					}
					if (doskip)
						SegSkip(theflip);
				} else {
					theflip->rotation = (int16)(theflip->rotation + y);
					y = 0;
				}
			} else {
				if (y <= dy) {
					y = (int16)(y - dy);
					theflip->rotation = (int16)(theflip->rotation + dy);
					theflip->vertex--;
					doskip = 1;
					if (ww.wraps) {
						if (theflip->vertex < 0)
							theflip->vertex = (int16)(ww.numsegs - 1);
					} else {
						if (theflip->vertex == 0) {
							theflip->vertex = 1;
							theflip->direction = (int16)-theflip->direction;
							y = (int16)-y;
							doskip = 0;
							if (LEVELI(theflip) == 0)
								Hero.lanestat[0] |= CapFlip;
						}
					}
					if (doskip)
						SegSkip(theflip);
				} else {
					theflip->rotation = (int16)(theflip->rotation + y);
					y = 0;
				}
			}
			if (theflip->rotation < 0)
				theflip->rotation = (int16)(theflip->rotation + ANGLES);
		}
	}

	/* The lane we belong to depends on the vertex we hang from and the angle. */
	theflip->lane = LaneSel[theflip->vertex][theflip->rotation];

	FlipperGeometry(theflip, &x, &y, &dx, &dy);

	/* A flipper on the rim tells the claw. */
	if (LEVELI(theflip) == 0)
		Hero.segmstat[theflip->vertex] |= CapFlip;

	/* Has the player hit us? */
	if (ShotHitTest(theflip->lane, LEVELI(theflip))) {
		ExplodeEnemy((int16)(x + (dx >> 1)), (int16)(y + (dy >> 1)), LEVELI(theflip), FLIPPERCOLOR);
		ThisLevel.flCount--;
		ThisLevel.totalCount--;
		IncreaseScore(ThisLevel.flPoints);
		theflip->state = inactive;
		ActiveFlippers--;
	}
}

/* Promotes the stars that have reached the tube to flippers and moves the
   rest, then maybe starts another. */
SEG_ENEMY void UpdateFlippers(void)
{
	Flipper *flp = Flippers;
	int16 i;

	for (i = 0; i < MAXFLIPPERS; i++) {
		if (flp->state < 0) {
			flp->vertex = (int16)(-(flp->state + 1) / STARDIVISION);
			flp->state = active;
			if (!ww.wraps && flp->vertex == 0)
				flp->vertex = 1;
			if (flp->vertex > ww.numsegs)
				flp->vertex = ww.numsegs;
			if (flp->direction < 0)
				flp->rotation = NextSeg[flp->vertex];
			else
				flp->rotation = PrevSeg[flp->vertex];
		} else {
			if (flp->state == active)
				UpdateActiveFlipper(flp);
		}
		flp++;
	}

	if (EnemySpawning && ActiveFlippers < MAXFLIPPERS && ThisLevel.flCount > ActiveFlippers) {
		if ((int32)(uint16)qd_random() < (int32)ThisLevel.flProb + ThisLevel.probIncrease)
			CreateNewFlipper();
	}

	ThisLevel.starCount = (int16)(ThisLevel.starCount + ThisLevel.flCount - ActiveFlippers);
	ThisLevel.activeCount = (int16)(ThisLevel.activeCount + ActiveFlippers);
}

SEG_ENEMY void DrawFlippers(void)
{
	Flipper *flp = Flippers;
	int16 i, x, y, dx, dy;

	for (i = 0; i < MAXFLIPPERS; i++, flp++) {
		if (flp->state != active)
			continue;
		FlipperGeometry(flp, &x, &y, &dx, &dy);
		if (flp->mirror)
			DrawFlipper((int16)(x + dx), (int16)(y + dy), (int16)-dx, (int16)-dy);
		else
			DrawFlipper(x, y, dx, dy);
	}
}

SEG_ENEMY void InitFlippers(void)
{
	int16 i;

	for (i = 0; i < MAXFLIPPERS; i++)
		Flippers[i].state = inactive;
	ActiveFlippers = 0;
}

SEG_ENEMY int16 ListFlippers(EnemyInfo *out, int16 max)
{
	int16 i, n = 0;

	for (i = 0; i < MAXFLIPPERS && n < max; i++)
		if (Flippers[i].state == active) {
			out[n].kind = EnemyFlipper;
			out[n].lane = Flippers[i].lane;
			out[n].level = LEVELI(&Flippers[i]);
			n++;
		}
	return n;
}
