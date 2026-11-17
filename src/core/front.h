/* The front end: attract mode with the title and demo games, the menu, level
   select, high scores, options, about, pause, game over and name entry. It
   draws with the renderer into the play area and reads plat_input, so a
   platform only calls these. */
#ifndef CORE_FRONT_H
#define CORE_FRONT_H

#include "storm.h"

/* After VAInit. Loads the saved options and scores and starts attract mode.
   Returns 0 when out of memory. */
SEG_FRONT cbool front_init(void);
SEG_FRONT void  front_close(void);
/* Runs whatever is due (steps, input, drawing). */
SEG_FRONT void  front_frame(void);
/* Plat ticks until front_frame has something to do. */
SEG_FRONT uint32 front_wait(void);
/* Another app or an alarm has the screen, or the app is stopping: a game
   being played stops at the pause menu, a demo ends, the saved data is
   written. */
SEG_FRONT void  front_pause(void);
/* The screen is back (not the game: it waits at the pause menu). */
SEG_FRONT void  front_resume(void);
/* After VAScreenChanged. */
SEG_FRONT void  front_screen_changed(void);

/* For tests and the platform: what the front end is showing. */
enum {
	FRONT_TITLE,        /* attract: logo page */
	FRONT_DEMO,         /* attract: a recorded game */
	FRONT_SCORES_PAGE,  /* attract: high scores */
	FRONT_MENU,
	FRONT_LEVELS,
	FRONT_SCORES,
	FRONT_OPTIONS,
	FRONT_ABOUT,
	FRONT_PLAY,
	FRONT_PAUSE,
	FRONT_OVER,
	FRONT_NAME,
	FRONT_TEXT          /* attract: credit pages */
};
SEG_FRONT int16 front_screen(void);
/* Test hooks: go straight to a screen, with demo id for FRONT_DEMO. */
SEG_FRONT void  front_goto(int16 screen, int16 arg);

#endif
