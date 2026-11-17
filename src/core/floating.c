/* The points that float above a dead fuseball, from STFloatingScores.c by
   Mike Zimmerman. A score shows for STARTAGE steps. The original draws it
   once and erases it later; here it is queued every frame. */
#include "storm.h"

#define STARTAGE   40
#define MAXFLSCORES 20

typedef struct {
	int16 age;
	int16 score;
	int16 x, y;
} FloatingScore;

/* The option "show floating scores"; on until the options screen has it. */
cbool ShowFloatingScores = 1;

static FloatingScore FLScores[MAXFLSCORES];

SEG_ENEMY void InitFloatingScores(void)
{
	int16 i;

	for (i = 0; i < MAXFLSCORES; i++)
		FLScores[i].age = 0;
}

SEG_ENEMY void AddFLScore(int16 x, int16 y, int16 score)
{
	int16 i;

	for (i = 0; i < MAXFLSCORES; i++)
		if (FLScores[i].age <= 0) {
			FLScores[i].age = STARTAGE;
			FLScores[i].score = score;
			FLScores[i].x = x;
			FLScores[i].y = y;
			return;
		}
}

/* They go when they are old, or at once when the claw flies down. */
SEG_ENEMY void UpdateFloatingScores(void)
{
	int16 i;

	for (i = 0; i < MAXFLSCORES; i++)
		if (FLScores[i].age > 0) {
			FLScores[i].age--;
			if (Hero.state == HeroFlying)
				FLScores[i].age = 0;
		}
}

SEG_ENEMY void DrawFloatingScores(void)
{
	int16 i, scale = (int16)(VA.frame.bottom >> 5);

	if (scale < 1)
		scale = 1;
	for (i = 0; i < MAXFLSCORES; i++)
		if (FLScores[i].age > 0) {
			VA.color = BG1;
			VA.segmscale = scale;
			VADrawNumber(FLScores[i].score, FLScores[i].x, FLScores[i].y);
		}
}

SEG_ENEMY int16 FloatingScoreCount(void)
{
	int16 i, n = 0;

	for (i = 0; i < MAXFLSCORES; i++)
		if (FLScores[i].age > 0)
			n++;
	return n;
}
