/* Renderer demo: a level's tube shape, from the Arashi Script and its SPCE
   resource, spinning as a wireframe, with text, spots, explosions, a frame
   counter and the frame rate measured with plat_ticks. */
#ifndef CORE_VADEMO_H
#define CORE_VADEMO_H

#include "types.h"

/* Call after VAInit (and again after VAScreenChanged). Runs the script
   for the level to find its shape and colours. Returns 0 if the shape
   is missing; the demo then spins a plain 16-sided tube. */
SEG_PAT cbool vademo_init(int16 level);
/* Queues one frame and calls VAStep. */
SEG_PAT void vademo_frame(void);
/* Frames per second times 10 over the last measured stretch, 0 before
   the first. */
SEG_PAT int32 vademo_fps10(void);

#endif
