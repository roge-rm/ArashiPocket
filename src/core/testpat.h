/* Screen test pattern: drawn into the plat framebuffer with no platform
   calls, so every backend and depth shows the same thing. */
#ifndef CORE_TESTPAT_H
#define CORE_TESTPAT_H

#include "plat.h"

/* Pixel value helpers, also used by the host test to know what to expect.
   A value is a grey level, a palette index or an RGB565 word, by format. */
SEG_PAT uint16 testpat_black(const PlatScreen *s);
SEG_PAT uint16 testpat_white(const PlatScreen *s);
/* Grey step i of n, 0 black to n - 1 white. */
SEG_PAT uint16 testpat_grey(const PlatScreen *s, int16 i, int16 n);
/* Number of grey steps in the ramp: 2, 4 or 16 for 1, 2 or 4 bit, 16 above. */
SEG_PAT int16 testpat_levels(const PlatScreen *s);
/* Colour swatch k, 0 to 6: red, green, blue, yellow, cyan, magenta, orange. */
SEG_PAT uint16 testpat_colour(const PlatScreen *s, int16 k);

SEG_PAT uint16 testpat_get(const PlatScreen *s, int16 x, int16 y);
SEG_PAT void testpat_put(const PlatScreen *s, int16 x, int16 y, uint16 v);

/* Where the ramp swatches are: level i of the ramp is the rectangle starting
   at x (play-area relative) with this width, from y for h rows. */
SEG_PAT void testpat_ramp_rect(const PlatScreen *s, int16 i, PlatRect *r);

/* Draws everything. A cross goes at the stylus point (play-area relative)
   when cross is set. */
SEG_PAT void testpat_draw(const PlatScreen *s, cbool cross, int16 cx, int16 cy);

#endif
