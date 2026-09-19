# GLQuake WarpOS performance plan — 2026-09-20 00:24 CEST

**Committed 2026-09-20 ~01:55 CEST** (working trees were clean checkpoints
of everything below):

| repo | branch | commit |
|---|---|---|
| BlitzQuake_src | `wos-timedeo-hang` | `ff307fb` |
| MiniGL_WOS_V19_mglQ3 | `main` | `48aee81` |
| p96-driver | `main` | `39fc924` |

Machine: Amiga 4000, 68060 host + Prometheus + Radeon 9200 (RV280),
MPC7410 PPC 400 MHz, WarpOS. Benchmark: `timedemo demo1` (969 frames),
fullscreen 800x600x32 (16-bit measured separately), `-noudp -nosound`,
`-benchmarkquit -benchmarklog`, CP emit on (`MGLPPC_CPEMIT=1`).

Status markers: **[DONE]** finished and verified locally · **[PENDING-HW]**
code ready, needs physical-machine runs · **[PENDING]** not started.

---

## 0. Where we are (from PERFORMANCE_RESULTS_2026-09-19_2339.md)

| Config | FPS | ms/frame |
|---|---:|---:|
| Pre-CP best (particles 1 px) | 5.089 | 196.5 |
| CP emit on, 800x600x32 | 12.158 | 82.3 |
| CP emit on, 800x600x16 | 15.009 | 66.6 |

Interpretation:

- CP emit helped Quake ~2.4x (not just the ~30-39% seen on gears). The 68k
  host-side command expansion was a real bottleneck and is now largely gone.
- The current #1 bottleneck is **unknown**. The 16-bit vs 32-bit gap (+23%)
  shows a pixel-format-dependent cost but does not prove GPU fill-rate
  saturation. No CP-aware per-frame attribution exists yet (the pre-CP
  publish profile counters are bypassed under CP emit).
- AltiVec vs scalar vertex serialization was within noise (12.158 vs 12.046)
  under CP emit. No further vertex-serialization work until profiling says
  otherwise.

Code-reading findings (this session, no hardware runs):

1. **Lightmap update amplification (strongest recurring-cost suspect).**
   `gl_rsurf.c` `R_DrawSequentialPoly()` (lines ~750-779) and the underwater
   path (~912-940) upload a dirty lightmap atlas *per surface draw*, so one
   atlas can be uploaded several times per frame. On the MiniGL side the
   retained-shadow TexSubImage path
   (`minigl_r200_warpos_compile_probe.c:5419-5479`) re-uploads the **entire**
   128x128 BGRA atlas (64 KiB) per update, with a flush/submit/barrier
   before every commit. More surfaces touched by dynamic light = more full
   atlas re-uploads and more pipeline barriers.
2. **CP surface-descriptor cache churn.** `ResolveCpSurface()` (frontend)
   caches only 16 descriptors, wipes the whole cache when full, and
   `PlatformCall()` wipes it on most resource-control commands. Every miss
   is a synchronous PPC->68k mailbox round trip. Quake (many textures +
   frequent lightmap commits) is a worst case. No counters exist today.
3. **Pickup flash first-use texture creation.** `Draw_AlphaFill()`
   (`gl_draw.c:1049-1095`) lazily creates one white 1x1 texture per alpha
   level *during gameplay* (first time each of 15 levels is hit). Cheap to
   fix by pre-creating at init. `gl_polyblend 0` is the quick A/B lever
   (disables ALL screen overlays: pickup, damage, contents, powerup).
4. **Misc confirmed dead work:** `V_UpdatePalette()` builds gamma ramps +
   256-color palette every changed frame but `VID_ShiftPalette()` is empty
   (`gl_vidamiga.c:224`). `-clpri N` overrides were parsed *before*
   `COM_InitArgv()` so the override argument never worked; the DEFAULT
   priority raise (5) did execute (pri stays 5 when the arg is absent), so
   historical runs ran at client priority 5 and the fix changes nothing for
   default runs — benchmark comparability intact.

## 1. Phase plan

### Phase 0 — Baseline provenance + quick A/B matrix — [DONE via profiler, see §4]
- B/C/D matrix rendered unnecessary: profiler measured BLEND (0.011 ms/frame)
  and lightmap updates (73 KB/demo) as negligible under CP emit.
