/* The grid and the tube, from GridTest.c by Juri Munkki: CreateFit
   fits the level's shape (SPCE) to the play area and MakeSpots tabulates its
   corners at every depth. The shape is walked in doubles as the original
   does. The fly-in of the field, the stars and the Superzapper Recharge
   text are not here yet. */
#include "storm.h"
#include "cmath.h"

#define PI 3.1415926535
#define TWIST_DIVIDE 32
#define DEPTHS (DEPTH - ZOOMDEPTH + 1)

GridWorld   ww;
worldstruct gridOutline;
int16       NextSeg[MAXSEGS + 1];
int16       PrevSeg[MAXSEGS + 1];
int8       *LaneSel[MAXSEGS + 1];
int16       LaneRim[MAXSEGS];
int16       LaneDir = 1;
/* Changes whenever MakeSpots changes the field. still_gen, the key of the
   still picture (VAStillBegin), changes with it or with the HUD in it. */
static uint16 field_gen, still_gen, still_field, still_hud;
cbool HudInStill;

static const int16 *gridMem;
static int8  *laneMem;

SEG_GAME cbool AllocGrids(void)
{
	int32 n = (int32)MAXSEGS * 4 * DEPTHS + DEPTHS;
	int32 i;
	const int16 *ip;
	int8 *cp;
	int16 zero[DEPTHS];

	if (gridMem)
		return 1;
	gridMem = (const int16 *)plat_store_alloc((uint32)n * sizeof(int16));
	laneMem = (int8 *)plat_alloc((uint32)(MAXSEGS + 1) * ANGLES);
	if (!gridMem || !laneMem) {
		FreeGrids();
		return 0;
	}
	for (i = 0; i < DEPTHS; i++)
		zero[i] = 0;
	for (i = 0; i < n; i += DEPTHS)
		plat_store_write((const uint8 *)gridMem, (uint32)i * sizeof(int16), zero,
		                 sizeof(zero));
	for (i = 0; i < (int32)(MAXSEGS + 1) * ANGLES; i++)
		laneMem[i] = 0;
	ip = gridMem;
	for (i = 0; i < MAXSEGS; i++) {
		ww.x[i] = (int16 *)ip - ZOOMDEPTH;  ip += DEPTHS;
		ww.y[i] = (int16 *)ip - ZOOMDEPTH;  ip += DEPTHS;
		ww.xc[i] = (int16 *)ip - ZOOMDEPTH; ip += DEPTHS;
		ww.yc[i] = (int16 *)ip - ZOOMDEPTH; ip += DEPTHS;
	}
	ww.unitlen = (int16 *)ip - ZOOMDEPTH;
	cp = laneMem;
	for (i = 0; i <= MAXSEGS; i++) {
		LaneSel[i] = cp;
		cp += ANGLES;
	}
	return 1;
}

/* LaneSel[vertex][rotation] as the original reads it. A split flipper's or
   pulsar's rotation can be outside 0 to ANGLES - 1 until it next moves (the
   original doesn't wrap it), and the rows are one block, so the read lands
   in the row before or after. Before the first row the original reads the
   end of ww.unitlen, whose bytes depend on the Mac's screen; that gives -1
   here (no lane), as does a read past the last row. */
SEG_GAME int16 LaneAt(int16 vertex, int16 rotation)
{
	int32 i = (int32)vertex * ANGLES + rotation;

	if (i < 0 || i >= (int32)(MAXSEGS + 1) * ANGLES)
		return -1;
	return LaneSel[0][i];
}

SEG_GAME void FreeGrids(void)
{
	if (gridMem)
		plat_store_free((const uint8 *)gridMem);
	if (laneMem)
		plat_free(laneMem);
	gridMem = NULL;
	laneMem = NULL;
}

SEG_GAME static void MakeSpots(const worldstruct *world, int16 xcen, int16 ycen);

