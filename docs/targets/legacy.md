# Legacy and cross-platform builds

This tree began as a multi-platform Quake/GLQuake source archive. The
Makefiles and assembly in this page are the original 1997–2002 build paths.
They are **not part of the 2026 maintained workflows** and were last touched
by the baseline import (commit `a153c31`). Keep them working when practical,
but do not assume they are tested.

For the maintained targets see [../building.md](../building.md),
[warpos.md](warpos.md) and [68k.md](68k.md).

## Makefile reference

### `glquake/`

| Makefile | Platform | Toolchain | Output |
|---|---|---|---|
| `Makefile.linuxi386` | Linux x86 | EGCS 1.1.2 (`gcc`), Mesa/X11 | `debugi386/bin/*`, `releasei386/bin/*`: `glquake`, `glquake.glx`, `glquake.3dfxgl`, `squake`, `quake.x11` |
| `Makefile.Solaris` | Solaris i386/sparc | `gcc` | `quake.sw`, `quake.xil` |
| `Makefile.MorphOS` | MorphOS PPC | vbcc `vc +morphos_wos`, pasm | `quakeMorphOS` |
| `Makefile.AmigaPUp` | Amiga PowerUP PPC+68k | vbcc `vc +ppc_wos` / `vc`, pasm + phxass | `quakePPC` |
| `Makefile.Amiga68k` | Amiga 68k WinQuake | vbcc `vc +m68kdb`, phxass 68060 | `quake68k` |
| `Makefile.Amiga68k_MGL` | Amiga 68k GLQuake | vbcc + MiniGL `mgl.library` | `glquake68k_vbcc` |
| `Makefile.Amiga68k_MGL_040` | Same, 68040 | vbcc `-cpu=68040` | `glquake68k_vbcc` |
| `Makefile.AmigaWOS` | WarpOS WinQuake | vbcc `vc +warposdb`, pasm | `quakeWOS` |
| `Makefile.AmigaWOS_MGL` | WarpOS GLQuake | vbcc + `mgl.library` | `glquakeWOS_vbcc` |
| `Makefile.GCCAmiga68k_MGL` | Amiga 68k GLQuake | `m68k-amigaos-gcc -noixemul`, phxass/hunk2gcc | `m68k_gcc/glquake/glquake68k_gcc` |
| `Makefile.GCCAmigaWOS_MGL` | WarpOS GLQuake | `ppc-amigaos-gcc -warpup` | `warpos_gcc/glquake/glquakeWOS_gcc` |

The classic Amiga GL builds use `mgl.library`/`mglasm.library`/Warp3D and the
direct vertex-buffer helpers in `miniglext.c`; they are the baseline the 2026
dispatch targets replaced.

### `glqwcl/`

QuakeWorld client + server. Seven Makefiles mirror the table above
(`Makefile.Amiga68k`, `Amiga68k_MGL`, `Amiga68k_MGL_040`, `AmigaWOS`,
`AmigaWOS_MGL`, `GCCAmiga68k_MGL`, `GCCAmigaWOS_MGL`), building
`qwcl68k`/`glqwcl68k_vbcc`/`qwclWOS`/`glqwclWOS_vbcc`/… into `m68k/`,
`warpos/` or `m68k_gcc/` staging dirs.

QuakeWorld-specific features from the 2002 era (see
[`glqwcl/qwcl68k.readme`](../../glqwcl/qwcl68k.readme)) include serverbrowser
with ping/status, proxy features and many FPS-booster cvars
(`r_explosions`, `r_teleports`, `bodyfilter`, `gibfilter`, `beamskip`,
`frameskip`, `r_fastcontents`, `r_skycolor`, `r_turbcolor`, `cl_muzzleflash`,
…). QuakeWorld's network layer lives in `glqwcl/client/*`; it does **not**
share the 2026 WOS networking fixes in `glquake/net_bsdsocket.c`.

## Assembly and generated headers in legacy builds

The vbcc/GCC Amiga builds assemble the full `*.s` renderer/rasterizer sets:

- 68k: `r_alias68k`, `r_bsp68k`, `r_draw68k`, `r_edge68k`, `r_light68k`,
  `r_misc68k`, `r_surf68k`, `r_aclip68k`, `d_*68k`, `common68k`,
  `mathlib68k`, `gl_rlight68k`, `snd_*68k`, `sys_amiga68k`, `in_amigamouse*`,
  `vid_c2p_040`, `vid_turboupd68k` (+ variants per target).
- PPC: `r_*PPC`, `d_*PPC`, `d_surfPPC`, `commonPPC`, `mathlibPPC`,
  `gl_rlightPPC`, `fconstPPC`, `snd_mixamigaPPC`, `sys_timerPPC`,
  `vid_c2p_PPC`, `vid_turboupdPPC`, `MulDiv64PPC`/`Modu64pPPC`.

These read offsets from generated `quakedef*.i` / `glquakedef*.i` headers
produced by `genasmheaders`; see
[../building.md](../building.md#assembly-offset-headers). The `glqwcl` tree
has its own copies of the generator and `.gen` files.

## Original documentation still in the tree

| File | Content |
|---|---|
| `BlitzQuake_src.readme` (root) | Aminet release blurb, credits, suggested improvements. |
| `glquake/glquake.readme` | 2002 GLQuake settings guide (video, texture, lightmap options). |
| `glquake/glquake68k.readme` | 68060 GLQuake beta notes and known bugs (luminance lightmaps, teleporter rendering, sprites). |
| `glquake/GLQuake_src.readme` | WarpOS GLQuake source release notes (Massimiliano Tretene). |
| `glqwcl/qwcl68k.readme` | QuakeWorld client features and cvars. |
| `glquake/pq_manual.txt` | Programmer's manual fragment. |
| `glquake/COPYING` | GPL. |

These describe the original ports; the 2026 work in this repository is
separate and documented under `docs/`.
