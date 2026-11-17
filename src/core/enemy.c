/* What the original's STInitialize and STUpdate run over every enemy
   (STInterface.c). A new enemy module adds its Init and Draw calls here and
   its Update call in game_tick. */
#include "storm.h"

cbool EnemySpawning = 1;

SEG_ENEMY void InitEnemies(void)
{
	InitCStars();
	InitSpikes();
	InitFlippers();
	InitFuseBalls();
	InitPulsars();
	InitTankers();
	InitCracks();
	InitFloatingScores();
}

SEG_ENEMY void DrawEnemies(void)
{
	DrawCStars();
	DrawFlippers();
	DrawFuseBalls();
	DrawPulsars();
	DrawTankers();
	DrawSpikes();
	DrawCracks();
	DrawFloatingScores();
}

SEG_ENEMY int16 ListEnemies(EnemyInfo *out, int16 max)
{
	int16 n = ListFlippers(out, max);

	n = (int16)(n + ListFuseBalls(out + n, (int16)(max - n)));
	n = (int16)(n + ListPulsars(out + n, (int16)(max - n)));
	return (int16)(n + ListTankers(out + n, (int16)(max - n)));
}
