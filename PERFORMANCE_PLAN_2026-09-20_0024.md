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

> **Superseded for direction:** the "Revised priorities" at the end of this
> section were re-audited in §7. The measurements stand; several
> interpretations do not (descriptor-cache attribution, alias claim).

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

> **Superseded for direction:** the transport gain is real, but §7 corrects
> the cache attribution, the 68k exoneration and the lightmap conclusion,
> and demotes the deployed B DLL to diagnostic-only until W-01/W-02 land.

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

---

## 7. PLAN REVIEW + CORRECTIONS (2026-09-20 session 2, code re-audit)

Supersedes the `Revised priorities` in §4 and the `Revised next steps` in §5.
No hardware was touched for this review; every claim below is tied to a saved
artifact in `/tmp/opencode` or to a source line.

### 7.1 Confirmed

- The transport work is real: P (profiled, before) **11.281 fps** ->
  B (profiled, after) **16.724 fps**, C (quiet) **17.205 fps**. Same boot,
  same demo, `CP_RUN ... failed=0`, rendering unchanged.
- Flash prewarm works: `overlay_first_use=0` in P, B and C.
- The 20 ms park storm is gone: `PPC_YIELD` calls 2526 -> 193.
- The 68k host is no longer doing the old per-frame command expansion; that
  bottleneck (2.4x CP gain) stays fixed.

### 7.2 Corrections

**C1. The descriptor-cache attribution was misread.**
Saved reports:
- P: `CP_SURF hit=796874 miss=37551 reset=2321 inval=87`
- B: `CP_SURF hit=798512 miss=36276 reset=36260 inval=0`

"2321 wipes caused by every texture control" is wrong: **2321 was the
capacity-reset counter**; lifecycle invalidation was 87. Miss queries fell
only **3.4%** (37551 -> 36276). Capacity (live token set > 16) is still the
entire miss cost. Also `reset` is not comparable across builds: before it
counted a whole-cache wipe, after it counts one entry overwritten.

**C2. The cache is not "~3 screen buffers".**
The emitter resolves four surfaces per draw: colour target (record[2]), depth
(record[3]), texture 0 (record[4]) and texture 1 (record[15]) -
`p96-driver/src/radeon3d_emit.c:1696-1700,1789-1791`. The native
`MGL_R200_PRIVATE_SURFACE_V1_QUERY` handler matches resident textures as well
as the targets (`classic_68k/.../library/r200_state.c:1688-1725`). Texture
tokens dominate the working set; a working set above 16 was expected, not
surprising.

**C3. SAFETY DEFECT in the deployed cache-lifetime relaxation (P0).**
Surface tokens are raw handle pointers: allocated at
`p96-driver/src/radeon3d_service.c:2222-2252`, freed at `:2297-2298`. Within
one device epoch a freed handle address can be reallocated to a different
surface. The relaxed policy keeps cached descriptors for texture commands, so
this sequence is possible:
1. cache entry `(token H -> descriptor D_old)`;
2. H is released (texture replaced/evicted) and later reused by another
   surface with different address/pitch/format;
3. a later `ResolveCpSurface(H)` hits the stale entry and the emitter bakes
   D_old into the CP stream.
`stale=0`/`failed=0` does not test handle reuse. **Treat the B DLL as
diagnostic-only until W-01/W-02 land.**

**C4. The 68k was not exonerated by the saved host log.**
`/tmp/opencode/host_prof.log` itself prints
`idle_is_cpu_idle=0` and `inclusive=1 mutually_exclusive=0`. Its "busy" sum is
a union of overlapping buckets (`control+barrier+total+ring+mailbox`) and
includes `mailbox_us=36.1s` of handler elapsed out of `wall_us=137s`; roughly
77 s is unaccounted setup/idle/polling. Correct statement: *host-side CP
expansion is no longer the old bottleneck, but synchronous metadata service
remains expensive and CPU saturation is not established.* Do not plan around
"68k at 5-10% CPU".

**C5. The lightmap dismissal is incomplete, and the active path is different
than assumed.**
- `FP_CountLMUpload` counts the requested Quake source rectangles (73 KiB
  total), not wire traffic.
- `/tmp/opencode/host_prof.log`:
  `HOST_SUBIMAGE count=35 handler_us=776872 native_texsub_us=744029`
  (~22 ms per host subimage call), plus
  `direct_begin=267 direct_commit=0 upload_fails=267`: on this run the
  direct full-atlas recommit theory does NOT describe what happened - all 267
  direct attempts fell back and the 35 subimages went through the native
  shared path.
- In the quiet C dump, **7 of the 8 slowest gameplay frames contain 1-4
  lightmap uploads** (frames 82/140/86/260/225/66/290; frame 797 has none).
  Correlation, not proof - but the `r_dynamic 0` control (W-30) must still be
  run on the corrected stack.

**C6. "ENTALIAS is real frontend vertex work this time" is premature.**
`R200PPCPlatformPublishSlot()` returns through `PublishCpSlot()` before the
instrumented publish path
(`backend_r200/library/minigl_r200_warpos_compile_probe.c:2039`), so on CP
builds the saved report shows `PPC_PUBLISH count=978 send_ticks=0
stall_ticks=0 ...` while ~15000 CP dispatches happened. ENTALIAS still
contains descriptor queries, emitter work and waits. W-10/W-11/W-12 close
this before any alias optimization is justified.

**C7. Real multitexture was never enabled in the recorded runs.**
`CheckMultiTextureExtensions()` requires `-extensions`
(`glquake/gl_vidamiga.c:353-361`); the benchmark commands do not pass it, and
with `gl_mtexable` false the default `gl_texsort 1` two-pass path is what the
CHAINS/LMBLEND buckets measure. `-extensions + gl_texsort 0` is an unmeasured
A/B (W-40), not a default-flip recommendation.

**C8. Profiler scope fixes needed.**
`FP` wall includes map load; the frame timer starts after `Host_FilterTime`,
excluding the `Sys_FloatTime()` gateway; only the worst-16 ring survives; the
analyzer's ">=1 s discard" is a crude load-frame proxy. Counter baselines
must start at the timedemo boundary, not at DLL/context init.

### 7.3 Revised execution order (replaces §4/§5 orders)

| Priority | Work | Gate |
|---|---|---|
| **P0** | W-01 safe descriptor invalidation; W-02 reuse regression | no stale descriptor after mutation/reallocation |
| **P1** | W-10..W-12 CP sub-phase instrumentation, timedemo-boundary reset, host per-command timing; W-13 re-attribution | query/emit/copy/wait split without double counting |
| **P2** | W-20 safe 16/64/128 capacity A/B; W-21 replacement policy | fewer timed queries + lower frame time |
| **P3** | W-30 `r_dynamic 0`; W-31 `gl_polyblend 0`; W-32 LM upload timer | matched slow-frame effect measured |
| **P4** | W-40 `-extensions + gl_texsort 0`; W-41 `r_worldbatch` | lower total cost, image correct |
| **P5** | W-50 alias/entity work; W-60 AltiVec - only if exclusive PPC work remains | exclusive time gain converts to wall time |
| **Gate** | W-70 three cold-boot median; W-71 gameplay soak | promote only the validated pair |

Keep the driver, host protocol, ring ownership and GPU fence semantics
unchanged for P0-P3; those keep the experiments small. Retain explicit
big-endian CPU/Radeon byte-order conversions in any later binary/ABI change,
and regenerate 68k/PPC assembly-offset headers for any shared-structure
change. Profiler buffers and caches stay bounded in the correct fast-memory
domain.

---

## 8. WORK LOG (all work that has to be done)

Status: **[DONE]** verified locally · **[CODE]** implemented, build pending ·
**[PENDING-HW]** needs physical runs · **[PENDING]** not started.

### P0 - descriptor-cache correctness (DONE this session)

- [x] **W-01 [DONE + HW]** Safe descriptor-cache lifetime in
  `backend_r200/library/minigl_r200_warpos_compile_probe.c`:
  drop cached entries for a texture's surface token *before* every command
  that can release/replace it, plus drop-on-mirror-replace and
  drop-on-mirror-invalidate. Capacity raised (16 -> 128) with the reset
  counter retained; new `drop=` counter in `CP_SURF`. Lifecycle commands
  still clear the cache.
- [x] **W-02 [DONE]** Regression:
  `tests/test_r200_cp_surface_cache.py` - same-epoch token reuse returns the
  new descriptor; lifecycle clear; capacity/eviction; texture-mirror drops;
  active-texture helper; the real `FrontendDropCachedSurfacesForCommand`
  switch is compiled and exercised with the real protocol value layouts.
