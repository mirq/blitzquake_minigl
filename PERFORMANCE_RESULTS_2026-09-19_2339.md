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


