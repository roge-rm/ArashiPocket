/* Sound for plat.h. On OS 5 (gDev.sampled) the original's samples play
   from their 'snd ' resources (128 up, made by tools/rsrc.py) through
   SndPlayResource, without waiting. Before OS 5 each sound is a short tone
   from SndDoCmd's sndCmdFrqOn, which doesn't wait either (OS 3.0 and on).
   Both are silent when the game sound preference is off; OS 5 also uses
   its volume. A sound of lower priority than the one still playing is
   dropped, as the original's channel A did. A resource is locked the first
   time it plays and stays locked (it is in storage, so it costs no dynamic
   heap) until sound_exit, as it may still be playing. */
#include "palm.h"

#define SND_TYPE 'snd '
#define SND_BASE 128
#define SND_HZ   11127UL
#ifndef TONE_SHORT_MS
#define TONE_SHORT_MS 15
#endif
#define TONE_LONG_PRI 100

/* A tone for each sound before OS 5: frequency in Hz and length in ms. */
static const UInt16 tones[SND_COUNT][2] = {
	{ 110, 120 },   /* Blast */
	{ 90, 80 },     /* Blow */
	{ 330, 15 },    /* Bonk */
	{ 1250, 45 },   /* Dziung */
	{ 1800, 20 },   /* PhazerIn */
	{ 600, 60 },    /* PhazerOut */
	{ 420, 200 },   /* FallYell */
	{ 200, 150 },   /* FallZap */
	{ 150, 300 },   /* FlyThru */
	{ 1000, 30 },   /* Phazer2 */
	{ 700, 40 },    /* Splitter */
	{ 500, 40 },    /* Springy */
	{ 900, 30 },    /* Swish */
	{ 800, 30 },    /* Swoosh */
	{ 2100, 40 },   /* Whiz */
	{ 250, 50 },    /* Zroom */
	{ 1500, 150 },  /* ZZFreeLife */
	{ 100, 200 }    /* ZZSuperZap */
};

#ifdef SND_PROFILE
UInt32 snd_calls, snd_ticks;    /* tone calls and the ticks spent in SndDoCmd */
#endif

static MemHandle res[SND_COUNT];        /* locked 'snd ' resources */
static UInt8 *data[SND_COUNT];
static UInt32 playing_end;      /* tick the sound playing ends */
static Int16 playing_pri;

/* The game sound volume, 0 to sndMaxAmp, 0 when the preference is off.
   prefGameSoundVolume is only read on OS 5. */
static UInt16 game_volume(void)
{
	UInt16 v;

	if (PrefGetPreference(prefGameSoundLevelV20) == slOff)
		return 0;
	if (!gDev.sampled)
		return sndMaxAmp;
	v = (UInt16)PrefGetPreference(prefGameSoundVolume);
	return v > sndMaxAmp ? sndMaxAmp : v;
}

void plat_sound(int16 snd, int16 priority)
{
	UInt32 now = TimGetTicks();
	UInt16 vol;

	if (snd < 0 || snd >= SND_COUNT)
		return;
	if (now < playing_end && priority < playing_pri)
		return;
	vol = game_volume();
	if (!vol)
		return;
	if (gDev.sampled) {
		UInt8 *p;
		UInt32 len;

		if (!res[snd]) {
			res[snd] = DmGetResource(SND_TYPE, (DmResID)(SND_BASE + snd));
			if (!res[snd])
				return;
			data[snd] = (UInt8 *)MemHandleLock(res[snd]);
		}
		p = data[snd];
		/* The sample count is in the sound header, 20 bytes in. */
		len = ((UInt32)p[24] << 24) | ((UInt32)p[25] << 16) | ((UInt32)p[26] << 8) | p[27];
		if (SndPlayResource(p, (Int32)vol * 1024 / sndMaxAmp, sndFlagAsync) != errNone)
			return;
		playing_end = now + len * gDev.ticksPerSecond / SND_HZ;
	} else {
		SndCommandType cmd;

		UInt16 ms = tones[snd][1];

		/* A tone slows the game while it sounds on the slowest devices, so
		   the short repeating sounds (priority under TONE_LONG_PRI: shots,
		   lane changes, kills, plasma) are cut to TONE_SHORT_MS. The long ones
		   (the claw's deaths, fly-through, the title) play in full. */
		if (priority < TONE_LONG_PRI && ms > TONE_SHORT_MS)
			ms = TONE_SHORT_MS;
		cmd.cmd = sndCmdFrqOn;
		cmd.reserved = 0;
		cmd.param1 = tones[snd][0];
#ifdef SND_MAXMS
		cmd.param2 = ms < SND_MAXMS ? ms : SND_MAXMS;
#else
		cmd.param2 = ms;
#endif
		cmd.param3 = vol;
#ifdef SND_PROFILE
		{
			UInt32 t0 = TimGetTicks();

			SndDoCmd(NULL, &cmd, true);
			snd_ticks += TimGetTicks() - t0;
			snd_calls++;
		}
#else
		SndDoCmd(NULL, &cmd, true);
#endif
		playing_end = now + (UInt32)ms * gDev.ticksPerSecond / 1000;
	}
	playing_pri = priority;
}

void sound_exit(void)
{
	Int16 i;

	for (i = 0; i < SND_COUNT; i++)
		if (res[i]) {
			MemHandleUnlock(res[i]);
			DmReleaseResource(res[i]);
			res[i] = NULL;
			data[i] = NULL;
		}
}
