# GLQuake WarpOS/PPC port — HANDOVER NOTES
Date: 2026-09-13. Status: **game runs on real hardware** (A4000, MPC7410
WarpOS, Prometheus + Radeon 9200, MiniGL R200 stack), ~2–3 fps in demo
playback at 640×480×32 windowed. Performance work is the next frontier.

## 1. What exists (files)

- `glquake/Makefile.GCCAmigaWOS_MGL_V19` — working build: ppc-amigaos-gcc
  8.4.0 + MOS2WOS SDK, produces `warpos_gcc_v19/glquake/glquake_wos.warpelf`
  (ET_REL). Includes the MiniGL tree config:
  `MGL_DIR=/home/mirek/MiniGL_WOS_V19_mglQ3` (makefile var `MGL_DIR`).
- `PPC_toolchain.md` — ALL toolchain/runtime landmines (read first!):
  MOS2WOS library-base rules, DateStamp timing (no GetSysTimePPC, no
  ppc.library from PPC tasks, no usleep), warm-reboot PPC corruption,
  ET_REL link contract, deployment pipeline, session protocol.
- `MiniGL_R200_BUG_REPORT.md` — 5 MiniGL frontend defects with timings;
  **the MiniGL developer already fixed them** (see
  `~/MiniGL_WOS_V19_mglQ3/backend_r200/GLQUAKE_DEFECT_FIXES.md`, branch
  fix/r200-glquake-texture-defects; deployed DLL 6C74A72D/305388,
  host 573F97A3/40624). Addendum with GLQuake validation + two new
  findings appended at the end of that file.
- Client source changes (all guarded, mostly `#ifdef WOS`):
  - `sys_amiga_std.c`: TimerBase as `struct Library *`, PowerPCBase
    defined-NULL (never open ppc.library!), `Delay(1)` instead of usleep,
    `WOS_EClockTime()` = DateStamp timing (replaces GetSysTimePPC),
    unbuffered stdout, `Sys_WOSTrace()` = RAM:gqtrace stage markers.
  - `gl_vidamiga.c`: opens intuition.library + graphics.library in
    VID_Init (old toolchain auto-opened them), closes in VID_Shutdown.
  - `gl_draw.c`: NULL-identifier guard in GL_LoadTexture (68k-ism:
    strcpy from NULL worked on 68k, fatal on PPC).
  - `net_bsdsocket.c`: WOS branch using netinclude BSD `__P*` layer +
    LP inline macros for CloseSocket/IoctlSocket/Inet_NtoA/SocketBaseTagList.
    **Disabled via `-noudp`** — fd-layer mixing unresolved (see issues).
  - `snd_amiga_AHI.c`: guarded missing `powerup/ppcinline/alib.h`.

## 2. Deployed on the Amiga

- `Work:games/quake/glquakeWOS` — converted executable (Elf2Exe2).
- `Work:games/quake/minigl_ppc.dll` — copy of the FIXED DLL
  (6C74A72D/305388) from `Work:ibdraw/`. **IMPORTANT: the client loads
  `minigl_ppc.dll` CWD-relative — if you re-copy an old DLL here the old
  defects return (that cost us a day).** Verify CRC 6C74A72D.
- `Work:games/quake/id1/` — PAK0.PAK + PAK1.PAK + config.cfg + autoexec.cfg
  (autoexec currently deleted; if you want measurements put
  `timedemo demo1` + `host_speeds 1` + `r_speeds 1` in it).
- Staging: `Work:wosbuild/` (Elf2Exe2 converter, zips, probes
  gqprobe/gqprobe2/gqprobe3, glp5 diagnostic probe).
- Host start script: `Work:ibdraw/gqhostrun` (FailAt 21, Stack 65536,
  `Work:ibdraw/mglhost >Work:ibdraw/host_gq.log`).

## 3. Session protocol (per run) — follow exactly

1. Cold boot preferred. Warm reboots can leave PPC executables broken
   (silently die before main — validated binaries included). After ANY
   client crash: full reboot before the next run (crash ghosts the host;
   `Break` does not work on wedged PPC tasks; wedged Software-Failure
   processes hold PPC RAM → later launches fail "not enough memory").
2. `C:InitPPC` — exactly once per boot.
3. Health check BEFORE starting the host (it would eat a host lifecycle):
   `Run >NIL: Work:wosbuild/gqprobe >Work:wosbuild/hc.log` → expect
   "MiniGLOpen failed" (no host) but NOT a crash.
4. Start host: `Run >NIL: Execute Work:ibdraw/gqhostrun`, wait ~8 s,
   verify port `MiniGLPPC.R200.Host.1` exists.
5. Launch: `cd Work:games/quake` + `Stack 1048576` +
   `Run >NIL: glquakeWOS -width 640 -height 480 -bpp 32 -windowmode
   -noudp -nosound`.
6. Quit via the game (ESC → Quit) or the window close gadget — NEVER
   Break. The host exits after a clean client close; a crashed client
   leaves it ghosted → reboot.

## 4. What works / what was fixed (client side)

- Full Quake boot: paks, wad, console, progs, models, menu; demo loop
  plays; GL context on the R200; textures render with correct colors;
  console text renders (after MiniGL fix + NULL-guard).