SEG_GAME cbool CreateFit(void)
{
	double x, y, sx, sy, xmin, xmax, ymin, ymax, a;
	double AngularConstant = PI * 2.0 / ANGLES;
	double xshift, yshift, normalmargin, twistBot;
	const uint8 *space;
	uint32 bytes = 0;
	int16 numsegs, wraps, xoff, yoff, ang[MAXSEGS];
	int16 i, j, k, xcen, ycen, size;
	int32 area;

	space = plat_resource(FOURCC('S', 'P', 'C', 'E'), ThisLevel.lvField, &bytes);
	if (!space || bytes < 8) {
		if (space)
			plat_release(space);
		return 0;
	}
	numsegs = gets16(space);
	wraps = space[2] != 0;
	xoff = (int8)space[4];
	yoff = (int8)space[5];
	if (numsegs > MAXSEGS - 2)
		numsegs = MAXSEGS - 2;
	if (numsegs < 2 || (uint32)(8 + 2 * numsegs) > bytes) {
		plat_release(space);
		return 0;
	}
	for (i = 0; i < numsegs; i++)
		ang[i] = gets16(space + 8 + 2 * i);
	plat_release(space);

	gridOutline.numsegs = numsegs;
	xmin = xmax = ymin = ymax = x = y = 0.0;
	a = 0.0;
	for (i = 0; i < numsegs; i++) {
		double t;

		a += ang[i];
		t = a * AngularConstant;
		x += cm_cos(t);
		y -= cm_sin(t);
		if (x < xmin)
			xmin = x;
		else if (x > xmax)
			xmax = x;
		if (y < ymin)
			ymin = y;
		else if (y > ymax)
			ymax = y;
	}

	/* Margins that leave room for the twist. */
	normalmargin = 1.05;
	xmin -= normalmargin;
	ymin -= normalmargin;
	xmax += normalmargin;
	ymax += normalmargin;
	xshift = xoff / (double)TWIST_DIVIDE;
	yshift = yoff / (double)TWIST_DIVIDE;
	twistBot = (xmax - xshift) * (double)STARTDEPTH / (DEPTH + STARTDEPTH) + xshift;
	if (twistBot > xmax)
		xmax = twistBot;
	twistBot = (xmin - xshift) * (double)STARTDEPTH / (DEPTH + STARTDEPTH) + xshift;
	if (twistBot < xmin)
		xmin = twistBot;
	twistBot = (ymax - yshift) * (double)STARTDEPTH / (DEPTH + STARTDEPTH) + yshift;
	if (twistBot > ymax)
		ymax = twistBot;
	twistBot = (ymin - yshift) * (double)STARTDEPTH / (DEPTH + STARTDEPTH) + yshift;
	if (twistBot < ymin)
		ymin = twistBot;

	x = -(xmin + xmax) / 2;
	y = -(ymin + ymax) / 2;
	xcen = (int16)((VA.frame.right + VA.frame.left) / 2);
	ycen = (int16)((VA.frame.bottom + VA.frame.top) / 2);
	sx = (VA.frame.right - VA.frame.left) / (xmax - xmin);
	sy = (VA.frame.bottom - VA.frame.top) / (ymax - ymin);
	if (sy < sx)
		sx = sy;

	gridOutline.xoff = (int16)(xoff * sx / TWIST_DIVIDE);
	gridOutline.yoff = (int16)(yoff * sx / TWIST_DIVIDE);
	gridOutline.unitlen = (int16)sx;
	x = sx * x + 0.5;
	y = sx * y + 0.5;
	a = 0.0;
	for (i = 0; i < numsegs; i++) {
		double t;

		a += ang[i];
		t = a * AngularConstant;
		gridOutline.x[i] = (int16)x;
		gridOutline.y[i] = (int16)y;
		x += cm_cos(t) * sx;
		y -= cm_sin(t) * sx;
	}
	gridOutline.x[i] = (int16)x;
	gridOutline.y[i] = (int16)y;
	gridOutline.wraps = wraps;
	if (wraps) {
		gridOutline.x[i] = gridOutline.x[0];
		gridOutline.y[i] = gridOutline.y[0];
	}

	j = 0;
	for (i = 0; i < numsegs; i++) {
		j = (int16)(j + ang[i]);
		NextSeg[i] = (int16)(((int32)j + 4 * ANGLES) % ANGLES);
	}
	NextSeg[i] = NextSeg[0];
	for (i = 0; i <= numsegs; i++)
		PrevSeg[i] = (int16)((ANGLES / 2 + NextSeg[(i - 1 + numsegs) % numsegs]) % ANGLES);

	k = 0;
	for (i = 0; i <= numsegs; i++) {
		k = (int16)((NextSeg[i] + PrevSeg[i]) / 2);
		if (NextSeg[i] < k)
			k = (int16)(k + ANGLES / 2);
		for (j = k; j < k + ANGLES / 2; j++)
			LaneSel[i][j % ANGLES] = (int8)i;
		for (j = (int16)(k + ANGLES / 2); j < k + ANGLES; j++)
			LaneSel[i][j % ANGLES] = (int8)(i - 1);
	}
	for (j = 0; j < ANGLES; j++)
		if (LaneSel[0][j] == -1)
			LaneSel[0][j] = (int8)(numsegs - 1);
	for (j = k; j < k + ANGLES / 2; j++)
		LaneSel[numsegs][j % ANGLES] = 0;
	if (wraps == 0) {
		PrevSeg[0] = -999;
		NextSeg[numsegs] = -999;
		for (j = 0; j < ANGLES; j++) {
			LaneSel[0][j] = 0;
			LaneSel[numsegs][j] = (int8)(numsegs - 1);
		}
	}

	MakeSpots(&gridOutline, xcen, ycen);

	/* Where each lane is seen from the middle, and which way round the
	   lane numbers go on the screen (the sign of the area they sweep). */
	size = VA.frame.right;
	area = 0;
	for (i = 0; i < numsegs; i++) {
		int32 rx = (int32)ww.xc[i][0] - xcen, ry = (int32)ww.yc[i][0] - ycen;
		int32 vx = (int32)ww.x[i + 1][0] - ww.x[i][0];
		int32 vy = (int32)ww.y[i + 1][0] - ww.y[i][0];

		LaneRim[i] = plat_rim_from_point(ww.xc[i][0], ww.yc[i][0], size);
		area += rx * vy - ry * vx;
	}
	LaneDir = (int16)(area >= 0 ? 1 : -1);
	return 1;
}

