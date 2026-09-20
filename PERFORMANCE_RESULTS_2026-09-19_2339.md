# GLQuake WarpOS performance work — results and change log

**Timestamp:** 2026-09-19 23:39 CEST (21:39 UTC)
**Machine:** Amiga 4000, 68060 host + Prometheus + Radeon 9200 (RV280),
MPC7410 PPC **400 MHz**, WarpOS. Results below are for this 400 MHz target.
**Benchmark:** `demo1`, 969 frames, fullscreen 800x600x32, `-noudp -nosound`,
no runtime diagnostics. Result written after the timed interval with
`-benchmarklog`. Every run exited cleanly unless stated.

---

## 1. Measured results

| # | Configuration | Client | DLL | Time (s) | FPS |
|---|---|---|---|---|---|
| 1 | Pre-optimization baseline (correct rendering, diagnostics off) | release build | qrelease | 214.260 | 4.523 |
| 2 | Batch-cost + texture-cache optimizations | F2B61BAD | 4987D383 | 211.840 / 211.340 | 4.574 / 4.585 |
| 3 | Flash guards (fade-tail skip + scissor skip) | A9EBEB3D | 4987D383 | ~204.9 (reported) | 4.729 |
| 4 | Particle quads, 3 px default | 6707A68F | F0089138 | 207.940 / 199.480 | 4.660 / 4.858 |
| 5 | Particle fast points, 1 px | 6707A68F / 6B7B0938 | F0089138 | 201.980 / 192.080 / 191.420 / 190.420 | 4.798 / 5.045 / 5.062 / 5.089 |
| 6 | **CP emit ON**, AltiVec auto (on) | 6B7B0938 | F0089138 | **79.700** | **12.158** |
| 7 | CP emit ON, AltiVec forced OFF | 6B7B0938 | F0089138 | 80.440 | 12.046 |

Current best: **12.158 FPS** (2.7x the original 4.523, 2.4x the 5.089
particle-default result). CP-emit host counters from run 6:
`CP_RUN dispatches=15024 dwords=7742502 failed=0`, `presents=978`.

### Measurement caveats

- First-run-after-boot was 1-9% slower in observed pairs; the 4.660 vs 4.858
  and 4.798 vs 5.045 pairs show the effect. Compare like-for-like order or
  take repeated samples.
- Rows 1-5 are single/double samples in the 4.5-5.1 range; treat differences
  under ~5% between different boots as noise.
- CP-emit rows 6/7 are same-boot A/B and dominated everything else.

---

## 2. What was changed

### 2.1 Correctness fixes required before performance work

- **Driver texture-content serial rejected** (`p96-driver/src/radeon3d_emit.c`):
  `textureState & ~RADEON3D_TEX_STATE_MASK` rejected MiniGL's documented
  upper-16-bit content serial, failing every lightmap-updating draw with
  service stage 84 (`fence=80090054`, `H13=0x0005001f`). Both texture units
  now accept `TEX_STATE_MASK|TEX_CONTENT_MASK`; reserved low bits still
  rejected. Regression: `tools/test_tex_serial.py` (76 checks; old mask
  mutant fails 24).
  Installed `LIBS:Picasso96/Radeon9200.chip` = **39C2F8AF** (was F46026B4).
- **Screen-clip intersection off by 1 ULP** (`MiniGL/.../r200_geometry.c`):
  the rising console edge produced `y = -1.430511e-5`, so the Radeon service
  rejected the whole packet and the loading screen froze. Clip-plane
  intersections now snap to the plane and final vertices are clamped to the
  drawable. Regression: `tools/test_screen_clip.py` (7,188 checks; old fused
  interpolation mutant fails 1,184).
- **WarpOS shutdown timer path** (`glquake/sys_amiga_std.c`): the unused
  timer.device request was opened and torn down from the PPC task even though
  WarpOS timing uses `DateStamp`; this produced the recurring
  "PPC Memory corruption detected during freeing" after clean quits.
  Timer open/cleanup is now `#ifndef WOS`, and `usleep` maps to `Delay`.

