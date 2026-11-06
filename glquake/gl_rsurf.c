/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_surf.c: surface-related refresh code

#include "quakedef.h"
#include "frame_profile.h"
#include "wos_stalltrace.h"

#ifndef MINIGL_DISPATCH_CLIENT
#include <mgl/mglmacros.h>
#endif
#include "miniglext.h" //optimized vertexbuffer construct

#ifdef MINIGL_DISPATCH_CLIENT
cvar_t gl_fake_multitexture = {"gl_fake_multitexture","0", true};
#define FAKE_MULTITEXTURE_VALUE 0
#else
cvar_t gl_fake_multitexture = {"gl_fake_multitexture","1", true};
#define FAKE_MULTITEXTURE_VALUE ((int)gl_fake_multitexture.value)
#endif

extern void R_DrawSkyBox();
extern cvar_t gl_fog;
extern void R_ClearSkyBox();


#ifndef GL_RGBA4
	#define GL_RGBA4  0
#endif

int     skytexturenum;

extern qboolean r_dowarp; //waterwarp switch by surgeon
extern qboolean mtexenabled; //multitexture state (defined below)

//extern GLfloat gl_debug;
extern int  gl_filter_min; //surgeon
extern int  gl_filter_max; //surgeon

int   lightmap_bytes;   // 1,2, or 4 
#ifndef AMIGA
int   lightmap_textures;
#else

/*
** surgeon: we set a static texnum-base for lightmaps in
** order to use narrow-range multitexture-sorting for TMU1
** with MiniGL
*/
int   lightmap_textures = 1024;
#endif

#ifdef LITFILES
qboolean eyecandy = false;
unsigned    blocklights[18*18*3]; //LordHavoc
#else
unsigned    blocklights[18*18];
#endif

#define BLOCK_WIDTH   128
#define BLOCK_HEIGHT  128

#define MAX_LIGHTMAPS 64

static int     active_lightmaps; /*surgeon:static*/

typedef struct glRect_s {
  unsigned char l,t,w,h;
} glRect_t;

static glpoly_t  *lightmap_polys[MAX_LIGHTMAPS];
static qboolean  lightmap_modified[MAX_LIGHTMAPS];
static glRect_t  lightmap_rectchange[MAX_LIGHTMAPS];
static int     allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];
/*surgeon:static*/

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte    lightmaps[4*MAX_LIGHTMAPS*BLOCK_WIDTH*BLOCK_HEIGHT];

// For gl_texsort 0
static msurface_t  *skychain = NULL;
static msurface_t  *waterchain = NULL;
/*surgeon:static*/

#ifdef MGL68K_RESIDENT
cvar_t r_worldbatch = {"r_worldbatch", "1", true};   /* the resident world */
#else
cvar_t r_worldbatch = {"r_worldbatch", "0", true};
#endif

#define WORLD_BATCH_MAX_VERTICES 65535

/* The MiniGL R200 frontend rejects a single draw with more than
   MAX_VERTICES indices (16384 in the 16K build), so every batched
   glDrawElements is split below that limit.  Must stay a multiple of
   three. */
#define BATCH_DRAW_MAX_INDICES 16380

typedef struct worldbatchsurface_s {
  int segment;
  int firstvert;
  int numverts;
  int drawframe;
  int lightmap_next;
} worldbatchsurface_t;

typedef struct worldbatchsegment_s {
  float *verts;
  int numverts;
} worldbatchsegment_t;

static worldbatchsurface_t *worldbatch_surfaces;
static worldbatchsegment_t *worldbatch_segments;
static unsigned short *worldbatch_indices;
static int worldbatch_numsegments;
static int worldbatch_maxindices;
static int worldbatch_lightmap_heads[MAX_LIGHTMAPS];
static qboolean worldbatch_blending_world;

#ifdef MGL68K_RESIDENT
/*
 * minigl68k.library resident world (include/libraries/minigl68k_ext.h):
 * every world-batch segment is copied once per map into two VRAM buffers,
 * one with the texture coordinates and one with the lightmap coordinates
 * (compact TCL layout, little-endian). Each frame only the index lists are
 * sent; the vertices never cross the Zorro bus again and the Radeon's TCL
 * transforms them.
 */
#include <libraries/minigl68k_ext.h>
extern const MGL68KExt *MiniGL68KGetExt (void);

#define R68K_MAX_SEGMENTS 4
#define R68K_SEG_VERTS (MGL68K_RESIDENT_MAX_BYTES / MGL68K_RESIDENT_STRIDE)

cvar_t r_resident = {"r_resident", "1"};
static const MGL68KExt *r68k_ext;
static qboolean r68k_ext_probed;
static ULONG worldbatch_res_tex[R68K_MAX_SEGMENTS];
static ULONG worldbatch_res_lm[R68K_MAX_SEGMENTS];
static qboolean worldbatch_resident;

/* Static index lists (whole map, per texture and per lightmap, per
 * segment) in one resident buffer: a visible texture draws all of its
 * world triangles from VRAM, no per-frame index traffic. */
cvar_t r_resstatic = {"r_resstatic", "1"};
static ULONG worldstatic_handle;
static qboolean worldstatic_built, worldstatic_ok;
static unsigned int *worldstatic_tex;  /* [seg][tex] offset, count pairs */
static unsigned int *worldstatic_lm;   /* [seg][lm] offset, count pairs */
static unsigned int worldstatic_lmall[R68K_MAX_SEGMENTS][2];

/* Lightmap atlas: every 128x128 block in one texture, 4 blocks a row, so
 * the lightmap pass of the world and of brush models is one draw. */
cvar_t r_lmatlas = {"r_lmatlas", "1"};
#define LMA_COLS 4
#define LMA_WIDTH (LMA_COLS * 128)
qboolean lma_active;
static int lma_height;
#define LMA_TEX (lightmap_textures + MAX_LIGHTMAPS)

/* Deferred unrotated brush entities: drawn together after the brush
 * pass, one ring draw per texture and one atlas lightmap draw. */
#define BMD_MAX 1024
static msurface_t *bmd_surf[BMD_MAX];
static texture_t *bmd_tex[BMD_MAX];
static float bmd_org[BMD_MAX][3];
static int bmd_count;
static qboolean bmd_lmpending;
cvar_t r_bmdefer = {"r_bmdefer", "1"};

static void R_LMAMap (int block, float s, float t, float *os, float *ot)
{
  *os = (float)((block % LMA_COLS) * 128) * (1.f / LMA_WIDTH) +
    s * (128.f / LMA_WIDTH);
  *ot = ((float)((block / LMA_COLS) * 128) + t * 128.f) / (float)lma_height;
}
static int worldstatic_numtex;
static int worldstatic_texnum = -1;

int r68k_now;
int r68k_draws, r68k_static_draws, r68k_frames, r68k_alias_ents, r68k_alias_verts;

#include "frame_profile.h"
static BOOL R68K_DrawIdx (ULONG h, ULONG o, ULONG ih, ULONG io, ULONG c)
{
  unsigned long t = FP_Enter (FP_RDRAW);
  BOOL r = r68k_ext->DrawResidentIndexed (h, o, ih, io, c);
  FP_Exit (FP_RDRAW, t);
  return r;
}
static BOOL R68K_Draw (ULONG h, ULONG o, const UWORD *i, ULONG c)
{
  unsigned long t = FP_Enter (FP_RDRAW);
  BOOL r = r68k_ext->DrawResident (h, o, i, c);
  FP_Exit (FP_RDRAW, t);
  return r;
}
static BOOL R68K_Streams (const MGL68KStreamDraw *d)
{
  unsigned long t = FP_Enter (FP_RDRAW);
  BOOL r = r68k_ext->DrawStreams (d);
  FP_Exit (FP_RDRAW, t);
  return r;
}

static const MGL68KExt *R_ResidentExt (void)
{
  if (!r68k_ext_probed)
  {
    r68k_ext_probed = true;
    r68k_ext = MiniGL68KGetExt ();
    if (r68k_ext && (r68k_ext->version != MGL68K_EXT_VERSION ||
        r68k_ext->structSize < sizeof(MGL68KExt)))
      r68k_ext = NULL;
    r68k_now = r68k_ext && (r68k_ext->layoutFlags & MGL68K_LAYOUT_NO_W);
    Con_DPrintf ("minigl68k resident buffers: %s%s\n",
      r68k_ext ? "available" : "not available",
      r68k_now ? " (no W)" : "");
  }
  return r_resident.value ? r68k_ext : NULL;
}

static ULONG R_SwapLE (ULONG v)
{
  return (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24);
}

#define PAT_IDENT_N  9364
#define PAT_QUADS    256
#define PAT_FANS     128
#define PAT_IDENT    0
#define PAT_QUAD     (PAT_IDENT + PAT_IDENT_N * 2)
#define PAT_FAN      (PAT_QUAD + PAT_QUADS * 6 * 2)
#define PAT_MODELS   (PAT_FAN + PAT_FANS * 48 * 2)
#define PAT_WORLD    (96 * 1024)    /* world static lists from here */
#define PAT_BYTES    (256 * 1024)
#define PAT_MAXMODELS 256
static ULONG pat_handle;
static volatile unsigned short *pat_cpu;
static qboolean R_PatternReady (void);
static ULONG store_handle;
static void R_PatternResetModels (void);

static void R_FreeWorldResident (void)
{
  int i;
  for (i = 0; i < R68K_MAX_SEGMENTS; i++)
  {
    if (r68k_ext && worldbatch_res_tex[i])
      r68k_ext->ResidentFree (worldbatch_res_tex[i]);
    if (r68k_ext && worldbatch_res_lm[i])
      r68k_ext->ResidentFree (worldbatch_res_lm[i]);
    worldbatch_res_tex[i] = worldbatch_res_lm[i] = 0;
  }
  worldstatic_handle = 0;   /* lives in the pattern buffer */
  worldstatic_built = worldstatic_ok = false;
  worldstatic_tex = worldstatic_lm = NULL;
  worldbatch_resident = false;
  lma_active = false;
  R_PatternResetModels ();
}

static void R_StaticPut (volatile unsigned short *d, int at, int v)
{
  d[at] = (unsigned short)(((v & 0xff) << 8) | ((v >> 8) & 0xff));
}

/* Built at the first world draw (lightmap numbers are final then). */
static void R_BuildWorldStatic (void)
{
  model_t *world = cl.worldmodel;
  int total = 0, at = 0, seg, k, i, j;
  int numtex = world->numtextures;
  APTR cpu;
  volatile unsigned short *d;

  worldstatic_built = true;
  worldstatic_ok = false;
  if (!r_resstatic.value)
    return;
  /* every triangle twice (texture and lightmap lists) plus padding */
  total = 2 * worldbatch_maxindices +
    worldbatch_numsegments * (numtex + MAX_LIGHTMAPS + 1);
  if (!R_PatternReady () ||
      PAT_WORLD + total * 2 > (int)MGL68K_RESIDENT_MAX_BYTES)
  {
    Con_DPrintf ("minigl68k: static world indices too big (%i)\n", total);
    return;
  }
  worldstatic_tex = Hunk_AllocName (worldbatch_numsegments * numtex * 2 *
    sizeof(unsigned int), "wstex");
  worldstatic_lm = Hunk_AllocName (worldbatch_numsegments * MAX_LIGHTMAPS *
    2 * sizeof(unsigned int), "wslm");
  worldstatic_numtex = numtex;
  worldstatic_handle = pat_handle;
  (void)cpu;
  d = pat_cpu + PAT_WORLD / 2;
  for (seg = 0; seg < worldbatch_numsegments; seg++)
  {
    int kmax = lma_active ? numtex + 1 : numtex + MAX_LIGHTMAPS;
    for (k = 0; k < kmax; k++)
    {
      int start = at;
      qboolean all = lma_active && k == numtex;
      texture_t *t = k < numtex ? world->textures[k] : NULL;
      unsigned int *slot = k < numtex ?
        &worldstatic_tex[(seg * numtex + k) * 2] : all ?
        worldstatic_lmall[seg] :
        &worldstatic_lm[(seg * MAX_LIGHTMAPS + (k - numtex)) * 2];

      if (k < numtex && !t)
      {
        slot[0] = slot[1] = 0;
        continue;
      }
      for (i = world->firstmodelsurface;
        i < world->firstmodelsurface + world->nummodelsurfaces; i++)
      {
        msurface_t *surf = &world->surfaces[i];
        worldbatchsurface_t *bs = &worldbatch_surfaces[i];
        if (bs->numverts < 3 || bs->segment != seg)
          continue;
        if (k < numtex ? surf->texinfo->texture != t :
          !all && surf->lightmaptexturenum != k - numtex)
          continue;
        for (j = 1; j < bs->numverts - 1; j++)
        {
          R_StaticPut (d, at++, bs->firstvert);
          R_StaticPut (d, at++, bs->firstvert + j);
          R_StaticPut (d, at++, bs->firstvert + j + 1);
        }
      }
      slot[0] = PAT_WORLD + start * 2;
      slot[1] = at - start;
      if (at & 1)
        R_StaticPut (d, at++, 0);
    }
  }
  (void)d[at - 1];
  worldstatic_ok = true;
  Con_DPrintf ("minigl68k: static world indices, %i\n", at);
}

/* Draws a static list in chunks; FALSE when nothing was drawn. */
static qboolean R_DrawStatic (ULONG handle, const unsigned int *slot)
{
  int first, count = slot[1];
  for (first = 0; first < count; first += 9360)
  {
    int n = count - first;
    if (n > 9360)
      n = 9360;
    r68k_static_draws++;
    if (!R68K_DrawIdx (handle, 0, worldstatic_handle,
        slot[0] + first * 2, n))
    {
      if (!first)
      {
        Con_DPrintf ("minigl68k: static draw refused\n");
        worldstatic_ok = false;
        return false;
      }
      return true;
    }
  }
  return true;
}

/* One VRAM buffer per segment and pass; texcoord is 3 (texture) or 5
 * (lightmap) within the glpoly vertex. */
static ULONG R_UploadResidentB (const float *verts, int numverts,
  int texcoord, const unsigned char *blocks);

static ULONG R_UploadResident (const float *verts, int numverts, int texcoord)
{
  return R_UploadResidentB (verts, numverts, texcoord, NULL);
}

/* blocks: lightmap block per vertex (atlas coordinates), or NULL */
static ULONG R_UploadResidentB (const float *verts, int numverts,
  int texcoord, const unsigned char *blocks)
{
  APTR cpu;
  volatile ULONG *d;
  ULONG handle;
  int i;
  union { float f; ULONG u; } x, y, z, s, t;

  handle = r68k_ext->ResidentAlloc (numverts * MGL68K_RESIDENT_STRIDE, &cpu);
  if (!handle)
    return 0;
  d = (volatile ULONG *)cpu;
  for (i = 0; i < numverts; i++, verts += VERTEXSIZE)
  {
    x.f = verts[0]; y.f = verts[1]; z.f = verts[2];
    s.f = verts[texcoord]; t.f = verts[texcoord + 1];
    if (blocks)
      R_LMAMap (blocks[i], verts[texcoord], verts[texcoord + 1], &s.f, &t.f);
    d[0] = R_SwapLE (x.u);
    d[1] = R_SwapLE (y.u);
    d[2] = R_SwapLE (z.u);
    if (r68k_now)
    {
      d[3] = 0xffffffff;
      d[4] = R_SwapLE (s.u);
      d[5] = R_SwapLE (t.u);
      d += 6;
    }
    else
    {
      d[3] = 0x0000803f;
      d[4] = 0xffffffff;
      d[5] = R_SwapLE (s.u);
      d[6] = R_SwapLE (t.u);
      d += 7;
    }
  }
  (void)d[-1];
  return handle;
}

/* Decides the atlas from the blocks in use (they are allocated by now). */
static void R_LMAPlan (void)
{
  int n, rows;
  lma_active = false;
  if (!r_lmatlas.value || !R_ResidentExt ())
    return;
  for (n = 0; n < MAX_LIGHTMAPS && allocated[n][0]; n++)
    ;
  rows = (n + LMA_COLS - 1) / LMA_COLS;
  for (lma_height = 128; lma_height < rows * 128; lma_height <<= 1)
    ;
  if (!n || lma_height > 1024)
    return;
  lma_active = true;
}

