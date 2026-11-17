/* The shots, from STShots.c by Juri Munkki. Same limit (9), speed from the
   level script, and hit test. The original draws a shot as it moves it;
   here UpdateShots keeps where each one was and DrawShots queues them. */
#include "storm.h"

Shot  shots[MAXSHOTS];
int16 shotcount;

static Shot   shown[MAXSHOTS];
static int16  shownCount;
static int16  shotrotation;

SEG_GAME static void DrawShot(int16 lane, int16 level);

/* A little explosion at (lane, level); it doesn't touch the shots. */
SEG_GAME void ExplodeShot(int16 lane, int16 level)
{
	int16 x = ww.xc[lane][level];
	int16 y = ww.yc[lane][level];

	level = (int16)(3 - (level << 2) / (DEPTH + 1));
	VAExplosion(x, y, level, VA.color);
}

/* Five spots: one in the middle, four turning round it (the outer ones only
   in the near third of the tube). */
SEG_GAME static void DrawShot(int16 lane, int16 level)
{
	int16 dx, dy, x, y;

	shotrotation = (int16)(shotrotation + 7);
	if (shotrotation >= ANGLES)
		shotrotation = (int16)(shotrotation - ANGLES);
	dx = (int16)((((int32)(ww.unitlen[level] >> 3)) * Cosins[shotrotation]) >> 8);
	dy = (int16)((((int32)(ww.unitlen[level] >> 3)) * Sins[shotrotation]) >> 8);

	x = ww.xc[lane][level];
	y = ww.yc[lane][level];

	VA.color = ThisLevel.shColor[4];
	VASpot((int16)(x + dx), (int16)(y + dy));

	if (level < DEPTH / 3) {
		VA.color = ThisLevel.shColor[3];
		VASpot((int16)(x - dx), (int16)(y - dy));
		VA.color = ThisLevel.shColor[2];
		VASpot((int16)(x + dy), (int16)(y - dx));
		VA.color = ThisLevel.shColor[1];
		VASpot((int16)(x - dy), (int16)(y + dx));
	}
	VA.color = ThisLevel.shColor[0];
	VASpot(x, y);
}

SEG_GAME cbool AddShot(int16 lane)
{
	if (shotcount < MAXSHOTS) {
		shots[shotcount].lane = lane;
		shots[shotcount].level = 0;
		shotcount++;
		return 1;
	}
	return 0;
}

SEG_GAME int16 ShotHitTest(int16 lane, int16 level)
{
	int16 i;

	if (Hero.superzapping) {
		if (Hero.superzapping == 2)     /* single enemy zap */
			Hero.superzapping = 0;
		return -2;
	}
	for (i = 0; i < shotcount; i++) {
		if (shots[i].lane == lane && level <= shots[i].level && level + 4 >= shots[i].level) {
			ExplodeShot(lane, level);
			shots[i] = shots[--shotcount];
			return -1;
		}
	}
	return 0;
}

SEG_GAME void UpdateShots(void)
{
	int16 i, bottom = DEPTH;

	shownCount = 0;
	if (Hero.flydepth)
		bottom = (int16)(DEPTH - Hero.flydepth);
	for (i = 0; i < shotcount; i++) {
		if (shots[i].level >= bottom) {
			shots[i] = shots[--shotcount];
			i--;
		} else {
			if (shots[i].level)
				shown[shownCount++] = shots[i];
			shots[i].level = (int16)(shots[i].level + ThisLevel.shSpeed);
			if (shots[i].level > bottom)
				shots[i].level = bottom;
		}
	}
}

SEG_GAME void DrawShots(void)
{
	int16 i;

	for (i = 0; i < shownCount; i++)
		DrawShot(shown[i].lane, shown[i].level);
}

SEG_GAME void InitShots(void)
{
	shotcount = 0;
	shownCount = 0;
}
