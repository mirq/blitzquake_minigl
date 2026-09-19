# Workstream A: GLQuake Renderer Architecture

Target: physical Amiga, 68060 at 50 MHz, Radeon RV280/Radeon 9200, V10
`minigl.library` R200 backend.

Primary benchmark: `demo1`, fullscreen 640x480x32, three buffers, video sync
disabled, three cold-boot runs, median result. Keep one variable per benchmark
series and use the existing `-benchmarkcfg`/MiniGL diagnostics path.

## Baseline

The accepted deferred-state baseline is 3.002 FPS (322.773 seconds for 969
frames). The renderer submits approximately:

- 46 draw records per frame
- 30,000 semantic record dwords per frame
- 227 state calls per frame
- 4.3 semantic submits per frame

MiniGL frontend work is the largest measured bucket. Quake-side BSP traversal,
lightmap generation, API calls and 2D drawing account for most of the remaining
CPU time.

## Hard constraint: do not repeat the rejected generic-array experiment

`STEP4_REJECTED_GENERIC_ARRAY_PATH.md` records the physical-hardware result of
replacing each short seven-float `glpoly_t` polygon with generic client-array
submission. It preserved geometry but dropped from 3.002 to 2.849 FPS.

The failure did not disprove batching. It proved that replacing one immediate
polygon with one generic-array draw is insufficient: the renderer retained the
same 43,087 draw records and generic per-component decoding cost more than the
removed dispatch calls.

Therefore:

- Never submit one generic array draw per world polygon.
- A1 must combine multiple surfaces into materially fewer indexed draws.
- A change is retained only after a three-run physical-hardware median beats
  the 3.002 FPS baseline and preserves geometry counters and screenshots.
- Client-array state is configured once around each world-batch pass and
  disabled before returning to legacy rendering. Do not cache array state
  across passes; alias cleanup changes the same states.

## A1. Static world indexed batches

### Goal

Convert ordinary opaque world geometry into persistent, immutable indexed
batches. Draw a batch per compatible render-state group instead of a draw per
surface, allowing the R200 backend's indexed resolved-vertex and retained-block
fast paths to operate on real Quake geometry.

### Geometry ownership

Build batches after `BuildSurfaceDisplayList()` has generated final polygons
and removed collinear vertices. Allocate batch data from the map hunk so it is
released with the BSP. Preserve every original `glpoly_t`; sky, water, brush
models and fallback paths still use it.

Triangulate each convex polygon as a fan:

```text
(0, 1, 2), (0, 2, 3), ...
```

Use `GL_UNSIGNED_SHORT` indices and split a batch before 65,536 vertices.
Maintain one source vertex per copied batch vertex initially. Vertex welding is
a later, separately measured change.

### Eligibility

First implementation includes only world surfaces which are:

- opaque
- not `SURF_DRAWSKY`
- not `SURF_DRAWTURB`
- not warped `SURF_UNDERWATER`
- not mirrors or translucent water
- using immutable positions and UVs

Inline brush entities remain on the existing path until world batching is
accepted.

### State grouping

The batch key must contain every state that changes submitted geometry or
sampling:

- animated base texture selected by `R_TextureAnimation()`
- lightmap atlas texture
- base/lightmap pass or dual-texture layout
- blend/depth/cull state relevant to the pass

Animated texture identity can change each frame. Geometry remains immutable,
but visible ranges must be placed under the texture selected for that frame.

### Visibility

Keep BSP/PVS traversal authoritative. `R_RecursiveWorldNode()` marks eligible
surfaces visible; it no longer draws those surfaces individually. Visible batch
ranges are accumulated into frame-local index lists, then issued by
`DrawTextureChains()`/`R_BlendLightmaps()` at the existing ordering points.

The first accepted implementation may compact visible indices into a contiguous
frame buffer. The persistent vertex array must never be modified in place: the
R200 persistent resolved-array cache assumes stable contents while pointer and
format state are unchanged.

### Texture-coordinate limitation

The current dispatch API has one generic client texture-coordinate selector and
no `glClientActiveTextureARB`. Do not silently replace the real multitexture
path with a semantically different layout.

Implementation order:

1. Prove batched base and lightmap passes in the texture-sorted two-pass path.
2. Benchmark total draw-record reduction and generic-array decode cost.
3. Add a dual-UV route only through an API that explicitly supports both UV
   sets. If no suitable client API exists, leave the real multitexture path on
   its current fallback until a dedicated seven-float MiniGL entry is added in
   Workstream B.

### Acceptance gate

- Correct screenshot and no missing brush/world surfaces.
- Input/submitted primitive parity, except for deliberate triangulation.
- Zero Execute failures, wait failures and RHW clamps.
- Material reduction from about 46 world/entity draw records per frame.
- Three-run median greater than 3.002 FPS. A gain below 2% is considered noise
  and is not sufficient to retain the added architecture.