- [x] **W-03 [DONE]** Local build of DLL warpelf + host; run existing suites
  (`test_r200_cp_emit.py`, `test_r200_surface_bridge.py`, `test_batch_*`,
  `test_texture_cache.py`, cp surface tests) - all pass. Pre-existing local
  failures, identical at HEAD (not ours): `tests/test_r200_transport_profile.py`
  and `tests/test_r200_bind_cache.py`.
- [x] **W-04 [DONE + HW]** Hardware validation (see §8.1).
- [x] **W-06 [DEFERRED]** Native-eviction hole: a shared upload can free a
  DIFFERENT texture's surface without naming it. A blanket clear per shared
  upload is too expensive (816 shared uploads; each refill ~35 queries);
  close it with an eviction counter in the shared-upload reply during the P1
  host work (W-12). Observed `texture_evictions=0` on every run.
- [ ] **W-05 [PENDING-HW]** Promote W-01c to the production game dir after
  W-70 (three independent cold boots). Keep B26FA0E2 as rollback; never
  deploy DLL and host from different trees.
- [x] **W-07 [DONE + HW]** Review fixes from the amiga-code-expert-flash
  audit (2026-09-20 session 2):
  - **HIGH:** `TEXTURE_UPLOAD_SHARED` carries the target enum in `value[0]`
    (GL_TEXTURE_2D = 174), not a texture name; the first switch version
    dropped texture #174 and left the real upload target exposed. Fixed by
    routing UPLOAD_SHARED/UPLOAD_BEGIN/UPLOAD_END through the active-texture
    helper, and by dropping the old mirror handle before the inline mirror
    update in the shared TexImage2D success path.
  - **LOW:** epoch-stale entries now replace in place instead of appending a
    second entry that shadowed the fresh one (one extra query per resolve
    until ring wrap).
  - **LOW, not applied:** the color-target rotation drop. Fullscreen screen
    buffers are context-lifetime surfaces and are not released on rotation,
    so there is no handle-reuse hazard; adding a drop would cost one query
    per flip. Revisit only if buffer reallocation is introduced.

### 8.1 Session results - W-01 hardware validation (2026-09-20 session 2)

Test dir `DH2:wosbuild/qprof`; production `Work:games/quake` untouched.
Host `F0343E03` was rebuilt locally and is byte-identical (CRC match), so
this is a DLL-only change.

| Artifact | CRC32 | Notes |
|---|---|---|
| quiet W-01a | `9A73B844` | first fix, subimage gap |
| quiet W-01b | `DCDAD2B9` | subimage drop-before-clear |
| profiled W-01b | `CEE8755C` | counters: hit=835801 miss=154 reset=0 inval=0 stale=0 drop=35 |
| profiled W-01c | `4D1F8E98` -> target `6430D950` | review fixes; counters: hit=835799 miss=154 reset=0 inval=0 stale=0 drop=35 |
| quiet W-01c (final) | `C2DE22A9` | review fixes; installed in qprof |
| host | `F0343E03` | unchanged, byte-identical rebuild |

Quiet timedemo samples (800x600x32, demo1, CP emit on):

| Run | fps | seconds |
|---|---:|---:|
| W-01a q1 | 24.153 | 40.120 |
| W-01a q2 | 24.057 | 40.280 |
| W-01a profiled | 24.141 | 40.140 |
| W-01b profiled | 24.069 | 40.260 |
| W-01b quiet | 23.950 | 40.460 |
| W-01c profiled | 24.177 | 40.080 |
| W-01c quiet q1 | 24.033 | 40.320 |
| W-01c quiet q2 | 23.985 | 40.400 |

All runs: `CP_RUN dispatches~15050 dwords~8.5M failed=0`, `presents=978`,
clean exits. Gears 640x480x16 3buf: **192.307 fps, error=0**.

Comparison: B/C checkpoint 17.205 fps quiet, P 11.281 fps profiled;
W-01c median **~24.01 fps, +40% over the checkpoint**, with descriptor miss
queries down 99.6% (36,276 -> 154) and `PPC_CONTROL` 37,116 -> 960. The
residual 154 misses are the first-touch of each live token plus the 35
lightmap-subimage re-queries.

Session notes:
- One client run was launched without a fresh host (the gears smoke had
  consumed it) and hung in registration; signals are ignored, so a cold
  reboot recovered it. No production artifact was changed and no GPU/CP
  hang occurred. Always start a fresh host per client run.