- Control run of yesterday's exact pair still wanted (§4 item 4).
- Record client/DLL/host/library/driver CRCs and build flags for every run.
  Note: the Quake Makefile DLL override defaults to `Work:ibdraw/minigl_ppc.dll`;
  the recorded Quake runs used the game-dir DLL — make this explicit per run.
- Confirm CP emit + MULTI_FENCE caps active; record present mode, sync,
  buffers, priorities, renderer cvars.
- Matrix at 800x600x32, CP on (order-balanced, >=2 same-boot samples each):
  | run | gl_polyblend | r_dynamic | purpose |
  |---|---|---|---|
  | A | 1 | 1 | control |
  | B | 0 | 1 | overlay cost |
  | C | 1 | 0 | lightmap-update cost (diagnostic only) |
  | D | 0 | 0 | interaction |
- Then: 640x480 vs 800x600 at 16/32; `r_particle_size 1 vs 3` under CP;
  a flash-repetitive demo segment vs the same segment with overlays pre-warmed.
- Accepted changes get 3 cold-boot medians before becoming default.

### Phase 1 — CP-aware frame profiler (client side) — [DONE + VALIDATED ON HARDWARE] see §2/§4
- New `glquake/frame_profile.{c,h}`: per-frame nested section timing on the
  PPC timebase (`mftb`), top-16 slowest-frame table, lightmap-upload and
  overlay counters, one-shot text dump after the timed interval.
  Runtime opt-in `-frameprofile [path]`; one flag test when disabled.
- Sections: SCREEN, VIEW, SCENE, WORLD, VIS, CHAINS, LMBLEND, LMBUILD,
  ENTBRUSH, ENTALIAS, PARTICLES, WATER, MIRROR, GLOW, CLEAR, BLEND, HUD,
  PRESENT (+ host_other derived). Counters: lm_upload_calls/bytes,
  overlay_draws, overlay_first_use.
- NOT used for timing: `Sys_FloatTime()` (DateStamp, 20 ms quantum, 68k
  gateway). Calibration: one 0.5 s `Delay(25)` at init when enabled.
- MiniGL-side CP-path attribution: descriptor-cache counters + one-line
  teardown report (see §2). Host CP_PROF already exists for drain/dispatch.
- [PENDING-HW] First profiling runs + `tools/analyze_frame_profile.py`
  interpretation (P50/P95/P99, worst frames vs flash/lightmap counters).

### Phase 2 — Small independent wins — see §2 for status
1. Pre-create flash alpha textures at Draw_Init — [DONE]
2. `-clpri` parse-order fix — [DONE]
3. CP surface-cache counters (measurement first) — [DONE]
4. CP surface-cache capacity policy (64/128 entries, LRU) — [PENDING]
   (only if counters show capacity/invalidate misses; keep invalidation
   conservative until lifetimes are proven)
5. Skip unused palette ramp build (V_UpdatePalette) — [PENDING]
6. Quake-side redundant world state-call suppression — [PENDING]

### Phase 3 — Coalesce world lightmap uploads — [PENDING]
- Gather visible world surfaces first; rebuild changed lightmaps; upload
  each dirty atlas once per world pass instead of per surface draw.
- Keep BSP/PVS traversal authoritative; brush models/mirrors unchanged at
  first. Acceptance: identical lighting, fewer uploads/barriers, lower
  lm_upload_bytes, better mean/tail frame time.
- Do NOT start by narrowing Quake's dirty rect only — MiniGL would still
  re-upload the full atlas per commit; fix the per-surface multiplication
  first (or both, measured separately).

### Phase 4 — Attack whatever the profiler shows — [PENDING]
Decision table:
| profiler says | do |
|---|---|
| PPC emit/dispatch CPU | batching within packet limits; find batch-split causes |
| descriptor/control round trips | cache policy (Phase 2.4); versioned resource replies |
| ~20 ms yield steps | CP-path progress-aware waits (bounded, deadline-safe) |
| host fence-drain dominant | driver-side submit tail / fence semantics (matched-pair deploy!) |
| present/blit dominant | P96 present-path attribution before any change |
| Quake frontend dominant | world batching (A1-style, indexed; NOT the rejected per-poly arrays) |

