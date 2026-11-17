/* Host-only extras for the PC backend. */
#ifndef HOST_PLAT_HOST_H
#define HOST_PLAT_HOST_H

#include "../core/plat.h"

/* Folder holding the TTTTTTTT_ID.bin resource files. */
void host_set_data_dir(const char *dir);

/* A clock the test sets (1000 ticks a second) in place of the real one, and
   the input plat_input returns. */
void host_set_clock(int on, uint32 ticks);
void host_set_input(const PlatInput *in);
/* The saved blob is gone, as on a first run. */
void host_forget_save(void);
/* 0 makes plat_keep_alloc fail (no still picture). */
extern int host_keep;

/* plat_sound keeps the last HOST_SOUNDS sounds played, and writes a line
   (name and priority) for each to the log file if one is open. */
#define HOST_SOUNDS 4096
extern int16 host_sound[HOST_SOUNDS];
extern int host_sounds;
void host_sound_log(const char *path);
const char *host_sound_name(int16 snd);

#endif
