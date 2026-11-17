/* PC backend for host tests: resources come from the files tools/rsrc.py
   writes, DATADIR/TTTTTTTT_ID.bin (type as 8 hex digits). Memory is
   malloc. The rest of plat.h isn't needed by the host tests yet. */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../core/plat.h"
#include "plat_host.h"

static const char *data_dir = "build/data";

void host_set_data_dir(const char *dir)
{
	data_dir = dir;
}

void *plat_alloc(uint32 size)
{
	return malloc(size ? size : 1);
}

void plat_free(void *p)
{
	free(p);
}

const uint8 *plat_resource(uint32 type, int16 id, uint32 *size)
{
	char path[512];
	FILE *f;
	long n;
	uint8 *data;

	sprintf(path, "%.400s/%08lx_%d.bin", data_dir, (unsigned long)type, (int)id);
	f = fopen(path, "rb");
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	data = (uint8 *)malloc(n > 0 ? (size_t)n : 1);
	if (data && n > 0 && fread(data, 1, (size_t)n, f) != (size_t)n) {
		free(data);
		data = NULL;
	}
	fclose(f);
	if (data && size)
		*size = (uint32)n;
	return data;
}

void plat_release(const uint8 *data)
{
	free((void *)data);
}

static int fake_clock;
static uint32 fake_ticks;
static PlatInput fake_input;

void host_set_clock(int on, uint32 ticks)
{
	fake_clock = on;
	fake_ticks = ticks;
}

void host_set_input(const PlatInput *in)
{
	fake_input = *in;
}

void plat_input(PlatInput *in)
{
	*in = fake_input;
}

uint32 plat_ticks(void)
{
	if (fake_clock)
		return fake_ticks;
	return (uint32)(clock() / (CLOCKS_PER_SEC / 1000));
}

uint32 plat_ticks_per_second(void)
{
	return 1000;
}

/* One saved blob kept in memory. */
static unsigned char saved_blob[4096];
static int32 saved_size = -1;

cbool plat_save(const char *name, const void *data, uint32 size)
{
	unsigned char *p = saved_blob;
	const unsigned char *d = (const unsigned char *)data;
	uint32 i;

	(void)name;
	if (size > sizeof(saved_blob))
		return 0;
	for (i = 0; i < size; i++)
		p[i] = d[i];
	saved_size = (int32)size;
	return 1;
}

int32 plat_load(const char *name, void *data, uint32 size)
{
	unsigned char *o = (unsigned char *)data;
	uint32 i;

	(void)name;
	if (saved_size < 0 || (uint32)saved_size > size)
		return -1;
	for (i = 0; i < (uint32)saved_size; i++)
		o[i] = saved_blob[i];
	return saved_size;
}

void host_forget_save(void)
{
	saved_size = -1;
}

/* ---- sound ---- */

int16 host_sound[HOST_SOUNDS];
int host_sounds;
static FILE *sound_log;

static const char *const sound_names[SND_COUNT] = {
	"Blast", "Blow", "Bonk", "Dziung", "PhazerIn", "PhazerOut", "FallYell", "FallZap", "FlyThru",
	"Phazer2", "Splitter", "Springy", "Swish", "Swoosh", "Whiz", "Zroom", "ZZFreeLife", "ZZSuperZap"
};

const char *host_sound_name(int16 snd)
{
	return snd >= 0 && snd < SND_COUNT ? sound_names[snd] : "?";
}

void host_sound_log(const char *path)
{
	if (sound_log)
		fclose(sound_log);
	sound_log = path ? fopen(path, "w") : NULL;
}

void plat_sound(int16 snd, int16 priority)
{
	if (host_sounds < HOST_SOUNDS)
		host_sound[host_sounds++] = snd;
	if (sound_log)
		fprintf(sound_log, "%s %d\n", host_sound_name(snd), priority);
}