- `CP_SURF` output is only capturable from profiled builds (the DLL console
  does not reach the client's `Run >file` redirect); quiet builds print it
  to the lost console. The `drop=35` count exactly matches the 35
  lightmap subimage uploads, confirming the pre-command invalidation path.

### 8.2 W-10 publish attribution + re-ranked frame budget (2026-09-20 session 3)

Profiled DLL W-10 (`8198D5D7` warpelf -> `72CEBE35` target), quiet W-10
(`14F2A439` -> `083A9E15` target, installed in qprof), 800x600x32 demo1.

`CP_PHASE` (whole context, raw mftb ticks, exclusive buckets):

| Bucket | 128 slots | 16 slots |
|---|---:|---:|
| publishes | 15,061 | 15,054 |
| wait (ring/GPU retirement) | **637,209,225** | 158,569,975 |
| resolve (descriptor) | 11,289,674 | **862,944,091** |
| emit (command construction) | **394,508,891** | 39,530,982 |
| copy (aperture swap+flush) | 10,493,730 | 9,586,202 |
| publish (entry/head) | 300,010 | 289,525 |
| fps | 24.177 | 17.523 |

Reading: at 128 slots the frame is **CP-publish-bound**: ~43% of the whole
session in ring/retirement wait and ~27% in command construction, while
descriptor queries, aperture copy and publication are ~1%. At 16 slots the
resolve query storm (863M ticks) returns and fps falls to B-level - the P2
falsifier confirms the capacity attribution. (The 16-slot `emit` number is
smaller because the slot spends most of its budget in nested queries; treat
the two splits as indicative, not additive across runs.)

Client `-frameprofile` on the W-01c stack (23.855 fps, ~0.7% overhead),
per-frame share of the 970-frame run:

| Bucket | s | note |
|---|---:|---|
| ENTALIAS | 17.7 | alias models + sprites + view model |
| host_other | 18.0 | outside SCR_UpdateScreen: input/demo/server/timing |
| ENTBRUSH | 7.6 | brush entities |
| WORLD | 5.4 | VIS 0.2 + CHAINS 3.1 + LMBLEND 2.1 |
| HUD | 2.3 | |
| PRESENT | 2.4 | was 5.6-6.7 before the cache fix |
| BLEND | 0.012 | pickup/damage overlays, confirmed negligible |
| LMBUILD | ~0 | lightmap CPU rebuilds negligible; 35 uploads/73 KB |

Slowest frames are now dominated by ENTBRUSH/ENTALIAS spikes and world
area spikes (e.g. frame 260: 172 ms, world 136 ms, 4 lightmap uploads;
frame 140: 154 ms, ENTBRUSH 85 ms; frame 82: 149 ms, world 116 ms).

Key new observation: **15.5 publishes/frame carrying only ~565 dwords each**
(8.5M dwords / 15,061 publishes), far below the 8192-dword slot capacity.
Both dominant CP buckets (wait, emit) are per-publish overheads, so the next
lever is publish granularity: find why state changes submit instead of
accumulating (state-batch expansion path, `mismatchSubmits`,
`R200_EXPAND_MISMATCH_MIN`, `CanExpandStateBatch` budget) and reduce the
number of publishes per frame. Only after that consider driver-side fence
semantics (the per-dispatch fence tail contains `WAIT_3D_IDLECLEAN`, which
makes retirement track near-full GPU drain).

### 8.3 W-11 trigger counters + INCIDENT (2026-09-20 session 3)

`CP_PHASE` (corrected decode; W-10 line 1 was misread as 394M emit - it is
39,450,891) and the new `CP_TRIG` line, profiled W-11 (default threshold 4),
800x600x32 demo1, **23.997 fps**, clean run:

| CP_PHASE bucket | ticks | share |
|---|---:|---:|
| wait (ring/GPU retirement) | 632,697,743 | ~91% |
| emit (command construction) | 39,387,283 | ~5.7% |
| resolve (descriptor) | 11,271,713 | ~1.6% |
| copy (aperture) | 10,470,912 | ~1.5% |
| publish | 293,430 | ~0.04% |
| publishes=15059 commit=12896 records=14,511,685 draws=2,726,611 | | |

So the CP publish path is **wait-dominated**: ~91% of it is waiting for ring
space/GPU retirement, and command construction is small. Batch shape: 15059
publishes, 963 record dwords and 181 draws per publish on average.

`CP_TRIG` (submit decisions, not publishes):

| Trigger | Count | Per frame |
|---|---:|---:|
| mismatch (state-batch header differs) | 7,937 | 8.2 |
| slot (payload/generated budget) | 4,214 | 4.3 |
| texture (upload barriers, level load) | 1,083 | 1.1 |
| mode change | 977 | 1.0 |
| present | 978 | 1.0 |
| rotate (stream segment) | 867 | 0.9 |
| batch full / clear / replay | 0 | 0 |

The mismatch count is ~2x the `R200_EXPAND_MISMATCH_MIN` threshold per
frame, i.e. the first 4 mismatches of each slot cycle submit before
expansion engages. Reducing publishes by expanding earlier looked like the
top lever.

**[INCIDENT] `MGLPPC_EXPAND_MIN=1` hard-hung the machine (grey screen).**
Deployed profiled `w11e1.dll` (threshold 1, warpelf `A0A02144`), started a
fresh host, ran demo1. The display went grey; the bridge stayed alive and
`Status` showed both `glqprof` and `mglhost` still resident (host log
unreadable while the host is alive). Per the platform rules this is the
grey-screen/hard-hang class: **operator cold power cycle required; a warm
reboot does not reliably reset the Radeon.**

- The only delta from the clean W-11 run is the expansion threshold, so the
  state-batch expansion path (`CanExpandStateBatch` / the backward-copy
  block, `r200_geometry.c:53-73, 383-412`) is the prime suspect. Do not
  retry threshold <4 on hardware until the path is audited and covered by a
  CPU regression (recordCount/generated accounting, offset table, buffer
  bounds, service acceptance).
- Recovery after the power cycle: verify PPC health, then reinstall the
  known-good quiet DLL (`DH2:wosbuild/qw01/w10q.dll` -> target `083A9E15`,
  or `w11q.dll` if it validates) and keep `MGLPPC_EXPAND_MIN=4` (default).
- The `MGLPPC_EXPAND_MIN` Makefile knob and the `#ifndef` guard stay
  (default 4, no behaviour change), but the experimental artifacts are
  quarantined.

**Recovery (operator cold power cycle, same session):** bridge back, clean
`Status`, volatile env restored (`MGLPPC_CPEMIT=1`, `MGLPPC_ALTIVEC=1`,
HOSTPRI archived), `C:InitPPC` once. The known-good quiet DLL was reinstalled
(`DH2:wosbuild/qw01/w10q.dll` -> qprof, target **083A9E15**) and a 120-frame
640x480x16 gears health check passed (**187.5 fps, error=0**). The hung run
left no DLL-side evidence: teardown never ran, and its host log was
overwritten by the health check.

**W-22 progress (local, no hardware):**
- The expansion block was extracted into a pure
  `R200ExpandStateBatch(slot, expandedGenerated)` helper in
  `r200_geometry.c` (no behavior change: the quiet warpelf is byte-identical
  to the previous build, `BB3F46E6`).
- New `tools/test_state_batch_expansion.py`: extracts the real helper and
  compiles it under ASan/UBSan with `records[]` as the last struct field
  (overruns hit the global redzone). 110 cases across draw counts (1..192)
  and header sizes (11..8192) check `recordCount == draws*H`, per-record
  header/vertex-count layout, `lastDrawOffset/HeaderDwords`, and state flags.
- `tools/batch_expansion_check.c` gained `CheckMixed`: a compact state batch
  of N shared headers followed by M headers that differ in texture handle
  and sampler state, emitted through the real `Radeon3DEmitStream` commit
  walk. It verifies the frontend's `generatedDwordCount` estimate
  (`R200ExpandedBatchCost` + one full draw per differing record) is within
  the 8192-dword budget and upper-bounds the actual emitter output. Only
  realizable prefixes are modelled (the frontend submits before an append
  that would exceed the budget), and dual-texture chains keep the validated
  unit0/unit1 pair and differ only in sampler state.
  Result: **BATCH_EXPANSION 2723 checks, 0 failures** (the full-state mutant
  still fails 24), all other local suites green.
- **Verdict:** the accounting invariants and the mixed-chain emitter bound
  are now CPU-proven. The threshold lever is no longer blocked by accounting
  evidence; a hardware retest is still a machine-risk decision (it may hang
  again, requiring another cold power cycle).

Next actions (in order, revised after the incident):
1. **W-22 [DONE, audit green]** Optional cautious hardware retest:
   `MGLPPC_EXPAND_MIN=2` profiled build is ready
   (`bin/minigl_ppc_r200_w12e2.warpelf`, warpelf `A3AE5A71`). Run only with
   operator awareness of a possible grey screen; rollback is the known-good
   quiet DLL `083A9E15`.
2. W-11 boundary reset so gameplay-only phase shares are available.
3. W-12 host timing + W-06 eviction counter.
4. P3/P4 items (controls, renderer path, and only then driver fence
   semantics: the per-dispatch fence tail's `WAIT_3D_IDLECLEAN` is the
   structural reason retirement tracks near-full GPU drain).

### P1 - CP attribution (needed before any entity optimization)

- [x] **W-10 [DONE + HW]** `PublishCpSlot()` split into exclusive wait /
  resolve / emit / copy / publish counters (mftb), reported as `CP_PHASE`
  at teardown (file append in profiled builds). Results in §8.2.
- [x] **W-11 [DONE — session 4; HW re-attribution still wanted]** Boundary
  reset implemented: `R200PPCResetCpAttribution()` is called from
  `Lib_glHint(MGL_PERF_COUNTERS_HINT, GL_NICEST)` — the exact call the
  client makes when the timedemo timer starts — so CP_SURF/CP_PHASE/CP_WAIT
  now cover only the demo interval (setup + map load excluded). The
  descriptor cache itself is not cleared by the reset. A bounded full-frame
  series remains optional and is not needed for the current question.
- [x] **W-12 [DONE-REVISED — session 4]** Re-scoped: a dedicated host
  CP_SURFACE timer is obsolete (W-20 left 154 queries/demo, a negligible
  host cost), and the profiled host already reports CP_PROF drain/dispatch
  plus pre-present drain and native present elapsed. The wait-side
  attribution the host could not provide moved to the PPC-side CP_WAIT
  counters (§9.2). The W-06 eviction counter in the shared-upload reply
  stays open as a release gate (`texture_evictions=0` on every run so far).
- [x] **W-13 [DONE + HW]** Re-attribution run completed: client
  `-frameprofile` + profiled DLL on the W-01c stack (see §8.2). Entity work
  is now the largest Quake bucket, but the CP publish path itself is
  publish-bound (wait + emit), which changes the next lever.

### P2 - descriptor cache capacity (safe variants only)

- [x] **W-20 [DONE + HW]** 16 vs 128 measured with the W-01 invalidation and
  pacing unchanged: 16 slots -> **17.523 fps** (resolve=863M ticks, query
  storm back); 128 slots -> **24.177 fps** (resolve=11.3M). Capacity is the
  lever. 64 is optional; 128 stays.
- [ ] **W-21 [CANCELLED]** No capacity misses at 128 (`reset=0`), so no
  replacement-policy work is needed.

### P3 - hitch controls (cheap, on the corrected stack)

- [ ] **W-30 [PENDING-HW]** `+r_dynamic 0` control (diagnostic only; not a
  quality-preserving optimization). Compare the same slow frames.
- [ ] **W-31 [PENDING-HW]** `+gl_polyblend 0` control; measure total frame and
  completed-present intervals, not `R_PolyBlend()` alone.
- [ ] **W-32 [PENDING]** Add an `LM_UPLOAD` elapsed timer around the three
  `glTexSubImage2D` sites and correlate with worst frames.

### P4 - renderer path comparison

- [ ] **W-40 [PENDING-HW]** `-extensions + gl_texsort 0` A/B under CP emit,
  verified cvars, image checked. Only keep if total cost drops.
- [ ] **W-41 [PENDING-HW]** `r_worldbatch 1` evaluation (texsort path) for
  the world spikes.
- [ ] **W-42 [PENDING]** Lightmap coalescing - only if W-30/W-32 show the
  upload multiplication in the ACTIVE path (not the direct path assumption
  that C5 disproved for this run).

### P5 - entity / AltiVec (gated)

- [ ] **W-50 [PENDING]** Alias-model and brush-entity profiling first
  (pose-lerp, per-poly overhead, `r_vertexarrays` under CP). No optimization
  without exclusive-time evidence from W-13.
- [ ] **W-60 [PENDING]** AltiVec candidates only if their buckets dominate:
  `R_BuildLightMap*`, texture-shadow BGRA conversion, CP swap-copy. Scalar
  fallbacks + G4 runtime probe mandatory; aperture-store hazards stay
  documented (stswi hang notes).

### Acceptance

- [ ] **W-70 [PENDING-HW]** Three independent cold-boot medians for the
  accepted pair, every sample reported.
- [ ] **W-71 [PENDING-HW]** Longer gameplay soak (not only demo1),
  resource-churn (TexSubImage/TexImage/delete) and clean quit, before
  promoting anything to `Work:games/quake` defaults.

### Deploy checklist for every hardware session

- [ ] Record client/DLL/host/minigl.library/chip CRCs and build flags.
- [ ] `SetEnv MGLPPC_CPEMIT 1`; fresh host per run; `MGLPPC_HOSTPRI=-1`.
- [ ] Serialize Amiga file ops; verify CRCs after every transfer.
- [ ] Keep rollback pair; never mix DLL and host from different trees.
- [ ] `-benchmarkquit -benchmarklog`; diagnostics off unless measuring them.

---

## 9. SESSION 4 (2026-09-20 ~11:00-12:30 CEST): CP_WAIT attribution,
reporter fix, lightmap-upload timer

Scope: critical-path attribution only, per the §7 revised order — no
optimization changes, no expansion-threshold experiments (still
quarantined). The two benchmark targets are **800x600x32** and
**640x480x16**; every result is reported per target, never averaged across
them. No hardware was touched; everything below is source + local builds
for the next session.

### 9.1 CP_PHASE reporter bug fixed (was corrupting the file report)

`R200PPCDispatchDeleteContext` printed CP_PHASE with two defects:

- a stray format string AFTER the printf/fprintf call — parsed as a comma
  expression (left operand discarded), harmless by itself;
- the profiled-file `fprintf` carried NINE conversions for SIX values
  (`commit/records/draws` were W-11 counters removed when `f2976f6`
  restored the W-10 source). Reading missing varargs is undefined; the
  file's trailing fields were garbage.

Evidence validity ruling: console `CP_PHASE` lines (six fields) were
always correct and remain quotable. `DH2:mglprof/cp_phase.log` lines from
W-10 builds have unreliable trailing fields — do not quote them. W-11-era
file lines carried their own matching counters and are fine. Both outputs
now print exactly the six maintained counters, plus the new CP_WAIT line.

### 9.2 CP_WAIT: what the publish wait actually waits for (new)

W-11 showed the publish path is wait-dominated (~91% of publish elapsed)
but not WHAT retirement waits for. `PublishCpSlot()` now samples the ring
every 256 spins while genuinely stalling (after the break/timeout checks)
and time-attributes the wait to the observed blocking state:

| CP_WAIT field | observed state | points at |
|---|---|---|
| `host=` | `head - completed >= slots` (consumption backlog full) | 68k host consumption/submission too slow |
| `fence=` | consumed but `completed - retired > 0` (fences pending) | GPU execution + fence observation latency |
| `other=` | neither (sampling race or stale line) | should be ~0; investigate if large |
| `maxhost=` / `maxpend=` | deepest backlog / pending depth seen | queue-depth sanity |

Decision table for the NEXT change (strict gate — measure first):

- `host=` dominant -> host RingPoll per-entry cost and the driver
  `DispatchIndirect` path. Host binary is unchanged (`F0343E03`);
  use the profiled host for the drain/dispatch/present split.
- `fence=` dominant -> the per-dispatch fence tail contains
  `WAIT_2D_IDLECLEAN | WAIT_3D_IDLECLEAN | WAIT_DMA_GUI_IDLE`
  (`p96-driver/src/radeon_cp.c`, fenceCommands), so retirement tracks
  near-full GPU drain. That is the structural target, but it is a DRIVER
  change (matched-pair deploy, cold-boot protocol) and stays out of scope
  until a run confirms `fence=` dominance.
- Do NOT read the small average packet size (~565 dwords/publish) as a
  batching opportunity on this evidence: commit records reference external
  vertex segments and can encode substantial draw work.

Reported once at teardown, gated on cpEnabled, quiet builds included
(plain counters, no clocks added to the hot path beyond one mftb read per
256 spins); profiled builds also append it to `DH2:mglprof/cp_phase.log`.

### 9.3 Counter reset at the timedemo boundary (closes W-11)

`R200PPCResetCpAttribution()` (new, frontend) clears CP_SURF / CP_PHASE /
CP_WAIT and is called from `Lib_glHint(MGL_PERF_COUNTERS_HINT, GL_NICEST)`
in `backend_r200/library/r200_state.c`. The client issues exactly that
hint when the timedemo timer starts, so teardown reports now cover the
demo interval only. Caveat for gears-style clients that also use the hint:
their counters reset at their first NICEST hint too (same-as-fresh-session
semantics; harmless).

### 9.4 Client FP_LMUPLOAD section (hitch attribution)

All three `glTexSubImage2D` sites in `gl_rsurf.c` (R_DrawSequentialPoly,
the underwater path, R_BlendLightmaps) are wrapped with
`FP_Enter/FP_Exit(FP_LMUPLOAD)`. The worst-frame dump gained `lmup=` per
frame; `tools/analyze_frame_profile.py` parses and prints it (old dumps
still parse; `lmup` defaults to 0). This measures the CLIENT-side call
elapsed only — the DLL-side staging/native upload work surfaces in
whichever section made the call (WORLD/CHAINS/LMBLEND), which is exactly
the correlation needed for the "7 of 8 slowest frames carry lightmap
uploads" observation (§8.2). The matched-frame `r_dynamic 0` control
(W-30) is still required to turn correlation into attribution.

### 9.5 Regression fixes this session

- `tests/test_r200_cp_surface_cache.py` FAILED at HEAD BEFORE any session-4
  change (verified by stash): its harness still referenced the W-11
  counters (`CpPhaseCommitPublishes`, `CpPhaseRecords`, `CpPhaseDraws`)
  that `f2976f6` removed. The silencer list now tracks the current counter
  set (including the new `CpWait*`); all 3 tests pass. The compile failure
  was the drift detector working as intended.
- Pre-existing, untouched, still documented: `test_r200_transport_profile.py`,
  `test_r200_bind_cache.py`.

### 9.6 Local suites and artifact identities

Suites (this session, all green):
`test_r200_cp_surface_cache.py` 3 OK · `test_r200_cp_emit.py` OK ·
`test_batch_budget.py` 579794/0 · `test_batch_expansion.py` 2723/0
(full-state mutant 24) · `test_state_batch_expansion.py` 110/0.

Baseline sanity BEFORE editing: the unmodified tree rebuilt
`bin/minigl_ppc_r200.warpelf` = **14F2A439** byte-identical (recorded
W-10 identity) and after the edits `bin/minigl_ppc_host` = **F0343E03**
byte-identical (host untouched). NOTE: `make -f Makefile.dll.r200.gcc
warpelf` from this tree now yields CFD151CA — it no longer reproduces
14F2A439 by design until the session-4 changes are committed/reverted.

New diagnostic artifacts (NOT deployed; production stays
`Work:games/quake/minigl_ppc.dll` = 083A9E15, client 6B7B0938, host
F0343E03):

| Artifact | CRC32 | Notes |
|---|---|---|
| `bin/minigl_ppc_r200.warpelf` (quiet DLL) | **CFD151CA** | all session-4 changes, MGLPPC_PROFILE=0 |
| `bin/minigl_ppc_r200_waitp.warpelf` (profiled DLL) | **54EADCD1** | + file logging of CP_SURF/CP_PHASE/CP_WAIT; ~3% slower — attribution only, never compare its fps to quiet |
| client `glquake/warpos_gcc_v19/glquake/glquake_wos.warpelf` | **0A4B2FB9** | + FP_LMUPLOAD; baked DLL path `DH2:wosbuild/qwait/minigl_ppc.dll` |

Deployment: fresh test dir `DH2:wosbuild/qwait/` (name <=16 chars);
`Elf2Exe2`-convert all three ELFs on target; install the DLL matching the
step (quiet for Step 1/3, profiled for Step 2) at
`DH2:wosbuild/qwait/minigl_ppc.dll`; client next to it. Production and the
`qprof` rollback dir stay untouched.

### 9.7 Runbook: two-target rerun + attribution (next hardware session)

Protocol per `how_to_benchmark.md`: cold boot, wait_bridge, explicit
reconnect, `InitPPC` once, fresh host per client run, serialized Amiga
file ops, CRC-verify every transfer. Env: `MGLPPC_CPEMIT=1`,
`MGLPPC_HOSTPRI=-1` (verify after boot — it is volatile), everything else
unset. Threshold stays 4.

**Step 1 — quiet baseline rerun** (quiet DLL + host F0343E03 + new
client), interleaved, two samples per target, order-balanced:

```text
800x600x32 -> 640x480x16 -> 800x600x32 -> 640x480x16
  -noudp -nosound -benchmarkquit -benchmarklog DH2:wosbuild/qwait/r<N>.txt
  +timedemo demo1
```

Purpose: today's per-target baseline on one boot; compare against the
recorded 24.117 (800x600x32) and 55.182 (640x480x16, single sample —
this rerun also firms that number up).

**Step 2 — attribution run per target** (profiled DLL + profiled host +
same client, `-frameprofile DH2:wosbuild/qwait/fp_<mode>.txt`). Pull:
`DH2:mglprof/cp_phase.log` (CP_SURF/CP_PHASE/CP_WAIT — demo-only now,
thanks to 9.3), the host log (CP_PROF drain/dispatch, presentwait/present)
and the FP dump (lmup per slow frame). READ CP_WAIT FIRST; the §9.2 table
decides the next lever.

**Step 3 — hitch controls, same boot, quiet pair, per target:**
one `+r_dynamic 0` run and one `+gl_polyblend 0` run; compare the MATCHED
slow frames (FPW ranks, now including `lmup=`), not just mean fps.
`r_dynamic 0` remains diagnostic-only; nothing is promoted from Step 3
without the W-70/W-71 gate.

### 9.8 What stays deferred (unchanged from §7.3)

Descriptor-cache size/policy (done: 128, no misses), W-21 (cancelled),
W-40/W-41 renderer-path A/Bs, W-50 entity work, W-60 AltiVec — all gated
behind the Step-2 attribution. Driver fence-tail semantics only if CP_WAIT
shows `fence=` dominance, and only as a matched-pair driver experiment.

---

## 10. SESSION 4 HARDWARE RESULTS (2026-09-20 ~13:00-15:30 CEST)

Targets: **800x600x32** and **640x480x16**. Every run `demo1`, CP emit on,
`-benchmarkquit -benchmarklog`, serialized bridge ops, fresh host per run,
CRC-verified transfers. Production stack untouched throughout
(`Work:games/quake`: client 6B7B0938 + DLL 083A9E15; qprof DLL restored to
083A9E15 after the session; `qprof/dll_prod.dll` holds the archived copy).

### 10.1 Two-target baseline matrix (quiet, 800x600x32 / 640x480x16)

| client | DLL | 800x600x32 | 640x480x16 | CP dwords @16-bit |
|---|---|---:|---:|---:|
| glquakeWOS `6B7B0938` (production) | 083A9E15 | 23.611 | 52.097 | 9,289,264 |
| **glqprof `E2A06D9A` (session-2 build)** | 083A9E15 | — | **52.209 / 52.835** | 9,306,476 |
| glqwait (session-4 rebuild) | 083A9E15 | — | 20.803 | 7,959,046 |
| glqwait (session-4 rebuild) | C6FD771F (session-4) | 17.236* | 20.732 | 7,964,156 |
| glqwait_p (session-4 tree, pristine rebuild) | 083A9E15 | — | 20.830 | 7,966,836 |

*r1 ran on the pre-reboot aged boot; all other cells on clean boots.

Clean-boot reproductions with soft reboot between tries: glqwait 20.776
(x1) vs glqprof 52.835 (x2) — deterministic, not boot noise.

**W-81 [CLOSED 2026-09-20 ~17:30 — NOT A BUG]: "client regression" was two
measurement artifacts; the tree is healthy.** Final validation, clean
boot, correct DLL (BA0A8531 profiled via `DH2:wosbuild/qwait/mgls4p.dll`):
today's tree client runs **51.270 fps @ 640x480x16** (-frameprofile on,
~52.5 quiet-equivalent) and **23.844 fps @ 800x600x32** — statistically
identical to the deployed binaries (52.1-52.8 / 23.6-24.2). Rendering,
exit, CP counters (`failed=0`) all clean.

