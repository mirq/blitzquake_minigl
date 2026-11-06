# AGENTS.md — BlitzQuake source tree

This file is the operating guide for automated agents (and humans) working in
this repository. The full documentation lives in **[docs/README.md](docs/README.md)**;
read the relevant page before changing an area you have not touched before.

## What this is

The 2002 BlitzQuake source (Christian Michael's Amiga port of GLQuake and
GLQuakeWorld, based on Frank Wille / Steffen Haeuser / Massimiliano Tretene
work) plus the ongoing 2026 Amiga port and optimization effort.

Actively maintained targets:

| Target | Build | Guide |
|---|---|---|
| 68060-class Amiga + PiStorm SDK MiniGL dispatch client | `glquake/Makefile.GCCAmiga68k_MiniGLDispatch` | [docs/targets/68k.md](docs/targets/68k.md) |
| A4000 + MPC7410 WarpOS PPC + Prometheus + Radeon 9200, MiniGL V19 R200 | `glquake/Makefile.GCCAmigaWOS_MGL_V19` | [docs/targets/warpos.md](docs/targets/warpos.md) |
| 68k front-end for the WarpOS client | `launcher/Makefile` | [docs/operations/launcher.md](docs/operations/launcher.md) |

If a task says "Amiga" without naming the target (68060 vs WarpOS/PPC),
**ask which one** before implementing. The two differ in toolchain, ABI,
runtime model and performance characteristics.

All other Makefiles (`Makefile.linuxi386`, `Makefile.Solaris`,
`Makefile.Amiga68k*`, `Makefile.AmigaWOS*`, `Makefile.AmigaPUp`,
`Makefile.MorphOS`, `glqwcl/*`) are legacy reference builds; see
[docs/targets/legacy.md](docs/targets/legacy.md).

## Non-negotiable platform rules

1. **Big-endian.** 68k and PPC are big-endian; all file/network data (PAK,
   BSP, MDL, SPR, WAV, packets) is little-endian and must use
   `LittleShort`/`LittleLong`/`BigShort`/`BigLong`. Never type-pun binary data
   or assume host endianness. The swap function pointers are set once in
   `COM_Init` (`glquake/common.c`).
