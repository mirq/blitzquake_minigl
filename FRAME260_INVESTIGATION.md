# Frame 260: measured lightmap-upload hitch

Targeted continuation of `PERFORMANCE_PLAN_2026-09-20_0024.md`, after W-80
was parked. Physical A4000 / MPC7410 400 MHz / 68060 / Prometheus / Radeon
9200; both requested video modes, `demo1`, 969 timed frames, no sound/network.

## Result

The historically numbered **FP frame 260** spends most of its elapsed time
inside four `glTexSubImage2D` calls, not in CPU lightmap generation or the
pickup overlay. These calls include synchronous MiniGL work and any waits
inside the API; they are **not exclusive CPU or GPU execution measurements**.

| Normal lighting (`r_dynamic 1`) | 800x600x32 | 640x480x16 |
|---|---:|---:|
| Timedemo FPS | 23.773 | 52.322 |
| Timedemo seconds | 40.760 | 18.520 |
| FP frame 260 elapsed | **167.723 ms** | **113.730 ms** |
| Lightmap upload calls | **129.038 ms** | **95.340 ms** |
| CPU lightmap build | 0.127 ms | 0.127 ms |
| World, inclusive of those uploads | 130.486 ms | 96.789 ms |
| Entities, inclusive | 33.810 ms | 13.287 ms |
| Overlay draws on this frame | 0 | 0 |
| FP frame 259 elapsed | 38.329 ms | 10.811 ms |
| FP frame 261 elapsed | 9.904 ms | 9.154 ms |

Both captures identify the same position: `host_frame=259`, `demo_frame=259`,
last server-message time **22.585360**. FP numbering is intentionally kept
compatible with older reports; it is one-based and includes the loading
frame. The server time is also logged because adjacent messages can share it.

### The four uploads

All are from the texture-sorted lightmap pass (`site=2`), all width 128, and
all have the same source rectangles in both captures:

| Atlas | Top row | Rows | Source bytes | 800x600x32 call | 640x480x16 call |
|---|---:|---:|---:|---:|---:|
| 3 | 53 | 22 | 2,816 | 58.543 ms | 25.757 ms |
| 5 | 118 | 10 | 1,280 | 21.244 ms | 20.643 ms |
| 6 | 0 | 16 | 2,048 | 22.024 ms | 22.775 ms |
| 8 | 71 | 53 | 6,784 | 27.226 ms | 26.164 ms |
| Total | | | **12,928** | **129.038 ms** | **95.340 ms** |

Source bytes are not actual aperture/VRAM traffic. Four *different* atlases
are involved: coalescing duplicate calls to the same atlas would not by
itself eliminate this frame's four calls. Most of the extra 32-bit upload
elapsed is in the first call (about 32.8 ms more), which suggests waiting for
earlier rendering; the new instrumentation does not yet separate that wait
from native conversion/allocation/copy work.

The operator identifies this as near the nailgun pickup. That is a useful
scene marker, **not a confirmed cause**: this capture did not record inventory
or weapon changes, and the specific FP frame 260 drew no screen overlay.

## Validity and failed controls

- 32-bit normal run: `CP_RUN dispatches=15067 dwords=8483750 failed=0`,
  `presents=978`, clean exit. The long bridge call timed out, but subsequent
  status/result/host checks proved the application had completed normally.
- After an operator cold power cycle, a production-client 16-bit health run
  completed at **53.067 FPS**: 15075 dispatches, 9324734 dwords, 978 presents,
  `failed=0`.
- Instrumented 16-bit normal run: 15137 dispatches, 9358888 dwords,
  978 presents, `failed=0`, clean exit.
- The `r_dynamic 0` control was attempted at each mode. Both attempts left
  the client and host resident without a benchmark result; the operator
  rebooted after the grey-screen/stall reports. Their archived host logs
  stop after context creation. **There is no valid dynamic-off measurement.**
  No claim is made that the cvar caused the stalls, that they occurred at
  frame 260, or that an optimization has already removed the hitch.
- No more dynamic-off tests were attempted. No software recovery or driver
  experimentation was performed. These are single normal-lighting samples,
  on different boots, not an optimization A/B or a cold-boot median study.

## Instrumentation delivered

