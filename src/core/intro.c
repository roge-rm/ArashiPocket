/* The level intro, from CreateFit and MakeSpots in GridTest.c and
   VAStarfield.c by Juri Munkki: stars rush out of the centre while the
   level number and "Superzapper Recharge" show, then the field flies in
   from far away and fades up. The original ran this inside CreateFit, one
   VAStep per StarStep or FieldRide; here each of those is one game step
   (intro_step), so the step count, the FastRandom calls and the frame
   counter come out as the original's. The grid itself is already built by
   CreateFit when the intro starts. */
#include "storm.h"
#include "qdrandom.h"

#define MAXSTARS     75
#define NEWNUMBER    2
#define STARSPEED    5
#define FIELDADVANCE 8
#define FIELDSTART   (2 * DEPTH)

typedef struct {
	int16 x, y, z;
} MegaStar;

typedef struct {
	int16 h, v;
} TwistPt;

/* The intro's stages, in the original's order. */
enum {
	InFeed,         /* StarSteps of CreateFit and MakeSpots' first loop */
	InOut,          /* MakeSpots' second loop: stars only, while there are any */
	InRide,         /* the field coming in, stars for the first 2n steps */
	InLast,         /* the field at distance 0 */
	InField,        /* the field drawn in place */
	InDone
};

static MegaStar *stars;         /* MAXSTARS, then the spots drawn this step */
static int16 *spots;            /* x, y, colour for each star drawn */
static int16 StarsUsed, nspots;
static TwistPt StarTwist, OldStarTwist, NewStarTwist;
static int16 Twister;
static int16 FieldDistance;
static int16 ride_dist;         /* where the field was drawn this step, or -1 */
static int16 stage, count, segs, feeds, outs;
static cbool zapmsg, texton;

SEG_GAME static void FeedStars(void);
SEG_GAME static void RideStars(void);
SEG_GAME static void StarColours(void);
SEG_GAME static void FieldRide(void);
SEG_GAME static void DrawZapRecharge(void);

/* OpenStars: colours 0 to 6 fade from the level's colour 8 down to black. */
SEG_GAME static void StarColours(void)
{
	uint8 buf[NUMCOLORS * 8];
	uint32 n = 0, i;
	int16 id = ThisLevel.lvColor, c, k;
	const uint8 *clot;

	if (id == 5)
		id = (int16)(id + 128);     /* the grey for invisible levels */
	clot = plat_resource(FOURCC('C', 'L', 'O', 'T'), id, &n);
	if (!clot)
		return;
	if (n > sizeof(buf))
		n = sizeof(buf);
	for (i = 0; i < n; i++)
		buf[i] = clot[i];
	plat_release(clot);
	if (n >= 72)
		for (c = 0; c < 7; c++)
			for (k = 2; k < 8; k += 2) {
				uint32 v = ((uint32)buf[64 + k] << 8 | buf[64 + k + 1]) * (uint32)(7 - c) / 7;

				buf[c * 8 + k] = (uint8)(v >> 8);
				buf[c * 8 + k + 1] = (uint8)v;
			}
	VASetColors(buf, n);
}

SEG_GAME void intro_begin(void)
{
	if (!stars) {
		stars = (MegaStar *)plat_alloc((uint32)MAXSTARS * (sizeof(MegaStar) + 3 * sizeof(int16)));
		if (stars)
			spots = (int16 *)(stars + MAXSTARS);
	}
	StarColours();
	StarTwist.h = StarTwist.v = 0;
	StarsUsed = 0;
	nspots = 0;
	ride_dist = -1;
	segs = gridOutline.numsegs;
	/* CreateFit's two shape walks, its lane table walk (a StarStep every 8
	   corners), three more, and four in MakeSpots. */
	feeds = (int16)(2 * segs + segs / 8 + 1 + 3 + 4);
	outs = 0;
	count = 0;
	stage = InFeed;
	zapmsg = (cbool)((Hero.Flags & HF_SHOWZAPMSG) != 0);
	texton = 1;
}

