/* Play and status areas for a screen, and the rim position of a point. */
#include "plat.h"

SEG_PAT void plat_layout(PlatScreen *s)
{
	int16 size = s->width < s->height ? s->width : s->height;
	int16 spare;

	s->play.w = s->play.h = size;
	s->status.x = s->status.y = s->status.w = s->status.h = 0;
	if (s->width > s->height) {
		spare = s->width - size;
		if (spare * 4 >= size) {
			s->play.x = 0;
			s->play.y = 0;
			s->status.x = size;
			s->status.y = 0;
			s->status.w = spare;
			s->status.h = size;
		} else {
			s->play.x = spare / 2;
			s->play.y = 0;
		}
	} else {
		spare = s->height - size;
		if (spare * 4 >= size) {
			s->play.x = 0;
			s->play.y = 0;
			s->status.x = 0;
			s->status.y = size;
			s->status.w = size;
			s->status.h = spare;
		} else {
			s->play.x = 0;
			s->play.y = spare / 2;
		}
	}
}

/* atan(n / d) for 0 <= n <= d, d > 0, in 1/256 of a turn (0 to 32), within
   a unit: 32 t + 11 t (1 - t). */
SEG_PAT static int16 atan_units(int32 n, int32 d)
{
	int32 t = (n << 8) / d;     /* 0..256 */

	return (int16)((32 * t + (11 * t * (256 - t)) / 256) / 256);
}

SEG_PAT int16 plat_rim_from_point(int16 x, int16 y, int16 size)
{
	int32 dx = 2 * (int32)x - size + 1;     /* doubled, so the centre is whole */
	int32 dy = 2 * (int32)y - size + 1;
	int32 ax = dx < 0 ? -dx : dx;
	int32 ay = dy < 0 ? -dy : dy;
	int16 t;

	if (ax == 0 && ay == 0)
		return -1;
	if (ay >= ax)
		t = atan_units(ax, ay);
	else
		t = (int16)(64 - atan_units(ay, ax));
	if (dx >= 0 && dy <= 0)
		return t & 255;
	if (dx >= 0)
		return (128 - t) & 255;
	if (dy > 0)
		return (128 + t) & 255;
	return (256 - t) & 255;
}