/* Writes one row of the grid tables (a value at each depth, from row[0] =
   ZOOMDEPTH) into the storage the tables live in. */
SEG_GAME static void put_row(int16 *at_zoom, const int16 *row)
{
	plat_store_write((const uint8 *)gridMem,
	                 (uint32)((const uint8 *)(at_zoom + ZOOMDEPTH) - (const uint8 *)gridMem),
	                 row, DEPTHS * sizeof(int16));
}

SEG_GAME static void MakeSpots(const worldstruct *world, int16 xcen, int16 ycen)
{
	int16 d, i;
	int16 row[DEPTHS];

	field_gen++;
	ww.numsegs = world->numsegs;
	ww.wraps = world->wraps;
	for (i = 0; i <= ww.numsegs; i++) {
		for (d = ZOOMDEPTH; d <= DEPTH; d++)
			row[d - ZOOMDEPTH] = (int16)(((world->x[i] - (int32)world->xoff) * STARTDEPTH) /
			                             (STARTDEPTH + d) + xcen + world->xoff);
		put_row(ww.x[i], row);
		for (d = ZOOMDEPTH; d <= DEPTH; d++)
			row[d - ZOOMDEPTH] = (int16)(((world->y[i] - (int32)world->yoff) * STARTDEPTH) /
			                             (STARTDEPTH + d) + ycen + world->yoff);
		put_row(ww.y[i], row);
	}
	for (d = ZOOMDEPTH; d <= DEPTH; d++)
		row[d - ZOOMDEPTH] = (int16)(((int32)world->unitlen * STARTDEPTH) / (STARTDEPTH + d));
	put_row(ww.unitlen, row);
	for (i = 0; i < ww.numsegs; i++) {
		for (d = ZOOMDEPTH; d <= DEPTH; d++)
			row[d - ZOOMDEPTH] = (int16)((ww.x[i][d] + ww.x[i + 1][d]) >> 1);
		put_row(ww.xc[i], row);
		for (d = ZOOMDEPTH; d <= DEPTH; d++)
			row[d - ZOOMDEPTH] = (int16)((ww.y[i][d] + ww.y[i + 1][d]) >> 1);
		put_row(ww.yc[i], row);
	}

	/* The paths of the stars spinning at the centre, one at the near end and
	   one at the far end of the tube. */
	ww.starsegs = (int16)(ww.numsegs * STARDIVISION);
	for (d = 0; d < 2; d++) {
		int16 j;

		for (i = 0; i <= ww.numsegs; i++) {
			ww.starx[i * STARDIVISION][d] = (int16)(((world->x[i] - (int32)world->xoff) * STARTDEPTH) /
			    (STARTDEPTH + DEPTH + (DEPTH * 2) * d) + xcen + world->xoff);
			ww.stary[i * STARDIVISION][d] = (int16)(((world->y[i] - (int32)world->yoff) * STARTDEPTH) /
			    (STARTDEPTH + DEPTH + (DEPTH * 2) * d) + ycen + world->yoff);
		}
		for (i = 0; i < ww.starsegs; i = (int16)(i + STARDIVISION))
			for (j = 1; j < STARDIVISION; j++) {
				ww.starx[i + j][d] = (int16)((ww.starx[i + STARDIVISION][d] - ww.starx[i][d]) * j /
				                             STARDIVISION + ww.starx[i][d]);
				ww.stary[i + j][d] = (int16)((ww.stary[i + STARDIVISION][d] - ww.stary[i][d]) * j /
				                             STARDIVISION + ww.stary[i][d]);
			}
	}
}