SEG_GAME cbool intro_running(void)
{
	return (cbool)(stage != InDone);
}

SEG_GAME void intro_stop(void)
{
	if (stars)
		plat_free(stars);
	stars = NULL;
	spots = NULL;
	StarsUsed = 0;
	nspots = 0;
	stage = InDone;
}

/* FeedStars: two new stars a step, anywhere over the screen, far away. */
SEG_GAME static void FeedStars(void)
{
	int16 i, w = VA.frame.right, h = VA.frame.bottom;
	MegaStar *sp;

	if (!stars || StarsUsed >= MAXSTARS)
		return;
	i = (int16)(MAXSTARS - StarsUsed < NEWNUMBER ? MAXSTARS - StarsUsed : NEWNUMBER);
	sp = stars + StarsUsed;
	StarsUsed = (int16)(StarsUsed + i);
	while (i--) {
		sp->x = (int16)((uint16)fast_random() % (uint16)w - (w >> 1) + StarTwist.h);
		sp->y = (int16)((uint16)fast_random() % (uint16)h - (h >> 1) + StarTwist.v);
		sp->z = DEPTH + STARTDEPTH;
		sp++;
	}
}

/* RideStars: each star comes STARSPEED closer; one off the screen or too
   close is dropped. Where each is drawn this step goes in spots. */
SEG_GAME static void RideStars(void)
{
	int16 i, x, y, mx, my;
	MegaStar *sp;

	if (StarTwist.h != NewStarTwist.h || StarTwist.v != NewStarTwist.v) {
		Twister++;
		StarTwist.h = (int16)(OldStarTwist.h + (((int32)(NewStarTwist.h - OldStarTwist.h) * Twister) >> 5));
		StarTwist.v = (int16)(OldStarTwist.v + (((int32)(NewStarTwist.v - OldStarTwist.v) * Twister) >> 5));
	} else {
		OldStarTwist = StarTwist;
		Twister = 0;
	}
	nspots = 0;
	if (!stars)
		return;
	mx = (int16)((VA.frame.right >> 1) + StarTwist.h);
	my = (int16)((VA.frame.bottom >> 1) + StarTwist.v);
	sp = stars;
	for (i = 0; i < StarsUsed; i++) {
		x = (int16)((int32)(sp->x - StarTwist.h) * STARTDEPTH / sp->z + mx);
		y = (int16)((int32)(sp->y - StarTwist.v) * STARTDEPTH / sp->z + my);
		if (x <= 0 || x >= VA.frame.right || y <= 0 || y >= VA.frame.bottom || sp->z <= STARSPEED) {
			*sp = stars[--StarsUsed];
			sp--;
			i--;
		} else {
			spots[nspots * 3] = x;
			spots[nspots * 3 + 1] = y;
			spots[nspots * 3 + 2] = (int16)(sp->z < STARTDEPTH * 2 ? 0 :
			                                (sp->z - STARTDEPTH) * 7 / (DEPTH - 1));
			nspots++;
			sp->z = (int16)(sp->z - STARSPEED);
		}
		sp++;
	}
}

/* FieldRide: the field is drawn at FieldDistance, then comes closer. */
SEG_GAME static void FieldRide(void)
{
	ride_dist = FieldDistance;
	FieldDistance = (int16)(FieldDistance - FIELDADVANCE);
	if (FieldDistance < 0)
		FieldDistance = 0;
}

/* One step of the intro. Returns 1 when it has finished (the step that
   finishes it is the last frame of the intro). */