### AltiVec — [PENDING] after profiler attribution
Candidates IF their buckets are large: lightmap accumulate/pack
(R_BuildLightMap/Color), texture-shadow BGRA conversion
(FrontendStageTextureBGRA / FrontendShadowPatch), CP swap-copy in
PublishCpSlot. Keep scalar fallbacks + runtime G4 probe; beware documented
aperture-store hazards (stswi GPU hang notes) — vertex stores being OK does
not validate other aperture paths. NOT a fullscreen-blend replacement
(that is GPU blending) and cannot remove sync waits.

## 2. What was done in this work session (2026-09-20 00:24-00:40 CEST)

All changes are source-level; **no hardware was touched and no benchmarks
were run**. Local builds/tests below were executed 00:37-00:39 CEST.

| item | files | status |
|---|---|---|
| plan document | PERFORMANCE_PLAN_2026-09-20_0024.md (this file) | DONE |
| `-clpri` parsed before `COM_InitArgv` (never worked) | glquake/sys_amiga_std.c | DONE |
| frame profiler (Phase 1 client side) | glquake/frame_profile.{c,h}, host.c, gl_screen.c, gl_rmain.c, gl_rsurf.c, cl_demo.c, sys_amiga_std.c, Makefile.GCCAmigaWOS_MGL_V19 | DONE |
| flash fill textures pre-created at Draw_Init | glquake/gl_draw.c (`Draw_AlphaFillPrewarm`) | DONE |
| overlay counter hooks in Draw_AlphaFill | glquake/gl_draw.c | DONE |
| lightmap upload counters at all 3 TexSubImage sites | glquake/gl_rsurf.c | DONE |
| CP surface-descriptor cache counters + one-line teardown report (`CP_SURF hit=... miss=... reset=... inval=... stale=...`) | MiniGL backend_r200/library/minigl_r200_warpos_compile_probe.c | DONE |
| build: client warpelf | `glquake/warpos_gcc_v19/glquake/glquake_wos.warpelf` CRC32 **59C0F253** (1079921 B), ET_REL validation passed | DONE |
| build: DLL warpelf | `MiniGL_WOS_V19_mglQ3/bin/minigl_ppc_r200.warpelf` CRC32 **937E20DC** (389920 B) | DONE |
| `tools/test_texture_cache.py` | PASS (identical to baseline) | DONE |
| `tools/test_batch_budget.py` | PASS 579794/0 (identical) | DONE |
| `tools/test_batch_expansion.py` | PASS 2247 checks, expected mutant failures (identical) | DONE |
| `tests/test_r200_cp_emit.py` | PASS (identical) | DONE |
| `tests/test_r200_transport_profile.py` | FAILS **identically to HEAD** — pre-existing, documented in MiniGL `backend_r200/NEXT_SESSION_HANDOVER.md` | DONE (no regression) |

### Additional findings folded in from MiniGL repo root docs (00:40 CEST)

From `MiniGL_WOS_V19_mglQ3/how_to_benchmark.md` (full hardware protocol),
`backend_r200/NEXT_SESSION_HANDOVER.md` and `backend_r200/HOST_TARGET_CACHE.md`:

- **Never mix DLL and host builds** — a stale/rebuilt combination drops
  gears to a deterministic ~7.8 fps with `presents=0`. Deploy DLL + host
  from the same build and record BOTH CRCs. This applies to every future
  MiniGL experiment in Phase 2.4 and beyond.
- The host already has a per-burst bridge-snapshot cache
  (`MGLPPC_HOST_TARGET_CACHE`, quantified: −63% queries, ~1.1% fps on
  gears) — Phase 4 "68k dispatch work" starts from there, not from zero.
- **No wide aperture stores** (stswi-class GPU hang) — reinforces keeping
  any AltiVec CP-copy idea behind explicit on-hardware verification.
- Profiler builds (MGLPPC_PROFILE=1) live in dedicated OBJDIRs and are
  never benchmarked against quiet numbers — our client `-frameprofile` is
  runtime-gated precisely so one binary can do both.
- Old sh-intro/tex-upload gates are obsolete; the current source stack is
  ~3x faster than the stale deployed pair recorded in older handovers.