The false alarm had two independent causes:

1. **Stale `minigl_ppc_open.o` (baked-path object).** The makefile bakes
   `MGL_DLL_OVERRIDE` into `minigl_ppc_open.o` at compile time, but the
   object's dependency is only the source file — changing the variable on
   the command line does NOT rebuild it. Today's rebuilds silently reused
   the session-2 object baked with `DH2:wosbuild/qprofdir/minigl_ppc.dll`
   (an ancient 16-slot pre-W-01 test DLL). Every "slow" run at
   20.7-21.2 fps @ 16-bit / 17.2 @ 32-bit was actually running THAT DLL —
   exactly matching its recorded signature (17.5 fps @ 32-bit, CP_SURF
   `miss≈36k reset≈36k` with no `drop=` field). Verified by `strings` on
   the ELFs and by deleting the object: after the rebuild the baked path
   was correct. **Rule: after changing MGL_DLL_OVERRIDE, delete
   `warpos_gcc_v19/glquake/minigl_ppc_open.o` (or the whole build dir)
   before rebuilding.**
2. **Degraded boot state.** After the object fix, the same binaries
   measured 25.5-25.8 fps @ 16-bit on a boot whose startup had chained
   `InitPPC`/host-launch into single bridge commands; the identical
   binaries on a fresh boot with strict one-command-per-step sequencing
   measure 51.3. The degraded boot slowed every DLL-loading client
   (including the profiled one) by ~2x while a same-boot run of a binary
   converted on an earlier boot stayed fast — consistent with a
   per-boot loader classification/load-path cache poisoning after
   in-place file replacement. Mitigation (mandatory): fresh boot per
   benchmark series; one operation per bridge command after InitPPC;
   never `Copy`-replace a DLL file in place — write a NEW filename per
   build; verify CRC AND protection bits of the exact file at the baked
   path on the exact boot used for measurement.

