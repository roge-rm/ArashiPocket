/* Sound Kit replacement. Sound comes later (M8) as Palm tones. */
#include "macshim.h"

typedef struct { int volume; } VVars;
extern VVars Vv;
extern int OldSound;
void SKVolume(int v);
void InitSoundKit(void);