`-frameprofile <path> -framewindow 250 270` now retains those 21 frames even
when they fall out of the existing worst-16 table. Storage is bounded to 32
window frames and 16 upload details per retained frame, with explicit dropped
detail counts. No per-frame allocation, file output, GL synchronization, or
driver calls were added. Output is written after the timed interval.

- `frame_profile.{c,h}`: frame-window retention, host/demo position, server
  time, per-atlas call timing and rectangle details; short formatted-output
  calls for the WarpOS runtime.
- `gl_rsurf.c`: timers at all three subimage sites; CPU-build coverage on
  both the texture-sorted and sequential/multitexture paths. Upload arguments
  and renderer ordering are unchanged.
- `tools/analyze_frame_profile.py`: v1/v2 parsing, independent window output,
  per-upload detail, and no false whole-demo percentile claim.
- `tools/test_frame_profile.py`: actual profiler code with a mocked clock,
  ASan/UBSan; retained fast frames, metadata, overflow, wraparound, disabled
  operation, aborted frame, legacy parsing and non-PPC no-op checks.

Validation: profiler test and texture-cache regression pass; full WOS build
passes ET_REL validation and has no undefined symbols; full 68060 dispatch
build also links (warnings remain in untouched legacy files). No assembly-
visible structures changed. The pre-existing trailing blank-line warning in
`PERFORMANCE_RESULTS_2026-09-19_2339.md` was not a code-test failure.

## Reproduction and provenance

The **existing production** MiniGL/driver chain was used unchanged:

| Component | CRC32 |
|---|---|
| MiniGL DLL, copied once to `DH2:wosbuild/q260/w10.dll` | 083A9E15 |
| Host `DH2:ibdraw/mglhost` (protection 00020000) | F0343E03 |
| Radeon9200.chip | 39C2F8AF |
| minigl.library | 62343673 |
| New diagnostic client ELF | 91F88DFC |
| New target client `DH2:wosbuild/q260/q260` | BB749961 |

Build from `glquake/`, using an isolated output directory to avoid the stale
baked-DLL-path object problem:

```sh
make -f Makefile.GCCAmigaWOS_MGL_V19 \
  ARCH=/tmp/opencode/quake260-wos \
  MGL_DLL_OVERRIDE=DH2:wosbuild/q260/w10.dll DIAGNOSTICS=0 -j4
python3 -B ../tools/test_frame_profile.py
```

After a fresh host, from `Work:games/quake`, stack 1048576:

```text
DH2:wosbuild/q260/q260 -width 800 -height 600 -bpp 32 -noudp -nosound -frameprofile DH2:wosbuild/q260/a32.fp -framewindow 250 270 -benchmarkquit -benchmarklog DH2:wosbuild/q260/a32.txt +gl_texsort 1 +r_worldbatch 0 +r_dynamic 1 +gl_polyblend 1 +timedemo demo1
```

The second normal run uses `-width 640 -height 480 -bpp 16` and `a16` output
names. CP emit was 1, host priority -1, fence grouping unset, no extra
InitPPC within a boot, a fresh host and a verified port before each launch.
The first run reused the already initialized machine; after operator recovery
PPC was initialized once before the health run. After the final reboot it was
left uninitialized/idle, with no client or host resident.

The original `id1/config.cfg` was restored and CRC-verified as **3325223F**.
Production executable, DLL, driver and library files were not replaced.
Test artifacts/logs are under `DH2:wosbuild/q260/`. Downloaded evidence is
under `tools/evidence/frame260/` (normal captures and completed/failed host logs).

## Deep attribution (session 2, DONE): what the four calls spend their time on

A profiled MiniGL DLL (`frame260deep.warpelf` `5A23BC62` source ELF; target
`00BF1888`) and profiled 68k host (`7244B782`, source in the isolated
`f2976f6` worktree) record per-upload phases on both CPUs. Built from the
stable W-10 source, driver/native library untouched. Records: 128 capacity,
35 captured, 0 dropped, both sides, both modes. Evidence:
`tools/evidence/frame260/` (`d32`/`d16` host+ppc files), target
`DH2:wosbuild/q260deep/`.

Frame-260 group = submission seqs 22-25 (names 1027/1029/1030/1032 =
atlases 3/5/6/8, consecutive).

### 800x600x32 (timedemo 23.657 fps, profiled pair)

