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

#endif
