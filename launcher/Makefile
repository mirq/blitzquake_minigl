# BlitzQuakePPC MiniGL Launcher by Mirq -- 68k AmigaOS build
#
# Cross-build from Linux with the same toolchain as the 68k dispatch target:
#   make -C launcher
# Output: build/BlitzLaunch (Amiga HUNK executable).
#
# The launcher itself is plain 68k: it only needs intuition, graphics,
# gadtools, asl and dos.  It never links the game or MiniGL.

CROSS_PREFIX ?= /opt/amiga/bin/m68k-amigaos-
CC            = $(CROSS_PREFIX)gcc
NDK          ?= /opt/amiga/m68k-amigaos/ndk-include
BUILDDIR      = build

CFLAGS  = -noixemul -m68060 -O2 -Wall -Wno-unknown-pragmas -fno-omit-frame-pointer -I$(NDK) -I.
LDFLAGS = -noixemul -m68060 -s

OBJS = $(BUILDDIR)/main.o $(BUILDDIR)/gui.o $(BUILDDIR)/config.o $(BUILDDIR)/launch.o

all: $(BUILDDIR)/BlitzLaunch

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: %.c launcher.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/BlitzLaunch: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

# Launch-mechanism smoke test (writes T:argdump.log when run on the Amiga)
$(BUILDDIR)/argdump: test/argdump.c | $(BUILDDIR)
	$(CC) -noixemul -m68060 -O2 -s -I$(NDK) -o $@ $<

# Settings save/load test (writes T:cfgtest.log when run on the Amiga)
$(BUILDDIR)/cfgtest: test/cfgtest.c config.c launcher.h | $(BUILDDIR)
	$(CC) -noixemul -m68060 -O2 -I$(NDK) -I. -o $@ test/cfgtest.c config.c

# Client-start path test: real lq_start_client() with argdump as the client.
# Deploy next to argdump (both in Work:wosbuild).
$(BUILDDIR)/launchtest: test/launchtest.c launch.c launcher.h | $(BUILDDIR)
	$(CC) -noixemul -m68060 -O2 -I$(NDK) -I. -o $@ test/launchtest.c launch.c

test: $(BUILDDIR)/argdump $(BUILDDIR)/cfgtest $(BUILDDIR)/launchtest

clean:
	rm -f $(BUILDDIR)/*.o $(BUILDDIR)/BlitzLaunch $(BUILDDIR)/argdump $(BUILDDIR)/cfgtest $(BUILDDIR)/launchtest

.PHONY: all clean test
