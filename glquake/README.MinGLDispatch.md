# Amiga 68k MiniGL-dispatch build

This target builds the GLQuake renderer for a 68060-class Amiga/PiStorm system
using the dispatch-based `minigl.library` client from the PiStorm SDK.  It does
not link against the legacy `mgl.library`, `mglasm.library`, or Warp3D library.

## Requirements

- `m68k-amigaos-gcc` with Amiga NDK headers.
- `vasmm68k_mot` with HUNK output support.
- PiStorm SDK containing `include/proto/minigl.h`, the bundled compile-time
  Warp3D type headers, and `lib/libminigl_dispatch.a`.
- AHI SDK headers, including `devices/ahi.h`.
- A real Amiga/PiStorm configuration that provides the corresponding
  `minigl.library` backend.

`Warp3D/Warp3D.h` is included only because the MiniGL headers declare Warp3D
types. This target does not link to a Warp3D library.

## Build

The defaults match the development machine. Override them for another setup:

```sh
make -f Makefile.GCCAmiga68k_MiniGLDispatch \
  CROSS_PREFIX=/opt/amiga/bin/m68k-amigaos- \
  VASM=/opt/amiga/bin/vasmm68k_mot \
  AMIGA_NDK=/opt/amiga/m68k-amigaos/ndk-include \
  PISTORM_SDK=/path/to/PiStorm_SDK_v10
```

The executable is written to `m68k_gcc/glquake/glquake68k_gcc` in Amiga HUNK
format. For a clean rebuild:

```sh
make -f Makefile.GCCAmiga68k_MiniGLDispatch clean
make -f Makefile.GCCAmiga68k_MiniGLDispatch
```

## Run

Copy the executable to the directory containing the licensed Quake `id1/`
game data. Run `launch_minigl` from that directory, or use equivalent commands:

```text
Stack 1048576
glquake68k_gcc -width 320 -height 240 -bpp 32 -windowmode
```

The `Stack 1048576` command is required when launching from an AmigaDOS shell
or script. Direct asynchronous launch helpers may not apply the executable's
requested stack size.

For Quake console logging after normal initialization, add `-condebug` (or
`-condebug filename`) to the launch command. The log is stored below the active
game directory, normally `id1/qconsole.log`.
