# Arashi for PalmOS 3.1 (Palm IIIx and up). Needs m68k-palmos-gcc
# (prc-tools-remix), pilrc and build-prc. Output goes to build/.
#
#   make             builds build/Arashi.prc
#   make check-jam   compiles the original game sources against the Mac shim

SDK      ?= ../palm-os-sdk/sdk-3.1
REF      = reference/Arashi/Arashi
BUILD    = build

CC       = m68k-palmos-gcc
SDKINC   = $(addprefix -I,$(shell find $(SDK)/include -type d))
CFLAGS   = -O2 -Wall -Wno-unknown-pragmas -mshort $(SDKINC)

PRC      = $(BUILD)/Arashi.prc
OBJS     = $(BUILD)/main.o

all: $(PRC)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c src/resource.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/Arashi: $(OBJS)
	$(CC) $(OBJS) -o $@

$(BUILD)/arashi.ro: src/arashi.rcp src/resource.h | $(BUILD)
	pilrc -q -ro -I src src/arashi.rcp $@

$(PRC): src/arashi.def $(BUILD)/Arashi $(BUILD)/arashi.ro
	build-prc -o $@ src/arashi.def $(BUILD)/Arashi $(BUILD)/arashi.ro

# Game sources from the original, unmodified. tools/prep.py copies them into
# build/jam/ with line ends, MacRoman, Pascal strings and Think C low-memory
# globals fixed, then each .c is compiled with src/macshim.h forced in.
# Not compiled: new.GameLoop.c (uses FirstLevelMask, defined nowhere),
# GamePause.new.c (older variant of GamePause.c, same names), LineTest.c
# (needs Line256.h, which isn't in the repo).
JAMDIR   = $(REF)/Game Source/jam src
JAMSKIP  = new.GameLoop.c GamePause.new.c LineTest.c
JAMCFLAGS = -O2 -mshort -Wno-unknown-pragmas -Isrc/shim -Isrc -I$(BUILD)/jam \
            -include src/macshim.h

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

clean:
	rm -rf build

.PHONY: all prep check-jam clean