/* lightmap block of every vertex of a segment */
static unsigned char *R_SegmentBlocks (int seg, int numverts)
{
  model_t *world = cl.worldmodel;
  unsigned char *b = malloc (numverts);
  int i, k;
  if (!b)
    return NULL;
  memset (b, 0, numverts);
  for (i = world->firstmodelsurface;
    i < world->firstmodelsurface + world->nummodelsurfaces; i++)
  {
    worldbatchsurface_t *bs = &worldbatch_surfaces[i];
    if (bs->numverts < 3 || bs->segment != seg)
      continue;
    for (k = 0; k < bs->numverts; k++)
      b[bs->firstvert + k] = (unsigned char)world->surfaces[i].lightmaptexturenum;
  }
  return b;
}

static void R_BuildWorldResident (void)
{
  int i;

  worldbatch_resident = false;
  R_LMAPlan ();
  if (!R_ResidentExt () || worldbatch_numsegments > R68K_MAX_SEGMENTS)
  {
    lma_active = false;
    return;
  }
  for (i = 0; i < worldbatch_numsegments; i++)
  {
    worldbatchsegment_t *seg = &worldbatch_segments[i];
    if (!seg->numverts || seg->numverts > R68K_SEG_VERTS)
    {
      R_FreeWorldResident ();
      return;
    }
    worldbatch_res_tex[i] = R_UploadResident (seg->verts, seg->numverts, 3);
    if (lma_active)
    {
      unsigned char *blocks = R_SegmentBlocks (i, seg->numverts);
      worldbatch_res_lm[i] = blocks ?
        R_UploadResidentB (seg->verts, seg->numverts, 5, blocks) : 0;
      if (blocks)
        free (blocks);
    }
    else
      worldbatch_res_lm[i] = R_UploadResident (seg->verts, seg->numverts, 5);
    if (!worldbatch_res_tex[i] || !worldbatch_res_lm[i])
    {
      Con_DPrintf ("minigl68k: resident world allocation failed\n");
      R_FreeWorldResident ();
      return;
    }
  }
  worldbatch_resident = true;
  Con_DPrintf ("minigl68k: resident world, %i segments\n",
    worldbatch_numsegments);
}

/*
 * Dynamic resident ring for per-frame geometry (alias models): two VRAM
 * buffers used on alternate frames. A frame's draws are retired before the
 * buffer is reused two frames later (the library keeps two batches in
 * flight and every frame submits more than that).
 */
#define R68K_RING_PER_FRAME 2
#define R68K_RING_BUFS (2 * R68K_RING_PER_FRAME)
static ULONG r68k_ring[R68K_RING_BUFS];
static volatile ULONG *r68k_ring_cpu[R68K_RING_BUFS];
static int r68k_ring_frame = -1, r68k_ring_parity, r68k_ring_sub;
static ULONG r68k_ring_cursor;
static qboolean r68k_ring_failed;

/* Room for numverts compact vertices in this frame's ring buffers (two
 * per frame, used one after the other). */
qboolean R_ResidentDynBegin (int numverts, volatile ULONG **dst,
  ULONG *handle, ULONG *offset)
{
  ULONG bytes = (ULONG)numverts * MGL68K_RESIDENT_STRIDE;
  int i, b;

  if (r68k_ring_failed || !R_ResidentExt () ||
      bytes > MGL68K_RESIDENT_MAX_BYTES)
    return false;
  if (!r68k_ring[0])
  {
    for (i = 0; i < R68K_RING_BUFS; i++)
    {
      APTR cpu;
      r68k_ring[i] = r68k_ext->ResidentAlloc (MGL68K_RESIDENT_MAX_BYTES, &cpu);
      r68k_ring_cpu[i] = (volatile ULONG *)cpu;
      if (!r68k_ring[i])
      {
        Con_DPrintf ("minigl68k: dynamic ring allocation failed\n");
        r68k_ring_failed = true;
        return false;
      }
    }
    Con_DPrintf ("minigl68k: dynamic ring ready\n");
  }
  if (r_framecount != r68k_ring_frame)
  {
    if (!(++r68k_frames & 127))
    {
      Con_DPrintf ("minigl68k: %i draws (%i static), %i alias models, "
        "%i alias vertices per frame\n", r68k_draws / 128,
        r68k_static_draws / 128, r68k_alias_ents / 128,
        r68k_alias_verts / 128);
      r68k_draws = r68k_static_draws = r68k_alias_ents = r68k_alias_verts = 0;
    }
    r68k_ring_frame = r_framecount;
    r68k_ring_parity ^= 1;
    r68k_ring_sub = 0;
    r68k_ring_cursor = 0;
  }
  if (r68k_ring_cursor + bytes > MGL68K_RESIDENT_MAX_BYTES)
  {
    if (r68k_ring_sub + 1 >= R68K_RING_PER_FRAME)
      return false;
    r68k_ring_sub++;
    r68k_ring_cursor = 0;
  }
  b = r68k_ring_parity * R68K_RING_PER_FRAME + r68k_ring_sub;
  *dst = r68k_ring_cpu[b] + (r68k_ring_cursor >> 2);
  *handle = r68k_ring[b];
  *offset = r68k_ring_cursor;
  r68k_ring_cursor += bytes;
  return true;
}

/*
 * Pattern indices in VRAM (one resident buffer, kept across maps): the
 * identity list (particles), quads (2D), 18-vertex fans (glows), then the
 * index lists of alias models, so dynamic draws write no indices.
 */
static ULONG pat_handle;
static volatile unsigned short *pat_cpu;
static qboolean pat_failed;
static int pat_cursor = PAT_MODELS, pat_nmodels;
static struct { const void *key; int verts, count, hash, offset; }
  pat_models[PAT_MAXMODELS];

static void PatPut (int at, int v)
{
  pat_cpu[at] = (unsigned short)(((v & 0xff) << 8) | ((v >> 8) & 0xff));
}

static qboolean R_PatternReady (void)
{
  APTR cpu;
  int i, j;
  if (pat_handle)
    return true;
  if (pat_failed || !R_ResidentExt ())
    return false;
  pat_handle = r68k_ext->ResidentAlloc (PAT_BYTES, &cpu);
  if (!pat_handle)
  {
    pat_failed = true;
    Con_DPrintf ("minigl68k: pattern index allocation failed\n");
    return false;
  }
  pat_cpu = (volatile unsigned short *)cpu;
  for (i = 0; i < PAT_IDENT_N; i++)
    PatPut (PAT_IDENT / 2 + i, i);
  for (i = 0; i < PAT_QUADS; i++)
  {
    int b = PAT_QUAD / 2 + i * 6;
    PatPut (b, i*4); PatPut (b+1, i*4+1); PatPut (b+2, i*4+2);
    PatPut (b+3, i*4); PatPut (b+4, i*4+2); PatPut (b+5, i*4+3);
  }
  for (i = 0; i < PAT_FANS; i++)
    for (j = 0; j < 16; j++)
    {
      int b = PAT_FAN / 2 + i * 48 + j * 3;
      PatPut (b, i * 18);
      PatPut (b + 1, i * 18 + 1 + j);
      PatPut (b + 2, i * 18 + 2 + j);
    }
  (void)pat_cpu[0];
  return true;
}

/* forget the model lists (a new map: the model cache may move) */
static void R_StoreReset (void);

static void R_PatternResetModels (void)
{
  pat_cursor = PAT_MODELS;
  pat_nmodels = 0;
  if (store_handle)
    R_StoreReset ();
}

qboolean R_ResidentPatternDraw (ULONG handle, ULONG offset, int pattern,
  int count)
{
  static const int base[3] = { PAT_IDENT, PAT_QUAD, PAT_FAN };
  if (!R_PatternReady () || pattern < 0 || pattern > 2)
    return false;
  r68k_draws++;
  return R68K_DrawIdx (handle, offset, pat_handle,
    base[pattern], count);
}

qboolean R_ResidentPointsOK (void)
{
  return r68k_ext && r68k_ext->structSize >= sizeof (MGL68KExt) &&
         r68k_ext->DrawResidentPoints && R_PatternReady ();
}

/* count points (current point size) from a resident buffer, through the
 * identity index pattern; false when the library has no points draw. */
qboolean R_ResidentPointsDraw (ULONG handle, ULONG offset, int count)
{
  qboolean r;
  unsigned long t;
  if (!R_PatternReady () || count > PAT_IDENT_N ||
      r68k_ext->structSize < sizeof (MGL68KExt) ||
      !r68k_ext->DrawResidentPoints)
    return false;
  r68k_draws++;
  t = FP_Enter (FP_RDRAW);
  r = r68k_ext->DrawResidentPoints (handle, offset, pat_handle, PAT_IDENT,
                                    count);
  FP_Exit (FP_RDRAW, t);
  return r;
}

/* A model's index list in VRAM: its byte offset, or -1. indices may be
 * NULL to only look it up. */
int R_ResidentModelIndices (const void *key, int verts, int count, int hash,
  const unsigned short *indices)
{
  int i, bytes;
  for (i = 0; i < pat_nmodels; i++)
    if (pat_models[i].key == key && pat_models[i].verts == verts &&
        pat_models[i].count == count && pat_models[i].hash == hash)
      return pat_models[i].offset;
  if (!indices || !R_PatternReady () || pat_nmodels >= PAT_MAXMODELS)
    return -1;
  bytes = (count * 2 + 3) & ~3;
  if (pat_cursor + bytes > PAT_WORLD)
    return -1;
  for (i = 0; i < count; i++)
    PatPut (pat_cursor / 2 + i, indices[i]);
  (void)pat_cpu[pat_cursor / 2];
  pat_models[pat_nmodels].key = key;
  pat_models[pat_nmodels].verts = verts;
  pat_models[pat_nmodels].count = count;
  pat_models[pat_nmodels].hash = hash;
  pat_models[pat_nmodels].offset = pat_cursor;
  pat_nmodels++;
  pat_cursor += bytes;
  return pat_models[pat_nmodels - 1].offset;
}

/*
 * Vertex store (VRAM outside the resident pool): static per-model and
 * per-pose streams, keyed by a pointer and a tag; forgotten with the map.
 */
#define STORE_BYTES (3 * 1024 * 1024)
#define STORE_SLOTS 4096
static ULONG store_handle;
static volatile ULONG *store_cpu;
static qboolean store_failed;
static int store_cursor;
static struct { const void *key; int tag, offset; } store_tab[STORE_SLOTS];
static int store_used;

static void R_StoreReset (void)
{
  memset (store_tab, 0, sizeof(store_tab));
  store_cursor = 0;
  store_used = 0;
}

qboolean R_StoreReady (void)
{
  APTR cpu;
  if (store_handle)
    return true;
  if (store_failed || !R_ResidentExt () || r68k_ext->structSize <
      offsetof(MGL68KExt, DrawStreams) + sizeof(void *))
    return false;
  store_handle = r68k_ext->StoreAlloc (STORE_BYTES, &cpu);
  if (!store_handle)
  {
    store_failed = true;
    Con_DPrintf ("minigl68k: vertex store allocation failed\n");
    return false;
  }
  store_cpu = (volatile ULONG *)cpu;
  R_StoreReset ();
  Con_DPrintf ("minigl68k: vertex store ready\n");
  return true;
}

/* Byte offset of (key, tag) in the store, or -1. *fill is set when the
 * caller must write the bytes now (a new entry). */
int R_StoreGet (const void *key, int tag, int bytes, volatile ULONG **fill)
{
  unsigned h = ((unsigned)key * 2654435761u + (unsigned)tag * 40503u) >>
    20 & (STORE_SLOTS - 1);
  *fill = NULL;
  if (!R_StoreReady ())
    return -1;
  while (store_tab[h].key)
  {
    if (store_tab[h].key == key && store_tab[h].tag == tag)
      return store_tab[h].offset;
    h = (h + 1) & (STORE_SLOTS - 1);
  }
  bytes = (bytes + 3) & ~3;
  if (store_used >= STORE_SLOTS / 2 || store_cursor + bytes > STORE_BYTES)
    return -1;
  store_tab[h].key = key;
  store_tab[h].tag = tag;
  store_tab[h].offset = store_cursor;
  store_used++;
  *fill = store_cpu + store_cursor / 4;
  store_cursor += bytes;
  return store_tab[h].offset;
}

/* Indexed triangles: positions and texcoords from the store, colours
 * from the ring, indices from the pattern buffer. */
qboolean R_ResidentStreams (int posoff, ULONG colhandle, ULONG coloff,
  int texoff, int idxoff, int count)
{
  MGL68KStreamDraw sd;
  if (!store_handle || !pat_handle)
    return false;
  sd.posHandle = store_handle; sd.posOffset = posoff; sd.posStride = 3;
  sd.colHandle = colhandle;    sd.colOffset = coloff; sd.colStride = 1;
  sd.texHandle = store_handle; sd.texOffset = texoff; sd.texStride = 2;
  sd.indexHandle = pat_handle; sd.indexOffset = idxoff; sd.count = count;
  r68k_draws++;
  return R68K_Streams (&sd);
}

qboolean R_ResidentIndexedDraw (ULONG handle, ULONG offset, int idxoffset,
  int count)
{
  if (!pat_handle)
    return false;
  r68k_draws++;
  return R68K_DrawIdx (handle, offset, pat_handle,
    idxoffset, count);
}

qboolean R_ResidentDynDraw (ULONG handle, ULONG offset,
  const unsigned short *indices, int count)
{
  r68k_draws++;
  return r68k_ext && R68K_Draw (handle, offset, indices, count);
}

/* Draws polygons (glpoly_t, one fan each) with ONE resident draw through
 * the ring; texcoord 3 = texture, 5 = lightmap. FALSE: nothing drawn. */
#define RING_POLY_MAX_VERTS 2048
static unsigned short ringpoly_indices[RING_POLY_MAX_VERTS * 3];

static qboolean R_RingDrawPolysO (glpoly_t **polys, int n, int texcoord,
  const unsigned char *blocks, float (*origins)[3]);

static qboolean R_RingDrawPolys (glpoly_t **polys, int n, int texcoord)
{
  return R_RingDrawPolysO (polys, n, texcoord, NULL, NULL);
}

static qboolean R_RingDrawPolysB (glpoly_t **polys, int n, int texcoord,
  const unsigned char *blocks)
{
  return R_RingDrawPolysO (polys, n, texcoord, blocks, NULL);
}

/* blocks: each polygon's lightmap block (atlas coordinates), or NULL;
 * origins: added to each polygon's positions (brush entities), or NULL */
static qboolean R_RingDrawPolysO (glpoly_t **polys, int n, int texcoord,
  const unsigned char *blocks, float (*origins)[3])
{
  int i, j, total = 0, icount = 0, base = 0;
  volatile ULONG *d;
  ULONG handle, offset;
  union { float f; ULONG u; } x, y, z, s0, t0;

  for (i = 0; i < n; i++)
    total += polys[i]->numverts;
  if (!total || total > RING_POLY_MAX_VERTS ||
      !R_ResidentDynBegin (total, &d, &handle, &offset))
    return false;
  for (i = 0; i < n; i++)
  {
    glpoly_t *p = polys[i];
    float *v = p->verts[0];
    for (j = 2; j < p->numverts; j++)
    {
      ringpoly_indices[icount++] = base;
      ringpoly_indices[icount++] = base + j - 1;
      ringpoly_indices[icount++] = base + j;
    }
    for (j = 0; j < p->numverts; j++, v += VERTEXSIZE)
    {
      x.f = v[0]; y.f = v[1]; z.f = v[2];
      if (origins)
      {
        x.f += origins[i][0];
        y.f += origins[i][1];
        z.f += origins[i][2];
      }
      s0.f = v[texcoord]; t0.f = v[texcoord + 1];
      if (blocks)
        R_LMAMap (blocks[i], v[texcoord], v[texcoord + 1], &s0.f, &t0.f);
      d[0] = R_SwapLE (x.u);
      d[1] = R_SwapLE (y.u);
      d[2] = R_SwapLE (z.u);
      if (r68k_now)
      {
        d[3] = 0xffffffff;
        d[4] = R_SwapLE (s0.u);
        d[5] = R_SwapLE (t0.u);
        d += 6;
      }
      else
      {
        d[3] = 0x0000803f;
        d[4] = 0xffffffff;
        d[5] = R_SwapLE (s0.u);
        d[6] = R_SwapLE (t0.u);
        d += 7;
      }
    }
    base += p->numverts;
  }
  (void)d[-1];
  return R_ResidentDynDraw (handle, offset, ringpoly_indices, icount);
}

