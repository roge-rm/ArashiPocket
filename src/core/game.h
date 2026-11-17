/* The game loop (GameLoop.c) at M4: a level's tube with the claw on its
   rim, moved by every input. Enemies, shots and the rest come later. */
#ifndef CORE_GAME_H
#define CORE_GAME_H

#include "storm.h"

/* Seeds randSeed (script) and VARandSeed (enemies, stars) from the clock. */
SEG_GAME void  game_seed(void);
/* After VAInit. Loads the level and its grid. Returns 0 when out of
   memory or the level is missing. */
SEG_GAME cbool game_init(int16 level);
SEG_GAME void  game_close(void);
/* Loads another level (1 to 99) and puts the claw back on its rim. */
SEG_GAME cbool game_load_level(int16 level);
SEG_GAME void  game_next_level(void);
/* After VAScreenChanged: fits the current level to the new play area. */
SEG_GAME cbool game_screen_changed(void);

/* Game steps run so far. */
extern uint32 game_steps;

/* One game step with the given input; spin is used in this step only. */
SEG_GAME void game_tick(const PlatInput *in);
/* Queues this step's picture and runs VAStep. */
SEG_GAME void game_draw(void);

/* Reads plat_ticks and runs the steps that are due, at most
   GAME_MAXCATCHUP, then draws once. Returns 0 when none was due. */
SEG_GAME cbool game_frame(void);
/* Plat ticks until the next step is due. */
SEG_GAME uint32 game_wait(void);
/* Restarts the clock after a pause or a long load. */
SEG_GAME void game_resume(void);

#endif
