/* What the original's STInitialize and STUpdate run over every enemy
   (STInterface.c). A new enemy module adds its Init and Draw calls here and
   its Update call in game_tick. */
#include "storm.h"

cbool EnemySpawning = 1;

SEG_ENEMY void InitEnemies(void)
{
	InitCStars();
	InitFlippers();
	InitTankers();
}

SEG_ENEMY void DrawEnemies(void)
{
	DrawCStars();
	DrawFlippers();
	DrawTankers();
}

SEG_ENEMY void ExplodeEnemy(int16 x, int16 y, int16 level, int16 color)
{
	VAExplosion(x, y, (int16)(3 - (level << 2) / (DEPTH + 1)), color);
}

SEG_ENEMY int16 ListEnemies(EnemyInfo *out, int16 max)
{
	int16 n = ListFlippers(out, max);

	return (int16)(n + ListTankers(out + n, (int16)(max - n)));
}