Earlier session-4 client edits are exonerated by direct test
(gl_rsurf.c revert: 25.785 vs 25.567 — noise; the whole-edit pristine
rebuild: identical). Per operator decision the `gl_rsurf.c`
FP_LMUPLOAD timer edits are REMOVED from the tree (commit state
restored); the unused `FP_LMUPLOAD` enum/dump plumbing stays in
frame_profile.{c,h} harmlessly. The session-2 `FP_CountLMUpload`
counters remain.

### 10.2 Session-4 DLL validated on the fast client

`glqprof` + session-4 quiet DLL C6FD771F @ 640x480x16: **52.835 fps** —
identical to the production DLL (52.835 same boot, x2 vs xv1). The
session-4 DLL (CP_WAIT counters, timedemo-boundary reset, reporter fix,
128-slot safe cache) is **kept** as the validated DLL line; production
promotion still waits for W-70/W-71.

### 10.3 THE ATTRIBUTION RESULT (profiled pair, CP emit on)

Pair: glqprof + profiled DLL BA0A8531 + profiled host 03DC3364,
`-frameprofile` on. Profile overhead ~4% (23.762 vs 23.611 quiet at
32-bit; 50.839 vs 52.1-52.8 at 16-bit).

```
800x600x32:  CP_PHASE publishes=15025 wait=639,669,011 resolve=11,873,626
             emit=39,329,947 copy=10,500,808 publish=294,811 (ticks, ~25MHz)
             CP_WAIT  host=16,477,956  fence=298,926,198  other=0
             host: CP_PROF drain=249ms dispatch=6087ms of 40.8s wall
640x480x16:  CP_PHASE publishes=15079 wait=106,088,669 resolve=9,959,197
             emit=41,302,217 copy=11,619,131 publish=299,324
             CP_WAIT  host=4,028,268   fence=69,879,760   other=0
```

Reading (per the §9.2 decision table):

1. **`fence=` dominance confirmed: ~95% of attributed CP wait at 32-bit.**
   The publish path stalls because GPU retirement lags — NOT because the
   68k host is slow (host= 2.6%, host drain total 249 ms of 40.8 s; the
   68k host question is now closed with data).
