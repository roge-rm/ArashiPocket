/* The pieces things break into, from STCrack.c and the BlowUp calls of
   Shapes.c by Juri Munkki. A piece moves in a straight line, turns, and is
   drawn as one half of a flipper, one half of the claw, a pulsar, or a
   single line, for count steps. UpdateCracks moves them and DrawCracks
   queues the picture. */
#include "storm.h"

#define MAXCRACKS 60

typedef struct {
	int16 x, y;
	int16 xv, yv;
	int16 size;
	int16 color;
	int16 partcode;
	int16 rotation;
	int16 rotspeed;
	int16 count;
} Crack;

static Crack Cracks[MAXCRACKS];
static int16 numcracks;

/* Where BlowUpLine breaks things from, and how fast and long the pieces go. */
static int16 BlowCenterX, BlowCenterY;
static int16 RotSpeed;
static int16 BlowSize;

SEG_ENEMY static void PlayerHalf1(int16 cx, int16 cy, int16 dx, int16 dy);
SEG_ENEMY static void PlayerHalf2(int16 cx, int16 cy, int16 dx, int16 dy);
SEG_ENEMY static void FlipperHalf1(int16 cx, int16 cy, int16 dx, int16 dy);
SEG_ENEMY static void BlowUpHalfLine(int16 x, int16 y);
SEG_ENEMY static int16 SquareRoot(int32 n);
SEG_ENEMY static int16 AngleOf(int16 h, int16 v);

SEG_ENEMY static void PlayerHalf1(int16 cx, int16 cy, int16 dx, int16 dy)
{
	int16 x2, y2, x4, y4, x8, y8;

	x4 = (int16)(dx >> 2);      y4 = (int16)(dy >> 2);
	x8 = (int16)(dx >> 3);      y8 = (int16)(dy >> 3);

	VAMoveTo(cx, cy);
	x2 = (int16)((dx >> 1) + y4);   y2 = (int16)((dy >> 1) - x4);
	VASafeLineTo((int16)(cx + x2), (int16)(cy + y2));

	x2 = (int16)((dx >> 1) + y8);   y2 = (int16)((dy >> 1) - x8);
	VASafeLineTo((int16)(cx + x2), (int16)(cy + y2));
	VASafeLineTo((int16)(cx + x8), (int16)(cy + y8));

	x2 = (int16)(x4 - y4);          y2 = (int16)(y4 + x4);
	VASafeLineTo((int16)(cx + x2), (int16)(cy + y2));
	VASafeLineTo(cx, cy);
}

SEG_ENEMY static void PlayerHalf2(int16 cx, int16 cy, int16 dx, int16 dy)
{
	int16 x2, y2, x4, y4, x8, y8;

	x4 = (int16)(dx >> 2);      y4 = (int16)(dy >> 2);
	x8 = (int16)(dx >> 3);      y8 = (int16)(dy >> 3);

	VAMoveTo(cx, cy);
	x2 = (int16)((dx >> 1) + y4);   y2 = (int16)((dy >> 1) - x4);
	VASafeLineTo((int16)(cx - x2), (int16)(cy + y2));

	x2 = (int16)((dx >> 1) + y8);   y2 = (int16)((dy >> 1) - x8);
	VASafeLineTo((int16)(cx - x2), (int16)(cy + y2));
	VASafeLineTo((int16)(cx - x8), (int16)(cy + y8));

	x2 = (int16)(x4 - y4);          y2 = (int16)(y4 + x4);
	VASafeLineTo((int16)(cx - x2), (int16)(cy + y2));
	VASafeLineTo(cx, cy);
}

SEG_ENEMY static void FlipperHalf1(int16 cx, int16 cy, int16 dx, int16 dy)
{
	int16 x2, y2, dx4, dx8, dy4, dy8;

	VA.color = 2;
	dx4 = (int16)(dx >> 2);     dy4 = (int16)(dy >> 2);
	dx8 = (int16)(dx >> 3);     dy8 = (int16)(dy >> 3);

	VAMoveTo(cx, cy);
	x2 = (int16)(dx / 2 + dy8);     y2 = (int16)(dy / 2 - dx8);
	VASafeLineTo((int16)(cx + x2), (int16)(cy + y2));
	x2 = (int16)(dx8 + dy4);        y2 = (int16)(dy8 - dx4);
	VASafeLineTo((int16)(cx + x2), (int16)(cy + y2));
	x2 = (int16)(dx4 + dy8);        y2 = (int16)(dy4 - dx8);
	VASafeLineTo((int16)(cx + x2), (int16)(cy + y2));
	VASafeLineTo(cx, cy);
}