## 3. How to run the next hardware session

Follow `MiniGL_WOS_V19_mglQ3/how_to_benchmark.md` top to bottom (cold boot,
wait_bridge, explicit reconnect, InitPPC once, fresh host per run, serialized
Amiga file ops, CRC-verify every transfer, ≤16-char names). Two client-side
additions this session:

```text
; profiling run (client built from this tree, ELF 59C0F253):
SetEnv MGLPPC_CPEMIT 1
C:InitPPC
Run >NIL: Execute DH2:ibdraw/hostrun
cd Work:games/quake
Stack 1048576
Run >NIL: glquakeWOS -width 800 -height 600 -bpp 32 -noudp -nosound \
     -frameprofile DH2:wosbuild/frameprof_A.txt \
     -benchmarkquit -benchmarklog DH2:wosbuild/result_A.txt +timedemo demo1
; then pull DH2:wosbuild/frameprof_A.txt and result_A.txt
```

- A/B matrix runs differ only in `+gl_polyblend 0` / `+r_dynamic 0` extras.
- The `CP_SURF hit=... miss=... reset=... inval=... stale=...` line prints
  ONCE from the DLL at context teardown (visible in the client console /
  redirect). Host still prints `CP_RUN ...` at shutdown.
- `overlay_first_use` in the FP COUNTERS line must be 15 (prewarm) and NOT
  grow during the demo — that is the acceptance check for the flash fix.
- If the descriptor counters show meaningful `miss`/`reset`/`inval`, the
  next MiniGL change is the Phase-2.4 cache policy (remember: rebuild and
  deploy DLL + host together from the same tree, record both CRCs).

Regression gates (all green except the documented pre-existing failure):

```sh
cd /home/mirek/BlitzQuake_src && python3 tools/test_texture_cache.py
cd /home/mirek/MiniGL_WOS_V19_mglQ3 && python3 tools/test_batch_budget.py \
  && python3 tools/test_batch_expansion.py \
  && python3 tests/test_r200_cp_emit.py
# test_r200_transport_profile.py fails at HEAD (pre-existing); rerun after
# the NEXT_SESSION_HANDOVER issues are addressed, not as our gate.
```

Rollback: all edits are in git dirty state on the three repos
(`git diff` per repo); no deployed artifact changed. The previous deployed
identities remain those recorded in PERFORMANCE_RESULTS_2026-09-19_2339.md §3.
Fresh build identities for this session: client ELF **59C0F253**,
DLL warpelf **937E20DC** (neither deployed yet).

## 4. HARDWARE SESSION RESULTS (2026-09-20 00:45-01:10 CEST)

Deployed test pair (dedicated dirs, production stack untouched):
- Client `Work:games/quake/glqprof` = **E2A06D9A** (730216 B; loads DLL from
  `DH2:wosbuild/qprof/minigl_ppc.dll` baked at build).
- DLL `DH2:wosbuild/qprof/minigl_ppc.dll` = **72BA4CEA** (319436 B; source
  warpelf 937E20DC; has CP_SURF counters).
- Host `DH2:wosbuild/qprof/mglhost` = **F0343E03** (45024 B, bits 00020000;
  rebuilt from current tree, classifier check passed; same-tree pair with DLL).
- Validation: gears 640x480x16 3buf nosync CP on → **176.470 fps, error=0,
  CP_RUN 1016/255920 failed=0** (in the recorded CP-on range).
- After the session: no host/client left running, env unchanged
  (MGLPPC_CPEMIT=1, MGLPPC_ALTIVEC=1, HOSTPRI=-1).

### Timedemo results (800x600x32, demo1, CP on, profiler enabled)

| run | fps | note |
|---|---:|---|
| A1 | 11.569 | first profiler build (display bugs only) |
| A2 | 11.613 | fixed dump; spread 0.4% -> profiler overhead negligible |