### 2.2 MiniGL (DLL) changes

- **Ring packet budget checks** (`r200_batch_budget.h`): commit prefix +
  draw-descriptor tail are now budgeted against the 8,192-dword ring slot
  before any copy, closing a real overwrite (`recordCount` could pass 8192
  while `recordCount+draws+2` exceeded it). 579,794 boundary checks pass.
- **Tighter, emitter-verified expansion cost** (`R200ExpandedBatchCost`):
  unlit shared-state batches keep the compact generated-command bound; lit
  batches keep the conservative bound. 64-draw unlit example: old reserve
  10,176 words, new 982, actual emission 722. Regression:
  `tools/test_batch_expansion.py` (2,247 checks; full-state mutant fails 24).
- **Cost-model fixes found by that test:** texture-generation matrices
  (+42 dwords) and content-serial invalidation (+4) are now charged.
- **Sized-point quads** (`r200_geometry.c`): points above 1.5 px emit one
  four-vertex QUADS record per point (edge points fall back to clipped
  triangles). Same look, -33% particle geometry.

### 2.3 Client changes

- **Flash guards**: `R_PolyBlend` skips the scissor enable/disable pair when
  the status bar occupies no rows; `Draw_AlphaFill` returns immediately when
  the quantized alpha is 0 (invisible fade-tail frames).
- **Texture-cache retention**: `R_DrawWorld`/`R_DrawBrushModel` no longer
  force `currenttexture = -1` in the dispatch build.
- **Particle rendering mode**: new cvar `r_particle_size`.
  - `1` (default): MiniGL hardware point path, one TCL vertex per particle.
    Measured ~4-7% over the 3 px path and the old flash baseline.
  - `0`: automatic resolution-scaled size using the quad path.
  - `3`: the old 3 px look.
- **Benchmark tooling**: `-benchmarkquit` and `-benchmarklog <file>` (result
  written after the timed interval, avoiding I/O during measurement).
- **Diagnostics**: opt-in `-stalltrace` markers and error-origin logging are
  compiled out with `WOS_DIAGNOSTICS=0` (release builds). No trace strings
  are present in the deployed binaries.

### 2.4 Where the time actually was

The 4.5-5.1 FPS plateau was **68k-host-bound**: the PPC client published
semantic records and the 68060 host parsed and expanded them into CP
commands every frame. Enabling CP emission (`MGLPPC_CPEMIT=1`) moves command
construction to the PPC frontend and the host mostly forwards, giving the
2.4x jump to ~12.2 FPS. Under CP emit, the AltiVec vs scalar vertex
serialization difference is within noise (12.158 vs 12.046, -0.9%).

---

## 3. Current target state (verified 2026-09-19 23:39 CEST)

| Component | Path | CRC32 |
|---|---|---|
| Client | `Work:games/quake/glquakeWOS` | **6B7B0938** |
| MiniGL DLL | `Work:games/quake/minigl_ppc.dll` | **F0089138** |
| 68k host | `DH2:ibdraw/mglhost` | **F0343E03** |
| Radeon chip driver | `LIBS:Picasso96/Radeon9200.chip` | **39C2F8AF** |
| minigl.library | `LIBS:minigl.library` | **62343673** |
| Client ELF (source) | `glquake/warpos_part/glquake/glquake_wos.warpelf` | B5DDBE7B |
| DLL ELF (source) | `MiniGL_WOS_V19_mglQ3/bin/qpart.warpelf` | C99DD4DF |

Environment:

- `ENV:MGLPPC_CPEMIT=1` — **volatile**, lost on reboot.
- `ENV:MGLPPC_NOALTIVEC` — removed; AltiVec auto-probe enabled.
- `ENVARC:MGLPPC_HOSTPRI=-1` — persistent 68k host priority.
- PPC client task priority: exec **5** (default in `sys_amiga_std.c`; `-clpri N`
  overrides, `-clpri 0` disables the raise).