SEG_GAME cbool intro_step(void)
{
	ride_dist = -1;
	switch (stage) {
	case InFeed:
		/* NewStarTwist is set after CreateFit's first walk of the shape. */
		if (count == segs) {
			NewStarTwist.h = gridOutline.xoff;
			NewStarTwist.v = gridOutline.yoff;
		}
		/* The text is erased after the StarStep that follows the lane
		   table walk. */
		texton = (cbool)(count < 2 * segs + segs / 8 + 2);
		FeedStars();
		RideStars();
		if (++count >= feeds) {
			stage = InOut;
			outs = 0;
		}
		return 0;
	case InOut:
		/* Depths -16 to 112 in 16s: nine chances, a frame only while
		   stars are left. */
		while (outs < 9) {
			outs++;
			if (StarsUsed) {
				RideStars();
				if (StarsUsed)
					return 0;
			}
		}
		nspots = 0;
		stage = InRide;
		FieldDistance = FIELDSTART;
		count = 0;
		/* fall through */
	case InRide:
		FieldRide();
		if (count < 2 * segs && StarsUsed)
			RideStars();
		else
			nspots = 0;
		count++;
		if (FieldDistance == 0)
			stage = InLast;
		return 0;
	case InLast:
		nspots = 0;
		FieldRide();
		stage = InField;
		return 0;
	case InField:
		VALoadColors(ThisLevel.lvColor);
		intro_stop();
		return 1;
	default:
		return 1;
	}
}

/* DoZapRecharge: the message along the bottom when the claw's superzapper
   is charged again. */
SEG_GAME static void DrawZapRecharge(void)
{
	int16 s = (int16)((VA.frame.right * 9 / 10 / 20 - 3) / 3);

	if (s < 1)
		s = 1;
	VA.segmscale = s;
	VAMoveTo((int16)((VA.frame.right - 20 * (s * 3 + 3)) / 2), (int16)(VA.frame.bottom - 2));
	VA.color = BG1;
	VADrawText("Superzapper Recharge", 0, 20);
}

SEG_GAME void intro_draw(void)
{
	int16 i;

	if (stage == InDone)
		return;
	for (i = 0; i < nspots; i++) {
		VA.color = spots[i * 3 + 2];
		VASafeSpot(spots[i * 3], spots[i * 3 + 1]);
	}
	if (ride_dist >= 0) {
		int32 d0 = STARTDEPTH + ride_dist, d1 = STARTDEPTH + DEPTH + ride_dist;
		int16 xc = (int16)((VA.frame.right + VA.frame.left) / 2);
		int16 yc = (int16)((VA.frame.bottom + VA.frame.top) / 2);
		int16 xo = gridOutline.xoff, yo = gridOutline.yoff;
		int16 x[MAXSEGS], y[MAXSEGS], xb[MAXSEGS], yb[MAXSEGS];

		if (ride_dist < STARTDEPTH * 2)
			VA.color = 0;
		else
			VA.color = (int16)((ride_dist - STARTDEPTH) * 7 / (FIELDSTART - STARTDEPTH - 1));
		if (VA.color > 6)
			VA.color = 6;
		for (i = 0; i <= segs; i++) {
			int32 dx = (int32)gridOutline.x[i] - xo, dy = (int32)gridOutline.y[i] - yo;

			x[i] = (int16)(dx * STARTDEPTH / d0 + xc + xo);
			y[i] = (int16)(dy * STARTDEPTH / d0 + yc + yo);
			xb[i] = (int16)(dx * STARTDEPTH / d1 + xc + xo);
			yb[i] = (int16)(dy * STARTDEPTH / d1 + yc + yo);
			VAMoveTo(x[i], y[i]);
			VASafeLineTo(xb[i], yb[i]);
		}
		VAMoveTo(x[0], y[0]);
		for (i = 1; i <= segs; i++)
			VASafeLineTo(x[i], y[i]);
		VAMoveTo(xb[0], yb[0]);
		for (i = 1; i <= segs; i++)
			VASafeLineTo(xb[i], yb[i]);
	}
	if (stage == InField)
		DrawField();
	if (zapmsg && texton && stage == InFeed)
		DrawZapRecharge();
}
