# Rejected and parked paths — do not retry blindly

This is the registry of experiments that were measured and **not** retained.
It exists so the same ideas are not re-tried without new evidence. Each entry
states what was tried, the measured result, the decision, and the condition
(if any) under which it could be revisited.

General rules that came out of this work:

- On this stack, client→DLL dispatch is cheap. **Batching client-side GL
  submission adds staging cost without removing the frontend's per-vertex
  work**, which dominates. Any batching idea must beat that math on
  hardware.
- A change is retained only if a three-run physical-hardware median beats the
  baseline and parity counters/screenshots are preserved. Gains under 2% are
  noise.
- Never enable an experiment below without the measurement protocol in
  [../operations/runbook.md](../operations/runbook.md).

## 1. One generic client-array draw per world polygon (68k R200)

- **Tried:** replace each short seven-float `glpoly_t` with
  `glVertexPointer`/`glTexCoordPointer` + `glDrawArrays(GL_POLYGON)`.
- **Result (68060/RV280, 640×480×32, 3 cold boots):** 3.002 → 2.849 FPS,
  **−5.4% elapsed**, with full geometry parity.
- **Why:** the generic array decoder per component costs more than the
  dispatch calls it removed on ~4.66-vertex polygons.
- **Decision:** rejected permanently for the per-polygon form.
- **Revisit only with:** a dedicated R200 Quake-polygon entry that consumes
  the seven-float layout directly and keeps one source vertex per submitted
  vertex (the original "future direction").
- Evidence: [history/STEP4_REJECTED_GENERIC_ARRAY_PATH.md](../performance/history/STEP4_REJECTED_GENERIC_ARRAY_PATH.md).

## 2. A1 world batching + A1b water/underwater staging (`r_worldbatch`)

- **Tried:** persistent immutable indexed world batches (one
  `glDrawElements` per texture/segment), then water/underwater staging, plus
  every submission variant (client arrays, `glDrawArrays`, batch-level
  FastPath direct writes at 32K).
- **Result (A4000 MPC7410+RV280, matched DLL/host, demo1, fullscreen and
  windowed):**

  | Variant | fps vs immediate control |
  |---|---:|
  | Client arrays + `glDrawElements` (A1) | −2 … −5% |
  | Item 2 water/underwater staging (`glDrawArrays`) | neutral-to-negative |
  | Batch-level FastPath direct writes (32K) | −1.7% |
  | FastPath per polygon in `mglTV23fv` | −3 … −4% |

- **Why:** the frontend does the same per-vertex work for every route;
  staging/conversion cancels the dispatch savings. This reproduces the 68k
  result on the PPC side.
- **Decision:** `r_worldbatch` stays **0**; the batching line is closed. The
  code remains in the tree for reference and must not be enabled.
- **Revisit only with:** a dedicated MiniGL Quake-polygon submission entry
  (see entry 1) and a measured dispatch reduction.
- Evidence: [history/WORKSTREAM_A_RENDERER_PLAN.md](../performance/history/WORKSTREAM_A_RENDERER_PLAN.md)
  (A1b verdict section).

## 3. W-80 driver fence coalescing

- **Tried:** split the per-dispatch full-idle fence tail into a consumption
  fence and a completion fence; coalesce the group.
- **Result:** three distinct failures on the production stack — hard wedge
  (group=4), corrupted rendering stuck at a console window (group=2, weak
  fix), silent submission stall after ~9 frames with only 9 presents
  (group=2, corrected fix).
- **Why:** this hardware/CP-microcode configuration does not survive
  back-to-back indirect dispatches without an intervening idle wait.
- **Decision:** **parked**. Source remains in the external trees, default-off
  and inert; production chip does not advertise the capability, rebuilt
  native library was not deployed. `MGLPPC_FENCE_GROUP` deleted.
- **Revisit only with:** CP-state register instrumentation and a much larger
  hardware budget; the expected gain no longer justifies the risk.
- Evidence: [history/PERFORMANCE_PLAN_2026-09-20_0024.md](../performance/history/PERFORMANCE_PLAN_2026-09-20_0024.md)
  §11.6–11.9.