/* Draws the index list in chunks; FALSE only when nothing was drawn. */
static qboolean R_DrawResidentIndices (ULONG handle, int numindices)
{
  /* The emitter bounds index count x stride by the space after the
   * buffer in the segment pool: stay within one segment's worth. */
#define R68K_MAX_CHUNK 9360
  int first;
  for (first = 0; first < numindices; first += R68K_MAX_CHUNK)
  {
    int n = numindices - first;
    if (n > R68K_MAX_CHUNK)
      n = R68K_MAX_CHUNK;
    static int ok, refused;
    if (!R68K_Draw (handle, 0, worldbatch_indices + first, n))
    {
      if (!refused++)
        Con_DPrintf ("minigl68k: resident draw refused (after %i ok)\n", ok);
      if (!first)
        return false;
      worldbatch_resident = false;   /* stop using it; rest falls back */
      return true;
    }
    if (!(++ok & 4095))
      Con_DPrintf ("minigl68k: %i resident draws, %i refused\n", ok, refused);
  }
  return true;
}
#endif


/* Uploads only the changed rectangle of lightmap block i (the rows of
 * the whole block width before): every texel crosses the Zorro bus. */
static void R_UploadLightmapRect (int i, glRect_t *r)
{
#ifdef MGL68K_RESIDENT
  if (lma_active)
  {
    int prev = currenttexture;
    int l = r->w > 0 ? r->l : 0, w = r->w > 0 ? r->w : BLOCK_WIDTH;
    GL_Bind (LMA_TEX);
    glPixelStorei (GL_UNPACK_ROW_LENGTH, BLOCK_WIDTH);
    glTexSubImage2D (GL_TEXTURE_2D, 0, (i % LMA_COLS) * 128 + l,
      (i / LMA_COLS) * 128 + r->t, w, r->h, gl_lightmap_format,
      GL_UNSIGNED_BYTE, lightmaps + ((i * BLOCK_HEIGHT + r->t) * BLOCK_WIDTH +
      l) * lightmap_bytes);
    glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
    if (prev >= 0)
      GL_Bind (prev);
    return;
  }
  if (r->w > 0 && r->w < BLOCK_WIDTH)
  {
    glPixelStorei (GL_UNPACK_ROW_LENGTH, BLOCK_WIDTH);
    glTexSubImage2D (GL_TEXTURE_2D, 0, r->l, r->t, r->w, r->h,
      gl_lightmap_format, GL_UNSIGNED_BYTE,
      lightmaps + ((i * BLOCK_HEIGHT + r->t) * BLOCK_WIDTH + r->l) *
        lightmap_bytes);
    glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
    return;
  }
#endif
  glTexSubImage2D (GL_TEXTURE_2D, 0, 0, r->t, BLOCK_WIDTH, r->h,
    gl_lightmap_format, GL_UNSIGNED_BYTE,
    lightmaps + (i * BLOCK_HEIGHT + r->t) * BLOCK_WIDTH * lightmap_bytes);
}

void R_RenderDynamicLightmaps (msurface_t *fa);

static qboolean R_WorldBatchSurface (msurface_t *surf)
{
  int surfnum;

  if (!gl_texsort.value || FAKE_MULTITEXTURE_VALUE)
    return false;
  if (!worldbatch_surfaces || !cl.worldmodel || !surf->polys)
    return false;
  if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB | SURF_UNDERWATER))
    return false;

  surfnum = surf - cl.worldmodel->surfaces;
  if (surfnum < 0 || surfnum >= cl.worldmodel->numsurfaces)
    return false;

  return worldbatch_surfaces[surfnum].numverts >= 3;
}

static void R_EnableWorldBatchArrays (worldbatchsegment_t *segment,
  int texcoord)
{
  glDisableClientState (GL_COLOR_ARRAY);
  glEnableClientState (GL_VERTEX_ARRAY);
  glEnableClientState (GL_TEXTURE_COORD_ARRAY);
  glVertexPointer (3, GL_FLOAT, VERTEXSIZE * sizeof(float), segment->verts);
  glTexCoordPointer (2, GL_FLOAT, VERTEXSIZE * sizeof(float),
    segment->verts + texcoord);
}

static void R_DisableWorldBatchArrays (void)
{
  glDisableClientState (GL_TEXTURE_COORD_ARRAY);
  glDisableClientState (GL_VERTEX_ARRAY);
}

#ifdef MGL_FASTPATH_VERSION
/* Batch-level FastPath staging (the Q3 tess model): the client writes
 * every visible fan triangle straight into the backend's vertex buffer.
 * One Begin/End pair per texture or lightmap atlas (chunked at the
 * buffer capacity), no dispatch per vertex and no generic client-array
 * decode in the frontend.  The caller keeps the target texture bound;
 * chunk flushes reuse it. */
static MGLFastPath worldfp;

static qboolean R_WorldFPBegin (void)
{
  return mglBeginFastPath (GL_TRIANGLES,
    MGL_FASTPATH_POSITION | MGL_FASTPATH_TEXCOORD0, &worldfp);
}

static void R_WorldFPEnd (void)
{
  mglEndFastPath (&worldfp);
}

static qboolean R_WorldFPTriangle (const float *a, const float *b,
  const float *c, int texcoord)
{
  if (worldfp.vertexCount + 3 > worldfp.vertexCapacity)
    if (!R_WorldFPBegin ())
      return false;

  mglFastPathTexCoord2f (&worldfp, a[texcoord], a[texcoord + 1]);
  mglFastPathVertex3f (&worldfp, a[0], a[1], a[2]);
  mglFastPathTexCoord2f (&worldfp, b[texcoord], b[texcoord + 1]);
  mglFastPathVertex3f (&worldfp, b[0], b[1], b[2]);
  mglFastPathTexCoord2f (&worldfp, c[texcoord], c[texcoord + 1]);
  mglFastPathVertex3f (&worldfp, c[0], c[1], c[2]);
  return true;
}
#endif

static void R_DrawWorldTextureBatch (texture_t *texture, msurface_t *chain,
  int segmentnum)
{
  texture_t *animated;

#ifdef MGL68K_RESIDENT
  if (worldbatch_resident && worldbatch_res_tex[segmentnum])
  {
    int numindices = 0;
    msurface_t *surf;

    if (!worldstatic_built)
      R_BuildWorldStatic ();
    if (worldstatic_ok && worldstatic_texnum >= 0 &&
      worldstatic_texnum < worldstatic_numtex)
    {
      const unsigned int *slot =
        &worldstatic_tex[(segmentnum * worldstatic_numtex +
          worldstatic_texnum) * 2];
      for (surf = chain; surf; surf = surf->texturechain)
      {
        worldbatchsurface_t *bs;
        if (!R_WorldBatchSurface (surf))
          continue;
        bs = &worldbatch_surfaces[surf - cl.worldmodel->surfaces];
        if (bs->drawframe == r_framecount && bs->segment == segmentnum)
          break;
      }
      if (!surf || !slot[1])
        return;
      animated = R_TextureAnimation (texture);
      GL_Bind (animated->gl_texturenum);
      if (R_DrawStatic (worldbatch_res_tex[segmentnum], slot))
        return;
    }

    for (surf = chain; surf; surf = surf->texturechain)
    {
      worldbatchsurface_t *batchsurf;
      int i;

      if (!R_WorldBatchSurface (surf))
        continue;
      batchsurf = &worldbatch_surfaces[surf - cl.worldmodel->surfaces];
      if (batchsurf->drawframe != r_framecount ||
        batchsurf->segment != segmentnum)
        continue;
      for (i = 1; i < batchsurf->numverts - 1; i++)
      {
        worldbatch_indices[numindices++] = batchsurf->firstvert;
        worldbatch_indices[numindices++] = batchsurf->firstvert + i;
        worldbatch_indices[numindices++] = batchsurf->firstvert + i + 1;
      }
    }
    if (!numindices)
      return;
    animated = R_TextureAnimation (texture);
    GL_Bind (animated->gl_texturenum);
    if (R_DrawResidentIndices (worldbatch_res_tex[segmentnum], numindices))
      return;
  }
#endif

#ifdef MGL_FASTPATH_VERSION
  /* Batch-level FastPath submission. */
  {
    msurface_t *surf;
    float *segmentverts;

    animated = R_TextureAnimation (texture);
    GL_Bind (animated->gl_texturenum);

    if (R_WorldFPBegin ())
    {
      segmentverts = worldbatch_segments[segmentnum].verts;

      for (surf = chain; surf; surf = surf->texturechain)
      {
        worldbatchsurface_t *batchsurf;
        int i;
        float *v;

        if (!R_WorldBatchSurface (surf))
          continue;

        batchsurf = &worldbatch_surfaces[surf - cl.worldmodel->surfaces];
        if (batchsurf->drawframe != r_framecount ||
          batchsurf->segment != segmentnum)
          continue;

        v = segmentverts + batchsurf->firstvert * VERTEXSIZE;
        for (i = 1; i < batchsurf->numverts - 1; i++)
          if (!R_WorldFPTriangle (v, v + i * VERTEXSIZE,
              v + (i + 1) * VERTEXSIZE, 3))
          {
            R_WorldFPEnd ();
            return;
          }
      }

      R_WorldFPEnd ();
      return;
    }
  }
#endif

  {
  int numindices = 0;
  msurface_t *surf;
  texture_t *animated;

  for (surf = chain; surf; surf = surf->texturechain)
  {
    worldbatchsurface_t *batchsurf;
    int i;

    if (!R_WorldBatchSurface (surf))
      continue;

    batchsurf = &worldbatch_surfaces[surf - cl.worldmodel->surfaces];
    if (batchsurf->drawframe != r_framecount ||
      batchsurf->segment != segmentnum)
      continue;

    for (i = 1; i < batchsurf->numverts - 1; i++)
    {
      if (numindices + 3 > BATCH_DRAW_MAX_INDICES)
      {
        animated = R_TextureAnimation (texture);
        GL_Bind (animated->gl_texturenum);
        glDrawElements (GL_TRIANGLES, numindices, GL_UNSIGNED_SHORT,
          worldbatch_indices);
        numindices = 0;
      }
      worldbatch_indices[numindices++] = batchsurf->firstvert;
      worldbatch_indices[numindices++] = batchsurf->firstvert + i;
      worldbatch_indices[numindices++] = batchsurf->firstvert + i + 1;
    }
  }

  if (!numindices)
    return;

  animated = R_TextureAnimation (texture);
  GL_Bind (animated->gl_texturenum);
  glDrawElements (GL_TRIANGLES, numindices, GL_UNSIGNED_SHORT,
    worldbatch_indices);
  }
}

static void R_DrawWorldLightmapBatch (int lightmapnum, int segmentnum)
{
#ifdef MGL68K_RESIDENT
  if (worldbatch_resident && worldbatch_res_lm[segmentnum])
  {
    int surfnum = worldbatch_lightmap_heads[lightmapnum];
    int numindices = 0;

    if (!worldstatic_built)
      R_BuildWorldStatic ();
    if (worldstatic_ok)
    {
      const unsigned int *slot =
        &worldstatic_lm[(segmentnum * MAX_LIGHTMAPS + lightmapnum) * 2];
      int sn;
      for (sn = surfnum; sn >= 0; sn = worldbatch_surfaces[sn].lightmap_next)
        if (worldbatch_surfaces[sn].segment == segmentnum)
          break;
      if (sn < 0 || !slot[1])
        return;
      if (R_DrawStatic (worldbatch_res_lm[segmentnum], slot))
        return;
    }

    while (surfnum >= 0)
    {
      worldbatchsurface_t *batchsurf = &worldbatch_surfaces[surfnum];
      int i;

      if (batchsurf->segment == segmentnum)
        for (i = 1; i < batchsurf->numverts - 1; i++)
        {
          worldbatch_indices[numindices++] = batchsurf->firstvert;
          worldbatch_indices[numindices++] = batchsurf->firstvert + i;
          worldbatch_indices[numindices++] = batchsurf->firstvert + i + 1;
        }
      surfnum = batchsurf->lightmap_next;
    }
    if (!numindices)
      return;
    if (R_DrawResidentIndices (worldbatch_res_lm[segmentnum], numindices))
      return;
  }
#endif
#ifdef MGL_FASTPATH_VERSION
  /* Batch-level FastPath submission (atlas is already bound). */
  if (R_WorldFPBegin ())
  {
    int surfnum = worldbatch_lightmap_heads[lightmapnum];
    float *segmentverts = worldbatch_segments[segmentnum].verts;

    while (surfnum >= 0)
    {
      worldbatchsurface_t *batchsurf = &worldbatch_surfaces[surfnum];
      int i;
      float *v;

      if (batchsurf->segment != segmentnum)
      {
        surfnum = batchsurf->lightmap_next;
        continue;
      }

      v = segmentverts + batchsurf->firstvert * VERTEXSIZE;
      for (i = 1; i < batchsurf->numverts - 1; i++)
        if (!R_WorldFPTriangle (v, v + i * VERTEXSIZE,
            v + (i + 1) * VERTEXSIZE, 5))
        {
          R_WorldFPEnd ();
          return;
        }

      surfnum = batchsurf->lightmap_next;
    }

    R_WorldFPEnd ();
    return;
  }
#endif

  {
  int surfnum = worldbatch_lightmap_heads[lightmapnum];
  int numindices = 0;

  while (surfnum >= 0)
  {
    worldbatchsurface_t *batchsurf = &worldbatch_surfaces[surfnum];
    int i;

    if (batchsurf->segment != segmentnum)
    {
      surfnum = batchsurf->lightmap_next;
      continue;
    }

    for (i = 1; i < batchsurf->numverts - 1; i++)
    {
      if (numindices + 3 > BATCH_DRAW_MAX_INDICES)
      {
        glDrawElements (GL_TRIANGLES, numindices, GL_UNSIGNED_SHORT,
          worldbatch_indices);
        numindices = 0;
      }
      worldbatch_indices[numindices++] = batchsurf->firstvert;
      worldbatch_indices[numindices++] = batchsurf->firstvert + i;
      worldbatch_indices[numindices++] = batchsurf->firstvert + i + 1;
    }

    surfnum = batchsurf->lightmap_next;
  }

  if (numindices)
    glDrawElements (GL_TRIANGLES, numindices, GL_UNSIGNED_SHORT,
      worldbatch_indices);
  }
}

/*
============================================================================

Water / underwater-warp batching (renderer plan item 2)

EmitWaterPolys() and DrawGLWaterPoly()/DrawGLWaterPolyLightmap() submit one
glBegin/glEnd polygon per surface.  Water positions are static (only the
turbulent texture coordinates are recomputed each frame) while the
underwater warp displaces positions; both fit the same submission model as
the world batch: stage vertices into a small fixed array and draw indexed
triangles, one call per water texture or lightmap atlas.

The staging buffers are frame-transient and are overwritten every frame on
purpose.  The MiniGL R200 frontend re-fetches every client-array vertex on
each glDrawElements call (its index cache is generation-stamped per draw),
which is the same contract the alias-model array path already relies on.

Every draw stays below the frontend's MAX_VERTICES (4096) per-call limit.
A subdivided warp polygon is at most 60 vertices (SubdividePolygon), so one
polygon always fits after a flush; surfaces whose polygons do not fit are
left to the immediate paths.

============================================================================
*/

#define WATER_BATCH_STAGE_VERTS 1024

static float *waterbatch_stageverts;
static unsigned short *waterbatch_stageindices;
static int waterbatch_stagevertcount;
static int waterbatch_stageindexcount;

static qboolean R_WaterBatchReady (void)
{
  return r_worldbatch.value && gl_texsort.value && !FAKE_MULTITEXTURE_VALUE &&
    waterbatch_stageverts != NULL && waterbatch_stageindices != NULL;
}

static qboolean R_WaterBatchPolySizeOk (glpoly_t *p)
{
  return p->numverts >= 3 && p->numverts <= WATER_BATCH_STAGE_VERTS;
}

static qboolean R_WaterBatchPolySizesOk (msurface_t *surf)
{
  glpoly_t *p;

  if (!surf->polys)
    return false;
  for (p = surf->polys; p; p = p->next)
    if (!R_WaterBatchPolySizeOk (p))
      return false;
  return true;
}

static qboolean R_WaterBatchTurbSurface (msurface_t *surf)
{
  if (!R_WaterBatchReady ())
    return false;
  if ((surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY)) != SURF_DRAWTURB)
    return false;
  return R_WaterBatchPolySizesOk (surf);
}