- Other tuning env vars unset: `MGLPPC_VERBOSE`, `MGLPPC_BATCH`,
  `MGLPPC_NODIRECT`, `MGLPPC_ALTIVEC`, `MGLPPC_ALTIVEC_VERIFY`.

### Rollback material

| Item | Path | CRC32 |
|---|---|---|
| Flash-baseline client | `DH2:wosbuild/qprepart_client` | A9EBEB3D |
| Previous DLL | `DH2:wosbuild/qprepart.dll` | 4987D383 |
| Pre-flash client (reported) | `DH2:wosbuild/qpreflash` | F2B61BAD |
| Previous chip driver | `LIBS:Picasso96/Radeon9200.chip.previous` | F46026B4 |
| Previous Prometheus driver | `LIBS:Picasso96/Prometheus.card.previous` | 18A453D6 |

---

## 4. Open items / next steps

1. **Persist and validate CP emit.** It is currently volatile. Before making
   it permanent (`SetEnv MGLPPC_CPEMIT 1 ENVARC:`), run a longer gameplay
   session (not just `demo1`) to confirm rendering and stability. The
   earlier CP-mode notes include token/target-patch caveats; this stack shows
   `failed=0` and rendering was confirmed for the CP run.
2. Re-measure the particle and flash work **under CP emit**, since the
   bottleneck moved from the 68k host to the PPC frontend.
3. Re-verify the 3 px vs 1 px particle trade-off under CP emit; the current
   default (`r_particle_size 1`) was chosen before CP emit existed.
4. Establish a multi-sample protocol (repeat runs per configuration, note
   boot order) because single-sample variance is 1-9%.

---

## 5. Reproduction

Launch (current best configuration):

```text
SetEnv MGLPPC_CPEMIT 1
C:InitPPC
Run >NIL: Execute DH2:ibdraw/hostrun
cd Work:games/quake
Stack 1048576
Run >NIL: glquakeWOS -width 800 -height 600 -bpp 32 -noudp -nosound \
     -benchmarkquit -benchmarklog DH2:wosbuild/result.txt +timedemo demo1
```

Local regressions (host, no Amiga):

```text
cd /home/mirek/p96-driver
python3 tools/test_tex_serial.py
python3 tools/test_tex_matrix.py
python3 tools/test_screen_clip.py

cd /home/mirek/MiniGL_WOS_V19_mglQ3
python3 tools/test_batch_budget.py
python3 tools/test_batch_expansion.py

cd /home/mirek/BlitzQuake_src
python3 tools/test_texture_cache.py
```

---

## 6. Gears benchmark (added 2026-09-20 ~00:05 CEST)

Procedure per `MiniGL_WOS_V19_mglQ3/how_to_benchmark.md`. Validated pair
restored before the run: DLL `Work:ibdraw/minigl_ppc.dll` = **FCEF7CE9**,
host `DH2:ibdraw/mglhost` = **FB712873** (protection 00020000). Chip
`39C2F8AF`, `minigl.library` `62343673`. Fresh bridge reboot, `C:InitPPC`
once, fresh host per run, `MGLPPC_CPEMIT=1`, `MGLPPC_ALTIVEC=1`,
`MGLPPC_HOSTPRI=-1`. `precalc -frames 3000 -nosync -nofps`.

| Run | Mode | CP emit | FPS | Notes |
|---|---|---|---|---|
| 1 | fullscreen 640x480x32, 3 buffers | on | **126.903** | ticks=1182, error=0 |
| 2 | fullscreen 640x480x16, 3 buffers | on | **187.032** | ticks=802, error=0 |
| 3 | fullscreen 640x480x16, 3 buffers | off | **134.288** | ticks=1117, error=0 |
| doc reference | fullscreen 640x480x16 | off | 134.5-136.4 | matches run 3 |

Host counters for run 2 (`DH2:ibdraw/host.log`):
`CP_RUN dispatches=10154 dwords=2557880 failed=0`, `presents=3000`.

