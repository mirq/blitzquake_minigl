# Quake presentation regression probe

Build from `glquake/`:

```
make -f Makefile.GCCAmigaWOS_probe PROBE=qpresent
```

Convert `qpresent.warpelf` using target-hosted Elf2Exe2, then install
`qpresent` beside the validated `minigl_ppc.dll`. It loads the DLL from
the current directory. Use a fresh host per client, InitPPC once per boot,
and the physical-device recovery protocol after any failed/wedged run.

The default runs six frames of each case at fullscreen 800x600x32,
three buffers, sync off, vertex buffer 3000, mtex buffer 6000:

| Case | Draw workload |
| --- | --- |
| 0 | 2D MGL_FLATFAN baseline |
| 1 | Perspective textured GL_POLYGON |
| 2 | Polygon plus GL_LUMINANCE blended lightmap pass |
| 3 | Dual texture coordinates / multitexture flush |
| 4 | Smooth triangle fan and strip |
| 5 | Sprite quad and 256 points |
| 6 | Lightmap subimage update followed by blended draw |
| 7 | 1000 polygon calls to exercise buffer submission |
| 8 | 1000 polygons alternating textures to force state changes |
| 9 | Alternating textures plus interleaved 2D/3D submission |
| 10 | Resident texture: update mip level 1, then draw via unit 0 |
| 11 | Same content-serial update, sampled via texture unit 1 |
| 12 | Arm mismatch expansion, accumulate 180 same-state fans, then change textures |
| 13 | Slide an 800x600 console quad across the top clipping plane (including top=-90) |

`-case N` runs only that case. `-window` changes only window mode.
`-finish` adds a separately checked glFinish before each swap; it is
deliberately OFF by default so it cannot hide a submission-order defect.

Example AmigaDOS script:

```
cd Work:games/quake
Stack 1048576
qpresent -case 1
```

`DH2:wosbuild/qpresent.log` is reset at startup and flushed after every
record, with case/frame/timestamp and errors at context creation, uploads,
lock, draw, optional finish, and swap. No performance counters are enabled.
The background alternates each frame for visual checking. No PAKs needed.

The probe stops on the first GL error, attempts normal context deletion,
and returns 20 on failure or 0 on success. The frame count is bounded, but
there is no unsafe forced termination of a blocked driver call: a missing
leave/exit record requires investigation and possibly a cold power cycle.
A PASS log proves API calls succeeded, not that scanout visibly changed.

## Evidence prompting this test

Quake with DLL CRC FCEF7CE9 advanced through signon 4 and later map loading
while the operator observed a frozen Necropolis loading display. The
diagnostic client returned 0 after loading-screen swaps but 89 (the SDK's
GL_INVALID_OPERATION) after gameplay swaps. Some gameplay frames also had
89 before swapping. Thus neither a loader hang nor a swap-only defect can
be assumed; the first failing draw/submission must be isolated.

This is a candidate reproducer, not yet a confirmed reproduction. A
passing small case does not rule out Quake's more complex scene/state.

Cases 0–9 passed on the FCEF7CE9 DLL / F46026B4 chip stack. A later Quake
rejection dump showed H13=0x0005001f (content serial 5). Cases 10/11 target
that serial-bearing path; host-side emitter regression proves the original
driver mask rejects the documented serial field. These new hardware cases
still need execution on the old and corrected driver.