static qboolean R_WaterBatchUnderwaterSurface (msurface_t *surf)
{
  if (!R_WaterBatchReady () || !r_dowarp)
    return false;
  if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
    return false;
  if (!(surf->flags & SURF_UNDERWATER))
    return false;
  return R_WaterBatchPolySizesOk (surf);
}

static qboolean R_WaterBatchSurface (msurface_t *surf)
{
  return R_WaterBatchTurbSurface (surf) ||
    R_WaterBatchUnderwaterSurface (surf);
}

static void R_WaterBatchReset (void)
{
  waterbatch_stagevertcount = 0;
  waterbatch_stageindexcount = 0;
}

static void R_EnableWaterBatchArrays (int texcoord)
{
  glDisableClientState (GL_COLOR_ARRAY);
  glEnableClientState (GL_VERTEX_ARRAY);
  glEnableClientState (GL_TEXTURE_COORD_ARRAY);
  glVertexPointer (3, GL_FLOAT, VERTEXSIZE * sizeof(float),
    waterbatch_stageverts);
  glTexCoordPointer (2, GL_FLOAT, VERTEXSIZE * sizeof(float),
    waterbatch_stageverts + texcoord);
}

static void R_WaterBatchDrawArrays (int texcoord)
{
  if (!waterbatch_stageindexcount)
  {
    R_WaterBatchReset ();
    return;
  }

  R_EnableWaterBatchArrays (texcoord);
  glDrawElements (GL_TRIANGLES, waterbatch_stageindexcount,
    GL_UNSIGNED_SHORT, waterbatch_stageindices);
  R_DisableWorldBatchArrays ();
  R_WaterBatchReset ();
}

/*
 * Makes room for one polygon, flushing the staged batch when needed.  The
 * flush uses the incoming polygon's texture-coordinate slot: every staging
 * session uses exactly one slot (turbulent water 5, underwater base 3,
 * underwater lightmap 5), so the already-staged vertices share it.
 */
static void R_WaterBatchRoom (int numverts, int texcoord)
{
  if (waterbatch_stagevertcount + numverts > WATER_BATCH_STAGE_VERTS ||
    waterbatch_stageindexcount + (numverts - 2) * 3 > BATCH_DRAW_MAX_INDICES)
    R_WaterBatchDrawArrays (texcoord);
}

static void R_WaterBatchAddIndices (int firstvert, int numverts)
{
  int i;

  for (i = 1; i < numverts - 1; i++)
  {
    waterbatch_stageindices[waterbatch_stageindexcount++] = firstvert;
    waterbatch_stageindices[waterbatch_stageindexcount++] = firstvert + i;
    waterbatch_stageindices[waterbatch_stageindexcount++] = firstvert + i + 1;
  }
}

static void R_StageTurbPoly (glpoly_t *p, int texcoord)
{
  int firstvert;

  R_WaterBatchRoom (p->numverts, texcoord);
  firstvert = waterbatch_stagevertcount;
  R_StageWaterPoly (p, waterbatch_stageverts + firstvert * VERTEXSIZE);
  waterbatch_stagevertcount += p->numverts;
  R_WaterBatchAddIndices (firstvert, p->numverts);
}

static void R_StageTurbSurface (msurface_t *surf, int texcoord)
{
  glpoly_t *p;

  for (p = surf->polys; p; p = p->next)
    R_StageTurbPoly (p, texcoord);
}

static void R_WarpWaterPosition (const float *v, float *out)
{
  out[0] = v[0] + 8*sin(v[1]*0.05+realtime)*sin(v[2]*0.05+realtime);
  out[1] = v[1] + 8*sin(v[0]*0.05+realtime)*sin(v[2]*0.05+realtime);
  out[2] = v[2];
}

static void R_StageUnderwaterPoly (glpoly_t *p, int texcoord)
{
  int firstvert, i;
  float *v, *dst;

  R_WaterBatchRoom (p->numverts, texcoord);
  firstvert = waterbatch_stagevertcount;
  dst = waterbatch_stageverts + firstvert * VERTEXSIZE;
  for (i = 0, v = p->verts[0]; i < p->numverts;
    i++, v += VERTEXSIZE, dst += VERTEXSIZE)
  {
    R_WarpWaterPosition (v, dst);
    dst[3] = v[3];
    dst[4] = v[4];
    dst[5] = v[5];
    dst[6] = v[6];
  }
  waterbatch_stagevertcount += p->numverts;
  R_WaterBatchAddIndices (firstvert, p->numverts);
}

static void R_StageUnderwaterSurface (msurface_t *surf, int texcoord)
{
  glpoly_t *p;

  for (p = surf->polys; p; p = p->next)
    R_StageUnderwaterPoly (p, texcoord);
}

static void R_ChainBrushLightmap (msurface_t *fa);

/*
 * Stages and draws the turbulent surfaces of one texture chain.  count_polys
 * keeps the brush-poly statistic counting when this replaces R_RenderBrushPoly
 * in the texture-sorted world pass; emit_fallback draws surfaces that do not
 * fit the staging limits immediately (the translucent water pass has no other
 * per-surface path).
 */
static void R_DrawTurbChain (msurface_t *chain, int gl_texturenum,
  qboolean count_polys, qboolean emit_fallback)
{
  msurface_t *surf;
  qboolean any = false;

  if (mtexenabled)
    GL_DisableMultitexture ();

  GL_Bind (gl_texturenum);

  for (surf = chain; surf; surf = surf->texturechain)
    if (R_WaterBatchTurbSurface (surf))
    {
      if (count_polys)
        c_brush_polys++;
      R_StageTurbSurface (surf, 5);
      any = true;
    }
    else if (emit_fallback)
      EmitWaterPolys (surf);

  if (any)
    R_WaterBatchDrawArrays (5);
}

/*
 * Batched replacement for the R_RenderBrushPoly() calls in the
 * texture-sorted world pass: turbulent water (warped texture coordinates)
 * and underwater-warped solid faces (warped positions, base texture
 * coordinates in slots 3/4).  Underwater surfaces still chain into
 * lightmap_polys for the R_BlendLightmaps() pass.
 */
static void R_DrawWaterBatchChain (texture_t *texture, msurface_t *chain)
{
  msurface_t *surf;
  qboolean any;

  R_DrawTurbChain (chain, R_TextureAnimation (texture)->gl_texturenum,
    true, false);

  any = false;
  if (mtexenabled)
    GL_DisableMultitexture ();
  GL_Bind (R_TextureAnimation (texture)->gl_texturenum);

  for (surf = chain; surf; surf = surf->texturechain)
    if (R_WaterBatchUnderwaterSurface (surf))
    {
      c_brush_polys++;
      R_StageUnderwaterSurface (surf, 3);
      R_ChainBrushLightmap (surf);
      any = true;
    }

  if (any)
    R_WaterBatchDrawArrays (3);
}

static qboolean R_ChainHasWaterBatch (msurface_t *chain)
{
  msurface_t *surf;

  for (surf = chain; surf; surf = surf->texturechain)
    if (R_WaterBatchSurface (surf))
      return true;
  return false;
}

/*
===============
R_AddDynamicLights
===============
*/

void R_AddDynamicLights (msurface_t *surf)
{
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	int			smax, tmax;
	mtexinfo_t	*tex;
 
#ifdef LITFILES
 // LordHavoc: .lit support begin
         float           cred, cgreen, cblue, brightness;
         unsigned        *bl;
 // LordHavoc: .lit support end
#endif

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if ( !(surf->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		rad = cl_dlights[lnum].radius;
		dist = DotProduct (cl_dlights[lnum].origin, surf->plane->normal) -
				surf->plane->dist;
		rad -= fabs(dist);
		minlight = cl_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		for (i=0 ; i<3 ; i++)
		{
			impact[i] = cl_dlights[lnum].origin[i] -
					surf->plane->normal[i]*dist;
		}

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1]; 

#ifdef LITFILES
		if(eyecandy)
		{
// LordHavoc: .lit support begin
                 bl = blocklights;

                 cred = cl_dlights[lnum].color[0] * 256.0f;
                 cgreen = cl_dlights[lnum].color[1] * 256.0f;
                 cblue = cl_dlights[lnum].color[2] * 256.0f;

// LordHavoc: .lit support end

		for (t = 0 ; t<tmax ; t++)
		{
			td = local[1] - t*16;
			if (td < 0)
				td = -td;
			for (s=0 ; s<smax ; s++)
			{
				sd = local[0] - s*16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td>>1);
				else
					dist = td + (sd>>1);

				if (dist < minlight)
				{  
                              brightness = rad - dist;

                              bl[0] += (brightness * cred);
                              bl[1] += (brightness * cgreen);
                              bl[2] += (brightness * cblue);
                              }
                              bl += 3;
			}
		}

		}
		else
		{
#endif
		for (t = 0 ; t<tmax ; t++)
		{
			td = local[1] - t*16;
			if (td < 0)
				td = -td;
			for (s=0 ; s<smax ; s++)
			{
				sd = local[0] - s*16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td>>1);
				else
					dist = td + (sd>>1);

				if (dist < minlight)
				{
				blocklights[t*smax + s] += (rad - dist)*256; 
				}
			}
		}
#ifdef LITFILES
		}
#endif
	}
}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/

#ifdef LITFILES

void R_BuildLightMapColor (msurface_t *surf, byte *dest, int stride)
{
         int                     smax, tmax;
         int                     t;
         int                     i, j, size;
         byte            *lightmap;
         unsigned        scale;
         int                     maps;
         int                     lightadj[4];
         unsigned        *bl;
	   unsigned r,g,b; //surgeon
	   UWORD *short_dest; //surgeon

         surf->cached_dlight = (surf->dlightframe == r_framecount);

         smax = (surf->extents[0]>>4)+1;
         tmax = (surf->extents[1]>>4)+1;
         size = smax*tmax;
         lightmap = surf->samples;

// set to full bright if no light data
	if (r_fullbright.value || !cl.worldmodel->lightdata)
	{
		bl = blocklights;
		for (i=0 ; i<size ; i++)
		{
			*bl++ = 255*256;
			*bl++ = 255*256;
			*bl++ = 255*256;
		}
		goto store;
	}

// clear to no light
		bl = blocklights;
		for (i=0 ; i<size ; i++)
		{
			*bl++ = 0;
			*bl++ = 0;
			*bl++ = 0;
		}

 
// add all the lightmaps 
	if (lightmap) 
	{ 
		for (maps = 0;maps < MAXLIGHTMAPS && surf->styles[maps] != 255;maps++) 
		{ 
			scale = d_lightstylevalue[surf->styles[maps]]; 
			surf->cached_light[maps] = scale;	// 8.8 fraction 
			bl = blocklights;
			for (i=0; i<size; i++)
			{ 
				*bl++ += *lightmap++ * scale;
				*bl++ += *lightmap++ * scale;
				*bl++ += *lightmap++ * scale;
			}
		} 
	} 

	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
             	R_AddDynamicLights (surf);

 // bound, invert, and shift

 store:
         switch (gl_lightmap_format)
         {
         case GL_RGBA:
                 stride -= (smax << 2);
                 bl = blocklights;
		   for (i=0 ; i<tmax ; i++, dest += stride)
		   {
			for (j=0 ; j<smax ; j++)
			{
			t = *bl++ >> 7;	if (t > 255) t = 255;
			*dest++ = t;
			t = *bl++ >> 7;	if (t > 255) t = 255;
			*dest++ = t;
			t = *bl++ >> 7;	if (t > 255) t = 255;
			*dest++ = t;
			*dest++ = 255;
			}
                 // LordHavoc: .lit support end
                 }
         break;
         case GL_ALPHA:
         case GL_LUMINANCE:
                 bl = blocklights;
		   for (i=0 ; i<tmax ; i++, dest += stride)
		   {
			for (j=0 ; j<smax ; j++)
			{
                 	t = ((bl[0] + bl[1] + bl[2]) * 85) >> 15; // LordHavoc: basically / 3, but faster and combined with >> 7 shift down, note: actual number would be 85.3333...
                                 bl += 3;
                                 // LordHavoc: .lit support end
                                 if (t > 255)
                                         t = 255;
                                 dest[j] = t;
			}
                 }
         break;

	   case MGL_UNSIGNED_SHORT_4_4_4_4: //by Surgeon
		bl = blocklights;
              stride -= (smax << 1);
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
                 	r = *bl++ >> 7; if (r > 255) r = 255;
                 	g = *bl++ >> 7; if (g > 255) g = 255;
                 	b = *bl++ >> 7; if (b > 255) b = 255;

			short_dest = (UWORD*)dest;
			*short_dest = ((0xf0<<8) | ((r & 0xf0) << 4) | (g&0xf0) | ((b&0xf0)>>4));
			dest += 2;
	      		}
		}
	   break;

	   case MGL_UNSIGNED_SHORT_5_6_5: //by Surgeon
		bl = blocklights;
              stride -= (smax << 1);
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
                 	r = *bl++ >> 7; if (r > 255) r = 255;
                 	g = *bl++ >> 7; if (g > 255) g = 255;
                 	b = *bl++ >> 7; if (b > 255) b = 255;

			short_dest = (UWORD*)dest;
			*short_dest = ( (UWORD)(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | ((b & 0xf8) >> 3)) );
			dest += 2;
	      		}
		}
	   break;

         default:
                 Sys_Error ("Bad lightmap format");
         }
}

#endif

void R_BuildLightMap (msurface_t *surf, byte *dest, int stride)
{
	int			smax, tmax;
	int			t;
	int			i, j, size;
	byte		*lightmap;
	unsigned	scale;
	int			maps;
	int			lightadj[4];
	unsigned	*bl;
	UWORD *short_dest; //surgeon

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	lightmap = surf->samples;

// set to full bright if no light data
	if (r_fullbright.value || !cl.worldmodel->lightdata)
	{
		for (i=0 ; i<size ; i++)
			blocklights[i] = 255*256;				goto store;
	}

// clear to no light
	for (i=0 ; i<size ; i++)
		blocklights[i] = 0;

// add all the lightmaps

	if (lightmap)
		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
		{
			scale = d_lightstylevalue[surf->styles[maps]];
			surf->cached_light[maps] = scale;	// 8.8 fraction 
			for (i=0 ; i<size ; i++)					blocklights[i] += lightmap[i] * scale; 
			lightmap += size;	// skip to next lightmap 
		}

// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights (surf);

// bound, invert, and shift
store:
	switch (gl_lightmap_format)
	{
	case GL_RGBA:
		stride -= (smax<<2);
		bl = blocklights;
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{ 
				t = *bl++; 
				t >>= 7;
				if (t > 255)
				t = 0;
				else
				t = 255-t;

				dest[3] = t;
				dest += 4; 
			}
		}
		break;
	case GL_ALPHA:
	case GL_LUMINANCE:
		bl = blocklights;
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				t = *bl++;
				t >>= 7; 

				if (t > 255)
				t = 255;
				dest[j] = t;
			}
		}
		break;

	 case MGL_UNSIGNED_SHORT_4_4_4_4: //by Surgeon
		stride -= (smax<<1);
		bl = blocklights;
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
	        		t = *bl++;
	        		t >>= 7;

	        		if (t > 255)
		  		t = 0;
				else 	  
		  		t = (255-t) & 0xf0;

		  		dest[0] = t; 	  

		  		dest += 2;
	      		}
		}
		break;
	 case MGL_UNSIGNED_SHORT_5_6_5: //by Surgeon
		stride -= (smax<<1);
		bl = blocklights;
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
	        		t = *bl++;
	        		t >>= 7;

	        		if (t > 255)
		  		t = 255;

				short_dest = (UWORD*)dest;
				*short_dest = ( (UWORD)(((t & 0xf8) << 8) | ((t & 0xfc) << 3) | ((t & 0xf8) >> 3)) );

		  		dest += 2;
	      		}
		}
		break;

	default:
		Sys_Error ("Bad lightmap format");
	}
}


/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation (texture_t *base)
{
  int   reletive;
  int   count;

  if (currententity->frame)
  {
    if (base->alternate_anims)
      base = base->alternate_anims;
  }

  if (!base->anim_total)
    return base;

  reletive = (int)(cl.time*10) % base->anim_total;

  count = 0;  
  while (base->anim_min > reletive || base->anim_max <= reletive)
  {
    base = base->anim_next;
    if (!base)
      Sys_Error ("R_TextureAnimation: broken cycle");
    if (++count > 100)
      Sys_Error ("R_TextureAnimation: infinite cycle");
  }

  return base;
}