SEG_ENEMY void InitCracks(void)
{
	numcracks = 0;
}

SEG_ENEMY int16 CrackCount(void)
{
	return numcracks;
}

SEG_ENEMY void AddCrack(int16 x, int16 y, int16 xv, int16 yv, int16 color, int16 unitsize,
                        int16 part, int16 rot, int16 rotspeed, int16 count)
{
	Crack *cp;

	if (numcracks < MAXCRACKS) {
		cp = Cracks + numcracks++;
		cp->x = x;
		cp->y = y;
		cp->xv = xv;
		cp->yv = yv;
		cp->color = color;
		cp->partcode = part;
		cp->rotation = rot;
		cp->rotspeed = rotspeed;
		cp->size = unitsize;
		cp->count = count;
	}
}

/* A piece that ran out last step goes now, so it is drawn on its last one. */
SEG_ENEMY void UpdateCracks(void)
{
	int16 i;
	Crack *cp = Cracks;

	for (i = 0; i < numcracks; i++) {
		if (cp->count <= 0) {
			*cp = Cracks[--numcracks];
			i--;
			continue;
		}
		cp->x = (int16)(cp->x + cp->xv);
		cp->y = (int16)(cp->y + cp->yv);
		cp->rotation = (int16)(cp->rotation + cp->rotspeed);
		while (cp->rotation >= ANGLES)
			cp->rotation = (int16)(cp->rotation - ANGLES);
		while (cp->rotation < 0)
			cp->rotation = (int16)(cp->rotation + ANGLES);
		cp->count--;
		cp++;
	}
}

SEG_ENEMY void DrawCracks(void)
{
	int16 i, dx, dy;
	const Crack *cp = Cracks;

	for (i = 0; i < numcracks; i++, cp++) {
		dx = (int16)(((int32)cp->size * Cosins[cp->rotation]) >> 8);
		dy = (int16)(((int32)cp->size * -Sins[cp->rotation]) >> 8);
		VA.color = cp->color;

		switch (cp->partcode) {
		case FlipperLeft:
			FlipperHalf1(cp->x, cp->y, dx, dy);
			break;
		case FlipperRight:      /* the original draws both halves this way */
			FlipperHalf1(cp->x, cp->y, dx, dy);
			break;
		case PlayerLeft:
			PlayerHalf1(cp->x, cp->y, dx, dy);
			break;
		case PlayerRight:
			PlayerHalf2(cp->x, cp->y, dx, dy);
			break;
		case PulsarCrack:
			DrawPulsar(cp->x, cp->y, dx, dy, (int16)(30 - cp->count * 2));
			break;
		case FlyingVector:
			dx = (int16)(dx >> 1);
			dy = (int16)(dy >> 1);
			VAMoveTo((int16)(cp->x - dx), (int16)(cp->y - dy));
			VASafeLineTo((int16)(cp->x + dx), (int16)(cp->y + dy));
			break;
		}
	}
}

SEG_ENEMY void CrackPlayer(void)
{
	int16 angle, delta, x, y, x2, y2, dx, dy;

	x = ww.x[Hero.lane][0];
	y = ww.y[Hero.lane][0];
	x2 = ww.x[Hero.lane + 1][0];
	y2 = ww.y[Hero.lane + 1][0];
	dx = (int16)(x - x2);
	dy = (int16)(y - y2);

	delta = (int16)(ww.unitlen[0] * 4 / 3);
	angle = NextSeg[Hero.lane];

	AddCrack(x, y, (int16)((dx >> 3) - (dy >> 2)), (int16)((dy >> 3) + (dx >> 2)),
	         ThisLevel.shColor[0], delta, PlayerLeft, angle, 4, POSTMORTEMTIMER);

	angle = (int16)(ANGLES - angle);
	if (angle >= ANGLES)
		angle = (int16)(angle - ANGLES);

	AddCrack(x2, y2, (int16)(-(dx >> 3) - (dy >> 2)), (int16)(-(dy >> 3) + (dx >> 2)),
	         ThisLevel.shColor[0], delta, PlayerRight, angle, 4, POSTMORTEMTIMER);
}