2. **Alignment.** 68060 and PPC fault on unaligned word/long accesses. Keep
   asm-visible structures naturally aligned, keep hunk data 16-byte aligned,
   and never pass a native MOS2WOS struct by pointer to 68k device/library
   code — use the packed mirrors in `glquake/wos_ahi_abi.h`
   ([ABI trap](docs/targets/warpos.md#the-packed-struct-abi-trap)).
3. **Bounded memory, no per-frame churn.** Map-lifetime data goes in the
   Quake hunk, allocated once per map; hot paths must not allocate. Chip RAM
   is scarce; WarpOS PPC RAM is separate from 68k RAM.
4. **WarpOS LP calls are expensive (ms).** Never add a PPC→68k OS call to a
   per-frame path; cache once per frame like `WOS_FrameClockTick()`.
5. **Assembly reads structure offsets.** The legacy Amiga builds generate
   `quakedef68k.i`/`quakedefPPC.i` etc. with `genasmheaders`. If you change a
   struct used by a `.s` file, regenerate the headers for **every affected
   target** ([how](docs/building.md#assembly-offset-headers)).
6. **Do not retry rejected experiments.** Read
   [docs/decisions/REJECTED_PATHS.md](docs/decisions/REJECTED_PATHS.md) first.
   In particular: the generic client-array path, the `r_worldbatch` batching
   line, W-80 fence coalescing, and `MGLPPC_EXPAND_MIN < 4` are closed or
   parked for measured reasons.

## Documentation is mandatory

**Every change that alters behaviour, the build, a runtime switch, or a
measured result MUST update the matching documentation page in the same
change set. Documentation is part of "done" — never deferred.**

The mapping from change to document:

| Change | Update |
|---|---|
| Renderer path, frame ordering, cvar | [docs/architecture/renderer.md](docs/architecture/renderer.md) |
| MiniGL client/host protocol, texture upload path, DLL expectations | [docs/architecture/minigl-dispatch.md](docs/architecture/minigl-dispatch.md) |
| sys/video/sound/network behaviour | [docs/architecture/platform.md](docs/architecture/platform.md) |
| Makefile, toolchain, build flags, artifact paths | [docs/building.md](docs/building.md) + matching [docs/targets/](docs/targets/) page |
| New WarpOS landmine or fix | [docs/targets/warpos.md](docs/targets/warpos.md) |
| 68k/assembly/header-generator change | [docs/targets/68k.md](docs/targets/68k.md) |
| Deployment, session, recovery procedure | [docs/operations/runbook.md](docs/operations/runbook.md) |
| New test, probe or evidence artifact | [docs/operations/testing.md](docs/operations/testing.md) |
| Benchmark / hitch / A-B result | [docs/performance/README.md](docs/performance/README.md) **and** a dated log in [docs/performance/history/](docs/performance/history/) |
| Rejected / parked / quarantined experiment | [docs/decisions/REJECTED_PATHS.md](docs/decisions/REJECTED_PATHS.md) |
| Add / rename / remove a doc | [docs/README.md](docs/README.md) index |

Mermaid diagrams exist twice by convention: the canonical source in
`docs/diagrams/<name>.mmd` and an inline ` ```mermaid ` fence preceded by
`<!-- mmd: <name>.mmd -->` in the page. **Update both copies together.**

Before finishing any task run:

```sh
python3 tools/check_docs.py
```

It validates relative links, the `.mmd` ↔ inline-mermaid contract, and that
every diagram is referenced. A failure blocks the task.

When recording measurements, always include: date, exact command line,
resolution/options, artifact CRC32s, parity counters (`failed=0`, presents,
dispatches/vertices), and the number of runs. Single samples and gains below
2% are noise ([protocol](docs/operations/runbook.md#benchmark-protocol)).

## Build commands

```sh
# 68060 + PiStorm MiniGL dispatch (Amiga HUNK output)
cd glquake && make -f Makefile.GCCAmiga68k_MiniGLDispatch

# 68k launcher for the WarpOS client (Amiga HUNK output)
cd launcher && make            # build/BlitzLaunch
cd launcher && make test       # build/argdump + build/cfgtest

# WarpOS/PPC MiniGL V19 (ET_REL .warpelf; convert on the Amiga with Elf2Exe2)
cd glquake && make -f Makefile.GCCAmigaWOS_MGL_V19 \
  MGL_DIR=/home/mirek/MiniGL_WOS_V19_mglQ3

# Isolate an experiment build (avoids stale .o files baking old DLL paths)
cd glquake && make -f Makefile.GCCAmigaWOS_MGL_V19 \
  ARCH=/tmp/opencode/quaketest MGL_DLL_OVERRIDE=Work:ibdraw/minigl_ppc.dll
```

WarpOS validation gates (zero undefined symbols, allowed relocation set,
release-marker scan) are described in [docs/building.md](docs/building.md).

## Tests and probes

```sh
python3 tools/test_texture_cache.py
python3 tools/test_frame_profile.py
python3 tools/check_docs.py
python3 tools/check_wos_release.py client.warpelf dll.warpelf host
```

Hardware session protocol, deployment pipeline and recovery: run it exactly
as written in [docs/operations/runbook.md](docs/operations/runbook.md). In
short: cold boot → `C:InitPPC` once → verify PPC health → fresh `mglhost` and
verify its port → launch client → quit through the game. **Never `Break` a
PPC task; a crashed client requires a reboot.**

## Layout

- `glquake/` — main Quake/GLQuake client and server; maintained Makefiles,
  platform layer, probes.
- `launcher/` — 68k BlitzQuakePPC MiniGL Launcher (`BlitzLaunch`), the
  Intuition front-end that starts `mglhost` and the WarpOS client from the
  game drawer.
- `glqwcl/` — QuakeWorld client/server (legacy tree, separate
  `genasmheaders` copies).
- `tools/` — host-side tests, analyzers, release guard, evidence.
- `docs/` — all documentation (start at [docs/README.md](docs/README.md)).
- Original 2002 release notes: `BlitzQuake_src.readme`, `glquake/*.readme`,
  `glqwcl/qwcl68k.readme` (historical; do not confuse with current docs).

## Known open issues

- Windowed-mode slowdown on the main-branch MiniGL DLL (fullscreen is fine).
- Per-frame lightmap uploads are not yet coalesced.
- No ASL ScreenModeRequester in the GL video path.
- Gamma default differs on Amiga; the original "gamma off" report is
  unresolved.
- Particles crash on exit when sprites are disabled (original source bug).

Current performance state and artifacts:
[docs/performance/README.md](docs/performance/README.md).