2. **The fence wait collapses 6x at 16-bit** (639.7M -> 106.1M ticks) at
   the SAME dispatch count (~15,025). It scales with GPU fill — this is
   the depth-ratio mechanism (24 vs 52 fps) and it is NOT GPU
   undershoot: the per-dispatch fence tail
   (`WAIT_2D_IDLECLEAN|WAIT_3D_IDLECLEAN|WAIT_DMA_GUI_IDLE` +
   `DSTCACHE_CTLSTAT` flush, `p96-driver/src/radeon_cp.c` fenceCommands)
   forces the GPU to drain the ENTIRE 2D+3D+DMA pipeline before each of
   the ~15.5 dispatches/frame writes its fence. The GPU can never
   pipeline across dispatches; the CPU-visible cost is the serialization,
   and it multiplies with per-pixel work.
3. Emit (command construction) is ~4% of wall; copy+resolve ~2%; host
   consumption stalls ~2.6%. **Client-side CP work and 68k service are
   solved problems.**
4. `maxpend=4294967295` is the documented sampling race (stale
   `completed` line vs newer `retired` wraps the unsigned difference);
   cosmetic, counter stays. `maxhost=9` likewise a mixed-line artifact
   (head from one read, completed from another).
5. Frameprofile totals still include the ~15-20 s load frame (970
   "frames" in the FP dump); the client-side FP counters need the same
   timedemo-boundary reset the DLL got (W-11b, trivial). Absolute
   per-frame bucket values are therefore inflated; use the CP-side
   (boundary-reset) numbers for attribution, FP for correlation.

### 10.4 Next lever (W-80, confirmed by data): driver fence-tail semantics

**STATUS 2026-09-20 ~19:30: IMPLEMENTED (all three repos), local gates green,
hardware A/B pending — see §11 for the design, audit, artifacts and
protocol; the original idea below is superseded by the coalescing design
in §11 (weaker per-dispatch fences were rejected as unsafe for vertex and
texture fetch; fewer full-idle fences with in-order coverage achieves the
same pipelining with exact semantics preserved).**

The §9.2 gate is satisfied: `fence=` dominates and scales with fill.

**W-80 [PENDING, driver — matched-pair deploy, cold-boot protocol]:**
split the fence meaning in `Radeon9200.chip`:
- reuse fence (CP slice / vertex segment release): tail needs only
  CONSUMPTION-side idle — `WAIT_HOST_IDLECLEAN|WAIT_DMA_GUI_IDLE`
  (CP finished dispatching = vertex data fetched) WITHOUT
  `WAIT_2D_IDLECLEAN|WAIT_3D_IDLECLEAN`, so the raster pipeline keeps
  running while buffers are released early;
- completion fence (PRESENT, texture BEGIN/COMMIT, readback): keeps the
  full-idle semantics (render-complete genuinely required there).
Design note: the driver already has ranged-fence validation
(RADEON3D_CAP_MULTI_FENCE) and its own Prepare3D fast path; the change is
the fenceCommands tail + a second tail variant selected by a new submit
flag, negotiated through the Radeon3D ABI (interface bump or a new cap
bit). Risk: correct consumption-vs-render distinction for every
surface/segment lifetime — audit each `retired` consumer first
(PublishRetired, DrainForSegment, PollQueuedFence). Alternative/parallel:
reduce dispatch count (15.5/frame) — but the expansion-threshold path has
a grey-screen history (§8.3); W-80 is the cleaner lever.
Expected gain envelope: up to ~60% of the 32-bit wall is fence wait;
even partial pipelining should move 800x600x32 well past 30 fps.

### 10.5 Incidents and process notes (this session)

- **Two no-host wedges** (client launched with the previous run's host
  already exited: rb1-attempt and xp2-attempt): client hangs in
  registration, Break ignored; both recovered by bridge soft reboot, no
  GPU/CP hang, no artifact damage. RULE (now enforced in the runbook):
  after every client run, VERIFY the host port is gone AND start a fresh
  host BEFORE the next launch; `amiga_list_ports` is the check.
- One degraded warm boot (host stuck pre-`port_added`, `?` port, Break
  ignored after a chained `GetEnv;GetEnv;InitPPC;Run` command): resolved
  by another soft reboot + strict one-command-at-a-time boot sequence
  (Wait 20 -> env check -> InitPPC alone -> host alone -> run). Do not
  chain InitPPC with other commands.
- Stripping the client ELF before Elf2Exe2: verified byte-identical
  output (glqwait vs glqwait_ns, both 506A4F66) — doc claim confirmed.

### 10.6 Artifacts and target state after the session

Production (unchanged, verified): client `Work:games/quake/glquakeWOS`
6B7B0938, DLL `Work:games/quake/minigl_ppc.dll` 083A9E15, host
`DH2:ibdraw/mglhost` F0343E03, qprof DLL restored to 083A9E15
(`qprof/dll_prod.dll` archive copy). Fast profiler-capable client for
future sessions: `Work:games/quake/glqprof` E2A06D9A (loads
`DH2:wosbuild/qprof/minigl_ppc.dll`).

Test dir `DH2:wosbuild/qwait/` (delete anytime): glqwait/glqwait_p/
glqwait.elf/glqwait_ns.elf/glqwait_p.elf, minigl_ppc.dll (083A9E15),
dll_s4.dll (C6FD771F quiet session-4), dll_prof.dll (BA0A8531 profiled
session-4), mglhost (F0343E03), hostp (03DC3364 profiled), all r*.txt /
x*.txt / host_*.txt logs, fp_p32.txt / fp_p16.txt.

Evidence pulled to `/tmp/opencode`: `fp_p32.txt`, `fp_p16.txt`,
`cp_phase_s4.log`, `host_p2.txt`.

Session-4 build identities (source in tree, builds clean, DLL validated
fast): quiet DLL warpelf CFD151CA (target C6FD771F), profiled DLL warpelf
54EADCD1 (target BA0A8531), profiled host 03DC3364, quiet host F0343E03
(rebuild byte-identical). Client builds are BLOCKED on W-81 (rebuilt
clients are the 2.5x-slow-at-16-bit variant regardless of source edits).

### 10.7 Revised execution order

| Priority | Work | Gate |
|---|---|---|
| **P0** | ~~W-81 client bisect~~ **CLOSED — tree healthy** (two measurement artifacts: stale baked-path object + degraded boot; see §10.1) | — |
| **P1** | **W-80** driver fence-tail split (consumption vs completion) | matched pair, cold boots, fence= wait drop + fps gain, full acceptance suite |
| **P2** | promote session-4 DLL line via W-70/W-71 (3 cold boots + soak) per target | per-target medians, texture-churn soak |
| **P3** | W-11b client FP boundary reset; W-30/W-31 controls; W-40/W-41 | matched slow-frame effect |
| **P4** | W-50/W-60 only if exclusive-time buckets remain after W-80 | per §7.3 |

---

## 11. W-80 IMPLEMENTATION (2026-09-20 ~19:30 CEST): fence coalescing

Status markers: **[DONE]** source + local gates · **[PENDING-HW]** hardware
A/B. Default behaviour is UNCHANGED: every gate defaults to off, so the
current stack runs exactly as before until `MGLPPC_FENCE_GROUP` is set.

### 11.1 Design (supersedes the "split the fence" sketch in §10.4)

The CP_WAIT data showed ~95% of the publish wait is the per-dispatch fence
tail (`WAIT_2D/3D_IDLECLEAN|...`, `radeon_cp.c`): every one of ~15.5
dispatches/frame forces the GPU to drain completely before the next
indirect buffer is even fetched. Weakening that wait per dispatch is NOT
safe (vertex segments and texture fetch continue after CP consumption).
Instead: **coalesce fences**.

- `RADEON3D_INDIRECT_NO_FENCE` (driver interface 19): a dispatch is
  submitted WITHOUT the fence tail. Ring order still applies.
- `Radeon3DSubmitFence(device, &fence)`: two CP PACKET2 no-ops followed by
  the standard full-idle fence tail. The fence retires only when every
  earlier submission (fenced or not) has completely drained. One fence
  meaning everywhere: "everything up to this serial is done".
- Host policy (gated on `RADEON3D_CAP_FENCE_COALESCE` + env): fence every
  Nth CP dispatch (`MGLPPC_FENCE_GROUP`, 2..8, clamped to the ring slot
  count; unset/1 = legacy). The closing fence records the union of the
  pending entries' vertex-segment masks.
- PPC: nothing in the ring protocol changes except a new zero-payload
  `MGLPPC_SEMANTIC_FLAG_DRAIN` entry published by retirement-waiters when
  the host advertised `MGLPPC_CAP_FENCE_DRAIN` (HELLO, offered when the
  host was started with a group > 1). The host closes the run and drains;
  without this a trailing fence-less dispatch could never retire while the
  publisher spins. Used by the non-present barrier and the segment-rotate
  wait; PRESENT already triggers a host-side drain.