/* The whole part of the square root of n. */
SEG_ENEMY static int16 SquareRoot(int32 n)
{
	int32 r = 0, bit = 0x40000000L;

	while (bit > n)
		bit >>= 2;
	while (bit) {
		if (n >= r + bit) {
			n -= r + bit;
			r = (r >> 1) + bit;
		} else {
			r >>= 1;
		}
		bit >>= 2;
	}
	return (int16)r;
}

/* The angle (0 to ANGLES - 1) of the screen vector h, v, the one whose
   direction (cos, -sin) is nearest to it. */
SEG_ENEMY static int16 AngleOf(int16 h, int16 v)
{
	int16 a, best = 0;
	int32 d, bestd = -0x7FFFFFFFL;

	for (a = 0; a < ANGLES; a++) {
		d = (int32)Cosins[a] * h - (int32)Sins[a] * v;
		if (d > bestd) {
			bestd = d;
			best = a;
		}
	}
	return best;
}

SEG_ENEMY void BlowSetup(int16 cx, int16 cy, int16 rotspeed, int16 size)
{
	BlowCenterX = cx;
	BlowCenterY = cy;
	RotSpeed = rotspeed;
	BlowSize = size;
}

SEG_ENEMY void BlowUpLine(int16 x, int16 y)
{
	int16 dh, dv, unitsize, angle;

	dh = (int16)(x - VA.CurrentX);
	dv = (int16)(y - VA.CurrentY);
	unitsize = SquareRoot((int32)dh * dh + (int32)dv * dv);
	angle = AngleOf(dh, (int16)-dv);
	angle = (int16)((ANGLES + angle) % ANGLES);

	AddCrack((int16)((VA.CurrentX + x) >> 1), (int16)((VA.CurrentY + y) >> 1),
	         (int16)(x - BlowCenterX - (dh >> 1)), (int16)(y - BlowCenterY - (dv >> 1)),
	         VA.color, unitsize, FlyingVector, angle, RotSpeed, BlowSize);

	RotSpeed = (int16)-RotSpeed;
	VAMoveTo(x, y);
}

SEG_ENEMY static void BlowUpHalfLine(int16 x, int16 y)
{
	BlowUpLine((int16)((x + VA.CurrentX) >> 1), (int16)((y + VA.CurrentY) >> 1));
	BlowUpLine(x, y);
}

SEG_ENEMY void BlowUpPlayer(int16 cx, int16 cy, int16 dx, int16 dy, int16 slant)
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

	BlowSetup((int16)((cx + x1) >> 1), (int16)((cy + y1) >> 1), 1, 40);

	VAMoveTo(cx, cy);
	x2 = (int16)((dxs >> 3) + y4);  y2 = (int16)((dys >> 3) - x4);
	BlowUpHalfLine((int16)(cx + x2), (int16)(cy + y2));
	BlowUpHalfLine(x1, y1);

	x2 = (int16)(-(dxs >> 4) - y4); y2 = (int16)(-(dys >> 4) + x4);
	BlowUpHalfLine((int16)(x1 + x2), (int16)(y1 + y2));
	BlowUpHalfLine((int16)(x1 - x8), (int16)(y1 - y8));

	x2 = (int16)((dxs >> 3) + y8);  y2 = (int16)((dys >> 3) - x8);
	BlowUpHalfLine((int16)(cx + x2), (int16)(cy + y2));
	BlowUpHalfLine((int16)(cx + x8), (int16)(cy + y8));

	x2 = (int16)((((8 - slant) * dx) >> 4) - y4);
	y2 = (int16)((((8 - slant) * dy) >> 4) + x4);
	BlowUpHalfLine((int16)(cx + x2), (int16)(cy + y2));
	BlowUpHalfLine(cx, cy);
}