## 4. MiniGL `MGLPPC_EXPAND_MIN` below 4

- **Result:** `MGLPPC_EXPAND_MIN=1` caused a grey-screen hard hang requiring
  a cold power cycle; `=2` artefacts are quarantined
  (`bin/minigl_ppc_r200_w12e2.warpelf`, A3AE5A71).
- **Decision:** leave the expansion threshold at ≥4; do not deploy
  low-threshold builds.

## 5. `MGLPPC_BATCH` two-tile upload

- Never enabled after a measured regression. Do not re-enable without a new
  measurement.

## 6. Mixing bsdsocket fd layers

- **Tried:** `socket()` from libglosswos plus raw `CloseSocket`/`IoctlSocket`
  LP macros.
- **Result:** every call returns −1; fd values are libglosswos table indices,
  not raw bsdsocket handles.
- **Decision:** resolved by using the pure `__P*` layer. Never mix layers and
  never `CloseLibrary(SocketBase)` (libglosswos owns it).

## 7. Forced AltiVec (`MGLPPC_ALTIVEC=1`)

- **Result:** under CP emit, forced vector vs auto vs scalar was within noise
  (12.158 / 12.046). Auto-probe already enables AltiVec on the 7410.
- **Decision:** env removed; do not re-add.

## 8. `r_dynamic 0` as a performance control

- **Result:** both control attempts stalled during startup with no valid
  benchmark; it also removes dynamic lighting.
- **Decision:** not a performance lever. CPU lightmap build was ~0.13 ms and
  the pickup overlay 0 ms on the measured hitches — the upload path was the
  cost, and it was fixed directly.

## 9. Our release host `F0343E03` vs validated `FB712873`

- **Result:** not significant on gears (126.475 vs 126.903); the validated
  host is the reference one.
- **Decision:** validated host deployed; ours archived. Do not swap hosts
  when comparing to historical numbers.

## 10. Native `dcbf`+`sync` sound buffer flush (resolved 2026-09-22)

- **Tried (2026-09-20):** replace the per-frame `CacheClearE` LP call with a
  native `dcbf`+`sync` over the painted range.
- **Result then:** produced silence; instruction encoding was verified correct.
- **Root cause found (2026-09-22):** the flush range was masked with
  `AHIBUFFERSIZE` (256 KB, the old low-level constant) instead of the actual
  high-level ring size (`wos_buflen`, 64 KB). After ~1.5 s of painting the
  mask pointed past the ring, so the native flush never touched the painted
  samples. `CacheClearE` had hidden the bug because the LP stub flushes the
  whole PPC cache. The mask is now the real ring size.
- **Decision:** native `dcbf`+`sync` is **adopted as the default**
  (`-sndflush native`; `-sndflush exec` keeps the old path). Validated on
  hardware: `wos_cache` proves the 68k sees `dcbf`'d fast-RAM data and does
  not serve stale lines; audio is audible and clean; 1.1 ms/frame cheaper.
- **Also found:** the dominant sound cost was never the flush but the AHI
  mode - CPU-mixing Paula modes cost ~4-5 ms/frame on the shared 68060; the
  AHI DMA mode costs ~0.1 ms. See
  [targets/warpos.md](../targets/warpos.md#ahi-on-warpos-use-the-q3-high-level-scheme-measured).

## 11. MiniGL `MAX_VERTICES` 4096

- **Result:** 4096 clipped batched draws (missing world geometry with the
  batching path enabled); 16384/32768 showed no further effect.
- **Decision:** stays **8192**. The 8192 DLL is the deployed one.

## 12. Known-open (not rejected)

These are still open, do **not** record them as closed:

- Coalescing the per-frame lightmap uploads (frame 260 issues four).
- ASL ScreenModeRequester in `VID_Init`.
- Windowed mode is pathologically slow on the main-branch DLL
  (18–20 fps vs 86 fps on W-10), independent of batching; fullscreen is
  unaffected.
- `mglWriteShotPPM` silent no-op on older DLLs (fixed in later ones; verify
  CRC).
