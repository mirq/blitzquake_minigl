# PPC Toolchain Notes — MOS2WOS/WarpOS porting landmines
# (learned porting BlitzQuake GLQuake to ppc-amigaos-gcc + MiniGL V19 R200, Sep 2026)

Target: Amiga 4000, MPC7410 WarpOS PPC card, Prometheus + Radeon 9200,
MiniGL V19 WOS stack (`minigl_ppc.dll` + 68k `mglhost`). Cross-build on Linux.

## Toolchain

- Compiler: `/home/mirek/ppc-amigaos-gcc/native-build/root-cross/bin/ppc-amigaos-gcc`
  (adtools GCC 8.4.0, targets AmigaOS4; fine for WarpOS with the right flags).
- SDK: MOS2WOS overlay at `/home/mirek/Downloads/gg/ppc-warpos`
  (`os-includeppc`, `include`, `netinclude`, `newlib`, `lib`).
- Shared config: `~/MiniGL_WOS_V19_mglQ3/Makefile.wos.gcc.common`
  (`WOS_ARCH_CFLAGS`, `WOS_CPPFLAGS`, ET_REL link contract + `WOS_VALIDATE_ETREL`).
- CFLAGS recipe for 2002-era C: `-std=gnu89 $(WOS_ARCH_CFLAGS) -O2 -w`.
  `-fno-jump-tables` is MANDATORY (ET_REL relocation contract).

## ET_REL link contract (warpelf)

```
ppc-amigaos-ld -r -EB -d -x -T warpos-etrel.ld \
    -L$SDK/newlib/lib -L$SDK/lib -o out.warpelf \
    $SDK/lib/startupwos.o  <objects...> <archives...> \
    --start-group -lamigawos -lgccwos -lm -lglosswos -lcwos -lglosswos -lgccwos --end-group \
    $SDK/lib/end.o
```

- `startupwos.o` MUST be first (`_start` at `.text+0`).
- Validate after link: zero undefined symbols, no COMMON symbols, only
  `R_PPC_ADDR32 / R_PPC_ADDR16_LO / R_PPC_ADDR16_HA / R_PPC_REL24` relocations
  (see `WOS_VALIDATE_ETREL` in Makefile.wos.gcc.common).
- **Elf2Exe2 (on the Amiga, needs ixemul V48) rejects undefined symbols** with
  `Error: Undefined Symbol: <name>` and exits. Always run
  `nm --undefined-only out.warpelf | wc -l` locally before deploying.
  A failing Elf2Exe2 run produces no output file and (without redirect) no
  visible error — always redirect: `Elf2Exe2 in.elf out >capture.txt`.
- Stripping the ELF before conversion is harmless (output identical), skip it.
- Converting a ~1 MB ELF takes ~30 s on the 68060; do it in a foreground
  `dos_command` with output captured, not inside long batch scripts.

## Deployment (bridge)

- `push_file` times out above ~450 KB. Compress locally (python zipfile),
  push the ZIP, `UnZip -o x.zip -d Work:wosbuild` on the Amiga, then CRC32
  verify both ends (`amiga_checksum` vs local `zlib.crc32`).
- NEVER run parallel Amiga filesystem/DOS operations; serialize everything.
- Amiga-side names ≤ 16 characters.
- Don't write mega-batch scripts through `run_script` — the MCP layer times
  out (~120 s) and leaves half-executed state on the Amiga. Do steps
  individually and verify each artifact.

## MOS2WOS runtime landmines (the ones that cost days)

1. **Library bases are YOURS to define and open.** The old PowerUp toolchain's
   `libppcamiga` auto-defined/auto-opened bases. MOS2WOS does not:
   - `TimerBase` is declared `extern struct Library *` by `proto/timer.h` —
     define it with EXACTLY that type (`struct Library *`, not `struct Device *`).
   - `IntuitionBase`, `GfxBase` etc.: define them yourself and open the
     libraries (we open intuition.library + graphics.library in VID_Init).
   - `PowerPCBase`: referenced by libglosswos/libcwos/libgccwos internals.
     Leaving it NULL is FINE for the libglosswos fallback paths
     (printf/write/open/malloc all work with NULL).
2. **NEVER `OpenLibrary("ppc.library")` from a PPC task.** ppclibemu's
   per-task 68k emulation init crashes (PowerPC Exception, PC=0). Same for
   `GetSysTimePPC`/`GetSysTime`/`SubTimePPC` (PowerUp LVOs -684/-66...).
3. **timer.device `ReadEClock`/`GetSysTime` LP calls crash from a PPC task**
   on this stack. The validated MiniGL gears client replaced PPC timing with
   `DateStamp()` (dos.library, 1/50 s). Use DateStamp.
4. **`usleep()` crashes when PowerPCBase is NULL** (libglosswos sleep path).
   Use dos `Delay()`.
