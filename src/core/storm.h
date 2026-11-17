/* Game state and grid, from STORM.h and STStruct.h by Juri Munkki, cut
   down to what the game loop, the field and the player claw use so far.
   Names and numbers are the original's. */
#ifndef CORE_STORM_H
#define CORE_STORM_H

#include "plat.h"
#include "va.h"

#define MAXSEGS      32
#define MAXSHOTS     9
#define DEPTH        120
#define STARTDEPTH   20
#define ZOOMDEPTH    0      /* the original starts at -19, for the fly-through */
#define STARDIVISION 4      /* steps between two lane corners for a centre star */
#define MAXCENTERSTARS 10

/* The original steps the game every 3 Mac ticks of 1/60.15 s. */
#define GAME_HZ      20
/* Most game steps run in one frame when the device is slow. */
#define GAME_MAXCATCHUP 4

/* The three kinds of tanker, as AnyTanker. speed is 16.16. */
typedef struct {
	int16  points;
	int16  count;
	uint16 prob;        /* chance in 65536 each step of one more appearing */
	Fixed  speed;
	int16  color;
} AnyTanker;

enum { FlipTank, FuseTank, PulsTank, TANKTYPES };

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
	int16 shSpeed;

	int16  flPoints;
	int16  flCount;
	uint16 flProb;
	Fixed  flSpeed;
	int16  flRot;
	int16  flColor;

	AnyTanker tk[TANKTYPES];

	int16 boredomCount; /* fewer enemies active than this: probIncrease = boredProb */
	int32 boredProb;
	int16 endTimer;
	int16 ShowSpikesMsg;
	int16 Doomsday;

	/* Not from the script. */
	int16 activeCount;  /* enemies active this step */
	int16 totalCount;   /* enemies left on the level */
	int16 starCount;    /* baby stars wanted at the centre */
	int16 edgeCount;    /* enemies waiting on the rim */
	int32 probIncrease;
} LevelInfo;

enum {
	HeroPlaying, HeroFlying, HeroDropping, HeroCracking,
	HeroDisintegrating, HeroPostMortem, HeroDead, HeroQuit
};

typedef struct {
	int16 state;
	int16 timer;
	int16 endtimer;
	int16 pendingshot;  /* a fire press was kept for the next step that can shoot */
	int16 couldshoot;   /* this step was one that can shoot */
	int16 superzapping; /* 1 superzap, 2 single enemy zap */
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

typedef struct {
	int16 lane;
	int16 level;        /* 0 at the rim to DEPTH at the far end */
} Shot;

/* What an enemy leaves in Hero.lanestat or segmstat when it has the claw
   (the original's FlipMask and the rest; the enemy modules set them). */
enum {
	CapFlip = 1, CapPuls = 2, CapFuse = 4, CapPlas = 8, CapSpike = 16
};

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
	int16  starsegs;    /* paths of the stars spinning at the centre */
	int16  starx[STARDIVISION * MAXSEGS][2];
	int16  stary[STARDIVISION * MAXSEGS][2];
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
extern Shot        shots[MAXSHOTS];
extern int16       shotcount;
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
/* The enemy at status has the claw: starts the death (STPlayer's
   PlayerCapture). */
SEG_GAME void PlayerCapture(int16 status);
/* Hero.state is HeroDead and the game has dealt with it. */
SEG_GAME void ReviveHero(void);

/* IncreaseScore of STPlayer.c: adds points (free lives come with M7). */
SEG_GAME void IncreaseScore(int32 points);

/* shots.c, from STShots.c */
SEG_GAME void InitShots(void);
/* Fires down lane. Returns 1 if a shot was free, 0 at MAXSHOTS. */
SEG_GAME cbool AddShot(int16 lane);
/* Kills a shot that is within 4 levels of level in lane and returns -1, or
   0. While superzapping everything is hit (-2). */
SEG_GAME int16 ShotHitTest(int16 lane, int16 level);
/* Moves the shots down one step and keeps this step's picture of them. */
SEG_GAME void UpdateShots(void);
SEG_GAME void DrawShots(void);
SEG_GAME void ExplodeShot(int16 lane, int16 level);

/* Enemies: stars.c (STStars.c), flipper.c (STFlipper.c), tanker.c
   (STTanker.c), enemy.c (what STInitialize and the draw calls do over all
   of them). An enemy appears as a star at the centre that grows to the rim,
   which tells it where it lands. Each module has Init, Update (one step:
   moves, hit test, scores, sets Hero.lanestat and segmstat) and Draw
   (queues the picture of the last step; the original draws while it
   updates). Fuseballs, pulsars and spikes are added the same way. */
/* One enemy on the field, for ListEnemies (the demo player and the tests). */
enum { EnemyFlipper, EnemyTanker };
typedef struct {
	int16 kind;
	int16 lane;
	int16 level;
} EnemyInfo;

/* 0 stops new enemies from appearing (host tests). */
extern cbool EnemySpawning;

SEG_ENEMY void AllocCStars(void);
SEG_ENEMY void InitCStars(void);
SEG_ENEMY void UpdateCStars(void);
SEG_ENEMY void DrawCStars(void);
/* *notify becomes 0, and when the star has arrived, -1 minus its place on
   the rim path (4 to a lane). */
SEG_ENEMY void StarApproach(int16 *notify, int16 color);

SEG_ENEMY void InitFlippers(void);
SEG_ENEMY void UpdateFlippers(void);
SEG_ENEMY void DrawFlippers(void);
SEG_ENEMY void CreateNewFlipper(void);
/* Adds the active flippers (tankers) to out, at most max. Returns how many. */
SEG_ENEMY int16 ListFlippers(EnemyInfo *out, int16 max);
/* Two flippers from a tanker killed at lane, level. */
SEG_ENEMY void CreateSplitFlippers(int16 lane, int16 level);

SEG_ENEMY void InitTankers(void);
SEG_ENEMY void UpdateTankers(void);
SEG_ENEMY void DrawTankers(void);
SEG_ENEMY void CreateNewTanker(int16 tanktype);
SEG_ENEMY int16 ListTankers(EnemyInfo *out, int16 max);

/* A bang in colour at x, y, sized by how deep level is. */
SEG_ENEMY void ExplodeEnemy(int16 x, int16 y, int16 level, int16 color);
/* The enemy part of STInitialize, after the level and grid are loaded. */
SEG_ENEMY void InitEnemies(void);
/* Every active enemy on the field (not the stars), at most max. */
SEG_ENEMY int16 ListEnemies(EnemyInfo *out, int16 max);
SEG_ENEMY void DrawEnemies(void);

#endif