Findings:

- The restored validated pair with CP emit off reproduces the documented
  reference (134.288 vs 134.5-136.4) — no regression from our driver/stack.
- **CP emit adds ~39%** on the gears workload (187.032 vs 134.288).
- An earlier 32-bit sample with our host `F0343E03` gave 126.475 vs
  126.903 with `FB712873`; host variant is not significant for gears.
- Previous assumption corrected: the "proper host" is `FB712873`, not
  `F0343E03`. The latter is retained at `DH2:wosbuild/host.qrel`.

Current env left as requested: `MGLPPC_CPEMIT=1`, `MGLPPC_ALTIVEC=1`,
`MGLPPC_HOSTPRI=-1`. No client left running; host port clean.

---

## 7. Quake timedemo 16-bit (added 2026-09-20 ~00:25 CEST)

Same game stack as rows 6/7 above: client `6B7B0938`, DLL `F0089138`
(from `Work:games/quake`), host `FB712873`, chip `39C2F8AF`,
`minigl.library` `62343673`. Env `MGLPPC_CPEMIT=1`, `MGLPPC_ALTIVEC=1`,
`MGLPPC_HOSTPRI=-1`. Fullscreen, `-noudp -nosound -benchmarkquit`,
`+timedemo demo1`.

| Mode | FPS | Time (s) |
|---|---|---|
| 800x600x**32**, CP emit on | 12.158 | 79.700 |
| 800x600x**16**, CP emit on | **15.009** | **64.560** |

Host counters for the 16-bit run: `CP_RUN dispatches=15032
dwords=7846942 failed=0`, `executes=15032 presents=978`, clean stop.

16-bit gives +23% over 32-bit at the same resolution and stack.

---

## 8. Cleanup verdict (added 2026-09-20 ~00:40 CEST)

Measured under the current best path (CP emit on, 800x600x16):
fast 1 px particles **15.009**, old 3 px quads **15.150** — within noise.
The particle mode is now a visual preference, not a performance item,
because CP emission removed the record-volume bottleneck.

### Keep — required for correctness

| Change | Why |
|---|---|
| Radeon serial-mask fix (`39C2F8AF`) | lightmaps rejected (stage 84) without it |
| Screen-clip ULP snap + clamp | console/loading freeze without it |
| WarpOS timer cleanup removal | "PPC memory corruption" on exit without it |
| Ring packet-capacity guards | prevents heap overwrite (`recordCount+draws+2 > 8192`) |
| `-benchmarkquit` / `-benchmarklog` | measurement infrastructure |

### Keep — worthwhile performance

| Change | Evidence |
|---|---|
| **CP emit** (`MGLPPC_CPEMIT=1`) | Quake 5.1 → 12.2/15.0 FPS; gears +39% (187 vs 134) |
| Flash guards (fade-tail + scissor) | +2.8..4% pre-CP; free |
| Texture-cache retention | free, avoids duplicate binds |
| Batch budget/cost model + texgen/serial charges | keeps budgets correct; small |
| `r_particle_size` + quad-per-point path | **neutral under CP** (15.009 vs 15.150); keep as visual options |

### Drop / neutral — cleaned up

| Item | Action |
|---|---|
| `MGLPPC_ALTIVEC=1` env | **removed**; forced vector vs auto vs scalar: 12.158 / 12.046 / n.a. — no effect under CP, auto-probe already enables it on the 7410 |
| Our release host `F0343E03` | not significant vs validated `FB712873` (126.475 vs 126.903); validated host restored, ours archived at `DH2:wosbuild/host.qrel` |
| Redundant `Work:ibdraw/dll.pre-part` | deleted (was a copy of the active `FCEF7CE9`) |
| `MGLPPC_BATCH` (two-tile upload) | never enabled; previously measured regression |
| Diagnostics (`-stalltrace`, `MGLPPC_ERROR_ORIGIN`, reject dump) | compile-time off / opt-in only; not in release binaries |

