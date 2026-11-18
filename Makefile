# Arashi for PalmOS 3.1 (Palm IIIx and up). Needs m68k-palmos-gcc
# (prc-tools-remix), pilrc and build-prc. Output goes to build/.
#
#   make             builds build/Arashi.prc
#   make DEBUG_SCRIPT=1
#                    same, but at launch the app dumps the Arashi Script
#                    levels 1-99 to the ArashiScriptDump database
#   make EXTRA=-DDEBUG_HEAP
#                    shows the free dynamic heap at the bottom left (now, least
#                    in a frame, least after any allocation, failed allocations
#                    and the first one's size, free and largest chunk). Touch
#                    src/palm/main.c and plat_palm.c before and after.
#   make release     builds build/ArashiPocket-$(VERSION).prc, the PRC under
#                    its release name (build from clean, no EXTRA)
#   make host        builds the core with the PC's gcc and runs the host tests
#                    (renderer PNGs in build/host/va/)
#   make check-frond-script
#                    runs the DEBUG_SCRIPT build on the IIIx in Frond and diffs
#                    its dump with the host's (needs make host and
#                    make DEBUG_SCRIPT=1 first; FRONDOUT sets the folder)
#   make check-jam   compiles the original game sources against the Mac shim
#   make check-data  checks the extracted resources against the source fork and the PRC

# The version, shown on the title and in the PRC's VERSION resource.
VERSION  = 0.2.3

SDK      ?= ../palm-os-sdk/sdk-5r3
REF      = reference/Arashi/Arashi
BUILD    = build

CC       = m68k-palmos-gcc
SDKINC   = $(addprefix -I,$(shell find $(SDK)/include -type d))
CFLAGS   = -O2 -Wall -Wno-unknown-pragmas -mshort -DPALM_SEGMENTS $(SDKINC) $(EXTRA)
LDFLAGS  = -L$(SDK)/lib/m68k-palmos-coff
# The core is plain C with no Palm headers.
CORECFLAGS = -O2 -Wall -mshort -DPALM_SEGMENTS -DARASHI_VERSION='"$(VERSION)"' $(EXTRA)

