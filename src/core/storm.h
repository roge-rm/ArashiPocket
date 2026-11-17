/* Game state and grid, from STORM.h and STStruct.h by Juri Munkki, cut
   down to what the game loop, the field and the player claw use so far.
   Names and numbers are the original's. */
#ifndef CORE_STORM_H
#define CORE_STORM_H

#include "plat.h"
#include "va.h"

#define MAXSEGS      32
#define DEPTH        120
#define STARTDEPTH   20
#define ZOOMDEPTH    0      /* the original starts at -19, for the fly-through */

/* The original steps the game every 3 Mac ticks of 1/60.15 s. */
#define GAME_HZ      20
/* Most game steps run in one frame when the device is slow. */
#define GAME_MAXCATCHUP 4

typedef struct {
	int16 lvNumber;
	int16 lvField;
	int16 lvColor;
	int32 lvBonus;
	int32 lvStBonus;
	int16 lvNext;
	int16 plMaxMove;
	int16 plSuperZaps;
	int16 shColor[5];
	int16 endTimer;
	int16 ShowSpikesMsg;
	int16 Doomsday;
} LevelInfo;

enum {
	HeroPlaying, HeroFlying, HeroDropping, HeroCracking,
	HeroDisintegrating, HeroPostMortem, HeroDead, HeroQuit
};

typedef struct {
	int16 state;
	int16 timer;
	int16 endtimer;
	int16 rawpos;       /* 8 steps to a lane */
	int16 lane;
	int16 oldlane;
	int16 shifter;      /* visual twist of the claw on its lane */
	int16 flydepth;
	int16 dropdepth;
	int16 lanestat[MAXSEGS];
	int16 segmstat[MAXSEGS + 1];
	int16 lives;
	int32 score;
	int16 drawscore;
} Player;

/* Tables of the grid at each depth, as GridWorld: x[i][d] and y[i][d] are the
   edges of segment i, xc and yc its middle, unitlen the width of a segment. */
typedef struct {
	int16  numsegs;
	int16  wraps;
	int16 *unitlen;
	int16 *x[MAXSEGS];
	int16 *y[MAXSEGS];
	int16 *xc[MAXSEGS];
	int16 *yc[MAXSEGS];
} GridWorld;

typedef struct {
	int16 numsegs;
	int16 wraps;
	int16 xoff, yoff;
	int16 unitlen;
	int16 x[MAXSEGS];
	int16 y[MAXSEGS];
} worldstruct;

extern LevelInfo   ThisLevel;
extern Player      Hero;
extern GridWorld   ww;
extern worldstruct gridOutline;
extern int16       NextSeg[MAXSEGS + 1];
extern int16       PrevSeg[MAXSEGS + 1];
extern int8       *LaneSel[MAXSEGS + 1];
/* Rim position (0 to 255, as plat_rim_from_point) of the middle of each
   lane at the near rim, and 1 when lane numbers go clockwise, else -1. */
extern int16       LaneRim[MAXSEGS];
extern int16       LaneDir;

/* grid.c */
SEG_GAME cbool AllocGrids(void);
SEG_GAME void  FreeGrids(void);
/* Builds the grid for ThisLevel.lvField fitted to VA.frame (CreateFit and
   MakeSpots). Returns 0 if the shape is missing. */
SEG_GAME cbool CreateFit(void);
/* The tube: a line down each lane edge and the near and far rims. */
SEG_GAME void  DrawField(void);

/* player.c */
SEG_GAME void DrawPlayer(int16 cx, int16 cy, int16 dx, int16 dy, int16 slant);
SEG_GAME void DrawStaticPlayer(int16 cx, int16 cy, int16 dx, int16 dy);
SEG_GAME void InitPlayer(void);
SEG_GAME void ZeroScore(void);
SEG_GAME void DisplayLives(void);
SEG_GAME void DisplayScore(void);
SEG_GAME void DrawLevelNumber(void);
/* One game step of the claw: moves it toward its target lane. */
SEG_GAME void UpdatePlayer(const PlatInput *in);
SEG_GAME void DrawHero(void);

#endif