### Current configuration after cleanup

- Client `Work:games/quake/glquakeWOS` = `6B7B0938` (particle default 1 px).
- DLL `Work:games/quake/minigl_ppc.dll` = `F0089138`.
- Gears/validated DLL `Work:ibdraw/minigl_ppc.dll` = `FCEF7CE9`,
  host `DH2:ibdraw/mglhost` = `FB712873`.
- `LIBS:Picasso96/Radeon9200.chip` = `39C2F8AF`, `LIBS:minigl.library` = `62343673`.
- `ENV:MGLPPC_CPEMIT=1` (volatile — set before a game session; keep unset for
  the reference gears benchmark), `ENVARC:MGLPPC_HOSTPRI=-1`, no
  `MGLPPC_ALTIVEC`/`MGLPPC_NOALTIVEC`.

### Still open

1. Persist CP emit (`SetEnv MGLPPC_CPEMIT 1 ENVARC:`) only after a longer
   gameplay validation; it changes reference benchmark conditions.
2. Re-validate 1 px vs 3 px particles in gameplay (explosions), not demo1.

---

## 9. 640x480x16 timedemo; CP emit persisted (added 2026-09-20 ~00:55 CEST)

Actions: `MGLPPC_CPEMIT=1` saved to `ENVARC:` (persistent, verified in both
`ENV:` and `ENVARC:`); particles left at the default (`r_particle_size 1`).
Fresh bridge reboot, `InitPPC` once, fresh host `FB712873`.

| Mode (CP emit on) | FPS | Time (s) |
|---|---|---|
| 800x600x16 | 15.009 | 64.560 |
| **640x480x16** | **15.870** | **61.060** |

Host counters: `CP_RUN dispatches=14991 dwords=7746014 failed=0`,
`executes=14991 presents=978`, clean stop.

Reducing resolution from 800x600 to 640x480 gains only ~5.7%, confirming
the post-CP-emit profile is CPU/frame-bound rather than fill-bound.

---

## 10. Fastest DLL promotion + build-from-tree state (2026-09-20)

The parallel session's descriptor-cache work (W-01..W-12) is the real
speed-up; the earlier 15 FPS game runs used the older game-dir DLL
(`F0089138`). Measured on hardware, same 969-frame `demo1`, CP emit on:

| Stack at 640x480x16 | FPS |
|---|---:|
| old game DLL `F0089138` | 15.870 |
| W-10 quiet `083A9E15` | 55.182 |
| W-11 quiet `8A5D32EA` | 54.195 |

Interleaved same-boot A/B at the 800x600x32 reference (two samples each):

| Run | W-10 `083A9E15` | W-11 `8A5D32EA` |
|---|---:|---:|
| 1 | 24.189 | 24.057 |
| 2 | 24.045 | 24.081 |
| **Mean** | **24.117** | **24.069** |

W-10 and W-11 are a statistical tie (0.2%). **W-10 was chosen and deployed**
because it is the plan's most hardware-validated artifact (W-01a/b/c, W-10,
recovery DLL, gears 192.3 FPS); W-11 was validated only in profiled form.

### Build provenance (important)

| Source state | Warpelf CRC32 | Converted target |
|---|---|---|
| git HEAD `48aee81` | `16C5361A` | (17.2 FPS checkpoint) |
| W-10 quiet | `14F2A439` | `083A9E15` |
| session-3 working tree (W-11/W-12/W-22) | `BB3F46E6` | `8A5D32EA` |
| **git HEAD `f2976f6`** (W-10 source restored) | `14F2A439` | `083A9E15` — **reproducible from git** |
| session-4 tree (CP_WAIT/reset/reporter fix, see perf plan §9) | `CFD151CA` | diagnostic only, `DH2:wosbuild/qwait/` |

