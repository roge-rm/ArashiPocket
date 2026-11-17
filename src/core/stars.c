/* The stars at the centre of the tube, from STStars.c by Juri Munkki. An
   enemy asks for one with StarApproach; the star spins round the centre
   until it is let out, grows to the rim and tells the enemy where it
   landed. UpdateCStars moves them and keeps where each is drawn, DrawCStars
   queues them. */
#include "storm.h"
#include "qdrandom.h"

#define ARRIVALSPEED     8
#define CENTERSTARTRATIO 96

typedef struct {
	int16  index[2];
	int16  direction[2];
	int16  ratio;
	int8   color;
	int8   state;
	int16 *notify;
} CenterStar;

enum { available, used, unavailable };

static CenterStar AllStars[MAXCENTERSTARS + 1];
static int16 TotalStars;
static int16 FirstUnused;
/* Where each star is drawn this step. */
static int16 pixX[MAXCENTERSTARS], pixY[MAXCENTERSTARS], pixColor[MAXCENTERSTARS];
static cbool pixShow[MAXCENTERSTARS];

/* VARandom, also for VAPosRandom. */
SEG_ENEMY static int16 rnd(void)
{
	return va_random();
}

SEG_ENEMY void AllocCStars(void)
{
	TotalStars = MAXCENTERSTARS;
	FirstUnused = 0;
}

SEG_ENEMY void InitCStars(void)
{
	int16 i, d;

	FirstUnused = 0;
	for (i = 0; i < TotalStars; i++) {
		for (d = 0; d < 2; d++) {
			AllStars[i].index[d] = (int16)((uint16)rnd() % (uint16)ww.starsegs);
			AllStars[i].direction[d] = (int16)(((rnd() & 1) << 1) - 1);
		}
		AllStars[i].ratio = CENTERSTARTRATIO;
		AllStars[i].color = (int8)(i & 7);
		AllStars[i].state = available;
		pixShow[i] = 0;
	}
}

SEG_ENEMY void UpdateCStars(void)
{
	int16 i, d, x1, x0, y1, y0;
	int16 availcount = ThisLevel.starCount;
	CenterStar *csp = AllStars;
	CenterStar temp;

	for (i = 0; i < TotalStars; i++) {
		x1 = ww.starx[csp->index[1]][1];
		x0 = (int16)(ww.starx[csp->index[0]][0] - x1);
		y1 = ww.stary[csp->index[1]][1];
		y0 = (int16)(ww.stary[csp->index[0]][0] - y1);

		if (csp->state == used) {
			csp->ratio = (int16)(csp->ratio + ARRIVALSPEED);
			if (csp->ratio >= 256) {
				csp->ratio = CENTERSTARTRATIO;
				csp->state = available;
				csp->color = 0;
				temp = *csp;

				for (d = 0; d < 2; d++) {
					csp->index[d] = (int16)((uint16)rnd() % (uint16)ww.starsegs);
					csp->direction[d] = (int16)(((rnd() & 1) << 1) - 1);
				}
				*csp = AllStars[--FirstUnused];
				AllStars[FirstUnused] = temp;
				*(temp.notify) = (int16)(-1 - temp.index[0]);
			}
		} else {
			if (availcount-- > 0) {
				csp->color++;
				if (csp->color > 6)
					csp->color = 0;
			} else {
				csp->state = unavailable;
			}
		}

		if (csp->state != unavailable) {
			pixShow[i] = 1;
			pixColor[i] = csp->color;
			pixX[i] = (int16)(x1 + (int32)x0 * csp->ratio / 256);
			pixY[i] = (int16)(y1 + (int32)y0 * csp->ratio / 256);
		} else {
			csp->state = available;
			pixShow[i] = 0;
		}

		for (d = 0; d < 2; d++) {
			csp->index[d] = (int16)(csp->index[d] + csp->direction[d]);
			if (ww.wraps) {
				if (csp->index[d] < 0)
					csp->index[d] = (int16)(csp->index[d] + ww.starsegs);
				else if (csp->index[d] >= ww.starsegs)
					csp->index[d] = (int16)(csp->index[d] - ww.starsegs);
			} else {
				if (csp->index[d] <= 0) {
					csp->index[d] = 0;
					csp->direction[d] = (int16)-csp->direction[d];
				}
				if (csp->index[d] >= ww.starsegs) {
					csp->index[d] = ww.starsegs;
					csp->direction[d] = (int16)-csp->direction[d];
				}
			}
		}
		csp++;
	}
}

SEG_ENEMY void DrawCStars(void)
{
	int16 i;

	for (i = 0; i < TotalStars; i++)
		if (pixShow[i]) {
			VA.color = pixColor[i];
			VASafePixel(pixX[i], pixY[i]);
		}
}

SEG_ENEMY void StarApproach(int16 *notify, int16 color)
{
	if (FirstUnused >= TotalStars) {
		*notify = (int16)-(int16)(((uint16)rnd()) % (uint16)(ww.numsegs - 1) + 1);
	} else {
		AllStars[FirstUnused].state = used;
		AllStars[FirstUnused].notify = notify;
		AllStars[FirstUnused].color = (int8)color;
		FirstUnused++;
		*notify = 0;
	}
}