| seq | atlas | PPC total | PPC old-work barrier | host native_texsub | residency delta |
|---|---|---:|---:|---:|---:|
| 22 | 3 | 53.7 ms | **30.9 ms** | 19.7 ms | 16.1 ms |
| 23 | 5 | 21.3 ms | 2.1 ms | 18.4 ms | 16.3 ms |
| 24 | 6 | 24.0 ms | 2.9 ms | 20.2 ms | 17.3 ms |
| 25 | 8 | 26.8 ms | 2.4 ms | 23.2 ms | 16.1 ms |
| sum | | 125.8 ms | 38.3 ms | 81.5 ms | 65.8 ms |

### 640x480x16 (timedemo 51.216 fps, profiled pair)

| seq | atlas | PPC total | barrier | native_texsub | residency delta |
|---|---|---:|---:|---:|---:|
| 22 | 3 | 26.0 | 5.2 | 19.8 | 16.2 |
| 23 | 5 | 32.7 | 2.1 | 29.8 | 15.9 |
| 24 | 6 | 36.2 | 2.3 | 33.2 | 22.9 |
| 25 | 8 | 27.7 | 3.3 | 23.4 | 16.2 |
| sum | | 122.6 | 12.9 | 106.3 | 71.3 |

### Findings

1. **Native residency re-import dominates**: ~16-17 ms per upload at BOTH
   depths (`HOST_LM_DELTA residency_ticks`; `upload_ticks=0`, `evictions=0`).
   The native TexSubImage path releases the atlas's old surface and re-imports
   residency under a fresh address on every update. Depth-independence shows
   this is 68k-side CPU/allocation/copyback work, not pixel throughput.
2. **The rest of native_texsub** (~3-7 ms per call) is the 68k-side rect
   convert/copy; PPC BGRA staging is 0.06-0.3 ms; mailbox overhead ~0.5 ms.
3. **The first upload of the frame pays the frame's render drain**: 30.9 ms
   of `old_work_barrier` (flush+submit+retire barrier) at 32-bit, vs 2-3 ms
   for the other three. This is the CP-publish wait reappearing inside the
   upload path — the same wait the CP_PHASE profile measured, sampled here at
   a hitch.
4. PPC totals slightly exceed the client-side `lmup` sums (profiled-pair
   overhead + per-boot pacing); treat the split as attribution, not absolutes.

### What this rules in/out

- Pixel conversion, staging and the PPC/68k mailbox round trip are negligible.
- The native 68k rect copy is a minor share; residency churn is the target.
- Deeper pixel formats do not make the uploads worse — depth is not the lever.
- `native_texsub` cost scales with per-frame upload COUNT: 4 calls/frame at
  the hitch means 4 residency re-imports. Coalescing to 1 would remove ~48-80
  ms of that frame's cost without touching the driver.

### Candidate fixes (measured, in order of preference)

1. **Fix the PPC direct-recommit path for lightmaps** (client-side): every
   direct attempt fell back on these runs (`direct_begin=267 upload_fails=267
   direct_commit=0` in host logs). The retained-shadow direct path exists
   precisely to avoid native residency churn (U0: ~11 ms per 256x256 upload
   including residency). Investigate why BEGIN fails for the unit-1 lightmap
   textures (eligibility gating: unit-0 keying, format/size rules) before any
   new mechanism.
2. **Coalesce per-frame lightmap uploads** (client-side, Phase 3 of the
   original plan): gather dirty atlases once per frame before the world pass.
   Removes duplicate residency re-imports; measured value ~3x on hitch frames.
3. NOT recommended: `r_dynamic 0` (loses lighting), CPU lightmap builder
   (0.13 ms), pickup blending (0 ms on this frame), driver fence changes.

Process notes: the second Elf2Exe2 attempt wedged because it was launched
before this boot's `C:InitPPC`; one production-client health check was lost
to a no-host launch (`glquakeWOS_PPC` Data Storage, DAR=0x10, NULL store in
the no-host registration path) — always start and verify the host port before
launching a client. Both were process errors, not code faults; the runbook in
PERFORMANCE_PLAN section 10.5 already covers both rules.

## FIX IMPLEMENTED AND VALIDATED (2026-09-20 late session)