- UPDATE 2026-09-20 (session 4): the "W-10 source is not in git" note below
  is RESOLVED — commit `f2976f6` restored it; `make -f
  Makefile.dll.r200.gcc warpelf` at that commit reproduces `14F2A439`
  byte-for-byte, and the host rebuilds `F0343E03` identically. The working
  tree has since moved on (CP_WAIT attribution counters, glHint-boundary
  reset, CP_PHASE reporter fix, client FP_LMUPLOAD) and builds `CFD151CA`
  — a diagnostic build for the next hardware session, not deployed.

- ~~The working tree rebuilds **W-11** exactly~~ (superseded: `f2976f6`
  restored the W-10 source; the session-4 tree builds `CFD151CA`, see the
  provenance table above).
- ~~W-10's exact source is not in git~~ — RESOLVED by `f2976f6`.
- W-10 and W-11 measured equal within 0.2%; **W-10 (`083A9E15`) remains the
  deployed and reproducible reference**.

### Deployed (verified)

| Artifact | CRC32 | Location |
|---|---|---|
| MiniGL DLL (W-10) | **083A9E15** | `Work:games/quake/minigl_ppc.dll` |
| MiniGL DLL (copy) | **083A9E15** | `DH2:ibdraw/minigl_ppc.dll` |
| 68k host (matched) | **F0343E03** | `DH2:ibdraw/mglhost` (bits 00020000) |
| Client | 6B7B0938 | `Work:games/quake/glquakeWOS` |

### Build from the working tree

```text
cd /home/mirek/MiniGL_WOS_V19_mglQ3
; at git HEAD f2976f6 (deployed W-10 reference):
make -f Makefile.dll.r200.gcc warpelf              # -> bin/minigl_ppc_r200.warpelf  = 14F2A439
make -f Makefile.dll.r200.gcc bin/minigl_ppc_host  # -> bin/minigl_ppc_host        = F0343E03
; at the session-4 tree (diagnostic, perf plan section 9):
make -f Makefile.dll.r200.gcc warpelf              # -> CFD151CA (quiet)
make -f Makefile.dll.r200.gcc MGLPPC_PROFILE=1 \
  OBJDIR=obj/dll-r200-waitp WARPELF=bin/minigl_ppc_r200_waitp.warpelf warpelf
                                                   # -> 54EADCD1 (profiled)
```

Target conversion:

```text
Stack 100000
DH2:wosbuild/Elf2Exe2 bin/minigl_ppc_r200.warpelf minigl_ppc.dll
```

This produces the W-11 DLL (`8A5D32EA`), equal to the deployed W-10 within
0.2%. The trees are dirty (W-xx descriptor work, clip fix, particle-quad
path, batch-cost model are uncommitted); the working tree is the
authoritative source.

### Quarantined — do not deploy

- `bin/minigl_ppc_r200_w12e2.warpelf` (A3AE5A71, `MGLPPC_EXPAND_MIN=2`) and
  any expansion-threshold experiment below 4: `MGLPPC_EXPAND_MIN=1` caused a
  grey-screen hard hang requiring a cold power cycle.

### Rollbacks

| Item | Path | CRC32 |
|---|---|---|
| old game DLL | `DH2:wosbuild/dll.pre-fast` | F0089138 |
| W-10 (previous deploy, same as active) | `DH2:wosbuild/dll.w10q` | 083A9E15 |
| W-11 quiet (tree-buildable) | `DH2:wosbuild/qw01/w11q.dll` | 8A5D32EA |
| user-validated DLL | `DH2:wosbuild/dll.fcef7ce9` | FCEF7CE9 |
| validated host | `DH2:wosbuild/hpreclean` | FB712873 |

### Cleanup performed

- Deleted the stale scratch dir `DH2:qbatchtest` (contained an old test DLL
  and probe); `Work:games/quake` now holds exactly one DLL.
- Evidence dirs `DH2:wosbuild/qw01` and `qprof` retained.

---

## 11. Session-4 hardware session: two-target matrix + first CP_WAIT
attribution (2026-09-20 ~13:00-15:30 CEST)