/*
=============================================================

  BRUSH MODELS

=============================================================
*/


extern  int   solidskytexture;
extern  int   alphaskytexture;
extern  float speedscale;   // for top sky and bottom sky

void DrawGLWaterPoly (glpoly_t *p);
void DrawGLWaterPolyLightmap (glpoly_t *p);


qboolean mtexenabled = false;

void GL_DisableMultitexture(void) 
{
    glDisable(GL_TEXTURE_2D);
    GL_SelectTexture(GL_TEXTURE0_ARB);
    mtexenabled = false;
}

void GL_EnableMultitexture(void) 
{
    GL_SelectTexture(GL_TEXTURE1_ARB);
    glEnable(GL_TEXTURE_2D);
    mtexenabled = true;
}

/*
================
R_DrawSequentialPoly

Systems that have fast state and texture changes can
just do everything as it passes with no need to sort
================
*/
void R_DrawSequentialPoly (msurface_t *s)
{
  glpoly_t  *p;
  float   *v;
  int     i;
  texture_t *t;
  vec3_t    nv, dir;
  float   ss, ss2, length;
  float   s1, t1;
  glRect_t  *theRect;


  //
  // normal lightmaped poly
  //


if (! (s->flags & (SURF_DRAWSKY|SURF_DRAWTURB|(r_dowarp ? SURF_UNDERWATER : 0)) ) ) 

  {
    R_RenderDynamicLightmaps (s);
    if (gl_mtexable)
    {
      p = s->polys;
      if(s->texinfo->texture->anim_total) t = R_TextureAnimation (s->texinfo->texture);
	else t = s->texinfo->texture;

      // Binds world to texture env 0:

      GL_SelectTexture(GL_TEXTURE0_ARB);
      GL_Bind (t->gl_texturenum);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

      // Binds lightmap to texenv 1:

      GL_EnableMultitexture();
      GL_Bind (lightmap_textures + s->lightmaptexturenum);
      i = s->lightmaptexturenum;

      if (lightmap_modified[i])
      {
        lightmap_modified[i] = false;
        theRect = &lightmap_rectchange[i];
        {
          unsigned long fp_t = FP_Enter (FP_LMUPLOAD);
          R_UploadLightmapRect (i, theRect);
          FP_ExitLMUpload (fp_t, i, theRect->t, theRect->h,
                          theRect->h * BLOCK_WIDTH * lightmap_bytes, 0);
        }
        FP_CountLMUpload (theRect->h * BLOCK_WIDTH * lightmap_bytes);
        theRect->l = BLOCK_WIDTH;
        theRect->t = BLOCK_HEIGHT;
        theRect->h = 0;
        theRect->w = 0;
      }

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);  //unit 1

      glBegin(GL_POLYGON);
      v = p->verts[0];
      for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
      {
 	glMultiTexCoord2fvARB (GL_TEXTURE0_ARB, (v+3));
	glMultiTexCoord2fvARB (GL_TEXTURE1_ARB, (v+5));
      glVertex3f (v[0], v[1], v[2]);
      }
      glEnd ();
      return;
    }
    else
    {
      p = s->polys;

      if(s->texinfo->texture->anim_total) t = R_TextureAnimation (s->texinfo->texture);
	else t = s->texinfo->texture;

      GL_Bind (t->gl_texturenum);

#ifndef AMIGA

      glBegin (GL_POLYGON);
	v = p->verts[0];
      for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
      {
        glTexCoord2f (v[3], v[4]);
	  glVertex3f (v[0], v[1], v[2]);
      }
      glEnd ();

      GL_Bind (lightmap_textures + s->lightmaptexturenum);

      glEnable (GL_BLEND);

      glBegin (GL_POLYGON);
      v = p->verts[0];
      for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
      {
        glTexCoord2f (v[5], v[6]);
        glVertex3f (v[0], v[1], v[2]);
      }
      glEnd ();

      glDisable (GL_BLEND);

#else


// fake single-pass multitexturing:

      if(FAKE_MULTITEXTURE_VALUE)
	{
	GL_SelectTexture(GL_TEXTURE1_ARB);
	glEnable(GL_TEXTURE_2D);
	GL_Bind(lightmap_textures + s->lightmaptexturenum);

	glBegin(GL_POLYGON);
	  mglMtexTV23fv(mini_CurrentContext, VERTEXSIZE, p);
	glEnd();

	glDisable(GL_TEXTURE_2D);
	GL_SelectTexture(GL_TEXTURE0_ARB);
	}
      else
	{

	glBegin(GL_POLYGON);
	mglTV23fv(mini_CurrentContext, VERTEXSIZE, p);
	glEnd();

      GL_Bind (lightmap_textures + s->lightmaptexturenum);

      glEnable (GL_BLEND);

	glBegin(GL_POLYGON);
	mglTV23fvLM(mini_CurrentContext, VERTEXSIZE, p);
	glEnd();

      glDisable (GL_BLEND);
	}

#endif

    }
    return;
  }

  //
  // subdivided water surface warp
  //

  if (s->flags & SURF_DRAWTURB)
  {
    if(mtexenabled) GL_DisableMultitexture();
    GL_Bind (s->texinfo->texture->gl_texturenum);
    EmitWaterPolys (s);
    return;
  }

  //
  // subdivided sky warp
  //
  if (s->flags & SURF_DRAWSKY)
  {
    if(mtexenabled) GL_DisableMultitexture();

    GL_Bind (solidskytexture);
    speedscale = realtime*8;
    speedscale -= (int)speedscale & ~127;

    EmitSkyPolys (s);

    glEnable (GL_BLEND);
    GL_Bind (alphaskytexture);
    speedscale = realtime*16;
    speedscale -= (int)speedscale & ~127;

    EmitSkyPolys (s);

    glDisable (GL_BLEND);
    return;
  }

  //
  // underwater warped with lightmap
  //

  R_RenderDynamicLightmaps (s);

  if (gl_mtexable)
  {
    p = s->polys;

 t = R_TextureAnimation (s->texinfo->texture);

    GL_SelectTexture(GL_TEXTURE0_ARB);

    GL_Bind (t->gl_texturenum);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    GL_EnableMultitexture();
    GL_Bind (lightmap_textures + s->lightmaptexturenum);

    i = s->lightmaptexturenum;
    if (lightmap_modified[i])
    {
      lightmap_modified[i] = false;
      theRect = &lightmap_rectchange[i];
      {
        unsigned long fp_t = FP_Enter (FP_LMUPLOAD);
        R_UploadLightmapRect (i, theRect);
        FP_ExitLMUpload (fp_t, i, theRect->t, theRect->h,
                        theRect->h * BLOCK_WIDTH * lightmap_bytes, 1);
      }
      FP_CountLMUpload (theRect->h * BLOCK_WIDTH * lightmap_bytes);
      theRect->l = BLOCK_WIDTH;
      theRect->t = BLOCK_HEIGHT;
      theRect->h = 0;
      theRect->w = 0;
    }

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glBegin (GL_TRIANGLE_FAN);
    v = p->verts[0];
    for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
    {
	glMultiTexCoord2fvARB (GL_TEXTURE0_ARB, (v+3));
	glMultiTexCoord2fvARB (GL_TEXTURE1_ARB, (v+5));

      nv[0] = v[0] + 8*sin(v[1]*0.05+realtime)*sin(v[2]*0.05+realtime);
      nv[1] = v[1] + 8*sin(v[0]*0.05+realtime)*sin(v[2]*0.05+realtime);
      nv[2] = v[2];

      glVertex3fv (nv);
    }
    glEnd ();

  } else {
    p = s->polys;

t = R_TextureAnimation (s->texinfo->texture);

    GL_Bind (t->gl_texturenum);

    DrawGLWaterPoly (p);

    GL_Bind (lightmap_textures + s->lightmaptexturenum);
    glEnable (GL_BLEND);

    DrawGLWaterPolyLightmap (p);

    glDisable (GL_BLEND);
    }
}


/*
================
DrawGLWaterPoly

Warp the vertex coordinates
================
*/
void DrawGLWaterPoly (glpoly_t *p)
{
  int   i;
  float *v;
  float s, t, os, ot;
  vec3_t  nv;

  if(mtexenabled) GL_DisableMultitexture();

  glBegin (GL_TRIANGLE_FAN);
  v = p->verts[0];
  for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
  {
    glTexCoord2f (v[3], v[4]);

    R_WarpWaterPosition (v, nv);

    glVertex3fv (nv);
  }
  glEnd ();
}

void DrawGLWaterPolyLightmap (glpoly_t *p)
{
  int   i;
  float *v;
  float s, t, os, ot;
  vec3_t  nv;

  if(mtexenabled) GL_DisableMultitexture();

  glBegin (GL_TRIANGLE_FAN);
  v = p->verts[0];
  for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
  {
    glTexCoord2f (v[5], v[6]);

    R_WarpWaterPosition (v, nv);

    glVertex3fv (nv);

  }
  glEnd ();
}

/*
================
DrawGLPoly
================
*/

void DrawGLPoly (glpoly_t *p)
{
  register int   i;
  register float *v;

  glBegin (GL_POLYGON);
  v = p->verts[0];
  for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
  {
    glTexCoord2f (v[3], v[4]);
    glVertex3f (v[0], v[1], v[2]);
  }
  glEnd ();

}


#ifdef AMIGA

static void R_DrawMultitextureBuffer(void)
{
	GLenum src, dst;

	switch(gl_lightmap_format)
	{
		case GL_ALPHA:
			src = GL_ZERO;
			dst = GL_SRC_ALPHA;
			break;

		case GL_LUMINANCE:
		case MGL_UNSIGNED_SHORT_5_6_5:
			src = GL_ZERO;
			dst = GL_SRC_COLOR;
			break;

		default:
#ifdef LITFILES
			if(eyecandy)
			{
				src = GL_ZERO;
				dst = GL_SRC_COLOR;
			}
			else
			{
#endif
				src = GL_SRC_ALPHA;
				dst = GL_ONE_MINUS_SRC_ALPHA;
#ifdef LITFILES
			}
#endif
			break;
	}

	mglDrawMultitexBuffer(src, dst, GL_REPLACE);
}

#endif

/*
================
R_BlendLightmaps
================
*/


void R_BlendLightmaps (void)
{
  int     i, j;
  glpoly_t  *p;
  float   *v;
  glRect_t  *theRect;
  int fake_multitexture;
  qboolean draw_underwater_batches = R_WaterBatchReady () && r_dowarp;

  if (r_fullbright.value)
    return;

  if (!gl_texsort.value)
    return;

  fake_multitexture = FAKE_MULTITEXTURE_VALUE;

  if(fake_multitexture == 2)
  {
	return;
  }

  if(!fake_multitexture)
  {
  glDepthMask (0);    // don't bother writing Z

#ifdef LITFILES
  if(eyecandy)
  {
	if (gl_lightmap_format == GL_ALPHA)
	{
		glBlendFunc (GL_ZERO, GL_SRC_ALPHA);
	}
	else
	{
		glBlendFunc (GL_ZERO, GL_SRC_COLOR);
	} 
  }
  else
  {
#endif
	if (gl_lightmap_format == GL_LUMINANCE || gl_lightmap_format == MGL_UNSIGNED_SHORT_5_6_5)
	{
		glBlendFunc (GL_ZERO, GL_SRC_COLOR);
	}
	else if (gl_lightmap_format == GL_ALPHA)
	{
		glBlendFunc (GL_ZERO, GL_SRC_ALPHA);
	}
#ifdef LITFILES
  }
#endif

    if (!r_lightmap.value)
    {
    glEnable (GL_BLEND);
    }

  glEnable(MGL_Z_OFFSET);
  }

  /* The immediate DrawGLWaterPolyLightmap() disables multitexture itself;
     do it once here for the staged path so the lightmap binds below land on
     unit 0 before any staging flush. */
  if (draw_underwater_batches && mtexenabled)
    GL_DisableMultitexture ();

#ifdef MGL68K_RESIDENT
  /* atlas: every dirty block up first, then the plain polygons of all
   * blocks as ring draws from the atlas (underwater ones stay below) */
  if (lma_active && !fake_multitexture)
  {
    static glpoly_t *apolys[RING_POLY_MAX_VERTS / 3];
    static unsigned char ablocks[RING_POLY_MAX_VERTS / 3];
    int n = 0, nverts = 0;
    qboolean failed = false;

    for (i = 0; i < MAX_LIGHTMAPS; i++)
      if (lightmap_modified[i])
      {
        lightmap_modified[i] = false;
        theRect = &lightmap_rectchange[i];
        R_UploadLightmapRect (i, theRect);
        theRect->l = BLOCK_WIDTH;
        theRect->t = BLOCK_HEIGHT;
        theRect->h = 0;
        theRect->w = 0;
      }
    GL_Bind (LMA_TEX);
    for (i = 0; i < MAX_LIGHTMAPS; i++)
    {
      glpoly_t *q, *rest = NULL, *next;
      for (q = lightmap_polys[i]; q; q = next)
      {
        next = q->chain;
        if (((q->flags & SURF_UNDERWATER) && r_dowarp) || failed ||
            q->numverts > RING_POLY_MAX_VERTS)
        {
          q->chain = rest;
          rest = q;
          continue;
        }
        if (n == RING_POLY_MAX_VERTS / 3 ||
            nverts + q->numverts > RING_POLY_MAX_VERTS)
        {
          if (!R_RingDrawPolysB (apolys, n, 5, ablocks))
          {
            int k;
            failed = true;
            for (k = 0; k < n; k++)
            {
              if (ablocks[k] == i)
              {
                apolys[k]->chain = rest;
                rest = apolys[k];
              }
              else
              {
                apolys[k]->chain = lightmap_polys[ablocks[k]];
                lightmap_polys[ablocks[k]] = apolys[k];
              }
            }
            q->chain = rest;
            rest = q;
            n = nverts = 0;
            continue;
          }
          n = nverts = 0;
        }
        apolys[n] = q;
        ablocks[n++] = (unsigned char)i;
        nverts += q->numverts;
      }
      lightmap_polys[i] = rest;
    }
    if (bmd_lmpending)
    {
      /* deferred brush entities: their lightmap polygons, moved by each
       * entity's origin */
      static float aorg[RING_POLY_MAX_VERTS / 3][3];
      int k, m = 0, mv = 0;
      if (n && R_RingDrawPolysB (apolys, n, 5, ablocks))
        n = nverts = 0;
      for (k = 0; k < bmd_count; k++)
      {
        glpoly_t *q = bmd_surf[k]->polys;
        if (m == RING_POLY_MAX_VERTS / 3 ||
            mv + q->numverts > RING_POLY_MAX_VERTS)
        {
          (void)R_RingDrawPolysO (apolys, m, 5, ablocks, aorg);
          m = mv = 0;
        }
        apolys[m] = q;
        ablocks[m] = (unsigned char)bmd_surf[k]->lightmaptexturenum;
        VectorCopy (bmd_org[k], aorg[m]);
        m++;
        mv += q->numverts;
      }
      if (m)
        (void)R_RingDrawPolysO (apolys, m, 5, ablocks, aorg);
    }
    if (n && !R_RingDrawPolysB (apolys, n, 5, ablocks))
    {
      /* not drawn: hand them back to their blocks */
      int k;
      for (k = 0; k < n; k++)
      {
        apolys[k]->chain = lightmap_polys[ablocks[k]];
        lightmap_polys[ablocks[k]] = apolys[k];
      }
    }
  }
#endif

  for (i=0 ; i<MAX_LIGHTMAPS ; i++)
  {
    qboolean draw_world_batch = worldbatch_blending_world &&
      worldbatch_lightmap_heads[i] >= 0;

    p = lightmap_polys[i];
    if (!p && !draw_world_batch)
      continue;

    GL_Bind(lightmap_textures+i);
    if (lightmap_modified[i])
    {
      lightmap_modified[i] = false;
      theRect = &lightmap_rectchange[i];
      {
        unsigned long fp_t = FP_Enter (FP_LMUPLOAD);
        R_UploadLightmapRect (i, theRect);
        FP_ExitLMUpload (fp_t, i, theRect->t, theRect->h,
                        theRect->h * BLOCK_WIDTH * lightmap_bytes, 2);
      }
      FP_CountLMUpload (theRect->h * BLOCK_WIDTH * lightmap_bytes);
      theRect->l = BLOCK_WIDTH;
      theRect->t = BLOCK_HEIGHT;
      theRect->h = 0;
      theRect->w = 0;
    }

if(fake_multitexture)
	continue;

#ifdef MGL68K_RESIDENT
    /* minigl68k: the plain polygons of this lightmap as one ring draw */
    if (R_ResidentExt ())
    {
      static glpoly_t *lmpolys[RING_POLY_MAX_VERTS / 3];
      int n = 0, nverts = 0;
      glpoly_t *q;

      for (q = p; q; q = q->chain)
        if (!((q->flags & SURF_UNDERWATER) && r_dowarp))
        {
          if (n == RING_POLY_MAX_VERTS / 3 ||
              nverts + q->numverts > RING_POLY_MAX_VERTS)
            break;
          lmpolys[n++] = q;
          nverts += q->numverts;
        }
      if (!q && n && R_RingDrawPolys (lmpolys, n, 5))
      {
        /* only underwater polys remain for the loop below */
        glpoly_t *rest = NULL, *next;
        for (q = p; q; q = next)
        {
          next = q->chain;
          if ((q->flags & SURF_UNDERWATER) && r_dowarp)
          {
            q->chain = rest;
            rest = q;
          }
        }
        p = rest;
      }
    }
#endif
    for ( ; p ; p=p->chain)
    {

	if ((p->flags & SURF_UNDERWATER) && r_dowarp)
        {
          if (draw_underwater_batches && R_WaterBatchPolySizeOk (p))
            R_StageUnderwaterPoly (p, 5);
          else
            DrawGLWaterPolyLightmap (p);
        }
      else
      {
#ifndef AMIGA
        glBegin (GL_POLYGON);
        v = p->verts[0];
        for (j=0 ; j<p->numverts ; j++, v+= VERTEXSIZE)
        {
          glTexCoord2f (v[5], v[5]);
	    glVertex3f (v[0], v[1], v[2]);
        }
        glEnd ();
#else
	glBegin(GL_POLYGON);
	mglTV23fvLM(mini_CurrentContext, VERTEXSIZE, p);
	glEnd();
#endif
      }
    }

    if (draw_underwater_batches)
      R_WaterBatchDrawArrays (5);
  }

#ifdef MGL68K_RESIDENT
  /* atlas: the whole world's lightmap pass, one static draw a segment */
  if (worldbatch_blending_world && worldbatch_surfaces && lma_active &&
      worldbatch_resident)
  {
    int segmentnum, any = 0;
    if (!worldstatic_built)
      R_BuildWorldStatic ();
    for (i = 0; i < MAX_LIGHTMAPS && !any; i++)
      any = worldbatch_lightmap_heads[i] >= 0;
    if (worldstatic_ok && any)
    {
      qboolean ok = true;
      GL_Bind (LMA_TEX);
      for (segmentnum = 0; segmentnum < worldbatch_numsegments; segmentnum++)
        if (worldstatic_lmall[segmentnum][1] &&
            !R_DrawStatic (worldbatch_res_lm[segmentnum],
              worldstatic_lmall[segmentnum]))
          ok = false;
      if (ok)
        worldbatch_blending_world = false;   /* done */
    }
  }
#endif
  if (worldbatch_blending_world && worldbatch_surfaces)
  {
    int segmentnum;

    for (segmentnum = 0; segmentnum < worldbatch_numsegments; segmentnum++)
    {
      R_EnableWorldBatchArrays (&worldbatch_segments[segmentnum], 5);
      for (i = 0; i < MAX_LIGHTMAPS; i++)
        if (worldbatch_lightmap_heads[i] >= 0)
        {
          GL_Bind (lightmap_textures + i);
          R_DrawWorldLightmapBatch (i, segmentnum);
        }
    }
    R_DisableWorldBatchArrays ();
  }

if(fake_multitexture)
{
	return;
}

  glDisable(MGL_Z_OFFSET);
  glDisable (GL_BLEND);

#ifdef LITFILES

  if(eyecandy)
  {
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }
  else
  {
  if (gl_lightmap_format != GL_RGBA && gl_lightmap_format != MGL_UNSIGNED_SHORT_4_4_4_4)
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }

#else

  if (gl_lightmap_format == GL_LUMINANCE || gl_lightmap_format == GL_ALPHA || gl_lightmap_format == MGL_UNSIGNED_SHORT_5_6_5)
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#endif

  glDepthMask (1);    // back to normal Z buffering
}



