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
#define ZOOMDEPTH    -19    /* levels in front of the rim, for the fly-through */
#define STARDIVISION 4      /* steps between two lane corners for a centre star */
#define MAXCENTERSTARS 10
#define POSTMORTEMTIMER 60  /* steps the claw's pieces fly after it dies */

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
	int16 shPower;      /* levels a spike loses to a shot */

	int16  flPoints;
	int16  flCount;
	uint16 flProb;
	Fixed  flSpeed;
	int16  flRot;
	int16  flColor;

	int16  fuPoints;
	int16  fuBullseye;
	int16  fuCount;
	uint16 fuProb;
	int16  fuWarpP;
	int16  fuPlayerPlus;
	int16  fuColor[5];

	int16  puPoints;
	int16  puCount;
	uint16 puProb;
	Fixed  puSpeed;
	int16  puRot;
	int16  puTime;
	int16  puColor;
	int16  puPulsDepth;

	AnyTanker tk[TANKTYPES];

	int16  spPoints;
	uint16 spProb;
	Fixed  spSpeed;
	int16  spStart;     /* spike length at the level start, -1 once set up */
	int16  spTop;
	int16  spPlasma;
	Fixed  spPlSpeed;
	int16  spPlPoints;
	int16  spSpikePoints;
	int16  spColor[3];

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

/* Hero.Flags (heroflags.h). The low four bits count a colour flash. */
#define HF_FLASH          0x000F
#define HF_STBONUSGIVEN   0x0010
#define HF_SHOWZAPMSG     0x0020
#define HF_SAVEDGAMESTART 0x0040
#define HF_SAVEDFIRSTDONE 0x0080
#define HF_FLAGSMASK      0x7FFF

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
	int32 freeLives;    /* the score that gives the next free life */
	uint16 Flags;       /* the HF_ bits above */
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
SEG_GAME void DrawStatusHud(void);
/* Plays a recorded game from the next step: n bytes of moves. */
SEG_GAME void demo_start(const uint8 *moves, int16 n);
SEG_GAME void demo_stop(void);
/* 0 stylus sets the claw's place, 1 dragging moves it. */
extern uint8 PlayControl;
SEG_GAME void DisplayScore(void);
SEG_GAME void DrawLevelNumber(void);
/* One game step of the claw: moves it toward its target lane. */
SEG_GAME void UpdatePlayer(const PlatInput *in);
SEG_GAME void DrawHero(void);
/* The enemy at status has the claw: starts the death (STPlayer's
   PlayerCapture). */
SEG_GAME void PlayerCapture(int16 status);

/* IncreaseScore of STPlayer.c: adds points and gives a free life each
   20000 points, up to 5 spare lives. */
SEG_GAME void IncreaseScore(int32 points);
SEG_GAME void AddLife(void);
SEG_GAME void RemoveLife(void);

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
   (STTanker.c), fuseball.c (STFuseBalls.c), pulsar.c (STPulsar.c), enemy.c (what STInitialize and the draw calls do over all
   of them). An enemy appears as a star at the centre that grows to the rim,
   which tells it where it lands. Each module has Init, Update (one step:
   moves, hit test, scores, sets Hero.lanestat and segmstat) and Draw
   (queues the picture of the last step; the original draws while it
   updates). Spikes are added the same way. */
/* One enemy on the field, for ListEnemies (the demo player and the tests). */
enum { EnemyFlipper, EnemyTanker, EnemyFuseball, EnemyPulsar };
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

SEG_ENEMY void InitFuseBalls(void);
SEG_ENEMY void UpdateFuseBalls(void);
SEG_ENEMY void DrawFuseBalls(void);
SEG_ENEMY void CreateNewBall(void);
SEG_ENEMY int16 ListFuseBalls(EnemyInfo *out, int16 max);
/* Two fuseballs from a tanker killed at lane, level. */
SEG_ENEMY void CreateSplitFuseBalls(int16 lane, int16 level);