### 11.2 Safety audit (what each consumer actually needs)

| Consumer | Needs | Covered by |
|---|---|---|
| PPC ring-slot wait (`head-retired<slots`) | CP consumed rings | closing fence (group ≤ slots, so every window contains one) |
| PPC segment rotate (`retired >= segmentLastEntry`) | GPU fetched vertex data | closing fence (full idle ⊃ fetch); DRAIN entry prevents stalls |
| PPC barrier(FALSE) (`retired==head`) | all submitted work done | DRAIN entry + closing fence |
| Host `DrainSemanticFence` (PRESENT, texture BEGIN/COMMIT, readback, teardown) | render complete | `CloseFenceGroup()` at its top: fence kick + drain |
| Host `DrainForSegment` | segment not referenced | pending mask checked too; closes then drains |
| `Radeon3DReleaseSurface` (waits `LastFence`) | all earlier work done | every release path drains first (audited: all texture mailbox handlers + teardown); `LastFence` only advances on fenced submissions, and the closing fence is newer than any pending dispatch |
| `AdvanceRetired` publishing `ringCompleted` | never publish unproven work | **fixed**: with `pendingUnfenced` and no queued fence it publishes NOTHING (was the one real hazard introduced by coalescing) |

Driver-side `Radeon3DSubmitFence` repeats the indirect path's CSQ-partition
/ DP_DATATYPE endian guard before submitting (a clobbered partition would
wedge any CP submission).

### 11.3 Local gates (all green, 2026-09-20)

| Gate | Result |
|---|---|
| `p96-driver/tools/test_indirect_dispatch.py` | **93 cases** (was ~70): no-fence accepted only at interface 19, flags mask, unfenced `fenceOut==0` + `LastFence` unchanged, SubmitFence stages 111–116, all existing dispatch/transition cases unchanged |
| `p96-driver/tools/test_tex_serial.py`, `test_tex_matrix.py` | pass |
| `p96-driver` build (`-Werror`) | clean; `Radeon9200.chip` = **D9A59210** |
| MiniGL `tests/test_r200_fence_group.py` (NEW) | retirement blocked over pending runs; closing fence mask/serial; failure keeps pending; queue-full fails; parser conservative (27 cases) |
| `tests/test_r200_cp_surface_cache.py`, `test_r200_cp_emit.py`, `test_r200_host_priority.py` | pass |
| `tools/test_batch_budget.py` 579794/0, `test_batch_expansion.py` 2723/0 (mutant 24), `test_state_batch_expansion.py` 110/0 | pass |

### 11.4 Artifacts (code in trees; nothing deployed)

**Superseded by §11.6 for the fixed line.** First-attempt identities were:
chip D9A59210, host 0A9FA0CE, DLL warpelfs 9010E362 / 4DDAF6CE. The chip and
host were REBUILT after the wedge fix (see §11.6); the DLLs are unchanged.

| Artifact | CRC32 (fixed, §11.6) | Notes |
|---|---|---|
| `p96-driver/Radeon9200.chip` | **6CCDC030** | interface 19 + 2D drain guard |
| MiniGL `bin/minigl_ppc_host` | **697FB0C3** | coalescing logic + DRAIN handling + accumulator fold |
| MiniGL quiet DLL warpelf | **9010E362** | PublishDrainEntry (gated) — unchanged by the fix |
| MiniGL profiled DLL warpelf | **4DDAF6CE** | for CP_WAIT attribution — unchanged |
| ABI headers | synced byte-identical to both vendored trees; protocol header synced to 3 copies | |

### 11.5 Hardware protocol (next session, matched pair)

1. Deploy to a fresh test dir, e.g. `DH2:wosbuild/qw80/`: convert the
   quiet+profiled DLLs, install chip D9A59210 via the driver pair
   procedure (backup both `LIBS:Picasso96/Radeon9200.chip` and
   `Prometheus.card` first, cold boot after, verify CRCs), copy host.
2. Control run (group unset): both targets; expect the session-4 numbers
   (≈51–53 @ 640×480×16, ≈23.6–24.2 @ 800×600×32) and unchanged CP_WAIT.
3. Experiment: `SetEnv MGLPPC_FENCE_GROUP 4` (try 2 and 8 after, one
   variable per series). Host prints `fence group=4` at startup.
4. Watch: fps both targets; host `CP_RUN`; profiled `CP_WAIT` fence= drop;
   absence of `drain_publish_fail`, `rotate=FAIL`, `present_drain_fail`;
   `failed=0`; clean exit; visual check (no tearing/corruption — the
   closing fence still means full completion, so correctness should be
   bit-identical).
5. Rollback is instant: unset the env (or group=1). Keep the chip's
   `.previous` pair for a full revert; a GPU hang still needs an operator
   cold power cycle.

Risks: a defect in the group bookkeeping would surface as a stall or a
vertex-segment reuse fault (corruption), not a silent slowdown; the
retirement guard and the DRAIN entry are the two invariants to re-check if
anything odd appears. Expected gain: fence waits drop roughly with the
group size (up to ~4x fewer drains); the 32-bit target has the most to
win.

### 11.6 FIRST HARDWARE ATTEMPT (2026-09-20 ~20:30 CEST) — WEDGE, ROOT CAUSE, FIX

Deployed the whole chain (chip D9A59210 + rebuilt native minigl.library
95949A38, host 0A9FA0CE, quiet DLL B3CB268F, validated client
`glqwait_nors`). Results:

| Run | Config | Result |
|---|---|---|
| Control | 640x480x16, `MGLPPC_FENCE_GROUP` unset | **53.300 fps**, 15122 dispatches / 9,408,810 dwords, `failed=0`, clean exit |
| Experiment | same cell, `MGLPPC_FENCE_GROUP=4` | host reached context creation, then the machine **hard-wedged during startup console activity** (operator cold power cycle required) |

The power cycle dropped the volatile env, so the machine came back in the
safe configuration automatically. Evidence: `DH2:wosbuild/qwait/
host_w80b.txt` (shows `fence group=4` then normal startup through context
creation), no client result file.

**Expert code review (amiga-code-expert-flash) found the root cause — a
consumer the §11.2 audit table missed:** the per-dispatch fence was also
the *mutual exclusion* for the shared 2D engine baseline. Every P96 2D
operation goes through `PrepareMmioEngine -> SynchronizeEngine`, whose
CP-pending branch is `RadeonCpWait() && RestoreEngineState()`.
`RestoreEngineState` rewrites `HOST_PATH_CNTL`, `RB3D_CNTL` and sets
`DP_DATATYPE = HOST_BIG_ENDIAN_EN` (the CP's indirect fetch depends on the
opposite of that bit; the driver's own 2026-09-08 note documents CP stalls
on garbage streams). `RadeonCpWait` waits for `PendingFence`, which
fence-less dispatches never set — under coalescing a console scroll's 2D
blit could therefore restore the engine baseline **while fence-less IB
fetches were in flight** -> the CP decodes garbage -> hard wedge. "Console
going up" is exactly a console.device 2D fill/blit.

Fixes applied (all local gates re-run green):

1. **Driver (Critical):** `RadeonCpState.PendingUnfenced` counts fence-less
   submissions since the last fenced one (`RadeonCpSubmitStream`); the new
   `RadeonCpWaitDrained()` polls `RB_RPTR == WritePointer` and clears it;
   `SynchronizeEngine`'s CP branch is now
   `RadeonCpWait && RadeonCpWaitDrained && RestoreEngineState`, so the 2D
   baseline rewrite can no longer race a fetch. `RadeonCpWait` success also
   clears the count, and CP recovery resets it. Guard test:
   `tools/test_cp_drain_guard.py` (16 source-structure checks incl. the
   call order and the fast-path prohibition).
2. **Host (High):** `FlushCommitAccumulator`'s fenced submission now folds
   `pendingSegMask` into its record and resets
   `pendingUnfenced`/`fenceGroupCount` — it closes the open run, so the
   group window invariant holds across accumulator flushes.
3. **Host (Medium):** the DRAIN branch stamps
   `ringLastGen[entry.slot] = entry.generation`.

Reviewed and found sound: the standalone `PACKET2,PACKET2` + fence-tail
submission; protocol/capability bit uniqueness; bounded waits and the
`AdvanceRetired` holdback; `CloseFenceGroup` re-entrancy. **Not yet
validated on this card: back-to-back indirect dispatches without an
intervening idle wait** (architecturally ring-legal per the R200 CP, but
this stack has only ever run fenced-per-dispatch) — hence the revised
first experiment below.