Targets: 800x600x32 and 640x480x16. Clean boots with soft reboot between
critical comparisons, fresh host per run, serialized bridge ops, CRC
checks everywhere. Production stack untouched; test dir
`DH2:wosbuild/qwait`. Full process detail in PERFORMANCE_PLAN
section 10.

### 11.1 Client matrix (quiet, demo1, CP emit on)

| client | DLL | 800x600x32 | 640x480x16 |
|---|---|---:|---:|
| glquakeWOS `6B7B0938` | 083A9E15 | 23.611 | 52.097 |
| glqprof `E2A06D9A` (session-2) | 083A9E15 | — | 52.209 / 52.835 |
| session-4 tree client, clean boot | BA0A8531 profiled | **23.844** | **51.270** |

**CORRECTION (same session, ~17:30 CEST):** the earlier "rebuilds are 2.5x
slower at 16-bit" alarm was NOT a tree regression. Two artifacts stacked:

1. A stale `minigl_ppc_open.o` silently baked an OLD test-DLL path
   (`DH2:wosbuild/qprofdir/minigl_ppc.dll`, a 16-slot pre-W-01 build) into
   every rebuild — the 17-21 fps runs were running THAT DLL (its recorded
   17.5 fps @ 32-bit and CP_SURF `miss≈36k reset≈36k` no-`drop=` signature
   match exactly).
2. One boot produced degraded loads (~2x slowdown for freshly replaced
   DLL files) after a chained `InitPPC`+launch command; a fresh boot with
   strict one-command sequencing cleared it.

`gl_rsurf.c` session-4 profiler edits were reverted and re-tested: no
effect (as predicted — the pristine build without them was equally
"slow" under the same artifacts). The tree is healthy; validation numbers
above. Full analysis in PERFORMANCE_PLAN section 10.1 (W-81 closed).

### 11.2 Session-4 DLL validated

`glqprof` + session-4 quiet DLL C6FD771F: 52.835 fps @ 640x480x16 —
identical to production DLL. CP_SURF: descriptor cache healthy at 128
slots. DLL line kept; promotion still gated on W-70/W-71.

### 11.3 First CP_WAIT attribution (profiled DLL BA0A8531 + profiled host
03DC3364, boundary-reset counters)

| | 800x600x32 (23.762 fps profiled) | 640x480x16 (50.839 fps profiled) |
|---|---:|---:|
| CP publishes | 15,025 | 15,079 |
| CP wait total | 639.7M ticks | 106.1M ticks |
| wait = `fence=` (GPU retirement) | 298.9M (~95% of attributed) | 69.9M |
| wait = `host=` (68k consumption) | 16.5M (~2.6%) | 4.0M |
| emit / copy / resolve | 39.3M / 10.5M / 11.9M | 41.3M / 11.6M / 10.0M |
| host drain total | 249 ms of 40.8 s | — |
| host dispatch total | 6,087 ms | — |

**Conclusion:** the 32-bit frame is bounded by GPU-retirement fencing:
every dispatch carries a full-idle fence tail
(`WAIT_2D/3D_IDLECLEAN|DMA_GUI_IDLE`), serializing the GPU ~15.5x per
frame; the wait scales with fill (6x drop at 16-bit, same dispatch
count) — that IS the depth ratio. The 68k host is exonerated with data
(2.6% stall share; 249 ms total drain). Next lever: **W-80** — split the
fence into a consumption-side reuse fence (HOST/DMA idle only) and a
completion fence (full idle, present/readback/texture paths only);
driver change, matched-pair deploy, cold-boot protocol.

### 11.4 Session state after

Production pair verified in place (6B7B0938 / 083A9E15 / F0343E03);
`qprof` restored to 083A9E15 with `dll_prod.dll` archive; `DH2:wosbuild/
qwait/` holds all test artifacts and logs (deletable). Two no-host
client wedges and one degraded warm boot occurred; all recovered via
soft reboot — runbook now requires verifying the host port is gone and
starting a fresh host before every client launch (PERFORMANCE_PLAN
section 10.5).