SEG_ENEMY void InitPulsars(void);
SEG_ENEMY void UpdatePulsars(void);
SEG_ENEMY void DrawPulsars(void);
SEG_ENEMY void CreateNewPulsar(void);
SEG_ENEMY int16 ListPulsars(EnemyInfo *out, int16 max);
/* Two pulsars from a tanker killed at lane, level. */
SEG_ENEMY void CreateSplitPulsars(int16 lane, int16 level);

SEG_ENEMY void InitTankers(void);
SEG_ENEMY void UpdateTankers(void);
SEG_ENEMY void DrawTankers(void);
SEG_ENEMY void CreateNewTanker(int16 tanktype);
SEG_ENEMY int16 ListTankers(EnemyInfo *out, int16 max);

/* spikes.c (STSpikes.c): the spikers climb a lane leaving a spike behind,
   shots wear the spike down, and they can fire plasma down the lane. */
SEG_ENEMY void InitSpikes(void);
SEG_ENEMY void UpdateSpikes(void);
SEG_ENEMY void DrawSpikes(void);
/* Takes the spikers off the field and keeps the spikes (RedrawSpikes). */
SEG_ENEMY void RedrawSpikes(void);
/* The fly-through's part, once a step while the claw flies down the tube:
   shots against spikes, and a spike that reaches the claw sets CapSpike on
   its lane. */
SEG_ENEMY void FlySpikes(void);
/* Queues the spike lines FlySpikes tested, for this step's picture. */
SEG_ENEMY void DrawFlySpikes(int16 color);
/* For tests: the spike's top level (DEPTH or more is none), the spiker
   ball's level or -1, the plasma's level or -1. */
/* For tests: no spikes, no spikers and no new ones. */
SEG_ENEMY void ClearSpikes(void);
SEG_ENEMY void SpikerInfo(int16 lane, int16 *top, int16 *ball, int16 *plasma);

/* crack.c (STCrack.c and the BlowUp calls of Shapes.c): pieces that fly
   apart and turn. */
enum { FlipperLeft, FlipperRight, PlayerLeft, PlayerRight, PulsarCrack, FlyingVector };
SEG_ENEMY void InitCracks(void);
SEG_ENEMY void AddCrack(int16 x, int16 y, int16 xv, int16 yv, int16 color, int16 unitsize,
                        int16 part, int16 rot, int16 rotspeed, int16 count);
SEG_ENEMY void UpdateCracks(void);
SEG_ENEMY void DrawCracks(void);
SEG_ENEMY int16 CrackCount(void);
/* Breaks the claw at the rim into its two halves (CrackPlayer). */
SEG_ENEMY void CrackPlayer(void);
/* Breaks the claw drawn by DrawPlayer into flying lines (BlowUpPlayer). */
SEG_ENEMY void BlowUpPlayer(int16 cx, int16 cy, int16 dx, int16 dy, int16 slant);
/* BlowFuseBall sets the centre, spin and life, then BlowUpLine breaks the
   line from the pen (VAMoveTo) to x, y into a piece and moves the pen. */
SEG_ENEMY void BlowSetup(int16 cx, int16 cy, int16 rotspeed, int16 size);
SEG_ENEMY void BlowUpLine(int16 x, int16 y);
/* The pulsar shape of flipper.c and pulsar.c, for the pulsar's pieces. */
SEG_ENEMY void DrawPulsar(int16 cx, int16 cy, int16 dx, int16 dy, int16 power);

/* floating.c (STFloatingScores.c): the points above a dead fuseball. */
extern cbool ShowFloatingScores;
SEG_ENEMY void InitFloatingScores(void);
SEG_ENEMY void AddFLScore(int16 x, int16 y, int16 score);
SEG_ENEMY void UpdateFloatingScores(void);
SEG_ENEMY void DrawFloatingScores(void);
SEG_ENEMY int16 FloatingScoreCount(void);
/* The enemy part of STInitialize, after the level and grid are loaded. */
SEG_ENEMY void InitEnemies(void);
/* Every active enemy on the field (not the stars), at most max. */
SEG_ENEMY int16 ListEnemies(EnemyInfo *out, int16 max);
SEG_ENEMY void DrawEnemies(void);

#endif