- Sound (-nosound currently) and networking (-noudp currently) disabled.

## 5. OPEN ISSUES (prioritized)

### 5.1 Performance: ~2–3 fps in demo playback (640×480×32 windowed)
Measured with `host_speeds 1` (put it in id1/autoexec.cfg):
`tot 279–499 server 99–240 gfx 0–19` per frame.
- Probe-measured per-call costs on this stack: textured flatfan quad
  0.075–0.27 ms; vertex pair 25–75 µs; glClear 2–7 ms. A Quake frame is
  thousands of immediate-mode calls → ~300–500 ms. **Death by per-call
  dispatch overhead**, not fill rate.
- The 68k version was faster because the classic 68k mgl.library ran
  in-process (~1–2 µs/call) and `mglTV23fv` wrote vertices DIRECTLY into
  the shared vertex buffer (see `miniglext.c` non-dispatch path). The WOS
  dispatch client emulates these with per-vertex glTexCoord2f/glVertex3f
  through the pipeline.
- MiniGL itself is fast on batched work: gears 640x480x16 fullscreen =
  134–136 fps on this stack.
- **Next steps (pick in order):**
  a. Client-side batching: port the later 68k BlitzQuake renderer
     optimizations ("worldbatch", `r_vertexarrays` — config.cfg already
     references them) to the dispatch client; or implement a batched
     world path via `glDrawArrays` (frontend supports it — probe-tested
     conceptually; verify).
  b. Reduce server-bucket cost (99–240 ms during demo playback is
     suspicious — DateStamp quantization is 20 ms; investigate what the
     "server" bucket actually measures during demo playback).
  c. Try 320×240 to separate resolution-dependent cost (present blit
     scales with pixels; dispatch does not).
- Measurement recipe: put `timedemo demo1` + `host_speeds 1` +
  `r_speeds 1` into `id1/autoexec.cfg`, read results from
  `id1/qconsole.log` (NOTE: `timedemo demo1`, not `timedemo 1` —
  "1.dem" does not exist).

### 5.2 "PPC Memory corruption detected during freeing" during demo playback
One occurrence (powerpc.library heap guard, free-time detection) after
~1 min of demo playback. Heap stomp somewhere in the gameplay path.
Approach: bisect by feature (particles → entities → temp entities) using
autoexec toggles; the RAM:gqtrace markers cover init only — extend
Sys_WOSTrace into the frame loop if needed. Expect reboot cycles.

### 5.3 Sound: "powerpc.library: Async Run68K function not supported"
Happens WITHOUT `-nosound`: `SNDDMA_Init` (snd_amiga_AHI.c, audio.device
or AHI branch) from a PPC task trips WarpOS's missing async-68k
execution. `-nosound` is the workaround. A real fix needs either
ppclibemu (crashes from PPC tasks — see toolchain notes) or a
PPC-safe sound interface (AHI "record/memory" style or a 68k helper
process doing the mixing). Deprioritized by the user.

### 5.4 Networking: `-noudp` workaround
net_bsdsocket.c WOS branch mixes two fd layers: POSIX `socket()` returns
a libglosswos fd-table index, but raw `CloseSocket`/`IoctlSocket` LP
macros expect raw bsdsocket handles → -1s. Pick ONE layer: either all
raw (open bsdsocket.library into our own base + LP macros for everything,
LVOs from `powerup/ppcinline/bsdsocket.h`) or all `__P*` (find
libglosswos's close/ioctl spellings). Game runs fine without network.

### 5.5 Screen-mode requester (user request, not started)
ASL ScreenModeRequester in VID_Init when no -width given; map
depth→mglChoosePixelDepth (15/16/24/32), windowed/fullscreen toggle.

## 6. Debugging recipes that worked

- `Sys_WOSTrace("tag\n")` (sys_amiga_std.c) → `RAM:gqtrace` per-line
  open/append/close markers; stdio-independent. The last marker = crash
  site. Markers exist at t0–t9 (main/init), h0–hB (Host_Init),
  v0–vB (VID_Init), d3–d5 (Draw_Init), r0/r1 (first frame).
- Crash triage: Software Failure `#80000004` = illegal instruction
  (usually NULL fn-pointer). PowerPC Exception dump: SRR0 = PC,
  DAR = faulting address (0x10 = NULL struct field write),
  DSISR 0x40000000 = store, PVR 800C1104 = MPC7410.
- Bridge screenshot works windowed; fullscreen VRAM grabs return gray
  noise (use the user's eyes or mglWriteShotPPM post-fix).
- The bridge crash handler only catches bridge-launched clients.
- Mouse injection can dismiss requesters (Continue/Close gadgets);
  Amiga close gadgets are at the TOP-LEFT of the window.

## 7. Gotchas that cost time (don't repeat)

- Forgetting `-nosound` → Async Run68K trap (nondeterministic-looking).
- Old DLL copies in the game dir shadow the fixed `Work:ibdraw` DLL.
- Stale host after a crashed client → next client hangs in MiniGLOpen.
- Elf2Exe2 fails silently without redirect — always capture output.
- `ld -r` does not resolve undefined symbols — check `nm --undefined-only`
  AND let Elf2Exe2 be the final gate (it catches everything).