### 11.7 Revised W-80 hardware protocol (after the fix)
1. Install the FIXED chip **6CCDC030** (backup pair first, cold boot),
   host **697FB0C3**, same DLL/client chain; control run group-unset should
   reproduce ~53 fps @ 640x480x16.
2. **First experiment: `MGLPPC_FENCE_GROUP 2` with console output fully
   redirected to NIL** (run via a redirect so no 2D fills run during the
   demo). This isolates the unvalidated back-to-back IB fetch from any
   remaining 2D interleave. If it survives, repeat with console output
   enabled — that exercises the Critical-1 fix directly.
3. Then group 4 and 8, still one variable per series, then both targets.
4. Watch for the same failure classes; keep the chip `.previous` pair and
   expect a possible operator power cycle on any wedge. Success criteria:
   fps gain vs the control, `fence=` CP_WAIT drop, `failed=0`, no
   `drain_publish_fail`/`rotate=FAIL`, visuals unchanged.

### 11.8 SECOND ATTEMPT + corrected fix (2026-09-20 ~22:00 CEST)

Staged retry on the fixed line (chip 6CCDC030, host 697FB0C3, quiet DLL
B3CB268F, client `glqwait_nors`), all with coalescing OFF to prove the
stack: sanity (production chain) 51.874, fixed host 51.543, W-80 DLL with
the old host 51.874, and the previously-crashing cell itself **52.951** —
all clean. The first fail was the Radeon's post-hard-wedge warm-boot state,
not the code.

`MGLPPC_FENCE_GROUP=2` then failed again, but differently: **not a wedge —
corrupted rendering, stuck when a console window rose** (the machine stayed
up, uptime unbroken). That is the same class as Critical 1, so the first
fix was incomplete:

1. **`RadeonCpWaitDrained` used `RB_RPTR == WritePointer` — too weak.**
   For an indirect dispatch the CP advances the read pointer as soon as it
   reads the 3-dword ring entry, while the IB fetch/execution is still in
   progress. The 2D `RestoreEngineState` could therefore still rewrite
   `HOST_PATH_CNTL`/`RB3D_CNTL`/`DP_DATATYPE` mid-fetch. It now calls
   `CpWaitGuiIdle()` (FIFO drained + `RBBM_ACTIVE` clear) — the same
   full-idle guarantee the old per-dispatch WAIT_UNTIL provided, paid only
   at 2D transitions.
2. **The per-dispatch MMIO guard was itself a race.** Every
   `Radeon3DDispatchIndirect` (and `Radeon3DSubmitFence`) read/wrote
   `CP_CSQ_MODE`/`DP_DATATYPE` over MMIO before submitting. With coalescing,
   that poke can land while an earlier fence-less IB is fetching. Both
   guards are now gated on the new `RadeonCpUnfencedPending()` query so they
   only run when the ring is quiescent; correctness is preserved because
   only a 2D transition can clobber the baseline and every 2D transition
   drains first (clearing the counter). Writer audit: the only production
   writers of those registers are the two guards, `RestoreEngineState` and
   the CP reset/recovery paths (all of which run with no fence-less work).
3. **The fence-only kick now uses a real register packet**
   (`PACKET0(SCRATCH_REG1,0)` + dummy) instead of two bare `PACKET2`
   no-ops, so the kick cannot introduce an untried packet form.

New chip **0D8A59C0**; host unchanged (697FB0C3); DLLs unchanged. CPU
gates: `test_indirect_dispatch.py` **94 cases** (new: guard withheld while
a fence-less submission is pending; kick shape), `test_cp_drain_guard.py`
**20 checks** (full-idle drain, not RB_RPTR; guard gating; kick shape).
The staged hardware retry (group 2 first, console activity included) is
the next step; if it fails again the remaining unvalidated assumption is
back-to-back indirect fetches without an intervening idle wait, and the
feature should be parked rather than burned through power cycles.

### 11.9 W-80 PARKED (2026-09-20 ~23:00 CEST) — final attempt + restore

The corrected chip (0D8A59C0: full-idle drain wait, gated MMIO guards,
PACKET0 fence kick) was deployed with the proven host 697FB0C3. The group=2
cell failed a third time, reproducibly: **rendering stops after ~9 frames**
while the demo keeps counting (`g2b.txt`: 969 frames / 12.880 s = a bogus
75.233 fps — but only 119 host dispatches and 9 presents; a healthy run is
~15,000 / 978). No failure trace is written (the client blocks before any
publish-timeout path), and the stall can take the bridge daemon with it for
~2 minutes without a machine reboot. Three distinct signatures across the
attempts: hard wedge (group=4, pre-fix), corrupted rendering stuck at a
console window (group=2, weak fix), and silent submission stall
(group=2, corrected fix).

Conclusion: **fence coalescing as designed is not viable on this CP/stack.**
The per-dispatch full-idle tail is not merely a retirement marker — this
hardware/CP-microcode configuration does not survive back-to-back indirect
dispatches without an intervening idle wait (the review's unresolved
HIGH 3). Proving that at the register level would need CP-state
instrumentation and many more hardware cycles; the expected gain no longer
justifies the risk.

State after this decision:

- **Production stack restored and verified**: chip 39C2F8AF, Prometheus.card
  18A453D6, minigl.library 62343673, host F0343E03, game DLL 083A9E15,
  client 6B7B0938. `MGLPPC_FENCE_GROUP` deleted; health run before/after
  restore: **51.985 fps @ 640x480x16, 15125 dispatches, failed=0**.
- The W-80 source changes (driver interface 19, host coalescing, DRAIN
  protocol) remain in the three trees, **default-off and inert** (group=1;
  the production chip does not advertise the capability, and the rebuilt
  native library was not deployed). They are documented experiments, not
  part of the shipped stack.
- Session-4 artifacts (DLL B3CB268F/476E0F36, host 697FB0C3, chips
  D9A59210/6CCDC030/0D8A59C0) are on `DH2:wosbuild/qwait/` for reference;
  `LIBS:*` backups (`.previous`, `minigl.library.prev`) are intact.

Where the performance work stands (unchanged, still valid): the CP_WAIT
attribution (fence-dominated waits at 800x600x32) is the key measurement;
the remaining lever for that target is reducing the number of dispatches
per frame on the client side (fewer, larger batches), not driver fence
restructuring. Next benchmark series: session-4 stack promotion via
W-70/W-71 for the 640x480x16 and 800x600x32 targets.

## 12. Frame-260 investigation (targeted continuation)

See **[FRAME260_INVESTIGATION.md](FRAME260_INVESTIGATION.md)** and
`tools/evidence/frame260/` for new, per-atlas measurements. The normal-lighting
frame is 167.723 ms at 800x600x32 and 113.730 ms at 640x480x16; respectively
129.038 ms and 95.340 ms are inside four lightmap upload calls, while CPU
lightmap construction is 0.127 ms in each. Both identify the same demo position.

W-32 call-site timers and a retained 250-270 frame window are now implemented
and hardware-validated. The two `r_dynamic 0` controls stalled during startup
and produced NO valid comparisons; they are quarantined after operator
recovery. Production stack and original game configuration were preserved.

**Update (late session): the direct-path fix is implemented and validated.**
Root cause: GLQuake's arithmetic texture names were never native-generated,
so all 267 direct BEGINs were refused. The host now binds the packet's name
explicitly before the BEGIN query and restores the shadow binding after; the
frontend routes only single-channel (lightmap-class) uploads direct so
world-texture mipmaps stay on the proven shared path. Validated on hardware
(qfix4: DLL target `CE3B2B2E`, client `489C7BF2`):

| Metric | 800x600x32 | 640x480x16 |
|---|---:|---:|
| Timedemo FPS | 24.128 | **54.377** |
| Frame 260 total | 167.7 -> **105.1 ms** | 113.7 -> **40.9 ms** |
| Frame 260 upload calls | 129.0 -> **66.1 ms** | 95.3 -> **29.9 ms** |
| Direct BEGINs | 48/48 ok=1 | 48/48 ok=1 |

All runs 969 frames / 978 presents / `failed=0`; gears health 120.967 fps
error=0. Remaining lever for frame 260: coalesce the four per-frame uploads
and/or reduce dispatches/frame for the lead render-drain barrier (~42 ms at
32-bit). See FRAME260_INVESTIGATION.md "FIX IMPLEMENTED AND VALIDATED".
