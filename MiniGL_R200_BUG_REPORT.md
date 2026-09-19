# MiniGL R200 (WarpOS PPC DLL) — defect report from GLQuake porting

Environment (as deployed and measured):
- Amiga 4000, MPC7410 (PVR 0x800C1104), WarpOS, Prometheus + Radeon 9200
- MiniGL V19 WOS stack: `minigl_ppc.dll` (CRC32 0x02605851, 294,768 bytes,
  the validated R200 build from `Work:ibdraw/`) + 68k `mglhost`
  (port `MiniGLPPC.R200.Host.1`), `minigl.library` v12, Picasso96 pair
  Prometheus.card 7.602 + Radeon9200.chip 3.0.
- Client: GLQuake (BlitzQuake) cross-built with ppc-amigaos-gcc 8.4.0,
  dispatch client via `libminigl.a`, V19 headers. Context: windowed,
  640x480, 32 bpp, triple buffer, sync off.
- All findings reproduced twice in an isolated probe client
  (raw `glTexImage2D`/`glTexSubImage2D`/draw calls only), and are consistent
  with in-game symptoms.

## Defect 1 — `glTexImage2D` rejects internalformat GL_RGBA (breaks text)

Call (exactly as GLQuake's font upload):

```c
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0,
             GL_RGBA, GL_UNSIGNED_BYTE, pixels /* RGBA8888 */);
```

Result: `glGetError()` returns **0x59** (MiniGL-specific code, 89 decimal —
not a GL 1.1 enum). The texture object is left unusable; any draw call that
binds it fails with **0x58**. 100% reproducible (multiple runs, both
texture-name idioms: glGenTextures and sequential ids).

Impact on GLQuake: the console font is uploaded with internalformat GL_RGBA
→ console/HUD text does not render.

Accepted workarounds verified client-side: `internalformat = GL_RGB` or
`GL_RGB5_A1` (with format = GL_RGBA) upload without error and render.

## Defect 2 — `glTexImage2D` rejects internalformat GL_LUMINANCE (breaks lightmaps)

```c
glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 128, 128, 0,
             GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);
```

Result: `glGetError()` = **0x58** (first call) and subsequent
`glTexSubImage2D` on the same object = **0x59**. `glTexImage2D` with
`internalformat = MGL_UNSIGNED_SHORT_5_6_5` fails identically (0x58).

Impact on GLQuake: lightmap textures (dynamic lighting) cannot be created.

## Defect 3 — `glTexSubImage2D` is pathologically slow (~0.5 MB/s)

Measured in the probe (windowed, sync off, `glFinish()` per iteration,
DateStamp timing, median behaviour stable across runs):

| operation                          | time          |
|------------------------------------|---------------|
| glTexSubImage2D 256x256 GL_RGBA    | **408–415 ms per update** |
| glTexSubImage2D 128x128 GL_RGBA    | **122–138 ms per update**  |
| (reference) glClear color+depth 320x240 | 2–7 ms  |
| (reference) 300 textured quad draw calls | 20–60 ms |
| (reference) 800 glTexCoord2f+glVertex2f | 20–60 ms |

The cost is dominated by a large FIXED overhead, not data size
(256x256 costs the same as 128x128 for `glTexImage2D` full uploads:
~101 ms/call at either size — see below). GLQuake updates lightmap
textures with `glTexSubImage2D` whenever dynamic lights fire
(muzzle flashes, explosions — several times per second), which reduces
the game to **~0.5 fps**. This is the game-breaking performance defect.

## Defect 4 — `glTexImage2D` has ~101 ms fixed overhead even when successful

`glTexImage2D` with *working* internal formats (GL_RGB, GL_RGB5_A1),
`glFinish()` per iteration:

| upload                    | ms/call |
|---------------------------|---------|
| 128x128 GL_RGB5_A1/GL_RGBA | 102.0  |
| 128x128 GL_RGB/GL_RGBA     | 100.7  |
| 256x256 GL_RGB/GL_RGBA     | 101.3  |

128x128 (64 KB) and 256x256 (256 KB) cost the same → the ~100 ms is a
fixed per-call latency (likely a blocking fence/flush/timeout in the
submission path), not transfer cost. Makes level load take 20–30 s
(~200 uploads) and rules out any per-frame texture updates.

## Defect 5 — `mglWriteShotPPM` is a silent no-op

`mglWriteShotPPM("RAM:file.ppm")` (absolute path, context current, called
after `glFinish()`) returns without creating a file and without setting
`glGetError()`. Console screenshots in GLQuake produce nothing.

## Confirmed working (for comparison)

- `glTexImage2D` with internalformat = GL_RGB or GL_RGB5_A1
  (format GL_RGBA or GL_RGB): err 0x0, renders correctly — world
  geometry textures display with correct colors in-game.
- `glClear` (color+depth): 2–7 ms at 320x240.
- `MGL_FLATFAN` / `GL_QUADS` draw calls with `glTexCoord2f`+`glVertex2f`:
  0.07–0.27 ms per quad.
- Vertex dispatch: 25–75 µs per glTexCoord2f+glVertex2f pair.
- Context creation (windowed 320x240 and 640x480, 16 and 32 bpp),
  `mglSwitchDisplay` present, `glGetString`, blend state.

## GLQuake client-side workarounds now applied (context for the above)

- Font texture: `internalformat = GL_RGB5_A1` instead of GL_RGBA (renders).
- Lightmaps: `internalformat = GL_RGB5_A1` with GL_RGBA data, and dynamic
  `glTexSubImage2D` lightmap updates disabled (static baked lightmaps only)
  until Defect 3 is fixed.
- Errors 0x58/0x59 are undocumented; please include them (or standard GL
  enums) in the next header/docs revision.

Contact data available on request; the isolated probe client
(`probe_gl.c`, WarpOS binary + stdout log + build makefile) can be
provided for reproduction.