/*
================
R_RenderBrushPoly
================
*/
/*
===============
R_ChainBrushLightmap

Adds a brush polygon to its lightmap chain and rebuilds the atlas rectangle
when a light style changed or a dynamic light touched the surface.  Shared
by the immediate R_RenderBrushPoly path and the batched underwater base path.
===============
*/
#ifdef MGL68K_RESIDENT
static qboolean bm_nochain;   /* deferred brush entities: update only */
#endif

static void R_ChainBrushLightmap (msurface_t *fa)
{
  byte    *base;
  register int     maps;
  register glRect_t    *theRect;
  int smax, tmax;

  // add the poly to the proper lightmap chain

#ifdef MGL68K_RESIDENT
  if (!bm_nochain)
#endif
  {
  fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
  lightmap_polys[fa->lightmaptexturenum] = fa->polys;
  }

  // check for lightmap modification
  for (maps = 0 ; maps < MAXLIGHTMAPS && fa->styles[maps] != 255 ;
     maps++) 
    if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
      goto dynamic;
  
  if (fa->dlightframe == r_framecount // dynamic this frame
    || fa->cached_dlight)     // dynamic previously
  {
dynamic:
    if (r_dynamic.value)
    {
      lightmap_modified[fa->lightmaptexturenum] = true;
      theRect = &lightmap_rectchange[fa->lightmaptexturenum];
      if (fa->light_t < theRect->t) {
        if (theRect->h)
          theRect->h += theRect->t - fa->light_t;
        theRect->t = fa->light_t;
      }
      if (fa->light_s < theRect->l) {
        if (theRect->w)
          theRect->w += theRect->l - fa->light_s;
        theRect->l = fa->light_s;
      }
      smax = (fa->extents[0]>>4)+1;
      tmax = (fa->extents[1]>>4)+1;
      if ((theRect->w + theRect->l) < (fa->light_s + smax))
        theRect->w = (fa->light_s-theRect->l)+smax;
      if ((theRect->h + theRect->t) < (fa->light_t + tmax))
        theRect->h = (fa->light_t-theRect->t)+tmax;
      base = lightmaps + fa->lightmaptexturenum*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
//      base += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
      base += (fa->light_t * BLOCK_WIDTH + fa->light_s) * lightmap_bytes;

#ifdef LITFILES
	if(eyecandy)
	{
	  unsigned long fp_t = FP_Enter (FP_LMBUILD);
	  R_BuildLightMapColor (fa, base, BLOCK_WIDTH*lightmap_bytes);
	  FP_Exit (FP_LMBUILD, fp_t);
	}
	else
#endif
      {
        unsigned long fp_t = FP_Enter (FP_LMBUILD);
        R_BuildLightMap (fa, base, BLOCK_WIDTH*lightmap_bytes);
        FP_Exit (FP_LMBUILD, fp_t);
      }
    }
  }
}

void R_RenderBrushPoly (msurface_t *fa)
{
  texture_t *t;
  qboolean lm_static;
  c_brush_polys++;

  if (fa->flags & SURF_DRAWSKY)
  { // warp texture, no lightmaps
    EmitBothSkyLayers (fa);
    return;
  }

  t = R_TextureAnimation (fa->texinfo->texture);

  GL_Bind (t->gl_texturenum); //unit 0

  if (fa->flags & SURF_DRAWTURB)
  { // warp texture, no lightmaps
    EmitWaterPolys (fa);
    return;
  }

  if ((fa->flags & SURF_UNDERWATER) && r_dowarp)
  {
      DrawGLWaterPoly (fa->polys);
  }
  else
  {
    if(FAKE_MULTITEXTURE_VALUE)
    {
	GL_SelectTexture(GL_TEXTURE1_ARB);
	glEnable(GL_TEXTURE_2D);
	GL_Bind(lightmap_textures + fa->lightmaptexturenum);

	glBegin(GL_POLYGON);
	mglMtexTV23fv(mini_CurrentContext, VERTEXSIZE, fa->polys);
	glEnd();

	glDisable(GL_TEXTURE_2D);
	GL_SelectTexture(GL_TEXTURE0_ARB);

	if(FAKE_MULTITEXTURE_VALUE == 2)
		return; //don't modify lightmaps in any way
    }
    else
    {
	glBegin(GL_POLYGON);
	mglTV23fv(mini_CurrentContext, VERTEXSIZE, fa->polys);
	glEnd();
    }
  }

  R_ChainBrushLightmap (fa);
}

/*
================
R_RenderDynamicLightmaps
Multitexture
================
*/
void R_RenderDynamicLightmaps (msurface_t *fa)
{
  texture_t *t;
  byte    *base;
  int     maps;
  glRect_t    *theRect;
  int smax, tmax;

  c_brush_polys++;

  if (fa->flags & ( SURF_DRAWSKY | SURF_DRAWTURB) )
    return;

  if (!R_WorldBatchSurface (fa))
  {
    fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
    lightmap_polys[fa->lightmaptexturenum] = fa->polys;
  }

  // check for lightmap modification
  for (maps = 0 ; maps < MAXLIGHTMAPS && fa->styles[maps] != 255 ;
     maps++) 
    if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
      goto dynamic;

  if (fa->dlightframe == r_framecount // dynamic this frame
    || fa->cached_dlight)     // dynamic previously
  {
dynamic:
    if (r_dynamic.value)
    {
      lightmap_modified[fa->lightmaptexturenum] = true;
      theRect = &lightmap_rectchange[fa->lightmaptexturenum];
      if (fa->light_t < theRect->t) {
        if (theRect->h)
          theRect->h += theRect->t - fa->light_t;
        theRect->t = fa->light_t;
      }
      if (fa->light_s < theRect->l) {
        if (theRect->w)
          theRect->w += theRect->l - fa->light_s;
        theRect->l = fa->light_s;
      }
      smax = (fa->extents[0]>>4)+1;
      tmax = (fa->extents[1]>>4)+1;
      if ((theRect->w + theRect->l) < (fa->light_s + smax))
        theRect->w = (fa->light_s-theRect->l)+smax;
      if ((theRect->h + theRect->t) < (fa->light_t + tmax))
        theRect->h = (fa->light_t-theRect->t)+tmax;
      base = lightmaps + fa->lightmaptexturenum*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
//      base += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
      base += (fa->light_t * BLOCK_WIDTH + fa->light_s) * lightmap_bytes;

#ifdef LITFILES
	if(eyecandy)
	{
	  unsigned long fp_t = FP_Enter (FP_LMBUILD);
	  R_BuildLightMapColor (fa, base, BLOCK_WIDTH*lightmap_bytes);
	  FP_Exit (FP_LMBUILD, fp_t);
	}
	else
#endif
      {
        unsigned long fp_t = FP_Enter (FP_LMBUILD);
        R_BuildLightMap (fa, base, BLOCK_WIDTH*lightmap_bytes);
        FP_Exit (FP_LMBUILD, fp_t);
      }
    }
  }
}

/*
================
R_MirrorChain
================
*/
void R_MirrorChain (msurface_t *s)
{
  if (mirror)
    return;
  mirror = true;
  mirror_plane = s->plane;
}


#if 0

/*
================
R_DrawWaterSurfaces
================
*/
void R_DrawWaterSurfaces (void)
{
  int     i;
  msurface_t  *s;
  texture_t *t;

  if (r_wateralpha.value == 1.0)
    return;

  //
  // go back to the world matrix
  //
    glLoadMatrixf (r_world_matrix);

  glEnable (GL_BLEND);
  glColor4f (1,1,1,r_wateralpha.value);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  for (i=0 ; i<cl.worldmodel->numtextures ; i++)
  {
    t = cl.worldmodel->textures[i];
    if (!t)
      continue;
    s = t->texturechain;
    if (!s)
      continue;
    if ( !(s->flags & SURF_DRAWTURB) )
      continue;

    // set modulate mode explicitly
    GL_Bind (t->gl_texturenum);

 for ( ; s ; s=s->texturechain)
      R_RenderBrushPoly (s);

  
    t->texturechain = NULL;
  }

  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  glColor4f (1,1,1,1);
  glDisable (GL_BLEND);
}

#else

/*
================
R_DrawWaterSurfaces
================
*/
void R_DrawWaterSurfaces (void)
{
  int     i;
  msurface_t  *s;
  texture_t *t;
  qboolean draw_water_batches = R_WaterBatchReady ();

  if (r_wateralpha.value == 1.0 && gl_texsort.value)
    return;

  //
  // go back to the world matrix
  //

    glLoadMatrixf (r_world_matrix);


  if (!gl_texsort.value)
  {
    if (!waterchain)
      return;

    if (r_wateralpha.value < 1.0)
    {
    glEnable (GL_BLEND);
    glColor4f (1,1,1,r_wateralpha.value);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }

    for ( s = waterchain ; s ; s=s->texturechain)
    {
      GL_Bind (s->texinfo->texture->gl_texturenum);
      EmitWaterPolys (s);
    }
    
    waterchain = NULL;
  }
  else
  {

    if (r_wateralpha.value < 1.0)
    {
    glEnable (GL_BLEND);
    glColor4f (1,1,1,r_wateralpha.value);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }

    for (i=0 ; i<cl.worldmodel->numtextures ; i++)
    {
      t = cl.worldmodel->textures[i];
      if (!t)
        continue;
      s = t->texturechain;
      if (!s)
        continue;
      if ( !(s->flags & SURF_DRAWTURB ) )
        continue;

      // set modulate mode explicitly

      if (draw_water_batches)
        R_DrawTurbChain (s, t->gl_texturenum, false, true);
      else
      {
        GL_Bind (t->gl_texturenum);

        for ( ; s ; s=s->texturechain)
          EmitWaterPolys (s);
      }

      t->texturechain = NULL;
    }

  }

  if (r_wateralpha.value < 1.0)
  {
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4f (1,1,1,1);
    glDisable (GL_BLEND);
  }
}

#endif

/*
================
DrawTextureChains
================
*/

void DrawTextureChains (void)
{
  int   i;
  msurface_t  *s;
  texture_t *t;
  qboolean draw_world_batches = r_worldbatch.value && worldbatch_surfaces;
  qboolean draw_water_batches = R_WaterBatchReady ();

  if (!gl_texsort.value) {
    if(mtexenabled) GL_DisableMultitexture();

    if (skychain) {
      R_DrawSkyChain(skychain);
      skychain = NULL;
    }

    return;
  } 

  for (i=0 ; i<cl.worldmodel->numtextures ; i++)
  {
    t = cl.worldmodel->textures[i];
    if (!t)
      continue;
    s = t->texturechain;
    if (!s)
      continue;
    if (i == skytexturenum)
      R_DrawSkyChain (s);
    else if (i == mirrortexturenum && r_mirroralpha.value != 1.0)
    {
      R_MirrorChain (s);
      continue;
    }
    else
    {
      if ((s->flags & SURF_DRAWTURB) && r_wateralpha.value != 1.0)
        continue; // draw translucent water later

      if (draw_water_batches && R_ChainHasWaterBatch (s))
        R_DrawWaterBatchChain (t, s);

      if (draw_world_batches)
      {
        msurface_t *batchsurf;
        qboolean has_batch = false;

        for (batchsurf = s; batchsurf; batchsurf = batchsurf->texturechain)
          if (R_WorldBatchSurface (batchsurf))
          {
            worldbatchsurface_t *batchmeta =
              &worldbatch_surfaces[batchsurf - cl.worldmodel->surfaces];
            batchmeta->drawframe = r_framecount;
            batchmeta->lightmap_next =
              worldbatch_lightmap_heads[batchsurf->lightmaptexturenum];
            worldbatch_lightmap_heads[batchsurf->lightmaptexturenum] =
              batchsurf - cl.worldmodel->surfaces;
            R_RenderDynamicLightmaps (batchsurf);
            has_batch = true;
          }

        for ( ; s ; s=s->texturechain)
          if (!R_WorldBatchSurface (s) && !R_WaterBatchSurface (s))
            R_RenderBrushPoly (s);

        if (has_batch)
          continue;
      }
      else
      {
        for ( ; s ; s=s->texturechain)
          if (!R_WaterBatchSurface (s))
            R_RenderBrushPoly (s);
      }

    }

    t->texturechain = NULL;
  }

  if (draw_world_batches)
  {
    int segmentnum;

    if (mtexenabled)
      GL_DisableMultitexture ();

    for (segmentnum = 0; segmentnum < worldbatch_numsegments; segmentnum++)
    {
      R_EnableWorldBatchArrays (&worldbatch_segments[segmentnum], 3);
      for (i = 0; i < cl.worldmodel->numtextures; i++)
      {
        t = cl.worldmodel->textures[i];
        if (t && t->texturechain && i != skytexturenum &&
          !(i == mirrortexturenum && r_mirroralpha.value != 1.0))
        {
#ifdef MGL68K_RESIDENT
          worldstatic_texnum = i;
#endif
          R_DrawWorldTextureBatch (t, t->texturechain, segmentnum);
#ifdef MGL68K_RESIDENT
          worldstatic_texnum = -1;
#endif
        }
      }
    }
    R_DisableWorldBatchArrays ();

    for (i = 0; i < cl.worldmodel->numtextures; i++)
    {
      t = cl.worldmodel->textures[i];
      if (!t ||
        (i == mirrortexturenum && r_mirroralpha.value != 1.0))
        continue;

      for (s = t->texturechain; s; s = s->texturechain)
        if (R_WorldBatchSurface (s) &&
          worldbatch_surfaces[s - cl.worldmodel->surfaces].drawframe ==
            r_framecount)
        {
          t->texturechain = NULL;
          break;
        }
    }
  }
}


