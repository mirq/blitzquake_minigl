# Testing and diagnostics

Testing in this project happens on two levels: **host-side unit tests** (no
Amiga needed) and **on-target probes/benchmarks**. There is no CI; the gates
below are run manually.

## Local gate set

Run these before and after code changes that touch the renderer, profiling or
texture cache:

```sh
cd /home/mirek/BlitzQuake_src
python3 tools/test_texture_cache.py      # binding-cache regression, CPU-only
python3 tools/test_frame_profile.py      # profiler unit tests (needs cc)
python3 tools/check_docs.py              # documentation links + diagrams
```

Plus, for WOS artifacts before deployment:

```sh
python3 tools/check_wos_release.py <client.warpelf> <dll.warpelf> <host>
```

`test_frame_profile.py` compiles the real `glquake/frame_profile.c` with a
mocked clock under ASan/UBSan (writes to `/tmp/opencode`). `test_texture_cache.py`
extracts the real `GL_Bind`/`GL_SelectTexture` logic and checks the dispatch
build uses fewer binds while ending with identical final bindings.

`check_docs.py` is described in
[../README.md](../README.md#documentation-checks).

## Tool reference

| Tool | Validates | Usage |
|---|---|---|
| `tools/test_texture_cache.py` | `GL_Bind`/`GL_SelectTexture` binding cache in dispatch vs classic mode; expects 2 setup sites and identical final bindings. | `python3 tools/test_texture_cache.py` |
| `tools/test_frame_profile.py` | Real profiler code with mocked clock: window retention, metadata, overflow, wraparound, disabled operation, aborted frame, legacy parsing, non-PPC no-op. | `python3 tools/test_frame_profile.py` |
| `tools/analyze_frame_profile.py` | Parses on-target `frameprof*.txt` v1/v2 dumps: worst-16 ring stats, section totals, per-frame/atlas upload details. | `python3 tools/analyze_frame_profile.py dump.txt [more…]` |
| `tools/check_wos_release.py` | Fails if diagnostic markers remain in client/DLL/host; prints size/CRC32/SHA256. | `python3 tools/check_wos_release.py client dll host` |
| `tools/check_docs.py` | Relative markdown links resolve; every `.mmd` is referenced; every inline `<!-- mmd: … -->` marker matches a file. | `python3 tools/check_docs.py` |

## Launcher tests (68k)

```sh
cd launcher
make test
```

| Binary | Validates | Output |
|---|---|---|
| `build/argdump` | Launch mechanism: `CreateNewProcTags` argv delivery, CLI creation, explicit stack size. Set `client=argdump` and `starthost=0` in the launcher config, press Run. | `T:argdump.log` |
| `build/launchtest` | The real `lq_start_client()` (launch.c) with `argdump` as the client: argument assembly, 1 MB stack, CLI, background start. Deploy next to `argdump` in `Work:wosbuild` and run it. | `T:launchtest.log` + `T:argdump.log` |
| `build/cfgtest` | Settings persistence: saves a distinctive configuration, reads it back and compares all fields. | `T:cfgtest.log` |

Build with `-DLQ_TRACE` for a line trace of the GUI startup
(`T:lq_trace.log`). Details: [launcher.md](launcher.md).

## Frame-profiler tools

The profiler itself is documented in
[../architecture/renderer.md](../architecture/renderer.md#frame-profiler).
Workflow:

```text
# on target
glquakeWOS ... -frameprofile DH2:wosbuild/frameprof.txt -framewindow 250 270 \
  -benchmarkquit -benchmarklog DH2:wosbuild/result.txt +timedemo demo1

# on host
python3 tools/analyze_frame_profile.py frameprof.txt
```

Caveats:

- The worst-frame table is not the whole demo; do not quote it as a
  whole-demo percentile.
- Sections nest (SCREEN contains VIEW/HUD/PRESENT; SCENE contains WORLD and
  ENT\*), so never sum across levels.
- `-framewindow first last` retains a specific frame range (up to 32 frames,
  16 upload details each) even when it falls out of the worst-16 table.
- Retention is bounded and allocation-free by design; dropped detail counts
  are reported explicitly.

## Probes

Probes live in `glquake/warpos_gcc_v19/probe/`. Build with:

```sh
cd glquake
make -f Makefile.GCCAmigaWOS_probe PROBE=qpresent   # needs MiniGL
make -f Makefile.GCCAmigaWOS_probe_io PROBE=wos_ahi # no GL dependency
```

Then convert with `Elf2Exe2` and run per the
[runbook](runbook.md). Probe outputs are logs under `DH2:wosbuild/` or
`Work:wosbuild/`.

| Probe | Purpose |
|---|---|
| `qpresent` | Presentation regression: 14 cases covering 2D fan, polygon, lightmap, multitexture, alias fan/strip, particles, texsub, 1000-poly draws, texture churn, content-serial mip updates, batch expansion, sliding console quad. Stops at the first GL error; per-stage log. Documented in [`probe/qpresent.md`](../../glquake/warpos_gcc_v19/probe/qpresent.md). |
| `wos_ahi` | Low-level AHI from a PPC task, staged and crash-durable (`-step N` writes each stage to `Work:wosbuild/wos_ahi_trace.log`). Validates the packed ABI and double-buffer streaming. |
| `wos_net` | Endian/ABI checks, `sockaddr_in` size, socket/bind/FIONBIO/sendto/recvfrom, and a live NetQuake `CCREQ_SERVER_INFO`/`CCREQ_CONNECT` exchange. |
| `wos_time` | Cost of PPC→68k LP calls: 200× `DateStamp`, timebase MHz, and `dcbf` vs `CacheClearE` per 8 KB. |
| `wos_cache` | PPC/68k coherency for the AHI ring: `dcbf`+`sync` visibility on fast RAM (cold and repeated 68k reads, via `OpenLibrary` as the 68k reader), chip-RAM behaviour, and `dcbf` vs `CacheClearE` timing. Trace at `DH2:wosbuild/wos_cache_trace.log`. |
| `wos_ahimodes` | Enumerates AHI audio modes (ID, name, bits, stereo, HiFi, mix-freq range); used to find the DMA mode and for `-ahiid`. |
| `qfail` | Read-only 68k Radeon3D rejection snapshot (`CommitFailStage` dump); no submission. |
| `probe.c`, `probe2.c`, `probe3.c`, `probe_gl.c`, `probe_stress.c` | MiniGL bring-up ladder (triangle → context/clear → uploads/draw cost) including `mglWriteShotPPM` dumps and `RAM:glp5_marker` markers. |

## Evidence

Captured hardware evidence lives under `tools/evidence/`. The frame-260 set
(`tools/evidence/frame260/`) has its own
[README](../../tools/evidence/frame260/README.md) explaining every file,
including which runs are valid and which failed. Rules:

- Pull evidence with CRC-verified transfer; keep the original files.
- No screenshot or timing from a failed/wedged run is an accepted result.
- Reference evidence from the matching entry in
  [performance/history/](../performance/history/), not vice versa.

## Adding a test or probe

1. Prefer host-side Python tests for pure logic; they run without hardware.
2. On-target probes must be staged and crash-durable: write a trace line
   before each risky call, provide `-step N`, return a distinct exit code.
3. Never add per-frame heap allocation, file I/O, GL synchronization or
   driver calls to a probe used for timing.
4. Document the probe/test in this page and link it from the change that adds
   it.