/* The static field of MakeSpots: a line down each lane edge, then the near
   and far rims, in colour 0, as the still picture, with the HUD that seldom
   changes where there is no status area. A lane a pulsar is charging shows
   its rim edge in colour 1, drawn over it each frame. */
SEG_GAME void DrawField(void)
{
	int16 i, d;
	uint16 hud;

	HudInStill = HudStill();
	hud = (uint16)(HudInStill ? HudStillGen() : 0);
	if (field_gen != still_field || hud != still_hud) {
		still_field = field_gen;
		still_hud = hud;
		still_gen++;
	}
	if (VAStillBegin(still_gen)) {
		VA.color = 0;
		for (i = 0; i < ww.numsegs; i++)
			VALine(ww.x[i][0], ww.y[i][0], ww.x[i][DEPTH], ww.y[i][DEPTH]);
		if (ww.wraps == 0)
			VALine(ww.x[i][0], ww.y[i][0], ww.x[i][DEPTH], ww.y[i][DEPTH]);
		for (d = 0; d <= DEPTH; d += DEPTH)
			for (i = 0; i < ww.numsegs; i++)
				VALine(ww.x[i][d], ww.y[i][d], ww.x[i + 1][d], ww.y[i + 1][d]);
		if (HudInStill)
			DrawHud(1, 0);
		VAStillEnd();
	}
	VA.color = 1;
	for (i = 0; i < ww.numsegs; i++)
		if (Hero.lanestat[i] & CapPuls)
			VALine(ww.x[i][0], ww.y[i][0], ww.x[i + 1][0], ww.y[i + 1][0]);
}