/*
=================
R_DrawBrushModel
=================
*/
#ifdef MGL68K_RESIDENT
#define BM_MAX_SURFS 256
static msurface_t *bm_surfs[BM_MAX_SURFS];
static int bm_count;

/* The collected brush-model surfaces, one ring draw per texture. */
static void R_DrawBrushRing (void)
{
  static glpoly_t *polys[BM_MAX_SURFS];
  int i, j, n;

  for (i = 0; i < bm_count; i++)
  {
    texture_t *t;
    if (!bm_surfs[i])
      continue;
    t = R_TextureAnimation (bm_surfs[i]->texinfo->texture);
    n = 0;
    for (j = i; j < bm_count; j++)
      if (bm_surfs[j] &&
          R_TextureAnimation (bm_surfs[j]->texinfo->texture) == t)
      {
        polys[n++] = bm_surfs[j]->polys;
        if (j != i)
          bm_surfs[j] = NULL;
      }
    GL_Bind (t->gl_texturenum);
    if (!R_RingDrawPolys (polys, n, 3))
    {
      /* fallback: the immediate path for this texture's polygons */
      for (j = 0; j < n; j++)
      {
        glBegin (GL_POLYGON);
        mglTV23fv (mini_CurrentContext, VERTEXSIZE, polys[j]);
        glEnd ();
      }
    }
    bm_surfs[i] = NULL;
  }
  bm_count = 0;
}
#endif

#ifdef MGL68K_RESIDENT
void R_FlushBrushDeferred (void)
{
  static glpoly_t *polys[RING_POLY_MAX_VERTS / 3];
  static float org[RING_POLY_MAX_VERTS / 3][3];
  static unsigned char done[BMD_MAX];
  int i, j, n, nv;

  if (!bmd_count)
    return;
  GL_SelectTexture (GL_TEXTURE0_ARB);
  memset (done, 0, bmd_count);
  for (i = 0; i < bmd_count; i++)
  {
    texture_t *t = bmd_tex[i];
    if (done[i])
      continue;
    GL_Bind (t->gl_texturenum);
    n = nv = 0;
    for (j = i; j < bmd_count; j++)
    {
      glpoly_t *q;
      if (done[j] || bmd_tex[j] != t)
        continue;
      q = bmd_surf[j]->polys;
      if (n == RING_POLY_MAX_VERTS / 3 || nv + q->numverts > RING_POLY_MAX_VERTS)
      {
        (void)R_RingDrawPolysO (polys, n, 3, NULL, org);
        n = nv = 0;
      }
      polys[n] = q;
      VectorCopy (bmd_org[j], org[n]);
      n++;
      nv += q->numverts;
      done[j] = 1;
    }
    if (n)
      (void)R_RingDrawPolysO (polys, n, 3, NULL, org);
  }
  memset (lightmap_polys, 0, sizeof(lightmap_polys));
  bmd_lmpending = true;
  R_BlendLightmaps ();
  bmd_lmpending = false;
  bmd_count = 0;
}
#endif

void R_DrawBrushModel (entity_t *e)
{
  int     j, k;
  vec3_t    mins, maxs;
  int     i, numsurfaces;
  msurface_t  *psurf;
  float   dot;
  mplane_t  *pplane;
  model_t   *clmodel;
  qboolean  rotated;

  currententity = e;
  GL_SelectTexture(GL_TEXTURE0_ARB);
#ifndef MINIGL_DISPATCH_CLIENT
  currenttexture = -1;
#endif

  clmodel = e->model;

  if (e->angles[0] || e->angles[1] || e->angles[2])
  {
    rotated = true;
    for (i=0 ; i<3 ; i++)
    {
      mins[i] = e->origin[i] - clmodel->radius;
      maxs[i] = e->origin[i] + clmodel->radius;
    }
  }
  else
  {
    rotated = false;
    VectorAdd (e->origin, clmodel->mins, mins);
    VectorAdd (e->origin, clmodel->maxs, maxs);
  }

  if (R_CullBox (mins, maxs))
    return;

//  glColor3f (1,1,1); - not needed because of entity-sorting

  memset (lightmap_polys, 0, sizeof(lightmap_polys));

  VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
  if (rotated)
  {
    vec3_t  temp;
    vec3_t  forward, right, up;

    VectorCopy (modelorg, temp);
    AngleVectors (e->angles, forward, right, up);
    modelorg[0] = DotProduct (temp, forward);
    modelorg[1] = -DotProduct (temp, right);
    modelorg[2] = DotProduct (temp, up);
  }

  psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

// calculate dynamic lighting for bmodel if it's not an
// instanced model
  if (clmodel->firstmodelsurface != 0 && !gl_flashblend.value)
  {
    for (k=0 ; k<MAX_DLIGHTS ; k++)
    {
      if ((cl_dlights[k].die < cl.time) ||
        (!cl_dlights[k].radius))
        continue;

      R_MarkLights (&cl_dlights[k], 1<<k,
        clmodel->nodes + clmodel->hulls[0].firstclipnode);
    }
  }

#ifdef MGL68K_RESIDENT
  /* unrotated: deferred, drawn with the others by R_FlushBrushDeferred */
  if (!rotated && lma_active && r_bmdefer.value && R_ResidentExt () &&
      gl_texsort.value && !FAKE_MULTITEXTURE_VALUE && !r_fullbright.value)
  {
    int start = bmd_count;
    qboolean ok = true;
    msurface_t *ps = &clmodel->surfaces[clmodel->firstmodelsurface];

    for (i = 0; i < clmodel->nummodelsurfaces; i++, ps++)
    {
      pplane = ps->plane;
      dot = DotProduct (modelorg, pplane->normal) - pplane->dist;
      if (!(((ps->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
            (!(ps->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))))
        continue;
      if (!ps->polys || (ps->flags & (SURF_DRAWSKY | SURF_DRAWTURB)) ||
          ((ps->flags & SURF_UNDERWATER) && r_dowarp) ||
          ps->polys->numverts > RING_POLY_MAX_VERTS || bmd_count >= BMD_MAX)
      {
        ok = false;
        break;
      }
      bmd_surf[bmd_count] = ps;
      bmd_tex[bmd_count] = R_TextureAnimation (ps->texinfo->texture);
      VectorCopy (e->origin, bmd_org[bmd_count]);
      bmd_count++;
    }
    if (ok)
    {
      bm_nochain = true;
      for (i = start; i < bmd_count; i++)
      {
        R_ChainBrushLightmap (bmd_surf[i]);
        c_brush_polys++;
      }
      bm_nochain = false;
      return;
    }
    bmd_count = start;
  }
#endif

    glPushMatrix ();
e->angles[0] = -e->angles[0]; // stupid quake bug
  R_RotateForEntity (e);
e->angles[0] = -e->angles[0]; // stupid quake bug

  //
  // draw texture
  //
#ifdef MGL68K_RESIDENT
  bm_count = 0;
#endif
  for (i=0 ; i<clmodel->nummodelsurfaces ; i++, psurf++)
  {
  // find which side of the node we are on
    pplane = psurf->plane;

    dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

  // draw the polygon
    if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
      (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
    {
      if (gl_texsort.value)
	{
#ifdef MGL68K_RESIDENT
        /* minigl68k: plain surfaces are collected and drawn per texture
         * through the ring after the loop; lightmaps chain as usual. */
        if (R_ResidentExt () && !FAKE_MULTITEXTURE_VALUE &&
            bm_count < BM_MAX_SURFS && psurf->polys &&
            !(psurf->flags & (SURF_DRAWSKY | SURF_DRAWTURB)) &&
            !((psurf->flags & SURF_UNDERWATER) && r_dowarp))
        {
          bm_surfs[bm_count++] = psurf;
          c_brush_polys++;
          R_ChainBrushLightmap (psurf);
          continue;
        }
#endif
        R_RenderBrushPoly (psurf);
  	}
      else
        R_DrawSequentialPoly (psurf);
    }
  }
#ifdef MGL68K_RESIDENT
  R_DrawBrushRing ();
#endif

  R_BlendLightmaps ();

  glPopMatrix ();
}

/*
=============================================================

  WORLD MODEL

=============================================================
*/


/*
================
R_RecursiveWorldNode
================
*/
extern mplane_t frustum[4];

/* clipflags: the frustum planes the node may still cross; a node wholly
 * inside a plane passes it on cleared, so its subtree skips that test */
static void R_RecursiveWorldNodeC (mnode_t *node, int clipflags);

void R_RecursiveWorldNode (mnode_t *node)
{
  R_RecursiveWorldNodeC (node, 15);
}

static void R_RecursiveWorldNodeC (mnode_t *node, int clipflags)
{
  int     i, c, side, *pindex;
  vec3_t    acceptpt, rejectpt;
  mplane_t  *plane;
  msurface_t  *surf, **mark;
  mleaf_t   *pleaf;
  double    d, dot;
  vec3_t    mins, maxs;


loc0:

  if (node->contents == CONTENTS_SOLID)
    return;   // solid

  if (node->visframe != r_visframecount)
    return;

  if (clipflags)
  {
    int p;
    for (p = 0; p < 4; p++)
    {
      int r;
      if (!(clipflags & (1 << p)))
        continue;
      r = frustum[p].BoxOnPlaneSideFunc (node->minmaxs, node->minmaxs+3, &frustum[p]);
      if (r == 2)
        return;
      if (r == 1)
        clipflags &= ~(1 << p);
    }
  }

// if a leaf node, draw stuff
  if (node->contents < 0)
  {
    pleaf = (mleaf_t *)node;

    mark = pleaf->firstmarksurface;
    c = pleaf->nummarksurfaces;

    if (c)
    {
      do
      {
        (*mark)->visframe = r_framecount;
        mark++;
      } while (--c);
    }

  // deal with model fragments in this leaf
    if (pleaf->efrags)
      R_StoreEfrags (&pleaf->efrags);

    return;
  }

// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
  plane = node->plane;

  switch (plane->type)
  {
  case PLANE_X:
    dot = modelorg[0] - plane->dist;
    break;
  case PLANE_Y:
    dot = modelorg[1] - plane->dist;
    break;
  case PLANE_Z:
    dot = modelorg[2] - plane->dist;
    break;
  default:
    dot = DotProduct (modelorg, plane->normal) - plane->dist;
    break;
  }

  if (dot >= 0)
    side = 0;
  else
    side = 1;

// recurse down the children, front side first
//  R_RecursiveWorldNode (node->children[side]);

if(node->children[side]->contents != CONTENTS_SOLID && node->children[side]->visframe == r_visframecount)
{
  R_RecursiveWorldNodeC (node->children[side], clipflags);
}

// draw stuff

  c = node->numsurfaces;

  if (c)
  {
    surf = cl.worldmodel->surfaces + node->firstsurface;

    if (dot < 0 -BACKFACE_EPSILON)
      side = SURF_PLANEBACK;
    else if (dot > BACKFACE_EPSILON)
      side = 0;
    {
      for ( ; c ; c--, surf++)
      {
        if (surf->visframe != r_framecount)
          continue;

        // don't backface underwater surfaces, because they warp
//surgeon: if no warping, then backface annyway

        if(!(surf->flags & (r_dowarp ? SURF_UNDERWATER : 0)) && ( (dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)) )
          continue;   // wrong side

        // if sorting by texture, just store it out
        if (gl_texsort.value)
        {
          if (!mirror
          || surf->texinfo->texture != cl.worldmodel->textures[mirrortexturenum])
          {
            surf->texturechain = surf->texinfo->texture->texturechain;
            surf->texinfo->texture->texturechain = surf;
          }
        } else if (surf->flags & SURF_DRAWSKY) {
          surf->texturechain = skychain;
          skychain = surf;
        } else if (surf->flags & SURF_DRAWTURB) {
          surf->texturechain = waterchain;
          waterchain = surf;
        } else
          R_DrawSequentialPoly (surf);
      }
    }

  }

// recurse down the back side

#if 0

  R_RecursiveWorldNode (node->children[!side]);

#else
  node = node->children[!side];
  goto loc0;
#endif
}

extern qboolean r_skybox;

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld (void)
{
  entity_t  ent;
  int     i;


  memset (&ent, 0, sizeof(ent));
  ent.model = cl.worldmodel;

  VectorCopy (r_refdef.vieworg, modelorg);

  currententity = &ent;
  GL_SelectTexture(GL_TEXTURE0_ARB);
#ifndef MINIGL_DISPATCH_CLIENT
  currenttexture = -1;
#endif

  glColor3f (1,1,1);

  memset (lightmap_polys, 0, sizeof(lightmap_polys));
  memset (worldbatch_lightmap_heads, -1, sizeof(worldbatch_lightmap_heads));


//#ifdef QUAKE2
if(r_skybox)
  R_ClearSkyBox ();
//#endif

  { unsigned long fp_t = FP_Enter (FP_VIS);
    R_RecursiveWorldNode (cl.worldmodel->nodes);
    FP_Exit (FP_VIS, fp_t); }

  { unsigned long fp_t = FP_Enter (FP_CHAINS);
    DrawTextureChains ();
    FP_Exit (FP_CHAINS, fp_t); }

  worldbatch_blending_world = true;
  { unsigned long fp_t = FP_Enter (FP_LMBLEND);
    R_BlendLightmaps ();
    FP_Exit (FP_LMBLEND, fp_t); }
  worldbatch_blending_world = false;
  memset (worldbatch_lightmap_heads, -1, sizeof(worldbatch_lightmap_heads));
  if(FAKE_MULTITEXTURE_VALUE)
  R_DrawMultitextureBuffer();

//#ifdef QUAKE2
if(r_skybox)
{
  if (gl_fog.value) glDisable(GL_FOG);

  R_DrawSkyBox ();

  if (gl_fog.value) glEnable(GL_FOG);
}
//#endif
}


/*
===============
R_MarkLeaves
===============
*/
void R_MarkLeaves (void)
{
  byte  *vis;
  mnode_t *node;
  int   i;
  byte  solid[4096];

  if (r_oldviewleaf == r_viewleaf && !r_novis.value)
    return;
  
  if (mirror)
    return;

  r_visframecount++;
  r_oldviewleaf = r_viewleaf;

  if (r_novis.value)
  {
    vis = solid;
    memset (solid, 0xff, (cl.worldmodel->numleafs+7)>>3);
  }
  else
    vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);
    
  for (i=0 ; i<cl.worldmodel->numleafs ; i++)
  {
    if (vis[i>>3] & (1<<(i&7)))
    {
      node = (mnode_t *)&cl.worldmodel->leafs[i+1];
      do
      {
        if (node->visframe == r_visframecount)
          break;
        node->visframe = r_visframecount;
        node = node->parent;
      } while (node);
    }
  }
}



/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

// returns a texture number and the position inside it

// returns a texture number and the position inside it
int AllocBlock (int w, int h, int *x, int *y)
{
  int   i, j;
  int   best, best2;
  int   texnum;

  for (texnum=0 ; texnum<MAX_LIGHTMAPS ; texnum++)
  {
    best = BLOCK_HEIGHT;

    for (i=0 ; i<BLOCK_WIDTH-w ; i++)
    {
      best2 = 0;

      for (j=0 ; j<w ; j++)
      {
        if (allocated[texnum][i+j] >= best)
          break;
        if (allocated[texnum][i+j] > best2)
          best2 = allocated[texnum][i+j];
      }
      if (j == w)
      { // this is a valid spot
        *x = i;
        *y = best = best2;
      }
    }

    if (best + h > BLOCK_HEIGHT)
      continue;

    for (i=0 ; i<w ; i++)
      allocated[texnum][*x + i] = best + h;

    return texnum;
  }

  Sys_Error ("AllocBlock: full");
  return 0;
}


mvertex_t *r_pcurrentvertbase;
model_t   *currentmodel;

int nColinElim;

/*
================
BuildSurfaceDisplayList
================
*/
void BuildSurfaceDisplayList (msurface_t *fa)
{
  int     i, lindex, lnumverts, s_axis, t_axis;
  //float   dist, lastdist, lzi, scale, u, v, frac;
  //unsigned  mask;
  //vec3_t    local, transformed;
  medge_t   *pedges, *r_pedge;
  mplane_t  *pplane;
  int     vertpage, newverts, newpage, lastvert;
  qboolean  visible;
  float   *vec;
  float   s, t;
  glpoly_t  *poly;

// reconstruct the polygon
  pedges = currentmodel->edges;
  lnumverts = fa->numedges;
  vertpage = 0;

  //
  // draw texture
  //
  poly = Hunk_AllocName (sizeof(glpoly_t) + (lnumverts-4) * VERTEXSIZE*sizeof(float), "surface");
  poly->next = fa->polys;
  poly->flags = fa->flags;
  fa->polys = poly;
  poly->numverts = lnumverts;

  for (i=0 ; i<lnumverts ; i++)
  {
    lindex = currentmodel->surfedges[fa->firstedge + i];

    if (lindex > 0)
    {
      r_pedge = &pedges[lindex];
      vec = r_pcurrentvertbase[r_pedge->v[0]].position;
    }
    else
    {
      r_pedge = &pedges[-lindex];
      vec = r_pcurrentvertbase[r_pedge->v[1]].position;
    }
    s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
    s /= fa->texinfo->texture->width;

    t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
    t /= fa->texinfo->texture->height;

    VectorCopy (vec, poly->verts[i]);
    poly->verts[i][3] = s;
    poly->verts[i][4] = t;

    //
    // lightmap texture coordinates
    //
    s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
    s -= fa->texturemins[0];
    s += fa->light_s*16;
    s += 8;
    s /= BLOCK_WIDTH*16; //fa->texinfo->texture->width;

    t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
    t -= fa->texturemins[1];
    t += fa->light_t*16;
    t += 8;
    t /= BLOCK_HEIGHT*16; //fa->texinfo->texture->height;

    poly->verts[i][5] = s;
    poly->verts[i][6] = t;
  }

  //
  // remove co-linear points - Ed
  //

  if (!gl_keeptjunctions.value && !(fa->flags & (r_dowarp ? SURF_UNDERWATER : 0)))
  {
    for (i = 0 ; i < lnumverts ; ++i)
    {
      vec3_t v1, v2;
      float *prev, *this, *next;
      float f;

      prev = poly->verts[(i + lnumverts - 1) % lnumverts];
      this = poly->verts[i];
      next = poly->verts[(i + 1) % lnumverts];

      VectorSubtract( this, prev, v1 );
      VectorNormalize( v1 );
      VectorSubtract( next, prev, v2 );
      VectorNormalize( v2 );

      // skip co-linear points
      #define COLINEAR_EPSILON 0.001
      if ((fabs( v1[0] - v2[0] ) <= COLINEAR_EPSILON) &&
        (fabs( v1[1] - v2[1] ) <= COLINEAR_EPSILON) && 
        (fabs( v1[2] - v2[2] ) <= COLINEAR_EPSILON))
      {
        int j;
        for (j = i + 1; j < lnumverts; ++j)
        {
          int k;
          for (k = 0; k < VERTEXSIZE; ++k)
            poly->verts[j - 1][k] = poly->verts[j][k];
        }
        --lnumverts;
        ++nColinElim;
        // retry next vertex next time, which is now current vertex
        --i;
      }
    }
  }
  poly->numverts = lnumverts;

}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap (msurface_t *surf)
{
  int   smax, tmax, s; //, t, l, i;
  byte  *base;

  if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
    return;

  smax = (surf->extents[0]>>4)+1;
  tmax = (surf->extents[1]>>4)+1;

  surf->lightmaptexturenum = AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);

  base = lightmaps + surf->lightmaptexturenum*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
  base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * lightmap_bytes;

#ifdef LITFILES
 if(eyecandy)
 R_BuildLightMapColor (surf, base, BLOCK_WIDTH*lightmap_bytes);
 else
#endif
  R_BuildLightMap (surf, base, BLOCK_WIDTH*lightmap_bytes);
}

void R_ClearWorldBatchData (void)
{
#ifdef MGL68K_RESIDENT
  R_FreeWorldResident ();
#endif
  worldbatch_surfaces = NULL;
  worldbatch_segments = NULL;
  worldbatch_indices = NULL;
  worldbatch_numsegments = 0;
  worldbatch_maxindices = 0;
  worldbatch_blending_world = false;
  memset (worldbatch_lightmap_heads, -1, sizeof(worldbatch_lightmap_heads));

  waterbatch_stageverts = NULL;
  waterbatch_stageindices = NULL;
  R_WaterBatchReset ();
}

/*
 * Fixed staging buffers for the water / underwater-warp batches.  They are
 * allocated once per map with the same opt-in gate as the world batch and
 * reused every frame; no per-frame allocation.
 */
static void R_BuildWaterBatchData (void)
{
  if (!r_worldbatch.value || !gl_texsort.value || FAKE_MULTITEXTURE_VALUE)
    return;

  waterbatch_stageverts = Hunk_AllocName (
    WATER_BATCH_STAGE_VERTS * VERTEXSIZE * sizeof(float), "wbstage");
  waterbatch_stageindices = Hunk_AllocName (
    BATCH_DRAW_MAX_INDICES * sizeof(unsigned short), "wbindex2");
  R_WaterBatchReset ();
}

static void R_BuildWorldBatchData (void)
{
  int i;
  int segmentnum = 0;
  int segmentverts = 0;
  int totalverts = 0;
  int totalindices = 0;
  int firstsurface;
  int lastsurface;
  int maxsegverts = WORLD_BATCH_MAX_VERTICES;
  model_t *world = cl.worldmodel;

  R_ClearWorldBatchData ();
#ifdef MGL68K_RESIDENT
  if (R_ResidentExt ())
    maxsegverts = R68K_SEG_VERTS;
#endif

  if (!r_worldbatch.value || !gl_texsort.value || FAKE_MULTITEXTURE_VALUE ||
    !world || !world->numsurfaces)
    return;

  firstsurface = world->firstmodelsurface;
  if (firstsurface < 0 || world->nummodelsurfaces < 0 ||
    firstsurface > world->numsurfaces ||
    world->nummodelsurfaces > world->numsurfaces - firstsurface)
    return;
  lastsurface = firstsurface + world->nummodelsurfaces;

  worldbatch_surfaces = Hunk_AllocName (
    world->numsurfaces * sizeof(*worldbatch_surfaces), "wbsurf");
  memset (worldbatch_surfaces, 0,
    world->numsurfaces * sizeof(*worldbatch_surfaces));

  for (i = firstsurface; i < lastsurface; i++)
  {
    msurface_t *surf = &world->surfaces[i];
    int numverts;

    if (!surf->polys ||
      (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB | SURF_UNDERWATER)))
      continue;

    numverts = surf->polys->numverts;
    if (numverts < 3 || numverts > WORLD_BATCH_MAX_VERTICES)
      continue;

    if (segmentverts && segmentverts + numverts > maxsegverts)
    {
      segmentnum++;
      segmentverts = 0;
    }

    worldbatch_surfaces[i].segment = segmentnum;
    worldbatch_surfaces[i].firstvert = segmentverts;
    worldbatch_surfaces[i].numverts = numverts;
    segmentverts += numverts;
    totalverts += numverts;
    totalindices += (numverts - 2) * 3;
  }

  if (!totalverts)
    return;

  worldbatch_numsegments = segmentnum + 1;
  worldbatch_maxindices = totalindices;
  worldbatch_segments = Hunk_AllocName (
    worldbatch_numsegments * sizeof(*worldbatch_segments), "wbseg");
  memset (worldbatch_segments, 0,
    worldbatch_numsegments * sizeof(*worldbatch_segments));

  for (i = firstsurface; i < lastsurface; i++)
  {
    worldbatchsurface_t *batchsurf = &worldbatch_surfaces[i];
    if (batchsurf->numverts)
      worldbatch_segments[batchsurf->segment].numverts += batchsurf->numverts;
  }

  for (i = 0; i < worldbatch_numsegments; i++)
    worldbatch_segments[i].verts = Hunk_AllocName (
      worldbatch_segments[i].numverts * VERTEXSIZE * sizeof(float), "wbverts");

  worldbatch_indices = Hunk_AllocName (
    worldbatch_maxindices * sizeof(*worldbatch_indices), "wbindex");

  for (i = firstsurface; i < lastsurface; i++)
  {
    worldbatchsurface_t *batchsurf = &worldbatch_surfaces[i];
    if (batchsurf->numverts)
      memcpy (worldbatch_segments[batchsurf->segment].verts +
        batchsurf->firstvert * VERTEXSIZE, world->surfaces[i].polys->verts[0],
        batchsurf->numverts * VERTEXSIZE * sizeof(float));
  }

  Con_DPrintf ("World batch: %i vertices, %i indices, %i segments\n",
    totalverts, totalindices, worldbatch_numsegments);
#ifdef MGL68K_RESIDENT
  R_BuildWorldResident ();
#endif
}


