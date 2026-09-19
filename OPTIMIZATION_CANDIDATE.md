# Optimization candidate and measured results

Timedemo environment: 969-frame `demo1`, fullscreen 800x600x32, `-nosound
-noudp`, no runtime diagnostics, benchmark result written after the timed
interval via `-benchmarklog`. Amiga is the physical A4000/MPC7410 machine.

## Measured results (all samples)

| Build | FPS samples | Notes |
| --- | --- | --- |
| Original pre-optimization | 4.523 | first release baseline |
| Batch/texture-cache optimizations | 4.574, 4.585 | `F2B61BAD` client |
| Flash guards (fade-tail + scissor) | 4.729 | `A9EBEB3D` client |
| Particle quads, 3 px default | 4.660, 4.858 | same-look path |
| Particle fast points, 1 px | 4.798, 5.045, 5.062, 5.089 | median **5.045**, latest 5.089 |

Observations:

- A first-run-after-boot penalty was measured (second run on the same boot
  faster by 1-9%). Order-controlled comparisons still put the 1 px hardware
  point path ~3-4% ahead of the 3 px quad path.
- Treat single samples as noisy; at least two samples per configuration are
  required for a reliable comparison.

## Client changes

1. `R_DrawWorld`/`R_DrawBrushModel` no longer discard the binding cache at
   entry in the dispatch renderer. All binds go through `GL_Bind` and unit
   switches save/restore through `GL_SelectTexture`; classic rendering keeps
   the old invalidations under `#ifndef MINIGL_DISPATCH_CLIENT`.
2. `R_PolyBlend` skips the scissor enable/disable pair when the status bar
   occupies no rows, and `Draw_AlphaFill` returns immediately when the
   quantized alpha level is zero (invisible fade-tail frames).
3. New cvar `r_particle_size` (default `1`):
   - `1` (default) uses MiniGL's hardware point path: one TCL vertex per
     particle, no CPU expansion. Measured ~3-4% faster than 3 px.
   - `0` keeps the automatic resolution-scaled size (`0.5 + width/320`),
     expanded to screen quads; `3` restores the old 3 px look explicitly.

## MiniGL changes

1. Tighter, emitter-verified batch cost model (see
   `../MiniGL_WOS_V19_mglQ3/backend_r200/BATCH_COST_OPTIMIZATION.md`):
   unlit expanded batches retain the compact generated-command bound; lit
   batches keep the conservative bound. All capacity checks unchanged.
2. Sized points above 1.5 px are emitted as one four-vertex QUADS record per
   point instead of two clipped triangles (six vertices); edge points fall
   back to the clipped triangle path so non-TCL records stay inside the
   render target.

## Verification

- `python3 tools/test_texture_cache.py` - binding helper regression (dispatch
  3 binds vs classic 7, identical final bindings).
- `python3 tools/test_batch_budget.py` - 579,794 packet/expansion checks.
- `python3 tools/test_batch_expansion.py` - 2,247 checks against the real
  Radeon emitter; unlit shared/expanded command streams byte-identical.
- Two particle samples completed and exited cleanly, no crash or stall.

## Artifacts (deployed for measurement)

- Client fast-points default: `glquake/warpos_part/glquake/glquake_wos.warpelf`
  CRC32 B5DDBE7B; installed on the Amiga as `Work:games/quake/glquakeWOS`
  CRC32 6B7B0938 after Elf2Exe2 conversion.
- MiniGL particle DLL: `../MiniGL_WOS_V19_mglQ3/bin/qpart.warpelf`
  CRC32 C99DD4DF (installed as `F0089138` after Elf2Exe2 conversion).
- Rollback pair on the Amiga:
  `DH2:wosbuild/qprepart_client` (flash client A9EBEB3D) and
  `DH2:wosbuild/qprepart.dll` (previous DLL 4987D383).

Latest confirmation with the fast-points default (no extra arguments):
969 frames / 190.420 seconds / 5.089 FPS, clean exit.

The 1 px default is a visual change chosen for speed; `r_particle_size 0` or
`3` restores the wider particle look without rebuilding.