The direct-path fix from candidate 1 is implemented in the isolated
`f2976f6` worktree (`/tmp/opencode/q260-minigl`), validated on hardware,
and changes only client-side MiniGL/host source. **No driver, protocol or
native-library change.**

### Root cause chain (each step proven by trace/experiment)

1. GLQuake never calls `glGenTextures` (arithmetic `texture_extension_number`
   names; lightmaps `1024+i`). Native `BeginDirectTexture` refuses
   `!texture->generated` → **all 267 direct BEGINs failed** on the production
   stack (host counters: `direct_begin=267 upload_fails=267 direct_commit=0`),
   forcing every upload through the shared 68k path with its residency churn.
2. Fix part 1 (host): `MGLPPC_CMD_TEXTURE_DIRECT_BEGIN` now binds the
   packet's name explicitly (native `glBindTexture` auto-generates it) before
   the BEGIN query and restores `semanticBoundTexture` afterwards — the same
   discipline the shared-subimage handler already used. Trace: 267/267 ok=1.
3. That exposed fix part 2: with every upload going direct, mipmapped world
   textures lost their level>0 uploads (native rejects them on a direct
   texture) and their retained shadows exhausted the 4 MiB cap before the
   last-loaded lightmaps. The frontend mirror cannot predict mips
   (`generated` is set by every bind; minFilter defaults to non-mip
   GL_NEAREST), so the final gate keys on the only safe predictor:
   **single-channel source formats** (`GL_LUMINANCE`/`GL_ALPHA`/
   `GL_LUMINANCE_ALPHA` — the lightmap class, never mipmapped in this
   engine). World textures return to the proven shared path with intact
   mips; shadows are only retained for the direct class (13 lightmap atlases
   x 64 KiB, well inside the cap).

### Validated results (qfix4; DLL target `CE3B2B2E`, client `489C7BF2`)

| Metric | 800x600x32 | 640x480x16 |
|---|---:|---:|
| Timedemo FPS (baseline) | 24.128 (23.6-24.2) | **54.377** (51-53) |
| Frame 260 total (baseline) | **105.1 ms** (167.7) | **40.9 ms** (113.7) |
| Frame 260 lmup (baseline) | **66.1 ms** (129.0) | **29.9 ms** (95.3) |
| Uploads 5/6/8 in frame 260 | 8.3/8.2/7.2 ms | 6.6/6.5/7.1 ms |
| Direct BEGINs | 48/48 ok=1 | 48/48 ok=1 |
| Non-lightmap direct uploads | 0 | 0 |
| Subimage recommits | 35 (all) | 35 (all) |

All runs: 969 frames, 978 presents, `CP_RUN ... failed=0`, clean exit. The
one remaining frame-260 cost is atlas 3's 42.4 ms (32-bit) — the lead
render-drain barrier, not the upload path. Gears health with the qfix4 DLL:
120.967 fps, error=0 (2 buffers/300 frames).

Acceptance notes: `r200_texture_update_test` shows the documented
pre-existing signature (CHECKS=3495 FAILURES=1, INDEXED SOURCE expectation)
on every stack including this one, and its uploads are shared-path; the
direct-path pixels are validated by the timedemo lightmaps (the world
lighting renders correctly) and the historical byte-identical
`FrontendSourceToBGRA32`/native `ConvertPixel` proof. **Operator visual
confirmation of the qfix4 runs is still wanted** (world mipmapping is
unchanged — world textures use the shared path).

Evidence: `tools/evidence/frame260/qfix4_*` (host traces and frame profiles
for both targets), `qfix2/qfix3` trace logs (the gate iterations),
`qfix4_updt.log`.

## Next targeted work

1. **Coalesce per-frame lightmap uploads** (client-side): frame 260 still
   issues four uploads; gathering dirty atlases once per frame would cut the
   remaining ~30-66 ms further. Measure frame-260 and the P95 tail.
2. The atlas-3 lead barrier (~42 ms at 32-bit) is the CP-wait lever: revisit
   via reduced dispatches/frame (W-40/W-41), not by touching fences.
3. Optional: extend the direct class to RGB/RGBA uploads that provably carry
   no mips (requires a client-visible "no mip" signal; do not infer it from
   `generated` or the mirror filter — both are proven unreliable).
