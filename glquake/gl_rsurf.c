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

cvar_t r_worldbatch = {"r_worldbatch", "0", true};

#define WORLD_BATCH_MAX_VERTICES 65535

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

static void R_DrawWorldTextureBatch (texture_t *texture, msurface_t *chain,
  int segmentnum)
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

static void R_DrawWorldLightmapBatch (int lightmapnum, int segmentnum)
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

/*
===============
R_AddDynamicLights
===============
*/


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
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t,
          BLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
          lightmaps+(i* BLOCK_HEIGHT + theRect->t) *BLOCK_WIDTH*lightmap_bytes);
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
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t,
        BLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
        lightmaps+(i* BLOCK_HEIGHT + theRect->t) *BLOCK_WIDTH*lightmap_bytes);
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

      nv[0] = v[0] + 8*sin(v[1]*0.05+realtime)*sin(v[2]*0.05+realtime);
      nv[1] = v[1] + 8*sin(v[0]*0.05+realtime)*sin(v[2]*0.05+realtime);
    nv[2] = v[2];

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

    nv[0] = v[0] + 8*sin(v[1]*0.05+realtime)*sin(v[2]*0.05+realtime);
    nv[1] = v[1] + 8*sin(v[0]*0.05+realtime)*sin(v[2]*0.05+realtime);
    nv[2] = v[2];

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
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t,
        BLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
        lightmaps+(i* BLOCK_HEIGHT + theRect->t) *BLOCK_WIDTH*lightmap_bytes);
      FP_CountLMUpload (theRect->h * BLOCK_WIDTH * lightmap_bytes);
      theRect->l = BLOCK_WIDTH;
      theRect->t = BLOCK_HEIGHT;
      theRect->h = 0;
      theRect->w = 0;
    }

if(fake_multitexture)
	continue;

    for ( ; p ; p=p->chain)
    {

	if ((p->flags & SURF_UNDERWATER) && r_dowarp)
        DrawGLWaterPolyLightmap (p);
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
  }

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
void R_RenderBrushPoly (msurface_t *fa)
{
  texture_t *t;
  byte    *base;
  register int     maps;
  register glRect_t    *theRect;
  int smax, tmax;
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
  // add the poly to the proper lightmap chain

  fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
  lightmap_polys[fa->lightmaptexturenum] = fa->polys;

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
	R_BuildLightMapColor (fa, base, BLOCK_WIDTH*lightmap_bytes);
	else
#endif
      R_BuildLightMap (fa, base, BLOCK_WIDTH*lightmap_bytes);
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
      
      GL_Bind (t->gl_texturenum);

      for ( ; s ; s=s->texturechain)
        EmitWaterPolys (s);
      
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
          if (!R_WorldBatchSurface (s))
            R_RenderBrushPoly (s);

        if (has_batch)
          continue;
      }
      else
      {
        for ( ; s ; s=s->texturechain)
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
          R_DrawWorldTextureBatch (t, t->texturechain, segmentnum);
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

    glPushMatrix ();
e->angles[0] = -e->angles[0]; // stupid quake bug
  R_RotateForEntity (e);
e->angles[0] = -e->angles[0]; // stupid quake bug

  //
  // draw texture
  //
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
        R_RenderBrushPoly (psurf);
  	}
      else
        R_DrawSequentialPoly (psurf);
    }
  }

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
void R_RecursiveWorldNode (mnode_t *node)
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

  if (R_CullBox (node->minmaxs, node->minmaxs+3))
    return;

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
  R_RecursiveWorldNode (node->children[side]);
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
  worldbatch_surfaces = NULL;
  worldbatch_segments = NULL;
  worldbatch_indices = NULL;
  worldbatch_numsegments = 0;
  worldbatch_maxindices = 0;
  worldbatch_blending_world = false;
  memset (worldbatch_lightmap_heads, -1, sizeof(worldbatch_lightmap_heads));
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
  model_t *world = cl.worldmodel;

  R_ClearWorldBatchData ();

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

    if (segmentverts && segmentverts + numverts > WORLD_BATCH_MAX_VERTICES)
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

}
