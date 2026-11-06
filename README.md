# BlitzQuake_src

Source tree for **BlitzQuake** — Christian Michael's 2002 Amiga port of
Quake/GLQuake and QuakeWorld — plus the 2026 Amiga port and optimization
work.

The 2026 work maintains two targets:

| Target | Build | Guide |
|---|---|---|
| 68060-class Amiga + PiStorm SDK MiniGL dispatch client | `make -f Makefile.GCCAmiga68k_MiniGLDispatch` in `glquake/` | [docs/targets/68k.md](docs/targets/68k.md) |
| A4000 + MPC7410 WarpOS PPC + Prometheus + Radeon 9200 (MiniGL V19 R200) | `make -f Makefile.GCCAmigaWOS_MGL_V19` in `glquake/` | [docs/targets/warpos.md](docs/targets/warpos.md) |

Everything else — Linux, Solaris, MorphOS, PowerUP, the 2002 vbcc/GCC Amiga
builds and the `glqwcl/` QuakeWorld tree — is retained as legacy reference
builds; see [docs/targets/legacy.md](docs/targets/legacy.md).

## Documentation

Start at **[docs/README.md](docs/README.md)**. Highlights:

- [Architecture overview](docs/architecture/overview.md) — what runs where.
- [Renderer](docs/architecture/renderer.md) — frame pipeline, world
  surfaces, lightmaps, cvars.
- [MiniGL dispatch](docs/architecture/minigl-dispatch.md) — how GL calls
  reach the Radeon, CP emit, texture uploads.
- [Platform layer](docs/architecture/platform.md) — sys, video, sound,
  network.
- [Building](docs/building.md) and [operations runbook](docs/operations/runbook.md).
- [Performance status](docs/performance/README.md) — current numbers and
  history.
- [Rejected paths](docs/decisions/REJECTED_PATHS.md) — measured dead ends.

## Quick build (WarpOS/PPC)

```sh
cd glquake
make -f Makefile.GCCAmigaWOS_MGL_V19 \
  MGL_DIR=/home/mirek/MiniGL_WOS_V19_mglQ3
# -> warpos_gcc_v19/glquake/glquake_wos.warpelf
# convert on the Amiga:
#   Stack 100000
#   Elf2Exe2 glquake_wos.warpelf glquakeWOS
```

Requires `ppc-amigaos-gcc`, the MOS2WOS WarpOS SDK overlay, and the MiniGL
V19 tree. See [docs/building.md](docs/building.md) for the 68k target,
validation gates and toolchain variables.

## Game data

Quake requires licensed `id1/` game data next to the executable (PAK files,
config). The repository contains source only. Add `timedemo demo1` and
`host_speeds 1` to `id1/autoexec.cfg` for benchmark sessions.

## Historical notes

- `BlitzQuake_src.readme` — original Aminet release blurb.
- `glquake/glquake.readme`, `glquake/glquake68k.readme`,
  `glquake/GLQuake_src.readme`, `glqwcl/qwcl68k.readme` — original 2002
  documentation.
- `glquake/COPYING` — GNU General Public License (the Quake source is GPL;
  the game data is not).