## A2. Lightmap traffic

After A1 is accepted:

1. Rebuild/upload only when a dynamic light touches the surface or a cached
   light style value changes.
2. Measure the existing RGBA path against the existing
   `MGL_UNSIGNED_SHORT_5_6_5` build path. Keep 565 only if screenshots and
   torch/flicker appearance remain acceptable.
3. Process `R_BuildLightMap()` in cache-sized rows and later replace its
   scale-add/pack loops with 68060 assembly under Workstream C.

Acceptance gate: lower texture-residency and Quake-side lightmap ticks with no
stale dynamic lights. Measure A2 separately from A1.

## A3. 2D, console and status bar batching

Accumulate textured quads for glyphs, console background, pictures and status
bar elements. Submit groups by texture through MiniGL FastPath if the V10
contract supports the required pre-projected textured attributes; otherwise use
one conventional indexed draw per texture.

Preserve ordering around blend/scissor changes. Do not merge translucent groups
across state boundaries.

Acceptance gate: lower 2D draw records and frontend ticks with pixel-identical
menu, console, intermission and status bar captures.

## A4. Particles

Replace per-particle `glColor*` plus `glVertex3f` capture with a packed per-vertex
color stream. Preserve point-size and 256-particle safety limits until the new
path passes. Then benchmark larger batches and restore a visually useful default
particle count.

Acceptance gate: particle-heavy scene with matching color/alpha and lower
frontend ticks per particle.

## A5. Sky and water

Keep these dynamic surfaces out of A1. Once static batching is stable:

- move pure scrolling components to the texture matrix where MiniGL semantics
  match the existing result
- retain CPU deformation for true turbulent position/UV warps
- avoid rewriting persistent vertices used by a retained array
- preserve skybox clipping and translucent-water ordering

Acceptance gate: pixel comparison at multiple times plus water/sky-heavy demos.

## A6. State call reduction

After draw grouping is correct, eliminate redundant texture-unit selection,
texture environment and blend/depth calls at the Quake layer. MiniGL already
rejects many redundant calls internally; application-side suppression avoids the
dispatch and validation cost entirely.

Target: fewer than 100 renderer state calls per frame without weakening ordering
boundaries for texture mutation, clear, presentation or readback.

## Implementation phases

1. Instrument world, lightmap, 2D and particle CPU sections without changing
   rendering.
2. Add persistent A1 geometry and eligibility metadata; existing renderer still
   draws everything.
3. Add visible indexed ranges and base-pass batching behind `r_worldbatch 0/1`.
4. Add batched lightmap pass; validate dynamic atlas updates.
5. Run the physical A1 benchmark and either accept or remove the runtime path.
6. Apply A2, A3, A4, A5 and A6 as independent benchmark series.

## Current implementation status

The first A1 slice is implemented behind `r_worldbatch` and compiles with the
GCC 6.5.0b 68060 MiniGL-dispatch target.

It currently:

- builds immutable seven-float world vertices in map-hunk storage
- excludes sky, turbulent and underwater surfaces
- splits vertex storage before the 16-bit index limit
- triangulates visible convex polygons into a frame index buffer
- issues one base-texture `glDrawElements(GL_TRIANGLES)` per texture/segment
- issues one lightmap `glDrawElements(GL_TRIANGLES)` per atlas/segment while
  preserving dynamic lightmap rebuild and upload behavior
- keeps client-array pointer, format and enable state stable across every draw
  in a segment, with one setup and teardown per pass
- preserves immediate rendering for all excluded surfaces
- leaves inline brush entities unchanged

For safety, this slice only activates with `gl_texsort 1`. It does not alter the
default `gl_texsort 0` real-multitexture path and rejects fake multitexture. The
batch data is allocated only when both `r_worldbatch 1` and `gl_texsort 1` are
set before map load; changing either cvar afterwards requires reloading the map.

The first hardware comparison must therefore use two texture-sorted controls:

```text
gl_texsort 1
r_worldbatch 0
timedemo demo1
```

and:

```text
gl_texsort 1
r_worldbatch 1
timedemo demo1
```

Compare both to each other first, then compare the accepted winner to the 3.002
FPS `gl_texsort 0` baseline. Record draw records, input/submitted vertices,
frontend/serialize/execute/wait/present ticks, failures and RHW clamps.

Physical validation is pending because the hardware bridge at
`192.168.1.21:2345` was unavailable during implementation.

## Runtime controls

New paths remain opt-in until physical validation:

- `r_worldbatch 0/1`
- `r_2dbatch 0/1`
- `r_particlebatch 0/1`

Do not add compatibility layers beyond these measurement switches. Once a path
is accepted and stable, make it the default and remove superseded experiments.