5. **exec/dos inline calls work** via `proto/*.h` → `powerup/ppcinline/*.h`
   LP stubs (validated: OpenLibrary, Open/Write, CreateMsgPort, sockets).
   `powerup/ppcproto/exec.h` and `proto/exec.h` are the same mechanism —
   MOS2WOS ships both spellings and both are safe.
6. **Sockets: two incompatible header sets.** `os-includeppc/clib/bsdsocket_protos.h`
   (AmigaOS `LONG` signatures) vs `netinclude/sys/socket.h` (BSD-style,
   `__P*` macros → libglosswos implementations). Never include both.
   - The `__P*` POSIX layer works: `socket()`, `bind()`, `connect()`,
     `gethostbyname()` etc. (libglosswos owns `SocketBase`, auto-opened by
     its `__init_bsdsocket` constructor; do NOT define `SocketBase` yourself).
   - fd values from `socket()` are libglosswos fd-table indices, NOT raw
     bsdsocket handles: raw `CloseSocket`/`IoctlSocket` on them return -1.
   - Amiga-named entry points (`CloseSocket`, `IoctlSocket`, `Inet_NtoA`,
     `SocketBaseTagList`) need LP inline macros copied verbatim from
     `powerup/ppcinline/bsdsocket.h` (including `#include
     <powerup/ppcinline/macros.h>` + `#define BSDSOCKET_BASE_NAME SocketBase`).
   - **TODO for networking**: pick ONE layer consistently. Either all-raw
     (open bsdsocket.library yourself into your own base + all LP macros) or
     all-`__P*` (and find libglosswos's close/ioctl spellings). Until then
     run clients with `-noudp`; Quake works fine loopback/single-player.
7. **`#pragma amiga-align` is ignored by GCC** — that's correct for MOS2WOS
   headers (natural PPC alignment is what they expect). Ignore the warnings.

## 68k-isms that become fatal on PPC

- **Reads from NULL/low memory.** On 68k, address 0-0x400 is readable (exec
  vectors), so bugs like `strcpy(dst, NULL)` or `GL_LoadTexture(identifier=NULL)
  → strcpy(glt->identifier, NULL)` silently "worked". On PPC the NULL page
  faults → PowerPC Exception. Guard every NULL that reaches str*/*printf*.
  (Found: `GL_LoadPicTexture` passes `identifier=NULL` into
  `GL_LoadTexture` which strcpy'd it — now guarded in gl_draw.c.)
- **Function pointers into unset tables**: Quake's `net_landrivers[]`/
  `net_drivers[]` config must match the linked driver set, else NULL calls.

## MiniGL V19 WOS client session protocol

- Client links `lib/libminigl.a`; at runtime `MiniGLOpen()` loads
  `minigl_ppc.dll` via the Hyperion loader. `Open("minigl_ppc.dll")` is
  CWD-relative → either put the exe next to the DLL, or compile
  `-DMINIGL_PPC_DLLNAME="\"Work:ibdraw/minigl_ppc.dll\""` (header supports
  the override).
- Per session: cold boot → `C:InitPPC` **exactly once** → start the 68k host
  (`Run >NIL: Execute Work:ibdraw/gqhostrun`; Stack 65536; fresh host per
  client run; host exits after a client's clean `MiniGLClose`) → run client
  with CWD = the dir containing `minigl_ppc.dll`.
- **After ANY client crash the host is ghosted** — the next client hangs in
  `MiniGLOpen`/`mglCreateContext`. `Break` is ignored by wedged PPC processes;
  `Work:wosbuild/minigl_dll_kill` clears the DLL port but not the host
  attachment. The only reliable recovery is a REBOOT. Wedged crash processes
  also stay in `Status` and hold PPC RAM (later launches fail with
  "not enough memory available" from the loader).
- Keep `MGLPPC_VERBOSE`/`MGLPPC_UPLOAD_TRACE` unset (console writes dominate
  frame time; the trace string also breaks the Sonnet loader classifier).
- Validated GL strings on this stack: `MiniGL_R200 / Radeon3D semantic R200 /
  1.1 MiniGL_R200 phase6`, extensions `GL_ARB_multitexture
  GL_MGL_performance_counters`.
- Known no-ops on this stack: `mglResizeContext` (mode changes need a
  restart), `glClientActiveTextureARB`.

## MiniGL R200 frontend restrictions (measured, see MiniGL_R200_BUG_REPORT.md)

- `glTexImage2D` internalformat: **GL_RGBA and GL_LUMINANCE fail** (MiniGL
  error 0x59/0x58); **GL_RGB and GL_RGB5_A1 work**. MGL_UNSIGNED_SHORT_5_6_5
  also fails (game's `-lm_RGB` path unusable).
- Consequence mapping for GLQuake: font texture (GL_RGBA) → broken console
  text; lightmaps (GL_LUMINANCE) → broken dynamic lighting.
- **`glTexImage2D` costs ~101 ms per call regardless of size** (128² =
  256² = ~101 ms) — fixed latency, likely blocking fence in the submit path.
  Level load ≈ 20–30 s from this alone.
- **`glTexSubImage2D` ≈ 136 ms per 128×128 / 408 ms per 256×256** update —
  Quake's per-frame lightmap updates make the game run at ~0.5 fps.
  Workaround applied: static lightmaps only (texsub call sites compiled out
  under WOS; dynamic lights lost).
- `mglWriteShotPPM` is a silent no-op (no file, no GL error) — game
  screenshots don't work; don't trust bridge P96 grabs in fullscreen either.
- Draw calls / vertex dispatch / glClear are FAST (0.07–0.27 ms/quad,
  25–75 µs/vertex-pair, 2–7 ms/clear) — the GPU frontend itself is fine.

## Porting landmines hit in the game path (client bugs)

- `GL_LoadTexture(identifier=NULL)` → `strcpy(dst, NULL)`: tolerated on 68k
  (low memory readable), fatal on PPC. Guard it.
- AHI sound: the classic AHI interrupt/callback path from a PPC task
  triggers "powerpc.library: Async Run68K function not supported" — keep
  `-nosound` until snd_amiga_AHI is ported to a PPC-safe interface.
- OPEN ISSUE: "PPC Memory corruption detected during freeing" during demo
  playback — a heap stomp somewhere in the gameplay path, detected at
  free-time by powerpc.library. Bisect with features disabled
  (particles/entities/etc.) next session.

## Debugging techniques that worked

- **Warm reboots can leave WarpOS PPC broken**: after a warm reset, PPC
  executables (even historically-validated ones like `gears_v19`) die
  silently before `main` — no output, no requester, redirect files never
  created — while everything 68k keeps working (Elf2Exe2, mglhost, DOpus).
  `C:InitPPC` completing without error does NOT mean the PPC side works.
  **Always verify PPC health with a known-good binary after every boot
  before debugging your own code; recover with a full COLD power cycle.**
- **Direct-DOS trace file**: a helper that does open/append/close per line to
  `RAM:gqtrace` via proto/dos.h — independent of stdio (which may be broken
  when PowerPCBase paths are the problem). Sprinkle markers around init
  stages; the last marker = crash site.
- Live console: pass `-stdout` (Quake suppresses stdout otherwise) +
  `setvbuf(stdout, NULL, _IONBF, 0)` at main() entry + `Run >CON:.../NOCLOSE`.
- Crash numbers: Software Failure `#80000004` = illegal instruction (usually
  a call through a NULL/garbage function pointer). PowerPC Exception dump:
  `SRR0` = crash PC, `DAR` = faulting address (0x10/0 → NULL struct write),
  `DSISR 0x40000000` = store, `PVR 800C1104` = MPC7410.
- `Status` shows the 68k view only; wedged PPC clients appear as their 68k
  loader process and cannot be killed (`Break N C` ignored).
- The bridge crash handler only catches bridge-launched clients, not `Run`'d
  ones (`amiga_last_crash` stays empty).
- `Work:` volume (= old DH2:) survives warm resets; PPC RAM state does not.

## BlitzQuake-specific fixes applied (see git diff)

- `snd_amiga_AHI.c`: guard `<powerup/ppcinline/alib.h>` (missing in MOS2WOS).
- `net_bsdsocket.c`: WOS branch using `__P*` + LP macros + `SocketBaseTagList`
  tag array; `-noudp` until the fd-layer mixing is resolved.
- `sys_amiga_std.c`: WOS `TimerBase`/`PowerPCBase` definitions, `Delay(1)`,
  `WOS_EClockTime()` (DateStamp), unbuffered stdout, `Sys_WOSTrace`.
- `gl_vidamiga.c`: define+open IntuitionBase/GfxBase; MINIGL_DISPATCH_CLIENT
  path is used throughout.
- `gl_draw.c`: NULL-identifier guard in `GL_LoadTexture`.
- Build: `glquake/Makefile.GCCAmigaWOS_MGL_V19` (targets
  `warpos_gcc_v19/glquake/glquake_wos.warpelf`).

## Launch recipe (current working state)

```
; after cold boot, once:
C:InitPPC
Run >NIL: Execute Work:ibdraw/gqhostrun
cd Work:games/quake
Stack 1048576
Run >NIL: glquakeWOS -width 320 -height 240 -bpp 16 -windowmode -nosound -stdout -condebug -noudp
; fullscreen 640x480x16: drop -windowmode, use -width 640 -height 480 -bpp 16
```

Deploy pipeline: build → zip → push → UnZip → Elf2Exe2 → Copy to
`Work:games/quake` → CRC verify → (fresh host) → run.