/*
==================
GL_BuildLightmaps

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void GL_BuildLightmaps (void)
{
  int   i, j;
  model_t *m;
  extern qboolean lmf_isset; //set in gl_vidamiga.c
  
  memset (allocated, 0, sizeof(allocated));

  r_framecount = 1;   // no dlightcache : was disabled by massimiliano!

#ifndef AMIGA
  if (!lightmap_textures)
  {
    lightmap_textures = texture_extension_number;
    texture_extension_number += MAX_LIGHTMAPS;
  }
#endif

#ifdef LITFILES
  if (COM_CheckParm ("-litfiles"))
	eyecandy = true;
#endif

#if !defined(AMIGA) || defined(MINIGL_DISPATCH_CLIENT)
  #ifdef MINIGL_DISPATCH_CLIENT
    gl_lightmap_format = GL_LUMINANCE;
  #else
    gl_lightmap_format = GL_RGBA;
  #endif
#else
    gl_lightmap_format = MGL_UNSIGNED_SHORT_4_4_4_4;
#endif

  if (COM_CheckParm ("-lm_LUMINANCE"))
    gl_lightmap_format = GL_LUMINANCE;
  else if (COM_CheckParm ("-lm_ALPHA"))
    gl_lightmap_format = GL_ALPHA;
  else if (COM_CheckParm ("-lm_RGBA"))
    gl_lightmap_format = GL_RGBA;
  else if (COM_CheckParm ("-lm_RGB"))
    gl_lightmap_format = MGL_UNSIGNED_SHORT_5_6_5;

  switch (gl_lightmap_format)
  {
  case GL_RGBA:
    lightmap_bytes = 4;
    break;
  case MGL_UNSIGNED_SHORT_4_4_4_4:
  case MGL_UNSIGNED_SHORT_5_6_5:
    lightmap_bytes = 2;
    break;
  case GL_LUMINANCE:
  case GL_ALPHA:
    lightmap_bytes = 1;
    break;
  }

  for (j=1 ; j<MAX_MODELS ; j++)
  {
    m = cl.model_precache[j];
    if (!m)
      break;
    if (m->name[0] == '*')
      continue;
    r_pcurrentvertbase = m->vertexes;
    currentmodel = m;
    WOS_STALL("lightmap build enter", m->name, m->numsurfaces);
    for (i=0 ; i<m->numsurfaces ; i++)
    {
      GL_CreateSurfaceLightmap (m->surfaces + i);
      if ( m->surfaces[i].flags & SURF_DRAWTURB )
        continue;
#ifndef QUAKE2
	if(!r_skybox)
      if ( m->surfaces[i].flags & SURF_DRAWSKY )
        continue;
#endif

      BuildSurfaceDisplayList (m->surfaces + i);
    }
  }

  WOS_STALL("worldbatch enter", "", 0);
  R_BuildWorldBatchData ();
  R_BuildWaterBatchData ();
  WOS_STALL("worldbatch leave", "", 0);

  if (gl_mtexable)
    GL_SelectTexture(GL_TEXTURE1_ARB);

  //
  // upload all lightmaps that were filled
  //
  for (i=0 ; i<MAX_LIGHTMAPS ; i++)
  {
    if (!allocated[i][0])
      break;    // no more used
    lightmap_modified[i] = false;
    lightmap_rectchange[i].l = BLOCK_WIDTH;
    lightmap_rectchange[i].t = BLOCK_HEIGHT;
    lightmap_rectchange[i].w = 0;
    lightmap_rectchange[i].h = 0;

    GL_Bind(lightmap_textures + i);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    WOS_STALL("lightmap upload enter", "", i);
    glTexImage2D (GL_TEXTURE_2D, 0, gl_lightmap_format, BLOCK_WIDTH, BLOCK_HEIGHT, 0, 
    gl_lightmap_format, GL_UNSIGNED_BYTE, lightmaps+(i*BLOCK_WIDTH*BLOCK_HEIGHT*lightmap_bytes));
    WOS_STALL("lightmap upload leave", "", i);
#ifdef MINIGL_DISPATCH_CLIENT
    GL_CheckErrors ("lightmap upload");
#endif
  }


  if (gl_mtexable)
    GL_SelectTexture(GL_TEXTURE0_ARB);

#ifdef MGL68K_RESIDENT
  /* the lightmap atlas from the filled blocks */
  if (lma_active && worldbatch_resident)
  {
    byte *buf = malloc (LMA_WIDTH * lma_height * lightmap_bytes);
    if (buf)
    {
      int b, row;
      memset (buf, 0, LMA_WIDTH * lma_height * lightmap_bytes);
      for (b = 0; b < MAX_LIGHTMAPS && allocated[b][0]; b++)
        for (row = 0; row < BLOCK_HEIGHT; row++)
          memcpy (buf + (((b / LMA_COLS) * 128 + row) * LMA_WIDTH +
                  (b % LMA_COLS) * 128) * lightmap_bytes,
                  lightmaps + ((b * BLOCK_HEIGHT + row) * BLOCK_WIDTH) *
                  lightmap_bytes, BLOCK_WIDTH * lightmap_bytes);
      GL_Bind (LMA_TEX);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexImage2D (GL_TEXTURE_2D, 0, gl_lightmap_format, LMA_WIDTH,
        lma_height, 0, gl_lightmap_format, GL_UNSIGNED_BYTE, buf);
      free (buf);
      Con_DPrintf ("minigl68k: lightmap atlas %ix%i\n", LMA_WIDTH, lma_height);
    }
    else
      lma_active = false;
  }
  else
    lma_active = false;
  /* upload every texture now, not at its first draw in the level */
  if (R_ResidentExt () && r68k_ext->structSize >=
      offsetof(MGL68KExt, PrepareTextures) + sizeof(void *))
  {
    Con_DPrintf ("minigl68k: %lu textures made resident\n",
      (unsigned long)r68k_ext->PrepareTextures ());
    /* The atlas's first sub-image update re-imports the whole texture
     * (no spare surface yet: ~250 ms over the bus mid-game). Do that one
     * here: rewrite a row and make it resident again. */
    if (lma_active)
    {
      glRect_t r;
      r.l = 0; r.t = 0; r.w = BLOCK_WIDTH; r.h = 1;
      R_UploadLightmapRect (0, &r);
      (void)r68k_ext->PrepareTextures ();
    }
  }
#endif
}