Reference from yesterday's different build pair: 12.158 fps. The ~4.5%
delta vs A-runs is NOT attributable yet (different host FB712873 vs
F0343E03, different DLL, boot state, 1-9% documented session variance).
Next session: one control run of `glquakeWOS` + `DH2:ibdraw/hostrun`
(yesterday's exact config) on a fresh boot to isolate.

### Frame budget — A2, per-frame averages (86.1 ms total at 11.6 fps)

| bucket | ms | % | what |
|---|---:|---:|---|
| ENTALIAS | 30.8 | 36% | alias models + sprites (R_DrawEntitiesOnList(2)) |
| ENTBRUSH | 21.9 | 25% | brush entities: items, rockets, grenades |
| WORLD chains | 10.9 | 13% | DrawTextureChains |
| LMBLEND | 7.6 | 9% | lightmap blend pass |
| PRESENT | 5.9 | 7% | mglSwitchDisplay (BLIT) |
| HUD | 1.8 | 2% | 2D + sbar + console |
| host_other | 2.7 | 3% | server/input/demo read |
| BLEND | 0.011 | ~0% | pickup/damage overlay |
| particles | 0.4 | <1% | |
| VIS + LMBUILD + misc | ~0.3 | <1% | |

### Direct answers to the session's questions

1. **Full-screen pickup blink is NOT a performance factor.** BLEND totals
   10.6 ms across the WHOLE demo (0.011 ms/frame); every one of the 15
   slowest frames has blend = 0.000-0.079 ms. The slow frames that happen
   to show a blink are slow for other reasons. A/B matrix runs B/C/D are
   therefore unnecessary for the blend question.
2. **Dynamic lightmap updates are NOT a bottleneck under CP emit.** Whole
   demo: 35 atlas uploads totalling 73 KB; LMBUILD 0.4 ms total. The
   pre-CP lightmap concern is gone with the bottleneck that created it.
3. **The real cost is entity rendering: 51% of the frame** (ENTALIAS 36%
   + ENTBRUSH 25%), then world 22%. These are PPC frontend costs (record
   building + per-vertex work inside the GL calls).
4. **Hitch families (150-243 ms frames):** (a) world spikes — frames with
   large visible area, world 128-181 ms (225, 82, 260, 290); (b) entity
   spikes — ENTBRUSH up to 154 ms (frames 542/538: simultaneous explosions?
   many brush entities), ENTALIAS up to 109 ms (81, 62); (c) rare present
   spikes ~25 ms (62, 469) and one HUD spike ~50 ms (215).
5. **68k host:** client never visibly blocks on it (host_other 2.7 ms;
   present avg 5.9 ms). BUT the entity/world buckets may still CONTAIN
   ring-space waits that the client profiler cannot split out — that
   attribution needs the MGLPPC_PROFILE=1 DLL (next step).
6. Flash prewarm accepted: overlay_first_use=0 across the demo (262
   overlay draws, all used pre-created textures).

### Known limitations found
- The DLL's CP_SURF teardown line goes to the DLL console, which is lost
  under `Run >NIL:` (host CP_RUN works because host stdout is redirected).
  Next session: capture it via a console-bearing client launch or route
  the report into a file.
- `wall_s` in the FP dump includes map load (~20 s, frame 1); analyzers
  must exclude frame 1 / compare against `-benchmarklog` interval.

### Revised priorities (supersedes §1 ordering)
1. [PENDING] MiniGL MGLPPC_PROFILE=1 attribution run (dedicated dir) to
   split frontend-pack vs serialize vs ring-wait inside ENTALIAS/ENTBRUSH
   — decides between Quake-side entity batching and MiniGL per-vertex work.
2. [PENDING] Entity path optimization: ENTBRUSH per-poly overhead (46-draw
   brush models), alias pose-lerp vertex stream, `r_vertexarrays` path
   evaluation under CP.
3. [PENDING] World spike mitigation: `r_worldbatch 1` (texsort) evaluation
   under CP for the CHAINS spikes.
4. [PENDING] Control run of old client pair to isolate the ~4.5% delta.
5. [DONE] A/B matrix B/C/D — answered by profiler data; skip.
6. Descriptor-cache policy (Phase 2.4) — only after CP_SURF capture shows
   miss pressure; keep counters in place.

Deployment rollback: production artifacts untouched
(glquakeWOS 6B7B0938, game DLL F0089138, ibdraw DLL FCEF7CE9, host
FB712873). Test dir `DH2:wosbuild/qprof` can be deleted at any time.

## 5. ATTRIBUTION + TRANSPORT-FIX SESSION (2026-09-20 01:15-01:50 CEST)

### Profiled attribution run (P) — what the buckets really were

Profiled DLL `BC7C2939` + profiled host `03DC3364` (same tree, dedicated dir
`DH2:wosbuild/qprofdir`, client `glqprofp` F5B5B070), 800x600x32 demo1:
**11.281 fps** (profile overhead ~3%). Findings:

- `PPC_YIELD calls=2526, avg=max=20.0 ms` — **every** yield parked a full
  50 Hz tick: ~50 s of the session spent parked in 20 ms chunks inside the
  publish/mailbox waits.
- `CP_SURF hit=796874 miss=37551 reset=2321` — the descriptor cache was
  wiped by every texture control (2321 wipes), each wiping forced ~16
  synchronous PPC->68k mailbox queries: 37.5k queries at ~1.6 ms.
- `CP_PROF entries=15024 drain_ms=233 dispatch_ms=5535` and host
  `wall=137 s busy=~14 s` — **the 68k host is exonerated** (~5-10% busy,
  idle the rest). It is NOT the bottleneck.
- The client profiler's ENTALIAS/ENTBRUSH/WORLD buckets were therefore
  mostly TRANSPORT STALLS (parks + query round trips), not vertex work.
- Pickup stutter mechanism confirmed: frame 215 has overlays=1 AND a
  49.7 ms HUD spike; pickups/text add publishes that land in the stall
  windows. Text is the trigger; the 20 ms yield quantum is the cost.

### Transport fixes (MiniGL frontend only, no protocol/host change)

1. Descriptor cache: texture/lifecycle control classes no longer wipe the
   cache (only UNREGISTER/DESTROY/SHUTDOWN clear it); capacity now
   overwrites the oldest entry (ring) instead of wiping all 16. Entry
   epoch re-validation unchanged.
2. PublishCpSlot ring wait + barrier present wait: progress-aware spin
   (only park after a full budget with zero retirement/consumption
   progress) — same policy as the previously validated drain loop.
3. PlatformMailboxWait: yield budget x32 (parks cost 20 ms, not 100 us).

### Results

| run | pair | fps | notes |
|---|---|---:|---|
| P (before) | profiled | 11.281 | baseline attribution |
| **B (after)** | profiled | **16.724** | +48% same-boot A/B |
| **C (after, quiet)** | quiet | **17.205** | `56.320 s, 969 frames`, clean exit |

- Yields 2526 -> 193; ENTBRUSH 20.8 s -> 7.4 s; SCREEN 77.3 s -> 51.9 s;
  worst-ring max 243 ms -> 190 ms; zero frames >200 ms.
- `CP_RUN dispatches=15046 dwords=8070596 failed=0`, `presents=978`,
  rendering identical, `overlay_first_use=0` (prewarm intact).
- Quiet DLL `DH2:wosbuild/qprof/minigl_ppc.dll` = **B26FA0E2**
  (warpelf 16C5361A); rollback `minigl_ppc.pre-cache` (B26FA0E2's
  predecessor, the 937E20DC-source build).
- **+42% over the 12.158 fps recorded 2026-09-19** — pending the
  three-cold-boot median protocol before calling it final.

### Revised next steps
1. Re-run P-style attribution on the fixed stack to re-rank buckets
   (ENTALIAS 25.6 s is now the top bucket — likely real frontend vertex
   work this time; ENTBRUSH dropped 64%).
2. Descriptor misses remain 36k via capacity overwrites — find why the
   live token working set exceeds 16 (expected ~3 screen buffers).
3. World spikes (frames 82/86/260, world 120-150 ms) — `r_worldbatch`
   evaluation under CP.
4. Three-cold-boot median validation of the fixed DLL; then promote to
   the default game-dir DLL with the matched host.

## 6. Open questions for the operator
- Primary acceptance target: 800x600x32 with unchanged visuals (16-bit separate)?
- Installed PPC fast RAM / 68k fast RAM sizes (profiler buffers are static,
  but Phase-3/4 options depend on it).
- A short demo segment with frequent pickups (for flash-stall reproduction).
  PARTIALLY ANSWERED by profiler data: the pickup stutter is transport-stall
  quantization, now reduced; see section 5.
