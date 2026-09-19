# Step 4 decision: reject the generic client-array path

Do **not** retry replacing `mglTV23fv()` / `mglTV23fvLM()` with the existing
`glVertexPointer()` + `glTexCoordPointer()` + `glDrawArrays(GL_POLYGON)` path
for Quake `glpoly_t` surfaces.

## Physical R200 result

Target: 68060/RV280, fullscreen 640x480x32, three-buffer `demo1`, three cold
boots, GCC 6.5.0b.  The corrected array experiment preserved rendering and
geometry counts, but was slower than the retained Step 3 immediate path.

| Build | Median seconds | Median FPS |
|---|---:|---:|
| Step 3 immediate | 322.773 | 3.002 |
| Generic arrays | 340.156 | 2.849 |

The array path was **5.38% slower** by elapsed time and **5.10% lower** in
FPS.  Corrected run times were 341.828, 340.156, and 338.699 seconds.

Parity was valid: each run had about 766,375 primitives, 3,570,509 input
vertices, 3,455,847 submitted vertices, and 43,087 draw records; execute and
wait failures and RHW clamps were zero.  The visual screenshot was textured
and correct.

Raw result archive on the physical target:
`Work:Games/BlitzQuake_68k/id1/phase4_arrays_results.log`.

## Why it lost

The R200 generic array implementation (`FetchArrayVertex()` / `ReadNumber()`)
decodes each position and texture component through type-generic reads before
calling the same primitive frontend.  On short Quake polygons (about 4.66
vertices per primitive), that per-vertex decoding costs more than the dispatch
calls removed.

The first array experiment also dropped brush surfaces after alias-model array
cleanup disabled client arrays while its local enable-state latch stayed true.
That was fixed before measurement, but it reinforces that client-array state
interacts poorly with this renderer.

## Future direction

If Step 4 is revisited, use a dedicated R200 Quake-polygon submission entry
that consumes the known seven-float `glpoly_t` layout directly, avoiding both
per-vertex dispatch and the generic array decoder.  Preserve one source vertex
per submitted vertex and benchmark against the Step 3 baseline before keeping
it.