CORE     = cmath qdrandom strdict tagbase parser scriptdump testpat layout va vatext vademo grid intro player shots game stars flipper fuseball pulsar tanker spikes crack floating enemy store ui menus panel front
CORESRC  = $(wildcard src/core/*.h)
PALMSRC  = $(wildcard src/palm/*.h)

# DEBUG_SCRIPT=1 builds into its own folder so switching never mixes objects.
ifeq ($(DEBUG_SCRIPT),1)
OBJDIR   = $(BUILD)/palm-dbg
CFLAGS  += -DDEBUG_SCRIPT
PALMCORE = $(CORE)
else
OBJDIR   = $(BUILD)/palm
PALMCORE = $(filter-out scriptdump,$(CORE))
endif

PRC      = $(BUILD)/Arashi.prc
LSEL     = $(BUILD)/data/4c53454c_128.bin
LVAL     = $(BUILD)/data/4c56414c_128.bin
# ASCR 128: the script, TEXT 2128 then 2129 and a 0, read in place.
ASCR     = $(BUILD)/data/41534352_128.bin
PALMOBJ  = main plat_palm device screen input sound
OBJS     = $(PALMOBJ:%=$(OBJDIR)/%.o) $(PALMCORE:%=$(OBJDIR)/core_%.o)

all: $(PRC)

$(BUILD) $(OBJDIR):
	mkdir -p $@

$(OBJDIR)/%.o: src/palm/%.c $(PALMSRC) $(CORESRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/core_%.o: src/core/%.c $(CORESRC) Makefile | $(OBJDIR)
	$(CC) $(CORECFLAGS) -c $< -o $@

# Code segments from arashi.def: multigen writes the segment tables, and
# segcheck fails the build on references that link but go wrong on the
# device (const tables outside the first segment, calls across segments
# without the segment attribute on the declaration).
$(OBJDIR)/sections.s $(OBJDIR)/sections.ld &: src/palm/arashi.def | $(OBJDIR)
	m68k-palmos-multigen -b $(OBJDIR)/sections src/palm/arashi.def

$(OBJDIR)/sections.o: $(OBJDIR)/sections.s
	$(CC) -c $< -o $@

$(OBJDIR)/Arashi: $(OBJS) $(OBJDIR)/sections.o $(OBJDIR)/sections.ld
	python3 tools/segtags.py src/core/*.c src/palm/*.c
	python3 tools/segcheck.py $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(OBJDIR)/sections.o $(OBJDIR)/sections.ld -o $@

# Game data: tools/rsrc.py pulls the resources the game loads out of the Mac
# resource fork (kept as AppleDouble in __MACOSX) into build/data/ as .bin
# files plus build/data.rcp, which arashi.rcp includes. Same type and ID as
# the original, byte for byte.
STORMFORK = reference/Arashi/__MACOSX/Arashi/Game Source/._ STORM.rsrc

$(BUILD)/data.rcp: tools/rsrc.py | $(BUILD)
	python3 -I tools/rsrc.py dump "$(STORMFORK)" $(BUILD)/data --rcp $@

# The menu font and logo (src/core/ui.c), made by tools/mkui.py from
# tools/ui/font.txt and LOGO 131.
UIART    = $(BUILD)/data/55494654_128.bin

$(UIART): tools/mkui.py tools/ui/font.txt $(BUILD)/data.rcp
	python3 -I tools/mkui.py tools/ui/font.txt $(BUILD)/data/4c4f474f_131.bin $(BUILD)/data

# Remade when the Makefile (and so VERSION) changes; arashi.rcp includes it.
$(BUILD)/version.rcp: Makefile | $(BUILD)
	echo 'VERSION "$(VERSION)"' > $@

$(BUILD)/arashi.ro: src/palm/arashi.rcp src/palm/resource.h $(BUILD)/version.rcp $(BUILD)/data.rcp $(LSEL) $(LVAL) $(ASCR) $(UIART) | $(BUILD)
	pilrc -q -ro -I src/palm -I . src/palm/arashi.rcp $@

# Always relinked, since the debug and normal builds share the PRC name.
$(PRC): src/palm/arashi.def $(OBJDIR)/Arashi $(BUILD)/arashi.ro FORCE
	build-prc -o $@ src/palm/arashi.def $(OBJDIR)/Arashi $(BUILD)/arashi.ro
	@m68k-palmos-size $(OBJDIR)/Arashi | tail -1 | awk '{print "code", $$1, "data", $$2, "bss", $$3}'
	@python3 tools/segsizes.py $(OBJDIR)/Arashi
	@ls -l $@ | awk '{print "PRC", $$5, "bytes"}'

release: $(PRC)
	cp $(PRC) $(BUILD)/ArashiPocket-$(VERSION).prc
	@echo "release PRC $(BUILD)/ArashiPocket-$(VERSION).prc"

# Host build of the core: PC gcc, 64-bit, little-endian, 32-bit int.
# build/host uses cmath, build/host-libm the C library's maths. The two
# level dumps are compared and the differences listed, which isn't a failure.
HOSTCC     = gcc
HOSTCFLAGS = -std=c89 -Wall -Wextra -g -O1 -fsanitize=address,undefined \
             -fno-sanitize-recover=all -DVA_STILL_CHECK -DARASHI_VERSION='"$(VERSION)"'
HOSTSRC    = $(filter-out src/core/va.c src/core/vatext.c src/core/vademo.c src/core/grid.c src/core/intro.c src/core/player.c src/core/shots.c src/core/game.c src/core/stars.c src/core/flipper.c src/core/fuseball.c src/core/pulsar.c src/core/tanker.c src/core/spikes.c src/core/crack.c src/core/floating.c src/core/enemy.c src/core/store.c src/core/ui.c src/core/menus.c src/core/panel.c src/core/front.c,$(CORE:%=src/core/%.c)) src/host/plat_host.c src/host/test_script.c

$(BUILD)/host/test_script: $(HOSTSRC) $(CORESRC) src/host/plat_host.h
	mkdir -p $(BUILD)/host
	$(HOSTCC) $(HOSTCFLAGS) $(HOSTSRC) -o $@ -lm

$(BUILD)/host-libm/test_script: $(HOSTSRC) $(CORESRC) src/host/plat_host.h
	mkdir -p $(BUILD)/host-libm
	$(HOSTCC) $(HOSTCFLAGS) -DCORE_LIBM $(HOSTSRC) -o $@ -lm

TPSRC      = src/core/testpat.c src/core/layout.c src/host/test_testpat.c src/host/png.c

$(BUILD)/host/test_testpat: $(TPSRC) $(CORESRC) src/host/png.h
	mkdir -p $(BUILD)/host
	$(HOSTCC) $(HOSTCFLAGS) $(TPSRC) -o $@

VASRC      = src/core/va.c src/core/vatext.c src/core/vademo.c src/core/testpat.c \
             src/core/layout.c src/core/parser.c src/core/strdict.c src/core/tagbase.c \
             src/core/cmath.c src/core/qdrandom.c src/host/plat_host.c src/host/png.c \
             src/host/test_va.c

$(BUILD)/host/test_va: $(VASRC) $(CORESRC) src/host/png.h src/host/plat_host.h
	mkdir -p $(BUILD)/host/va
	$(HOSTCC) $(HOSTCFLAGS) $(VASRC) -o $@ -lm

GAMESRC    = src/core/game.c src/core/grid.c src/core/intro.c src/core/player.c src/core/shots.c \
             src/core/stars.c src/core/flipper.c src/core/fuseball.c src/core/pulsar.c src/core/tanker.c src/core/spikes.c src/core/crack.c src/core/floating.c src/core/enemy.c src/core/store.c src/core/ui.c src/core/menus.c src/core/panel.c src/core/front.c src/core/va.c src/core/vatext.c \
             src/core/testpat.c src/core/layout.c src/core/parser.c src/core/strdict.c \
             src/core/tagbase.c src/core/cmath.c src/core/qdrandom.c src/host/plat_host.c \
             src/host/png.c src/host/test_game.c

$(BUILD)/host/test_game: $(GAMESRC) $(CORESRC) src/host/png.h src/host/plat_host.h
	mkdir -p $(BUILD)/host/game
	$(HOSTCC) $(HOSTCFLAGS) -DRAND_TRACE $(GAMESRC) -o $@ -lm

$(BUILD)/host/test_front: $(GAMESRC:src/host/test_game.c=src/host/test_front.c) $(CORESRC) src/host/png.h src/host/plat_host.h
	mkdir -p $(BUILD)/host/front
	$(HOSTCC) $(HOSTCFLAGS) $(GAMESRC:src/host/test_game.c=src/host/test_front.c) -o $@ -lm

# LSEL 128: the level select's facts of each level, from the script run on
# the host (game_level_choices reads it instead of running the script).
MKLSELSRC  = $(GAMESRC:src/host/test_game.c=src/host/mklsel.c)

$(BUILD)/host/mklsel: $(MKLSELSRC) $(CORESRC) src/host/plat_host.h
	mkdir -p $(BUILD)/host
	$(HOSTCC) $(HOSTCFLAGS) $(MKLSELSRC) -o $@ -lm

$(LSEL): $(BUILD)/host/mklsel $(BUILD)/data.rcp $(ASCR)
	$(BUILD)/host/mklsel $(BUILD)/data $@

# LVAL 128: every value of levels 1 to 99 from the script run on the host
# (a level in it loads without running the script).
MKLVALSRC  = $(GAMESRC:src/host/test_game.c=src/host/mklval.c)

$(BUILD)/host/mklval: $(MKLVALSRC) $(CORESRC) src/host/plat_host.h
	mkdir -p $(BUILD)/host
	$(HOSTCC) $(HOSTCFLAGS) $(MKLVALSRC) -o $@ -lm

$(LVAL): $(BUILD)/host/mklval $(BUILD)/data.rcp $(ASCR)
	$(BUILD)/host/mklval $(BUILD)/data $@

$(ASCR): $(BUILD)/data.rcp
	cat $(BUILD)/data/54455854_2128.bin $(BUILD)/data/54455854_2129.bin > $@
	printf '\000' >> $@

host: $(LSEL) $(LVAL) $(ASCR) $(UIART) $(BUILD)/host/test_front $(BUILD)/host/test_script $(BUILD)/host-libm/test_script $(BUILD)/host/test_testpat $(BUILD)/host/test_va $(BUILD)/host/test_game $(BUILD)/data.rcp
	$(BUILD)/host/test_testpat $(BUILD)/host
	$(BUILD)/host/test_va $(BUILD)/data $(BUILD)/host/va
	$(BUILD)/host/test_game $(BUILD)/data $(BUILD)/host/game
	$(BUILD)/host/test_front $(BUILD)/data $(BUILD)/host/front
	$(BUILD)/host/test_script $(BUILD)/data $(BUILD)/host/levels.txt $(BUILD)/host/maths.txt
	$(BUILD)/host-libm/test_script $(BUILD)/data $(BUILD)/host-libm/levels.txt
	@if diff $(BUILD)/host/levels.txt $(BUILD)/host-libm/levels.txt > $(BUILD)/host/libm.diff; \
	then echo "cmath and libm level dumps match"; \
	else echo "cmath and libm level dumps differ in $$(grep -c '^<' $(BUILD)/host/libm.diff) lines, see $(BUILD)/host/libm.diff"; fi

# The PRC must be a DEBUG_SCRIPT=1 build. Frond runs uncapped; the dump's
# own tick counts are guest time, so they still give IIIx speed.
FRONDOUT ?= /srv/downloads/temp/debug/ArashiPocket/script-dump
check-frond-script:
	python3 tools/frond-iiix.py $(FRONDOUT) --seconds 120 --speed uncapped \
	  --export-db ArashiScriptDump
	python3 -I tools/pdbtext.py $(FRONDOUT)/ArashiScriptDump.pdb $(FRONDOUT)/frond-dump.txt
	@grep '^#' $(FRONDOUT)/frond-dump.txt
	cat $(BUILD)/host/levels.txt $(BUILD)/host/maths.txt | grep -v '^#' > $(FRONDOUT)/host-dump.txt
	grep -v '^#' $(FRONDOUT)/frond-dump.txt | diff $(FRONDOUT)/host-dump.txt - \
	  > $(FRONDOUT)/host-vs-frond.diff && echo "host and Frond dumps match"

# Game sources from the original, unmodified. tools/prep.py copies them into
# build/jam/ with line ends, MacRoman, Pascal strings and Think C low-memory
# globals fixed, then each .c is compiled with src/jamcheck/macshim.h forced
# in. Not compiled: new.GameLoop.c (uses FirstLevelMask, defined nowhere),
# GamePause.new.c (older variant of GamePause.c, same names), LineTest.c
# (needs Line256.h, which isn't in the repo).
JAMDIR   = $(REF)/Game Source/jam src
JAMSKIP  = new.GameLoop.c GamePause.new.c LineTest.c
JAMCFLAGS = -O2 -mshort -Wno-unknown-pragmas -Isrc/jamcheck/shim -Isrc/jamcheck \
            -I$(BUILD)/jam -include src/jamcheck/macshim.h

prep: | $(BUILD)
	python3 -I tools/prep.py $(BUILD)/jam "c:$(JAMDIR)" \
	  "$(REF)/Game Source/mtz/mtz src" "$(REF)/Game Source/jaf src" \
	  "$(REF)/Game Source/GameCompiler (jam)" \
	  "$(REF)/For your think c folder/VA Kit ƒ"

check-jam: prep
	@fail=0; for f in $(BUILD)/jam/*.c; do \
	  n=$$(basename $$f); \
	  case " $(JAMSKIP) " in *" $$n "*) echo "skip  $$n"; continue;; esac; \
	  if $(CC) $(JAMCFLAGS) -c $$f -o $(BUILD)/jam/$$n.o 2>$(BUILD)/jam/$$n.log; \
	  then echo "ok    $$n"; else echo "FAIL  $$n"; fail=1; fi; \
	done; exit $$fail

check-data: $(PRC)
	python3 -I tools/check-data.py "$(STORMFORK)" $(BUILD)/data $(PRC)

clean:
	rm -rf build

.PHONY: all release host check-frond-script prep check-jam check-data clean FORCE
FORCE:
