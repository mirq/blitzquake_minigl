# BlitzQuake Source Code

Multi-platform Quake I/II renderer and QuakeWorld client (cross-compiled from 2002).

## Structure

- `glquake/` - GL renderer for Quake I/II (main directory of overall source tree)
- `glqwcl/` - QuakeWorld client (additional client code)

## Building (Linux)

### Quick debug build:
```bash
cd glquake
make -f Makefile.linuxi386 BUILDDIR=build_debug
make -f Makefile.linuxi386 BUILDDIR=build_debug ALL
```

### Release build:
```bash
make -f Makefile.linuxi386 BUILDDIR=build_release
make -f Makefile.linuxi386 BUILDDIR=build_release ALL
```

Builds target: `bin/glquake`, `bin/glquake.glx` (Mesa3D), `bin/quake.x11`.

Requires EGCS/GCC compiler; Mesa3D/X11 libraries.

## Cross-platform builds

Multiple platform Makefiles in both subdirectories:

- `Makefile.linuxi386` - Linux x86 (EGCS)
- `Makefile.Amiga68k` - Amiga 680x0 (StormC)
- `Makefile.AmigaPUp` - Amiga PowerUp (Turboc)
- `Makefile.GCCAmiga.*` - Amiga with GCC
- `Makefile.AmigaWOS` - Amiga WarpOS
- `Makefile.MorphOS` - MorphOS
- `Makefile.Solaris` - Solaris
- Platform-specific subdirectories for input, sound, video drivers

Assembly optimizations: platform-specific `.s` files (e.g., `r_alias68k.s`, `mathlib68k.s`, `snd_mixamigaPPC.s`).

See per-target `*.readme` files for configuration notes.

## Known issues (from original source)

- Particles crash on exit with sprites disabled
- Gamma失调 on Amiga
- Assembly header generator required: `genasmheaders` calculates structure offsets for assembly files

## Original era notes

Codebase originally ported 1996-2002; last updated Dec 2002.
Legacy build system using Makefiles; no modern CI or automated tests.