/* The game loop (GameLoop.c): a level's tube with the claw on its rim,
   enemies, the fly-through to the next level, bonuses and lives. */
#ifndef CORE_GAME_H
#define CORE_GAME_H

#include "storm.h"

/* Seeds randSeed (script) and VARandSeed (enemies, stars) from the clock. */
SEG_GAME void  game_seed(void);
/* After VAInit. Loads the level and its grid. Returns 0 when out of
   memory or the level is missing. */
SEG_GAME cbool game_init(int16 level);
SEG_GAME void  game_close(void);
/* Starts a game on level (the level select's choice, as PlayGame): the
   score, lives and bonus flags are new, and the starting bonus of that level
   is paid when it is cleared. saved = 1 is the select's "saved game" entry:
   the score and lives of the last game carry on. Returns 0 when the level
   can't be loaded. game_init(level) is the same after allocating. */
SEG_GAME cbool game_start(int16 level, cbool saved);
/* 0 once the game has stopped (a level could not be loaded). */
SEG_GAME cbool game_running(void);

/* What the level select lists for a level: the field (shape) and colour it
   is drawn with and the starting bonus paid on clearing it. */
typedef struct {
	int16 level;
	int16 field;
	int16 color;
	int32 startBonus;
} LevelChoice;
/* The levels the select offers (CreateStable): those the script allows a
   restart on, up to level highest (at least 9) in arcade mode, all of them
   in practice mode. At most max, 95 in the original. Returns how many. */
SEG_GAME int16 game_level_choices(cbool practice, int16 highest, LevelChoice *out, int16 max);
/* The same, with use_table 0 running the script for every level (host
   check of the LSEL table). */
SEG_GAME int16 game_level_choices_from(cbool use_table, cbool practice, int16 highest,
                                       LevelChoice *out, int16 max);

/* One level's script as the level select reads it. LSEL 128 holds these
   for the levels the select walks, worked out at build time by
   build/host/mklsel: LSEL_ENTRY bytes each, big-endian, level, next,
   arcade, practice, field and colour as int16, then the start bonus as
   int32. */
typedef struct {
	int16 level, next;
	int16 arcade, practice;     /* AllowArcadeRestart, AllowPracticeRestart */
	int16 field, color;
	int32 startBonus;
} LevelRestart;
#define LSEL_ENTRY 16
/* Runs the level's script (slow on a 68k). Returns 0 if the script can't
   be loaded. */
SEG_GAME cbool game_level_restart(int16 level, LevelRestart *r);

/* LVAL 128: the script's values of levels 1 to LVAL_LEVELS, LVAL_ENTRY
   bytes a level (54 int16 and 17 int32), worked out at build time by
   build/host/mklval. game_level_values runs the script for one level and
   writes its entry to out (0 if the script can't be loaded). */
#define LVAL_LEVELS 99
#define LVAL_ENTRY 176
SEG_GAME cbool game_level_values(int16 level, uint8 *out);

/* Loads a level, whatever the game state, and puts the claw back on its
   rim (host tests, DEBUG_LEVELS). */
SEG_GAME cbool game_load_level(int16 level);
/* Loads the level the script says follows this one. */
SEG_GAME void  game_next_level(void);
/* After VAScreenChanged: fits the current level to the new play area. */
SEG_GAME cbool game_screen_changed(void);

/* 0 keeps a level from ending when its enemies are gone (host tests). */
extern cbool LevelEnds;
/* 0 skips the level intro (host tests). */
extern cbool LevelIntro;

/* Game steps run so far. */
extern uint32 game_steps;

/* One game step with the given input; spin is used in this step only. */
SEG_GAME void game_tick(const PlatInput *in);
/* Queues this step's picture and runs VAStep. */
SEG_GAME void game_draw(void);

/* Reads plat_ticks and runs the steps that are due, at most
   GAME_MAXCATCHUP, then draws once. Returns 0 when none was due. */
SEG_GAME cbool game_frame(void);
/* game_frame with the input given by the caller (the front end reads it
   once for itself). */
SEG_GAME cbool game_frame_in(const PlatInput *in);
/* 1 once the last claw has died; the game has stopped and the score is
   Hero.score. */
SEG_GAME cbool game_over_now(void);
/* The level number being played. */
SEG_GAME int16 game_level(void);
/* Plays recorded game 0 to 7 (MYGA 128 to 135) as a game of its own: the
   claw follows the recording and the game ends where it does. */
/* The demo the recordings started with (MYGA 133); the attract loop starts there. */
#define DEMO_FIRST 5
SEG_GAME cbool game_demo_start(int16 id);
SEG_GAME void  game_demo_stop(void);
/* Plat ticks until the next step is due. */
SEG_GAME uint32 game_wait(void);
/* Restarts the clock after a pause or a long load. */
SEG_GAME void game_resume(void);
#ifdef GAME_PROFILE
/* Line 0 or 1 of the frame profile into out (40 bytes), when new since
   line 1 was last taken; else 0. */
SEG_GAME cbool game_profile_text(int16 line, char *out);
#endif

#endif
