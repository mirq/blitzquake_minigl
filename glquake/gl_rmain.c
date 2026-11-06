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
// r_main.c

//Surgeon: notes regarding model-flatshading:
//precalc dotproducts are within the range [0.70;1.99]
//I have selected 1.20 as constant flatshade multiplier


#include "quakedef.h"
#ifdef MGL68K_RESIDENT
extern int r68k_now;
#endif
#include "frame_profile.h"

#ifndef MINIGL_DISPATCH_CLIENT
#include <mgl/mglmacros.h>
#endif

cvar_t r_interpolations = {"r_interpolations","0"};

//surgeon:
extern qboolean	mtexenabled;

//surgeon: some constants 
#define MPIMUL (M_PI*180)
#define MPIDIV (M_PI/360)


extern void R_DrawWaterSurfaces (void);
extern void R_DrawParticles (void);
extern void R_RenderBrushPoly (msurface_t *fa);
extern void V_CalcBlend (void);
extern void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
extern void RotatePointAroundVectorNEW( vec3_t dst, vec3_t dst2, const vec3_t dir, const vec3_t point, float degrees );

entity_t  r_worldentity;

qboolean  r_cache_thrash;   // compatability

vec3_t    modelorg, r_entorigin;
entity_t  *currententity;

int     r_visframecount;  // bumped when going to a new PVS
int     r_framecount;   // used for dlight push checking

mplane_t  frustum[4];

int     c_brush_polys, c_alias_polys;

qboolean  envmap;       // true during envmap command capture 

int     currenttexture = -1;    // to avoid unnecessary texture sets

int     cnttextures[2] = {-1, -1};     // cached

int     particletexture;  // little dot for particles
int     playertextures;   // up to 16 color translated skins

int     mirrortexturenum; // quake texturenum, not gltexturenum
qboolean  mirror;
mplane_t  *mirror_plane;

qboolean	r_dowarp = true; //waterwarp switch by SuRgEoN

//
// view origin
//
vec3_t  vup;
vec3_t  vpn;
vec3_t  vright;
vec3_t  r_origin;

float r_world_matrix[16];
float r_base_world_matrix[16];

//
// screen size info
//
refdef_t  r_refdef;

mleaf_t   *r_viewleaf, *r_oldviewleaf;

texture_t *r_notexture_mip;

int   d_lightstylevalue[256]; // 8.8 fraction of base light value


void R_MarkLeaves (void);


cvar_t  r_elim_areasize = {"r_elim_areasize", "0.50", true};
cvar_t  r_cullworld = {"r_cullworld","0"}; //Surgeon
cvar_t  r_norefresh = {"r_norefresh","0"};
cvar_t  r_drawentities = {"r_drawentities","1"};
cvar_t  r_drawviewmodel = {"r_drawviewmodel","1"};
cvar_t  r_speeds = {"r_speeds","0"};
cvar_t  r_fullbright = {"r_fullbright","0"};
cvar_t  r_lightmap = {"r_lightmap","0"};
cvar_t  r_shadows = {"r_shadows","0"};
cvar_t  r_mirroralpha = {"r_mirroralpha","1"};
cvar_t  r_wateralpha = {"r_wateralpha","1"};
cvar_t  r_waterwarp = {"r_waterwarp","1"}; //by surgeon
cvar_t  r_dynamic = {"r_dynamic","1"};
cvar_t  r_novis = {"r_novis","0"};

cvar_t  gl_finish = {"gl_finish","0"};
cvar_t  gl_clear = {"gl_clear","0"};
cvar_t  gl_cull = {"gl_cull","1"};
cvar_t  gl_texsort = {"gl_texsort","1"};
cvar_t  gl_smoothmodels = {"gl_smoothmodels","1"};
cvar_t  gl_affinemodels = {"gl_affinemodels","0"};
cvar_t  gl_polyblend = {"gl_polyblend","1"};
cvar_t  gl_flashblend = {"gl_flashblend","1"};
cvar_t  gl_playermip = {"gl_playermip","0"};
cvar_t  gl_nocolors = {"gl_nocolors","0"};
cvar_t  gl_keeptjunctions = {"gl_keeptjunctions","0"};
cvar_t  gl_reporttjunctions = {"gl_reporttjunctions","0"};
cvar_t  gl_doubleeyes = {"gl_doubleeys", "1"};
cvar_t  gl_fog = {"gl_fog", "0"};
cvar_t  gl_glows = {"gl_glows", "1", true};

extern  cvar_t  gl_ztrick;
extern  qboolean gl_warp;

#ifdef AMIGA
cvar_t  r_cull_aggressive = {"r_cull_aggressive","0", true};
cvar_t  r_vertexarrays = {"r_vertexarrays","1", true};
cvar_t  r_model_maxdist = {"r_model_maxdist","2048"};
//default 4096 units
static ULONG r_mod_maxdist;
#endif


typedef struct qglarray_s
{
	float x,y,z;	
	float s,t,w;	//w is for MiniGL
	int xi,yi,zi;	//MiniGL has fast integer pipeline
	ULONG c;	//rgba packed
} qglarray_t;

static qglarray_t *global_qglarray = NULL;

void qgl_InitArrays(void)
{
	int i;

	global_qglarray = malloc(sizeof(qglarray_t)*4096);

	#define maxlight 0xffffffff

	for (i=0; i<4096; i++)
	{
		global_qglarray[i].w = 1.0;
		global_qglarray[i].c = maxlight;
	}

}

void qgl_FreeArrays(void)
{
	free(global_qglarray);
}

static void qgl_EnableArray()
{
#ifdef MINIGL_DISPATCH_CLIENT
	static qboolean checked_arrays;
#endif

	glEnableClientState(GL_VERTEX_ARRAY);

#if defined(OPTIMIZE_060) && !defined(MINIGL_DISPATCH_CLIENT)
	if(r_interpolations.value)
	{
		glVertexPointer(3, GL_FLOAT, sizeof(qglarray_t), &global_qglarray->x);

	}
	else
	{
		glVertexPointer(3, GL_INT, sizeof(qglarray_t), &global_qglarray->xi);
	}

#else
		glVertexPointer(3, GL_FLOAT, sizeof(qglarray_t), &global_qglarray->x);
#endif

	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(3, GL_FLOAT, sizeof(qglarray_t), &global_qglarray->s);

	if(gl_smoothmodels.value)
	{
		glEnableClientState(GL_COLOR_ARRAY);
		glColorPointer(4, MGL_UBYTE_ARGB, sizeof(qglarray_t), &global_qglarray->c);
	}

#ifdef MINIGL_DISPATCH_CLIENT
	if (!checked_arrays)
	{
		GL_CheckErrors("alias vertex arrays");
		checked_arrays = true;
	}
#endif
}

static void qgl_DisableArray()
{
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
}


#ifndef FASTBOPS //made a macro in mathlib.h
/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
  int   i;

  for (i=0 ; i<4 ; i++)
    if (BoxOnPlaneSide (mins, maxs, &frustum[i]) == 2)
      return true;
  return false;
}
#endif

void R_RotateForEntity (entity_t *e)
{
    glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

#ifdef AMIGA
    glRotatefEXT(e->angles[1], GLROT_001);
    glRotatefEXT(-e->angles[0], GLROT_010);
  //ZOID: fixed z angle
    glRotatefEXT(e->angles[2], GLROT_100);
#else
    glRotatef (e->angles[1],  0, 0, 1);
    glRotatef (-e->angles[0],  0, 1, 0);
    glRotatef (e->angles[2],  1, 0, 0);
#endif
}

void R_RotateInterpolatedForEntity (entity_t *e)
{
              float blend;
              vec3_t d;
              int i;

              // positional interpolation

              if (e->translate_start_time == 0)
              {
                  e->translate_start_time = realtime;
                  VectorCopy (e->origin, e->origin1);
                  VectorCopy (e->origin, e->origin2);
              }

              if (!VectorCompare (e->origin, e->origin2))
              {
                  e->translate_start_time = realtime;
                  VectorCopy (e->origin2, e->origin1);
                  VectorCopy (e->origin,  e->origin2);
                  blend = 0;
              }
              else
              {
                  blend = (realtime - e->translate_start_time) / 0.1;

                  if (cl.paused || blend > 1) blend = 1;
              }

              VectorSubtract (e->origin2, e->origin1, d);

              glTranslatef (
                  e->origin1[0] + (blend * d[0]),
                  e->origin1[1] + (blend * d[1]),
                  e->origin1[2] + (blend * d[2]));

              // orientation interpolation (Euler angles, yuck!)

              if (e->rotate_start_time == 0)
              {
                  e->rotate_start_time = realtime;
                  VectorCopy (e->angles, e->angles1);
                  VectorCopy (e->angles, e->angles2);
              }

              if (!VectorCompare (e->angles, e->angles2))
              {
                  e->rotate_start_time = realtime;
                  VectorCopy (e->angles2, e->angles1);
                  VectorCopy (e->angles,  e->angles2);
                  blend = 0;
              }
              else
              {
                  blend = (realtime - e->rotate_start_time) / 0.1;
  
                  if (cl.paused || blend > 1) blend = 1;
              }

              VectorSubtract (e->angles2, e->angles1, d);

              // always interpolate along the shortest path
              for (i = 0; i < 3; i++) 
              {
                  if (d[i] > 180)
                  {
                      d[i] -= 360;
                  }
                  else if (d[i] < -180)
                  {
                      d[i] += 360;
                  }
              }

              glRotatefEXT ( e->angles1[1] + ( blend * d[1]),  GLROT_001);
              glRotatefEXT (-e->angles1[0] + (-blend * d[0]),  GLROT_010);
              glRotatefEXT ( e->angles1[2] + ( blend * d[2]),  GLROT_100);
}


/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame (entity_t *currententity)
{
  msprite_t   *psprite;
  mspritegroup_t  *pspritegroup;
  mspriteframe_t  *pspriteframe;
  int       i, numframes, frame;
  float     *pintervals, fullinterval, targettime, time;

  psprite = currententity->model->cache.data;
  frame = currententity->frame;

  if ((frame >= psprite->numframes) || (frame < 0))
  {
    Con_Printf ("R_DrawSprite: no such frame %d\n", frame);
    frame = 0;
  }

  if (psprite->frames[frame].type == SPR_SINGLE)
  {
    pspriteframe = psprite->frames[frame].frameptr;
  }
  else
  {
    pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
    pintervals = pspritegroup->intervals;
    numframes = pspritegroup->numframes;
    fullinterval = pintervals[numframes-1];

    time = cl.time + currententity->syncbase;

  // when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
  // are positive, so we don't have to worry about division by 0
    targettime = time - ((int)(time / fullinterval)) * fullinterval;

    for (i=0 ; i<(numframes-1) ; i++)
    {
      if (pintervals[i] > targettime)
        break;
    }

    pspriteframe = pspritegroup->frames[i];
  }

  return pspriteframe;
}


/*
=================
R_DrawSpriteModel

=================
*/

void R_DrawSpriteModel (entity_t *e)
{
  vec3_t  point;
  mspriteframe_t  *frame;
  float   *up, *right;
  vec3_t    v_forward, v_right, v_up;
  msprite_t   *psprite;

  // don't even bother culling, because it's just a single
  // polygon without a surface cache
  frame = R_GetSpriteFrame (e);
  psprite = currententity->model->cache.data;

  if (psprite->type == SPR_ORIENTED)
  { // bullet marks on walls
    AngleVectors (currententity->angles, v_forward, v_right, v_up);
    up = v_up;
    right = v_right;
  }
  else
  { // normal sprite
    up = vup;
    right = vright;
  }


if(mtexenabled)  GL_DisableMultitexture();

//Con_DPrintf("drawing spr\n");

  GL_Bind(frame->gl_texturenum);

  glColor3f(1,1,1);
  glEnable(GL_ALPHA_TEST);

  glBegin (GL_QUADS);


#if 1 //VectorMA inlined
  point[0] = e->origin[0] + frame->down*up[0];
  point[1] = e->origin[1] + frame->down*up[1];
  point[2] = e->origin[2] + frame->down*up[2];
  point[0] += frame->left*right[0];
  point[1] += frame->left*right[1];
  point[2] += frame->left*right[2];

  glTexCoord2f (0, 1);
  glVertex3fv (point);

  point[0] = e->origin[0] + frame->up*up[0];
  point[1] = e->origin[1] + frame->up*up[1];
  point[2] = e->origin[2] + frame->up*up[2];
  point[0] += frame->left*right[0];
  point[1] += frame->left*right[1];
  point[2] += frame->left*right[2];

  glTexCoord2f (0, 0);
  glVertex3fv (point);

  point[0] = e->origin[0] + frame->up*up[0];
  point[1] = e->origin[1] + frame->up*up[1];
  point[2] = e->origin[2] + frame->up*up[2];
  point[0] += frame->right*right[0];
  point[1] += frame->right*right[1];
  point[2] += frame->right*right[2];

  glTexCoord2f (1, 0);
  glVertex3fv (point);

  point[0] = e->origin[0] + frame->down*up[0];
  point[1] = e->origin[1] + frame->down*up[1];
  point[2] = e->origin[2] + frame->down*up[2];
  point[0] += frame->right*right[0];
  point[1] += frame->right*right[1];
  point[2] += frame->right*right[2];

  glTexCoord2f (1, 1);
  glVertex3fv (point);

#else

  glTexCoord2f (0, 1);
  VectorMA (e->origin, frame->down, up, point);
  VectorMA (point, frame->left, right, point);
  glVertex3fv (point);

  glTexCoord2f (0, 0);
  VectorMA (e->origin, frame->up, up, point);
  VectorMA (point, frame->left, right, point);
  glVertex3fv (point);

  glTexCoord2f (1, 0);
  VectorMA (e->origin, frame->up, up, point);
  VectorMA (point, frame->right, right, point);
  glVertex3fv (point);

  glTexCoord2f (1, 1);
  VectorMA (e->origin, frame->down, up, point);
  VectorMA (point, frame->right, right, point);
  glVertex3fv (point);

 #endif
 
  glEnd ();

  glDisable(GL_ALPHA_TEST);
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/

/* moved to r_part.c
#define NUMVERTEXNORMALS  162

float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};
*/

vec3_t  shadevector;

float shadelight, ambientlight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
//surgeon: static
static float r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
;

static float *shadedots = r_avertexnormal_dots[0];

int lastposenum;

#ifdef LITFILES
extern vec3_t lightcolor;
extern qboolean eyecandy;
extern int R_LightPointColor (vec3_t p);
#endif

/*
=============
GL_DrawAliasFrame
=============
*/
void GL_DrawAliasInterpolatedFrame (aliashdr_t *paliashdr, int posenum, int oldposenum, int interp) 
{ 
#ifdef LITFILES
    float r,g,b;
#endif
    float s, t; 
    float  l; 
    float interpolations; 
    int  i, j; 
    int  index; 
    trivertx_t *v, *verts, *oldverts; 
    int  list; 
    int  *order; 
    vec3_t point; 
    float *normal; 
    int  count; 

    lastposenum = posenum;
    interpolations = interp/r_interpolations.value; 
    verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata); 
    oldverts = verts; 

    verts += posenum * paliashdr->poseverts; 

    if (oldposenum >= 0) 
       oldverts += oldposenum * paliashdr->poseverts; 
    else 
       oldverts += posenum * paliashdr->poseverts; 

    order = (int *)((byte *)paliashdr + paliashdr->commands); 

#ifndef LITFILES
  if(!gl_smoothmodels.value) //Surgeon 
  {
  	l = shadelight * 1.20;
  	if (l > 255) l = 255.f;

	l *= 1.f / 255.f;
	glColor3f(l,l,l);
  }
#else
  if(!gl_smoothmodels.value) //Surgeon 
  {
	if(eyecandy)
	{
		r = (1.2/255.0)*lightcolor[0]; if(r > 1.0) r = 1.0;
		g = (1.2/255.0)*lightcolor[1]; if(g > 1.0) g = 1.0;
		b = (1.2/255.0)*lightcolor[2]; if(b > 1.0) b = 1.0;
		glColor3f(r,g,b);
	}
	else
	{
  		l = shadelight * 1.20;
  		if (l > 255) l = 255.f;

		l *= 1.f / 255.f;
		glColor3f(l,l,l);
	}
  }
#endif

    while (1) 
    { 
       // get the vertex count and primitive type 
       count = *order++; 
       if (!count) 
          break;  // done 
       if (count <  0) 
          { 
          count = -count; 
          glBegin (GL_TRIANGLE_FAN); 
          } 
       else
	    { 
          glBegin (GL_TRIANGLE_STRIP); 
	    }

	if(gl_smoothmodels.value)
	{
        do 
        { 
          // texture coordinates come from the draw list 
          glTexCoord2f (((float *)order)[0], ((float *)order)[1]); 
          order += 2; 
          // normals and vertexes come from the frame list 

#ifndef LITFILES
          l = shadedots[verts->lightnormalindex] * shadelight;
	    if(l>255) l=255; //clamp
	    l *= 1.0 / 255.0;

          glColor3f (l, l, l); 
#else
	    if(eyecandy)
	    {
          l = shadedots[verts->lightnormalindex]*(1.0/255.0);
	    r = l*lightcolor[0]; if(r > 1.0) r = 1.0;
	    g = l*lightcolor[1]; if(g > 1.0) g = 1.0;
	    b = l*lightcolor[2]; if(b > 1.0) b = 1.0;

	    glColor3f(r,g,b);
	    }
	    else
	    {
          l = shadedots[verts->lightnormalindex] * shadelight;
	    if(l>255) l=255; //clamp
	    l *= 1.0 / 255.0;

          glColor3f (l, l, l); 
	    }
#endif

          glVertex3f (oldverts->v[0] + ((verts->v[0] - oldverts->v[0])*interpolations), 
                      oldverts->v[1] + ((verts->v[1] - oldverts->v[1])*interpolations),  
                      oldverts->v[2] + ((verts->v[2] - oldverts->v[2])*interpolations)); 
          verts++; 
          oldverts++; 
        } while (--count);
	}
	else
	{
        do 
        { 
          // texture coordinates come from the draw list 
          glTexCoord2f (((float *)order)[0], ((float *)order)[1]); 
          order += 2; 

          glVertex3f (oldverts->v[0] + ((verts->v[0] - oldverts->v[0])*interpolations), 
                      oldverts->v[1] + ((verts->v[1] - oldverts->v[1])*interpolations),  
                      oldverts->v[2] + ((verts->v[2] - oldverts->v[2])*interpolations)); 
          verts++; 
          oldverts++; 
        } while (--count); 
	}
       glEnd (); 
	} 
} 


void GL_DrawAliasFrame (aliashdr_t *paliashdr, int posenum)
{
#ifdef LITFILES
    float r,g,b;
#endif
  float s, t;
  float   l;
  int   i, j;
  int   index;
  trivertx_t  *v, *verts;
  int   list;
  int   *order;
  vec3_t  point;
  float *normal;
  int   count;

  lastposenum = posenum;

  verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
  verts += posenum * paliashdr->poseverts;
  order = (int *)((byte *)paliashdr + paliashdr->commands);

#ifndef LITFILES
  if(!gl_smoothmodels.value) //Surgeon 
  {
  	l = shadelight * 1.20;
  	if (l > 255) l = 255.f;

	l *= 1.f / 255.f;
	glColor3f(l,l,l);
  }
#else
  if(!gl_smoothmodels.value) //Surgeon 
  {
	if(eyecandy)
	{
	r = (1.2/255.0)*lightcolor[0]; if(r > 1.0) r = 1.0;
	g = (1.2/255.0)*lightcolor[1]; if(g > 1.0) g = 1.0;
	b = (1.2/255.0)*lightcolor[2]; if(b > 1.0) b = 1.0;

	glColor3f(r,g,b);
	}
	else
	{
  	l = shadelight * 1.20;
  	if (l > 255) l = 255.f;

	l *= 1.f / 255.f;
	glColor3f(l,l,l);
	}
  }
#endif


  while (1)
  {
    // get the vertex count and primitive type
    count = *order++;
    if (!count)
      break;    // done
    if (count < 0)
    {
      count = -count;
      glBegin (GL_TRIANGLE_FAN);
    }
    else
    {
      glBegin (GL_TRIANGLE_STRIP);
    }

// Surgeon: HACK - flatshading implemented - works fine

  if (gl_smoothmodels.value)
    do
    {
      // texture coordinates come from the draw list
      glTexCoord2f (((float *)order)[0], ((float *)order)[1]); 
      order += 2;

      // normals and vertexes come from the frame list

#ifndef LITFILES
      l = shadedots[verts->lightnormalindex] * shadelight;

	//printf("%f\n", l); //for debugging

	if(l>255) l=255; //clamp
	l *= 1.0/255.0;

      glColor3f (l, l, l);
#else
	    if(eyecandy)
	    {
          l = shadedots[verts->lightnormalindex]*(1.0/255.0);
	    r = l*lightcolor[0]; if(r > 1.0) r = 1.0;
	    g = l*lightcolor[1]; if(g > 1.0) g = 1.0;
	    b = l*lightcolor[2]; if(b > 1.0) b = 1.0;

	    glColor3f(r,g,b);
	    }
	    else
	    {
          l = shadedots[verts->lightnormalindex] * shadelight;
	    if(l>255) l=255; //clamp
	    l *= 1.0 / 255.0;

          glColor3f (l, l, l); 
	    }
#endif

      glVertex3f (verts->v[0], verts->v[1], verts->v[2]);

      verts++;
    } while (--count);

  else //moved glColor outside loop
  {
   do
    {
      // texture coordinates come from the draw list

      glTexCoord2f (((float *)order)[0], ((float *)order)[1]); 

      order += 2;

      glVertex3f (verts->v[0], verts->v[1], verts->v[2]);

      verts++;
    } while (--count);
  }
    glEnd ();
  }
}



#if 0
static GLsizei fcount[256];
static GLsizei scount[256];
static GLint   ffirst[256];
static GLint   sfirst[256];
#endif

#ifndef __PPC__
/*
** Surgeon: used for vertexarrays
** Table of 256 unsigned longs mimmicking the rgba ubytes
*/

const ULONG lighttab[256] = {
#include "whitelight.h"
};
#endif

#ifdef MINIGL_DISPATCH_CLIENT
/*
 * Dispatch client: the whole model as ONE indexed triangle list.
 * Quake's glcmds are short strips and fans; drawn one glDrawArrays each,
 * every command became its own draw record with a full TCL state header
 * (strips/fans cannot merge in the R200 backend), which made alias models
 * the most expensive part of a frame on the 68060. The vertices of all
 * commands go into the vertex array back to back and the strips/fans are
 * expanded into a GL_TRIANGLES index list, which takes the backend's
 * fused indexed path (one record, no per-command state).
 */
#define ALIAS_MAX_VERTS 4096
#define ALIAS_MAX_INDICES (3 * ALIAS_MAX_VERTS)
static UWORD alias_indices[ALIAS_MAX_INDICES];

#ifdef MGL68K_RESIDENT
/*
 * minigl68k resident alias draw: the model's command vertices are written
 * straight into a VRAM ring buffer (compact TCL layout, little-endian) and
 * drawn as ONE indexed triangle list; the Radeon's TCL transforms them.
 * No per-vertex work in the library and no triangle expansion.
 */
extern qboolean R_ResidentDynBegin (int numverts, volatile ULONG **dst,
  ULONG *handle, ULONG *offset);
extern qboolean R_ResidentDynDraw (ULONG handle, ULONG offset,
  const unsigned short *indices, int count);
extern int R_ResidentModelIndices (const void *key, int verts, int count,
  int hash, const unsigned short *indices);
extern qboolean R_ResidentIndexedDraw (ULONG handle, ULONG offset,
  int idxoffset, int count);

static ULONG A_SwapLE (ULONG v)
{
  return (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24);
}

extern int R_StoreGet (const void *key, int tag, int bytes,
  volatile ULONG **fill);
extern qboolean R_ResidentStreams (int posoff, ULONG colhandle, ULONG coloff,
  int texoff, int idxoff, int count);

/* Alias colour of one vertex (GLQuake's shadedots lighting) */
static ULONG R_AliasColour (const trivertx_t *v, qboolean smooth, ULONG flatc)
{
  if (!smooth)
    return flatc;
#ifdef LITFILES
  if (eyecandy)
  {
    float l = shadedots[v->lightnormalindex];
    ULONG r = (ULONG)(lightcolor[0]*l);
    ULONG g = (ULONG)(lightcolor[1]*l);
    ULONG b = (ULONG)(lightcolor[2]*l);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return 0xFF000000 | (r<<16) | (g<<8) | b;
  }
#endif
  {
    ULONG l = (ULONG)(shadedots[v->lightnormalindex] * shadelight);
    if (l > 255) l = 255;
    return lighttab[l];
  }
}

/* Streams: texcoords per model and positions per pose stay in VRAM (sent
 * once); a frame writes one colour dword a vertex. */
/* VRAM keys of the model being drawn: its model_t (fixed for the level)
 * and pose. The alias data itself lives in Quake's cache, which can evict
 * and reload it elsewhere (sound loading at a pickup did): keyed by the
 * data's address, a stale entry then served another model's positions. */
static const void *alias_key;
static int alias_pose;

static qboolean R_DrawAliasStreams (trivertx_t *verts, int *order,
  qboolean smooth, ULONG flatc, int total, int itotal, int idxoff)
{
  volatile ULONG *fill, *d;
  int texoff, posoff, i, count;
  ULONG handle, offset;
  int *o;
  union { float f; ULONG u; } x;

  if (idxoff < 0 || !r68k_now)
    return false;
  texoff = R_StoreGet (alias_key, 1, total * 8, &fill);
  if (texoff < 0)
    return false;
  if (fill)
  {
    o = order;
    while ((count = *o++) != 0)
    {
      if (count < 0)
        count = -count;
      do
      {
        x.f = ((float *)o)[0]; *fill++ = A_SwapLE (x.u);
        x.f = ((float *)o)[1]; *fill++ = A_SwapLE (x.u);
        o += 2;
      } while (--count);
    }
    (void)fill[-1];
  }
  posoff = R_StoreGet (alias_key, 2 + alias_pose, total * 12, &fill);
  if (posoff < 0)
    return false;
  if (fill)
  {
    const trivertx_t *v = verts;
    for (i = 0; i < total; i++, v++)
    {
      x.f = v->v[0]; *fill++ = A_SwapLE (x.u);
      x.f = v->v[1]; *fill++ = A_SwapLE (x.u);
      x.f = v->v[2]; *fill++ = A_SwapLE (x.u);
    }
    (void)fill[-1];
  }
  if (!R_ResidentDynBegin ((total * 4 + 27) / 28, &d, &handle, &offset))
    return false;
  if (smooth)
  {
    const trivertx_t *v = verts;
    for (i = 0; i < total; i++, v++)
      d[i] = A_SwapLE (R_AliasColour (v, true, flatc));
  }
  else
  {
    ULONG c = A_SwapLE (flatc);
    for (i = 0; i < total; i++)
      d[i] = c;
  }
  (void)d[total - 1];
  return R_ResidentStreams (posoff, handle, offset, texoff, idxoff, itotal);
}

static qboolean R_DrawAliasResident (aliashdr_t *paliashdr,
  trivertx_t *verts, int *order, qboolean smooth)
{
  int *order0 = order;
  int *o = order;
  int count, total = 0, icount = 0, base = 0, i;
  int itotal = 0, hash = 0, idxoff;
  qboolean gen;
  volatile ULONG *d;
  ULONG handle, offset, flatc = 0xffffffff;
  union { float f; ULONG u; } x, y, z, s, t;

  /* size of the vertex list */
  while ((count = *o++) != 0)
  {
    hash = hash * 31 + count;
    if (count < 0)
      count = -count;
    total += count;
    itotal += (count - 2) * 3;
    o += 2 * count;
  }
  /* the index list is the same every frame: kept in VRAM per model */
  idxoff = R_ResidentModelIndices (alias_key, total, itotal, hash, NULL);
  gen = idxoff < 0;
  if (total > ALIAS_MAX_VERTS)
    return false;
  { extern int r68k_alias_ents, r68k_alias_verts;
    r68k_alias_ents++; r68k_alias_verts += total; }

  if (!smooth)
  {
    float lr, lg, lb;
#ifdef LITFILES
    if (eyecandy)
    {
      lr = (1.2/255.0)*lightcolor[0]; if (lr > 1.0) lr = 1.0;
      lg = (1.2/255.0)*lightcolor[1]; if (lg > 1.0) lg = 1.0;
      lb = (1.2/255.0)*lightcolor[2]; if (lb > 1.0) lb = 1.0;
    }
    else
#endif
    {
      float l = shadelight * 1.20;
      if (l > 255) l = 255.f;
      lr = lg = lb = l * (1.f / 255.f);
    }
    flatc = 0xff000000 | ((ULONG)(lr * 255.f) << 16) |
            ((ULONG)(lg * 255.f) << 8) | (ULONG)(lb * 255.f);
  }

  if (!gen && R_DrawAliasStreams (verts, order0, smooth, flatc, total,
      itotal, idxoff))
    return true;
  if (!R_ResidentDynBegin (total, &d, &handle, &offset))
    return false;

  while ((count = *order++) != 0)
  {
    qboolean fan = count < 0;
    if (fan)
      count = -count;
    if (gen)
    for (i = 2; i < count; i++)
    {
      if (fan)
      {
        alias_indices[icount++] = base;
        alias_indices[icount++] = base + i - 1;
        alias_indices[icount++] = base + i;
      }
      else if (i & 1)
      {
        alias_indices[icount++] = base + i - 1;
        alias_indices[icount++] = base + i - 2;
        alias_indices[icount++] = base + i;
      }
      else
      {
        alias_indices[icount++] = base + i - 2;
        alias_indices[icount++] = base + i - 1;
        alias_indices[icount++] = base + i;
      }
    }
    base += count;
    do
    {
      ULONG c = flatc;
      s.f = ((float *)order)[0];
      t.f = ((float *)order)[1];
      order += 2;
      if (smooth)
      {
#ifdef LITFILES
        if (eyecandy)
        {
          float l = shadedots[verts->lightnormalindex];
          ULONG r = (ULONG)(lightcolor[0]*l);
          ULONG g = (ULONG)(lightcolor[1]*l);
          ULONG b = (ULONG)(lightcolor[2]*l);
          if (r > 255) r = 255;
          if (g > 255) g = 255;
          if (b > 255) b = 255;
          c = 0xFF000000 | (r<<16) | (g<<8) | b;
        }
        else
#endif
        {
          ULONG l = (ULONG)(shadedots[verts->lightnormalindex] * shadelight);
          if (l > 255) l = 255;
          c = lighttab[l];
        }
      }
      x.f = verts->v[0];
      y.f = verts->v[1];
      z.f = verts->v[2];
      d[0] = A_SwapLE (x.u);
      d[1] = A_SwapLE (y.u);
      d[2] = A_SwapLE (z.u);
      if (r68k_now)
      {
        d[3] = A_SwapLE (c);
        d[4] = A_SwapLE (s.u);
        d[5] = A_SwapLE (t.u);
        d += 6;
      }
      else
      {
        d[3] = 0x0000803f;
        d[4] = A_SwapLE (c);
        d[5] = A_SwapLE (s.u);
        d[6] = A_SwapLE (t.u);
        d += 7;
      }
      verts++;
    } while (--count);
  }
  (void)d[-1];
  if (gen && icount)
    idxoff = R_ResidentModelIndices (alias_key, total, itotal, hash,
      alias_indices);
  if (idxoff >= 0 && R_ResidentIndexedDraw (handle, offset, idxoff, itotal))
    return true;
  if (!gen)
    return false;   /* nothing drawn; the caller takes its own path */
  return icount ? R_ResidentDynDraw (handle, offset, alias_indices, icount)
                : true;
}
#endif

void GL_DrawAliasArrayFrame (aliashdr_t *paliashdr, int posenum)
{
#ifdef LITFILES
  ULONG r,g,b;
#endif
  float   l;
  trivertx_t  *verts;
  int   *order;
  int   count, i;
  int   vcount = 0, icount = 0;
  qglarray_t *lva = global_qglarray;
  qboolean smooth = gl_smoothmodels.value != 0;

  lastposenum = posenum;

  verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
  verts += posenum * paliashdr->poseverts;
  order = (int *)((byte *)paliashdr + paliashdr->commands);

#ifdef MGL68K_RESIDENT
  alias_key = currententity->model;
  alias_pose = posenum;
  if (alias_key && R_DrawAliasResident (paliashdr, verts, order, smooth))
    return;
#endif

  if (!smooth)
  {
#ifdef LITFILES
    if (eyecandy)
    {
      float fr,fg,fb;
      fr = (1.2/255.0)*lightcolor[0]; if(fr > 1.0) fr = 1.0;
      fg = (1.2/255.0)*lightcolor[1]; if(fg > 1.0) fg = 1.0;
      fb = (1.2/255.0)*lightcolor[2]; if(fb > 1.0) fb = 1.0;
      glColor3f(fr, fg, fb);
    }
    else
#endif
    {
      l = shadelight * 1.20;
      if (l > 255) l = 255.f;
      l *= 1.f / 255.f;
      glColor3f(l,l,l);
    }
  }

  while (1)
  {
    int base;
    qboolean fan;

    count = *order++;
    if (!count)
      break;
    fan = count < 0;
    if (fan)
      count = -count;

    if (vcount + count > ALIAS_MAX_VERTS ||
        icount + 3 * (count - 2) > ALIAS_MAX_INDICES)
    {
      if (icount)
        glDrawElements (GL_TRIANGLES, icount, GL_UNSIGNED_SHORT, alias_indices);
      vcount = icount = 0;
      lva = global_qglarray;
    }

    base = vcount;
    if (fan)
    {
      for (i = 2; i < count; i++)
      {
        alias_indices[icount++] = base;
        alias_indices[icount++] = base + i - 1;
        alias_indices[icount++] = base + i;
      }
    }
    else
    {
      for (i = 2; i < count; i++)
      {
        if (i & 1)
        {
          alias_indices[icount++] = base + i - 1;
          alias_indices[icount++] = base + i - 2;
          alias_indices[icount++] = base + i;
        }
        else
        {
          alias_indices[icount++] = base + i - 2;
          alias_indices[icount++] = base + i - 1;
          alias_indices[icount++] = base + i;
        }
      }
    }
    vcount += count;

    do
    {
      lva->s = ((float *)order)[0];
      lva->t = ((float *)order)[1];
      order += 2;
      if (smooth)
      {
#ifdef LITFILES
        if (eyecandy)
        {
          l = shadedots[verts->lightnormalindex];
          r = (ULONG)(lightcolor[0]*l);
          g = (ULONG)(lightcolor[1]*l);
          b = (ULONG)(lightcolor[2]*l);
          if(r > 255) r = 255;
          if(g > 255) g = 255;
          if(b > 255) b = 255;
          lva->c = (0xFF000000 | (r<<16) | (g<<8) | b);
        }
        else
#endif
        {
          ULONG c = (ULONG)(shadedots[verts->lightnormalindex] * shadelight);
          if (c > 255) c = 255;
          lva->c = lighttab[c];
        }
      }
      /* float positions: the R200 frontend's direct array fast path */
      lva->x = (float)verts->v[0];
      lva->y = (float)verts->v[1];
      lva->z = (float)verts->v[2];
      lva++;
      verts++;
    } while (--count);
  }
  if (icount)
    glDrawElements (GL_TRIANGLES, icount, GL_UNSIGNED_SHORT, alias_indices);
}
#else

void GL_DrawAliasArrayFrame (aliashdr_t *paliashdr, int posenum)
{
#ifdef LITFILES
    ULONG r,g,b;
#endif
  float   l;
  trivertx_t  *verts;
  int   *order;
  int   count;
  int   vcount;
  GLenum prim;
  qglarray_t *lva;

#if 0
  int fans, strips;

  fans = 0;
  strips = 0;
  vcount = 0;
  lva = global_qglarray;
#endif

  lastposenum = posenum;

  verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
  verts += posenum * paliashdr->poseverts;
  order = (int *)((byte *)paliashdr + paliashdr->commands);

#ifndef LITFILES
  if(!gl_smoothmodels.value) //Surgeon 
  {
  	l = shadelight * 1.20;
  	if (l > 255) l = 255.f;

	l *= 1.f / 255.f;
	glColor3f(l,l,l);
  }
#else
  if(!gl_smoothmodels.value) //Surgeon 
  {
	if(eyecandy)
	{
      float fr,fg,fb;

	fr = (1.2/255.0)*lightcolor[0]; if(fr > 1.0) fr = 1.0;
	fg = (1.2/255.0)*lightcolor[1]; if(fg > 1.0) fg = 1.0;
	fb = (1.2/255.0)*lightcolor[2]; if(fb > 1.0) fb = 1.0;

	glColor3f(fr, fg, fb);
	}
	else
	{
  	l = shadelight * 1.20;
  	if (l > 255) l = 255.f;

	l *= 1.f / 255.f;
	glColor3f(l,l,l);
	}
  }
#endif

  while (1)
  {
    // get the vertex count and primitive type
    count = *order++;

    if (!count)
      break;    // done
#if 1
    if (count < 0)
    {
		count = -count;
		prim = GL_TRIANGLE_FAN;
		vcount = count;
		lva = global_qglarray;
    }
    else
    {
		prim = GL_TRIANGLE_STRIP;
		vcount = count;
		lva = global_qglarray;
    }
#else
    if (count < 0)
    {
		count = -count;
		ffirst[fans] = vcount;
		fcount[fans] = count;
		vcount += count;
		fans++;
    }
    else
    {
		sfirst[strips] = vcount;
		scount[strips] = count;
		strips++;
		vcount += count;
    }
#endif


// Surgeon: HACK - flatshading implemented - works fine

  if (gl_smoothmodels.value)
  {
    ULONG c;
    do
    {
      // texture coordinates come from the draw list
	lva->s = ((float *)order++)[0];
	lva->t = ((float *)order++)[0];

      // normals and vertexes come from the frame list
#ifndef LITFILES
      c = (ULONG)(shadedots[verts->lightnormalindex] * shadelight);
	if(c>255) c=255; //clamp

	#ifndef __PPC__
	lva->c = lighttab[c];
	#else
	lva->c = 0xFF000000 | (c<<16) | (c<<8) | c;
	#endif
#else
	if(eyecandy)
	{
	l = shadedots[verts->lightnormalindex];

	r = (ULONG)(lightcolor[0]*l);
	g = (ULONG)(lightcolor[1]*l);
	b = (ULONG)(lightcolor[2]*l);
 
	if(r > 255) r = 255;
	if(g > 255) g = 255;
	if(b > 255) b = 255;

	lva->c = (0xFF000000 | (r<<16) | (g<<8) | b);
	}
	else
	{
      c = (ULONG)(shadedots[verts->lightnormalindex] * shadelight);
	if(c>255) c=255; //clamp

	#ifndef __PPC__
	lva->c = lighttab[c];
	#else
	lva->c = 0xFF000000 | (c<<16) | (c<<8) | c;
	#endif
	}
#endif
#ifdef OPTIMIZE_060
	lva->xi = verts->v[0];
	lva->yi = verts->v[1];
	lva->zi = verts->v[2];
#else
	lva->x = (float)verts->v[0];
	lva->y = (float)verts->v[1];
	lva->z = (float)verts->v[2];
#endif
	lva++;
      verts++;
    } while (--count);
  }

  else //glColor moved outside loop
  {
    do
    {
      // texture coordinates come from the draw list
	lva->s = ((float *)order++)[0];
	lva->t = ((float *)order++)[0];

#ifdef OPTIMIZE_060
	lva->xi = verts->v[0];
	lva->yi = verts->v[1];
	lva->zi = verts->v[2];
#else
	lva->x = (float)verts->v[0];
	lva->y = (float)verts->v[1];
	lva->z = (float)verts->v[2];
#endif
	lva++;
      verts++;
    } while (--count);
  }

	#if 0
	if(r_vertexarrays.value == 2)
	{
	  UWORD index[256];
	  for (count=0; count<vcount; count++) index[count] = count;
  
	  glDrawElements(prim, vcount, GL_UNSIGNED_SHORT, (void *)index);
	}
	else
	#endif

	glDrawArrays(prim, 0, vcount);
  }

#if 0
  if(fans)
  {
	glMultiDrawArrays(GL_TRIANGLE_FAN, ffirst, fcount, fans);
  }

  if(strips)
  {
	glMultiDrawArrays(GL_TRIANGLE_STRIP, sfirst, scount, strips);
  }
#endif
}
#endif /* MINIGL_DISPATCH_CLIENT */


#if 0
//test for MiniGL:

void GL_DrawAliasTrisFrame (aliashdr_t *paliashdr, int posenum)
{
#ifdef LITFILES
    ULONG r,g,b;
#endif
  float   l;
  trivertx_t  *verts;
  int   *order;
  int   count;
  GLenum primitive;
  ULONG vcount;
  qglarray_t *lva;
  float cbuf;

  int i,k;
  int vamode;
  UWORD index[256];

  index[0] = 0;
  index[1] = 1;
  index[2] = 2;

  vamode = (int)r_vertexarrays.value;
  lastposenum = posenum;

  verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
  verts += posenum * paliashdr->poseverts;
  order = (int *)((byte *)paliashdr + paliashdr->commands);

#ifndef LITFILES
  if(!gl_smoothmodels.value) //Surgeon 
  {
  	l = shadelight * 1.20;
  	if (l > 255) l = 255.f;

	l *= 1.f / 255.f;
	glColor3f(l,l,l);
  }
#else
  if(!gl_smoothmodels.value) //Surgeon 
  {
	if(eyecandy)
	{
      float fr,fg,fb;

	fr = (1.2/255.0)*lightcolor[0]; if(fr > 1.0) fr = 1.0;
	fg = (1.2/255.0)*lightcolor[1]; if(fg > 1.0) fg = 1.0;
	fb = (1.2/255.0)*lightcolor[2]; if(fb > 1.0) fb = 1.0;

	glColor3f(fr, fg, fb);
	}
	else
	{
  	l = shadelight * 1.20;
  	if (l > 255) l = 255.f;

	l *= 1.f / 255.f;
	glColor3f(l,l,l);
	}
  }
#endif

  while (1)
  {
    // get the vertex count and primitive type
    count = *order++;

    if (!count)
      break;    // done

    k = 3;

    if (count < 0)
    {
      count = -count;
	for(i=3; i<count; i++, k+=3)
	{
	  index[k] = 0;
	  index[k+1] = i-1;
	  index[k+2] = i;
	}
    }
    else
    {
	for(i=3; i<count; i++, k+=3)
	{
	  if(k%2)
	  {
	  index[k]   = i;
	  index[k+1] = i-1;
	  index[k+2] = i-2;
	  }
	  else
	  {
	  index[k]   = i-2;
	  index[k+1] = i-1;
	  index[k+2] = i;
	  }
	}
    }
	lva = &global_qglarray[0];
	vcount = count;

// Surgeon: HACK - flatshading implemented - works fine

  if (gl_smoothmodels.value)
  {
    ULONG c;
    do
    {
      // texture coordinates come from the draw list
	lva->s = ((float *)order++)[0];
	lva->t = ((float *)order++)[0];

      // normals and vertexes come from the frame list
#ifndef LITFILES
      c = (ULONG)(shadedots[verts->lightnormalindex] * shadelight);
	if(c>255) c=255; //clamp

	#ifndef __PPC__
	lva->c = lighttab[c];
	#else
	lva->c = 0xFF000000 | (c<<16) | (c<<8) | c;
	#endif
#else
	if(eyecandy)
	{
	l = shadedots[verts->lightnormalindex];

	r = (ULONG)(lightcolor[0]*l);
	g = (ULONG)(lightcolor[1]*l);
	b = (ULONG)(lightcolor[2]*l);
 
	if(r > 255) r = 255;
	if(g > 255) g = 255;
	if(b > 255) b = 255;

	lva->c = (0xFF000000 | (r<<16) | (g<<8) | b);
	}
	else
	{
      c = (ULONG)(shadedots[verts->lightnormalindex] * shadelight);
	if(c>255) c=255; //clamp

	#ifndef __PPC__
	lva->c = lighttab[c];
	#else
	lva->c = 0xFF000000 | (c<<16) | (c<<8) | c;
	#endif
	}
#endif
#ifdef OPTIMIZE_060
	lva->xi = verts->v[0];
	lva->yi = verts->v[1];
	lva->zi = verts->v[2];
#else
	lva->x = (float)verts->v[0];
	lva->y = (float)verts->v[1];
	lva->z = (float)verts->v[2];
#endif
	lva++;
      verts++;
    } while (--count);
  }
  else //glColor moved outside loop
  {
    do
    {
      // texture coordinates come from the draw list
	lva->s = ((float *)order++)[0];
	lva->t = ((float *)order++)[0];

#ifdef OPTIMIZE_060
	lva->xi = verts->v[0];
	lva->yi = verts->v[1];
	lva->zi = verts->v[2];
#else
	lva->x = (float)verts->v[0];
	lva->y = (float)verts->v[1];
	lva->z = (float)verts->v[2];
#endif

	lva++;
      verts++;
    } while (--count);
  }

	if(r_vertexarrays.value == 3)
	{
	   glDrawElements(GL_TRIANGLES, k, GL_UNSIGNED_SHORT, (void *)index);
	}
	else
	{
	   glLockArrays(0, vcount);

	   glDrawElements(GL_TRIANGLES, k, GL_UNSIGNED_SHORT, (void *)index);

	   glUnlockArrays();
	}
  }
}
#endif

void GL_DrawAliasArrayInterpolatedFrame (aliashdr_t *paliashdr, int posenum, int oldposenum, int interp)
{
#ifdef LITFILES
    ULONG r,g,b;
#endif
    GLenum primitive;
    ULONG vcount;
    qglarray_t *lva;
    float s, t; 
    float  l; 
    float interpolations; 
    int  i, j; 
    int  index; 
    trivertx_t *v, *verts, *oldverts; 
    int  list; 
    int  *order; 
    vec3_t point; 
    float *normal; 
    int  count; 
    float cbuf;


    lastposenum = posenum;
    interpolations = interp/r_interpolations.value; 
    verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata); 
    oldverts = verts; 
    verts += posenum * paliashdr->poseverts; 
    if (oldposenum >= 0) 
       oldverts += oldposenum * paliashdr->poseverts; 
    else 
       oldverts += posenum * paliashdr->poseverts; 

    order = (int *)((byte *)paliashdr + paliashdr->commands); 


#ifndef LITFILES
  if(!gl_smoothmodels.value) //Surgeon 
  {
  	l = shadelight * 1.20;
  	if (l > 255) l = 255.f;

	l *= 1.f / 255.f;
	glColor3f(l,l,l);
  }
#else
  if(!gl_smoothmodels.value) //Surgeon 
  {
	if(eyecandy)
	{
      float fr,fg,fb;

	fr = (1.2/255.0)*lightcolor[0]; if(fr > 1.0) fr = 1.0;
	fg = (1.2/255.0)*lightcolor[1]; if(fg > 1.0) fg = 1.0;
	fb = (1.2/255.0)*lightcolor[2]; if(fb > 1.0) fb = 1.0;

	glColor3f(fr, fg, fb);
	}
	else
	{
  	l = shadelight * 1.20;
  	if (l > 255) l = 255.f;

	l *= 1.f / 255.f;
	glColor3f(l,l,l);
	}
  }
#endif

  while (1)
  {
    // get the vertex count and primitive type
    count = *order++;

    if (!count)
      break;    // done

    if (count < 0)
    {
      count = -count;
		primitive = GL_TRIANGLE_FAN;
    }
    else
    {
		primitive = GL_TRIANGLE_STRIP;
    }

	lva = &global_qglarray[0];

	vcount = count;

// Surgeon: HACK - flatshading implemented - works fine

  if (gl_smoothmodels.value)
  {
    ULONG c;
    do
    {
      // texture coordinates come from the draw list
	lva->s = ((float *)order)[0];
	lva->t = ((float *)order)[1];

      order += 2;

      // normals and vertexes come from the frame list

#ifndef LITFILES
      c = (ULONG)(shadedots[verts->lightnormalindex] * shadelight);
	if(c>255) c=255; //clamp

	#ifndef __PPC__
	lva->c = lighttab[c];
	#else
	lva->c = 0xFF000000 | (c<<16) | (c<<8) | c;
	#endif
#else
	if(eyecandy)
	{
	l = shadedots[verts->lightnormalindex];

	r = (ULONG)(lightcolor[0]*l);
	g = (ULONG)(lightcolor[1]*l);
	b = (ULONG)(lightcolor[2]*l);
 
	if(r > 255) r = 255;
	if(g > 255) g = 255;
	if(b > 255) b = 255;

	lva->c = (0xFF000000 | (r<<16) | (g<<8) | b);
	}
	else
	{
      c = (ULONG)(shadedots[verts->lightnormalindex] * shadelight);
	if(c>255) c=255; //clamp

	#ifndef __PPC__
	lva->c = lighttab[c];
	#else
	lva->c = 0xFF000000 | (c<<16) | (c<<8) | c;
	#endif
	}
#endif

	lva->x = (oldverts->v[0] + ((verts->v[0] - oldverts->v[0])*interpolations));
	lva->y = (oldverts->v[1] + ((verts->v[1] - oldverts->v[1])*interpolations));
	lva->z = (oldverts->v[2] + ((verts->v[2] - oldverts->v[2])*interpolations));
	
	lva++;
	oldverts++;
      verts++;
    } while (--count);
  }
  else //glColor moved outside loop
  {
    do
    {
      // texture coordinates come from the draw list
	lva->s = ((float *)order)[0];
	lva->t = ((float *)order)[1];

      order += 2;

	lva->x = (oldverts->v[0] + ((verts->v[0] - oldverts->v[0])*interpolations));
	lva->y = (oldverts->v[1] + ((verts->v[1] - oldverts->v[1])*interpolations));
	lva->z = (oldverts->v[2] + ((verts->v[2] - oldverts->v[2])*interpolations));

	
	lva++;
	oldverts++;
      verts++;
    } while (--count);
  }

	glDrawArrays(primitive, 0, vcount);
  }
}

/*
=============
GL_DrawAliasShadow
=============
*/
extern  vec3_t      lightspot;

void GL_DrawAliasShadow (aliashdr_t *paliashdr, int posenum)
{
  float s, t, l;
  int   i, j;
  int   index;
  trivertx_t  *v, *verts;
  int   list;
  int   *order;
  vec3_t  point;
  float *normal;
  float height, lheight;
  int   count;

  lheight = currententity->origin[2] - lightspot[2];

  height = 0;
  verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
  verts += posenum * paliashdr->poseverts;
  order = (int *)((byte *)paliashdr + paliashdr->commands);

  height = -lheight + 1.0;

  while (1)
  {
    // get the vertex count and primitive type
    count = *order++;
    if (!count)
      break;    // done
    if (count < 0)
    {
      count = -count;
      glBegin (GL_TRIANGLE_FAN);
    }
    else
    {
      glBegin (GL_TRIANGLE_STRIP);
    }

    do
    {
      // texture coordinates come from the draw list
      // (skipped for shadows) glTexCoord2fv ((float *)order);
      order += 2;

      // normals and vertexes come from the frame list
      point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
      point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
      point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

      point[0] -= shadevector[0]*(point[2]+lheight);
      point[1] -= shadevector[1]*(point[2]+lheight);
      point[2] = height;
//      height -= 0.001;
      glVertex3fv (point);

      verts++;
    } while (--count);

    glEnd ();
  } 
}


/*
=================
R_SetupAliasFrame

=================
*/
void R_SetupAliasInterpolatedFrame (int frame, int lastframe, float interp, aliashdr_t *paliashdr) 
{ 
    int   pose, numposes, oldpose; 
    float   interval; 


    if ((frame >= paliashdr->numframes) || (frame <  0)) 
       { 
       Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame); 
       frame = 0; 
       } 
    if ((lastframe >= paliashdr->numframes) || (lastframe <  0)) 
       { 
       Con_DPrintf ("R_AliasSetupFrame: no such last frame %d\n", lastframe); 
       lastframe = 0; 
       } 
    pose = paliashdr->frames[frame].firstpose; 
    numposes = paliashdr->frames[frame].numposes; 

    if (numposes > 1) 
       { 
       interval = paliashdr->frames[frame].interval; 
       pose += (int)(cl.time / interval) % numposes; 
       } 

    oldpose = paliashdr->frames[lastframe].firstpose; 
    numposes = paliashdr->frames[lastframe].numposes; 
  
  if (numposes > 1) 
       { 
       interval = paliashdr->frames[lastframe].interval; 
       oldpose += (int)(cl.time / interval) % numposes; 
       } 


  if (!r_vertexarrays.value)
  {
    GL_DrawAliasInterpolatedFrame (paliashdr, pose, oldpose, interp);
  }
  else
  {
	GL_DrawAliasArrayInterpolatedFrame (paliashdr, pose, oldpose, interp);
  }
} 


void R_SetupAliasFrame (int frame, aliashdr_t *paliashdr)
{
  int       pose, numposes;
  float     interval;


  if ((frame >= paliashdr->numframes) || (frame < 0))
  {
    Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
    frame = 0;
  }

  pose = paliashdr->frames[frame].firstpose;
  numposes = paliashdr->frames[frame].numposes;

  if (numposes > 1)
  {
    interval = paliashdr->frames[frame].interval;
    pose += (int)(cl.time / interval) % numposes;
  }


  if (!r_vertexarrays.value)
  {
	GL_DrawAliasFrame (paliashdr, pose);
  }
  else
  {
#if 0
	if (r_vertexarrays.value > 2)
		GL_DrawAliasTrisFrame (paliashdr, pose);
	else
#endif
		GL_DrawAliasArrayFrame (paliashdr, pose);
  }
}



/*
=================
R_DrawAliasModel

=================
*/

#ifdef GLOWEFFECTS
extern void R_AddGlow (entity_t *e); //gl_rlight.c
extern void R_RenderGlows (void);
#endif


/* An entity that has not moved sees the same light until the light
 * styles step (10 Hz): its BSP light trace is reused for up to 0.1 s.
 * Dynamic lights are added per frame after this, as before. */
static int R_LightPointCached (entity_t *e)
{
  static struct { entity_t *e; vec3_t org; double time; int value; } c[64];
  int h = ((unsigned)e >> 4) & 63;
  if (c[h].e == e && VectorCompare (c[h].org, e->origin) &&
      cl.time >= c[h].time && cl.time - c[h].time < 0.1)
    return c[h].value;
  c[h].e = e;
  VectorCopy (e->origin, c[h].org);
  c[h].time = cl.time;
  c[h].value = R_LightPoint (e->origin);
  return c[h].value;
}

void R_DrawAliasModel (entity_t *e)
{
#ifdef LITFILES
  extern vec3_t lightcolor;
#endif
  int     i, j;
  int     lnum;
  vec3_t    dist;
  float   add;
  model_t   *clmodel;
  vec3_t    mins, maxs;
  aliashdr_t  *paliashdr;
  trivertx_t  *verts, *v;
  int     index;
  float   s, t, an;
  int     anim;

  float clipdist;
  vec3_t cvec, mod_orig;
  float distfromplane, reference;
  float d_forward, d_right, d_up;
  float viewfield, vd_ratio;
  extern cvar_t scr_fov;

  clmodel = currententity->model;

  VectorAdd (currententity->origin, clmodel->mins, mins);
  VectorAdd (currententity->origin, clmodel->maxs, maxs);

#ifdef FASTBOPS
//Surgeon: bugfix
  if (currententity->model->flags & EF_ROTATE)
  {
  mins[2] += 16;
  maxs[2] += 16;
  }
#endif

  if (R_CullBox (mins, maxs))
    return;
  unsigned long fp_al = FP_Enter (FP_ALIGHT), fp_as = 0, fp_ad = 0;

//Surgeon: far culling
	cvec[0] = r_refdef.vieworg[0] - currententity->origin[0];
	cvec[1] = r_refdef.vieworg[1] - currententity->origin[1];

  if (currententity->model->flags & EF_ROTATE)
	cvec[2] = r_refdef.vieworg[2] - currententity->origin[2] + 32;
  else
	cvec[2] = r_refdef.vieworg[2] - currententity->origin[2];

	clipdist = (cvec[0]*cvec[0] + cvec[1]*cvec[1] + cvec[2]*cvec[2]);
	if (clipdist > r_mod_maxdist)
		return;


//optional aggressive frustum culling

	if(r_cull_aggressive.value && (clipdist > 256))
	{

	mod_orig[0] = currententity->origin[0];
	mod_orig[1] = currententity->origin[1];

	if (currententity->model->flags & EF_ROTATE)
	{
		mod_orig[2] = currententity->origin[2] + 32;
	}
	else
	{
		mod_orig[2] = currententity->origin[2];
	}

	distfromplane = DotProduct(mod_orig, vpn);
	reference = DotProduct(r_origin, vpn); 
	d_forward = distfromplane - reference;

	if(d_forward < 0.0)
		return;
	else
	{
	viewfield = scr_fov.value;
	if(viewfield > 90.0)
		vd_ratio = viewfield / (90.0 - (viewfield - 90.0));

	else
		vd_ratio = viewfield * (1.0/90.0);

	distfromplane = DotProduct(mod_orig, vright);
	reference = DotProduct(r_origin, vright); 

	d_right = fabs(distfromplane - reference);	

	d_right /= d_forward;

	if(d_right > vd_ratio)
		return;


	distfromplane = DotProduct(mod_orig, vup);
	reference = DotProduct(r_origin, vup); 

	d_up = fabs(distfromplane - reference);	

	d_up /= d_forward;

	if(d_up > vd_ratio * 0.75)
		return;
	}
	}



  VectorCopy (currententity->origin, r_entorigin);
  VectorSubtract (r_origin, r_entorigin, modelorg);


#ifdef LITFILES
if(!eyecandy)
{
#endif

  // Surgeon: HACK HACK HACK -- no lightcalc for torches

  if (strcmp (clmodel->name, "progs/flame2.mdl")
    && strcmp (clmodel->name, "progs/flame.mdl") )
  ambientlight = shadelight = (float)R_LightPointCached (currententity);

  // allways give the gun some light
  if (e == &cl.viewent && ambientlight < 24)
    ambientlight = shadelight = 24;


  if (strcmp (clmodel->name, "progs/flame2.mdl")
    && strcmp (clmodel->name, "progs/flame.mdl") ) //surgeon
  for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
  {
    if (cl_dlights[lnum].die >= cl.time)
    {
      VectorSubtract (currententity->origin,
              cl_dlights[lnum].origin,
              dist);
      add = cl_dlights[lnum].radius - Length(dist);

      if (add > 0) {
        ambientlight += add;
        //ZOID models should be affected by dlights as well
        shadelight += add;
      }
    }
  }

  // clamp lighting so it doesn't overbright as much
  if (ambientlight > 128)
    ambientlight = 128;
  if (ambientlight + shadelight > 192)
    shadelight = 192 - ambientlight;

  // ZOID: never allow players to go totally black
  i = currententity - cl_entities;

  if (i >= 1 && i<=cl.maxclients)
  {
    if (ambientlight < 8)
      ambientlight = shadelight = 8;
  }

  // HACK HACK HACK -- no fullbright colors, so make torches full light
  if (!strcmp (clmodel->name, "progs/flame2.mdl")
    || !strcmp (clmodel->name, "progs/flame.mdl") )
    ambientlight = shadelight = 256;
//Surgeon: this should be done before calculating light!

  shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

//  shadelight = shadelight / 200.0;

    shadelight *= 1.275; //use ubytes unstead
  
if(r_shadows.value) //optimization by SuRgEoN
{
//  an = e->angles[1]/180*M_PI;
  an = e->angles[1]/MPIMUL;
  shadevector[0] = cos(-an);
  shadevector[1] = sin(-an);
  shadevector[2] = 1;
  VectorNormalize (shadevector);
}

#ifdef LITFILES
}
else
{
	// 
	// get lighting information 
	// 
  if (strcmp (clmodel->name, "progs/flame2.mdl")
    && strcmp (clmodel->name, "progs/flame.mdl") ) //surgeon
  { 
	R_LightPointColor(currententity->origin); // LordHavoc: lightcolor is all that matters from this 
  }

	if (e == &cl.viewent) 
	{ 
		if (lightcolor[0] < 24) 
			lightcolor[0] = 24; 
		if (lightcolor[1] < 24) 
			lightcolor[1] = 24; 
		if (lightcolor[2] < 24) 
			lightcolor[2] = 24; 
	} 

	// HACK HACK HACK -- no fullbright colors, so make torches full light 
 
  if (strcmp (clmodel->name, "progs/flame2.mdl")
    && strcmp (clmodel->name, "progs/flame.mdl") ) //surgeon
  {
	for (lnum=0 ; lnum < MAX_DLIGHTS ; lnum++) 
	{ 
		if (cl_dlights[lnum].die >= cl.time) 
		{ 
		VectorSubtract (currententity->origin, 
		cl_dlights[lnum].origin, dist); 

		add = cl_dlights[lnum].radius - Length(dist); 
 
			if (add > 0) 
			{ 
			lightcolor[0] += add * cl_dlights[lnum].color[0]; 
			lightcolor[1] += add * cl_dlights[lnum].color[1]; 
			lightcolor[2] += add * cl_dlights[lnum].color[2]; 
			} 
 		} 
	} 
 
 
	// ZOID: never allow players to go totally black 
	i = currententity - cl_entities; 
	if (i >= 1 && i<=cl.maxclients) 
	{ 
		if (lightcolor[0] < 8) 
			lightcolor[0] = 8; 
		if (lightcolor[1] < 8) 
			lightcolor[1] = 8; 
		if (lightcolor[2] < 8) 
			lightcolor[2] = 8; 
	} 

	VectorScale(lightcolor, 1.275, lightcolor); 

	// clamp lighting so it doesn't overbright 
	if(lightcolor[0] > 255.0) lightcolor[0] = 255.0;
	if(lightcolor[1] > 255.0) lightcolor[1] = 255.0;
	if(lightcolor[2] > 255.0) lightcolor[2] = 255.0;
  }
  else
  {
	lightcolor[0] = lightcolor[1] = lightcolor[2] = 255; 
  }

	shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)]; 

	 
	if(r_shadows.value)
	{
	an = e->angles[1]/180*M_PI; 
	shadevector[0] = cos(-an); 
	shadevector[1] = sin(-an); 
	shadevector[2] = 1; 
	VectorNormalize (shadevector); 
	}
}
#endif

  FP_Exit (FP_ALIGHT, fp_al);
  fp_as = FP_Enter (FP_ASETUP);
  //
  // locate the proper data
  //
  paliashdr = (aliashdr_t *)Mod_Extradata (currententity->model);

  c_alias_polys += paliashdr->numtris;

  //
  // draw all the triangles
  //

  if(mtexenabled) GL_DisableMultitexture();

    glPushMatrix ();

#ifdef MGL68K_RESIDENT
  /* one matrix call instead of translate, three rotates, translate and
   * scale (each a trip through the library) */
  if (!(r_interpolations.value && currententity != &cl.viewent) &&
      !(!strcmp (clmodel->name, "progs/eyes.mdl") && gl_doubleeyes.value))
  {
    float m[16], R[3][3];
    float cy = cos (e->angles[1] * (M_PI / 180)), sy = sin (e->angles[1] * (M_PI / 180));
    float cp = cos (-e->angles[0] * (M_PI / 180)), sp = sin (-e->angles[0] * (M_PI / 180));
    float cr = cos (e->angles[2] * (M_PI / 180)), sr = sin (e->angles[2] * (M_PI / 180));
    float *so = paliashdr->scale_origin, *sc = paliashdr->scale;
    int r, c;
    /* Rz(yaw) * Ry(-pitch) * Rx(roll), as glRotatef builds them */
    R[0][0] = cy*cp; R[0][1] = cy*sp*sr - sy*cr; R[0][2] = cy*sp*cr + sy*sr;
    R[1][0] = sy*cp; R[1][1] = sy*sp*sr + cy*cr; R[1][2] = sy*sp*cr - cy*sr;
    R[2][0] = -sp;   R[2][1] = cp*sr;            R[2][2] = cp*cr;
    for (c = 0; c < 3; c++)
      for (r = 0; r < 3; r++)
        m[c*4 + r] = R[r][c] * sc[c];
    m[3] = m[7] = m[11] = 0;
    for (r = 0; r < 3; r++)
      m[12 + r] = e->origin[r] + R[r][0]*so[0] + R[r][1]*so[1] + R[r][2]*so[2];
    m[15] = 1;
    glMultMatrixf (m);
  }
  else
#endif
  {
if(r_interpolations.value && currententity != &cl.viewent)
  R_RotateInterpolatedForEntity (e);
else
  R_RotateForEntity (e);

  if (!strcmp (clmodel->name, "progs/eyes.mdl") && gl_doubleeyes.value) {
    glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
// double size of eyes, since they are really hard to see in gl
    glScalef (paliashdr->scale[0]*2, paliashdr->scale[1]*2, paliashdr->scale[2]*2);
  } else {
    glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
    glScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
  }
#ifdef MGL68K_RESIDENT
  }
#endif

  anim = (int)(cl.time*10) & 3;
    GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]);

  // we can't dynamically colormap textures, so they are cached
  // seperately for the players.  Heads are just uncolored.
  if (currententity->colormap != vid.colormap && !gl_nocolors.value)
  {
    i = currententity - cl_entities;
    if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
        GL_Bind(playertextures - 1 + i);
  }


  if(r_shadows.value)
  {
  if (gl_smoothmodels.value)
    glShadeModel (GL_SMOOTH);
  else
    glShadeModel (GL_FLAT); //SuRgEoN

  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  }

  FP_Exit (FP_ASETUP, fp_as);
  fp_ad = FP_Enter (FP_ADRAW);
/*
  R_SetupAliasFrame (currententity->frame, paliashdr);
*/
    // Topaz: Interpolation code
    
    if (r_interpolations.value && currententity->interpolation <  r_interpolations.value) 
       currententity->interpolation++;  
    if (r_interpolations.value) 
       { 
       if (currententity->frame != currententity->current_frame) 
          { 
          currententity->last_frame = currententity->current_frame; 
          currententity->current_frame = currententity->frame; 
          currententity->interpolation = 1; 
          } 
       R_SetupAliasInterpolatedFrame (currententity->current_frame,  
                                      currententity->last_frame,  
                                      currententity->interpolation, 
                                      paliashdr); 
       } 
    else 
       R_SetupAliasFrame (currententity->frame, paliashdr); 

  FP_Exit (FP_ADRAW, fp_ad);

  glPopMatrix ();

#ifdef GLOWEFFECTS
if(gl_glows.value)
{
	if( (!strcmp(clmodel->name, "progs/flame.mdl")) || (!strcmp(clmodel->name, "progs/flame2.mdl")) || (!strcmp(clmodel->name, "progs/bolt.mdl")) || (!strcmp(clmodel->name, "progs/bolt2.mdl")) || (!strcmp(clmodel->name, "progs/w_spike.mdl")) || (!strcmp(clmodel->name, "progs/s_light.mdl")) || (!strcmp(clmodel->name, "progs/quaddama.mdl")) || (!strcmp(clmodel->name, "progs/invulner.mdl")) )
	  R_AddGlow (e);
}
#endif

  if (r_shadows.value)
  {
    glPushMatrix ();
    R_RotateForEntity (e);
    glDisable (GL_TEXTURE_2D);
    glEnable (GL_BLEND);
    glColor4f (0,0,0,0.5);

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glShadeModel (GL_FLAT);

    GL_DrawAliasShadow (paliashdr, lastposenum);
    glEnable (GL_TEXTURE_2D);
    glDisable (GL_BLEND);
    glColor4f (1,1,1,1);
    glPopMatrix ();
  }

}


//==================================================================================

void R_DrawViewModel (void);

/*
=============
R_DrawEntitiesOnList
=============
*/

void R_DrawEntitiesOnList (int routine)
{
	int   i;
	model_t *clmodel;
  
	if (!r_drawentities.value)
		return;

	if(routine == 1)
	{
		for (i=0; i<cl_numvisedicts; i++)
		{
		currententity = cl_visedicts[i];
  		if (currententity->model->type != mod_brush)
			continue;

		R_DrawBrushModel(currententity);
		}

	return;
	}

	if(routine == 2)
	{
	// draw sprites seperately, because of alpha blending

		if(mtexenabled) GL_DisableMultitexture();

		if (gl_affinemodels.value)
		#if !defined(AMIGA)
		    glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
		#elif !defined(MINIGL_DISPATCH_CLIENT)
		    glDisable(MGL_PERSPECTIVE_MAPPING);
		#endif

		glDisable(GL_CULL_FACE);
		//glDepthMask(GL_FALSE);
	
		for (i=0;i<cl_numvisedicts;i++)
		{
		currententity = cl_visedicts[i];
		if (currententity->model->type != mod_sprite)
			continue;

		{ unsigned long fp_t = FP_Enter (FP_SPRITES);
		R_DrawSpriteModel(currententity);
		FP_Exit (FP_SPRITES, fp_t); }
		}
		//glDepthMask(GL_TRUE);

		if(gl_cull.value)
		glEnable(GL_CULL_FACE);

		if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);

		if(r_vertexarrays.value)
		qgl_EnableArray();


		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);


		for (i=0; i<cl_numvisedicts; i++)
		{
		currententity = cl_visedicts[i];
		if (currententity->model->type != mod_alias)
			continue;

		R_DrawAliasModel(currententity);
		}

		if(r_vertexarrays.value)
		{ unsigned long fp_t = FP_Enter (FP_VIEWMDL);
		R_DrawViewModel ();
		FP_Exit (FP_VIEWMDL, fp_t); }

		if(r_vertexarrays.value)
		qgl_DisableArray();

		if(!r_vertexarrays.value)
		R_DrawViewModel ();

		if (gl_affinemodels.value)
		#if !defined(AMIGA)
		    glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
		#elif !defined(MINIGL_DISPATCH_CLIENT)
		    glEnable(MGL_PERSPECTIVE_MAPPING);
		#endif

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	return;
	}
}

extern  cvar_t  scr_fov; //Surgeon

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{

  if (!r_drawviewmodel.value)
    return;

//  if(r_framecount < 2) //surgeon
//    return;

  if (scr_fov.value > 90) //surgeon: like soft renderer
    return;

  if (chase_active.value)
    return;

  if (envmap)
    return;

  if (!r_drawentities.value)
    return;

  if (cl.items & IT_INVISIBILITY)
    return;

  if (cl.stats[STAT_HEALTH] <= 0)
    return;

  currententity = &cl.viewent;

  if (!currententity->model)
    return;

  // hack the depth range to prevent view model from poking into walls
  glDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));


  R_DrawAliasModel (currententity);


  glDepthRange (gldepthmin, gldepthmax);
}


#ifdef AMIGA // MiniGL device-coordinate path

/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
  if (!gl_polyblend.value)
    return;
  if (!v_blend[3])
    return;

  if(mtexenabled) GL_DisableMultitexture();

#ifdef MGL68K_RESIDENT
  {
    extern qboolean pb_pending; extern float pb_rgba[4]; extern int pb_height;
    if (r68k_now)
    {
      pb_rgba[0] = v_blend[0]; pb_rgba[1] = v_blend[1];
      pb_rgba[2] = v_blend[2]; pb_rgba[3] = v_blend[3];
      pb_height = vid.height - sb_lines;
      pb_pending = true;
      return;
    }
  }
#endif
#ifdef MINIGL_DISPATCH_CLIENT
  /* Protect the status-bar rows in hardware as well as by geometry. */
  if (sb_lines)
  {
    glEnable (GL_SCISSOR_TEST);
    glScissor (0, sb_lines, vid.width, vid.height - sb_lines);
  }
  Draw_AlphaFill (0, 0, vid.width, vid.height - sb_lines,
                   v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
  if (sb_lines)
    glDisable (GL_SCISSOR_TEST);
  return;
#endif

  glDisable (GL_CULL_FACE);
  glDisable (GL_ALPHA_TEST);
  glDisable (GL_DEPTH_TEST);
  glDisable (GL_TEXTURE_2D);
  glEnable (GL_BLEND);

  glColor4fv (v_blend);

  glBegin (MGL_FLATFAN);

  glVertex2f (0, 0);
  glVertex2f (vid.width, 0);
  glVertex2f (vid.width, vid.height - sb_lines);
  glVertex2f (0, vid.height - sb_lines);

  glEnd ();

  glColor4f (1,1,1,1);

  glDisable (GL_BLEND);
  glEnable (GL_TEXTURE_2D);
//  glEnable (GL_ALPHA_TEST);
}

#else

/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
  if (!gl_polyblend.value)
    return;

  if (!v_blend[3])
    return;
  
  if(mtexenabled) GL_DisableMultitexture();

  glEnable (GL_BLEND);
  glDisable (GL_DEPTH_TEST);
  glDisable (GL_TEXTURE_2D);

    glLoadIdentity ();

    glRotatef (-90,  1, 0, 0);      // put Z going up
    glRotatef (90,  0, 0, 1);     // put Z going up

  glColor4fv (v_blend);

  glBegin (GL_QUADS);

  glVertex3f (10, 100, 100);
  glVertex3f (10, -100, 100);
  glVertex3f (10, -100, -100);
  glVertex3f (10, 100, -100);

  glEnd ();

  glDisable (GL_BLEND);
  glEnable (GL_TEXTURE_2D);
//  glEnable (GL_ALPHA_TEST);
}

#endif


int SignbitsForPlane (mplane_t *out)
{
  int bits, j;

  // for fast box on planeside test

  bits = 0;
  for (j=0 ; j<3 ; j++)
  {
    if (out->normal[j] < 0)
      bits |= 1<<j;
  }
  return bits;
}


void R_SetFrustum (void)
{
  int   i;
  float vpnrx, vpnry; //SuRgEoN

  if (r_refdef.fov_x == 90) 
  {
    // front side is visible

    VectorAdd (vpn, vright, frustum[0].normal);
    VectorSubtract (vpn, vright, frustum[1].normal);

    VectorAdd (vpn, vup, frustum[2].normal);
    VectorSubtract (vpn, vup, frustum[3].normal);
  }
  else
  {
	vpnrx = (90-r_refdef.fov_x / 2);
	vpnry = 90-r_refdef.fov_y / 2;

    RotatePointAroundVectorNEW( frustum[0].normal, frustum[1].normal, vup, vpn, -(90-r_refdef.fov_x / 2));
    RotatePointAroundVectorNEW( frustum[2].normal, frustum[3].normal, vright, vpn, 90-r_refdef.fov_y / 2);

/*
    // rotate VPN right by FOV_X/2 degrees
    RotatePointAroundVector( frustum[0].normal, vup, vpn, -vpnrx);
    // rotate VPN left by FOV_X/2 degrees
    RotatePointAroundVector( frustum[1].normal, vup, vpn, vpnrx);
    // rotate VPN up by FOV_X/2 degrees
    RotatePointAroundVector( frustum[2].normal, vright, vpn, vpnry);
    // rotate VPN down by FOV_X/2 degrees
    RotatePointAroundVector( frustum[3].normal, vright, vpn, -vpnry);
*/
/*
    // rotate VPN right by FOV_X/2 degrees
    RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
    // rotate VPN left by FOV_X/2 degrees
    RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
    // rotate VPN up by FOV_X/2 degrees
    RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
    // rotate VPN down by FOV_X/2 degrees
    RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );
*/
  }

#ifndef FASTBOPS
  for (i=0 ; i<4 ; i++)
  {
    frustum[i].type = PLANE_ANYZ;
    frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
    frustum[i].signbits = SignbitsForPlane (&frustum[i]);
#else
    frustum[0].type = PLANE_ANYZ;
    frustum[1].type = PLANE_ANYZ;
    frustum[2].type = PLANE_ANYZ;
    frustum[3].type = PLANE_ANYZ;

    frustum[0].dist = DotProduct (r_origin, frustum[0].normal);
    frustum[1].dist = DotProduct (r_origin, frustum[1].normal);
    frustum[2].dist = DotProduct (r_origin, frustum[2].normal);
    frustum[3].dist = DotProduct (r_origin, frustum[3].normal);

    BoxOnPlaneSideClassify(&frustum[0]);
    BoxOnPlaneSideClassify(&frustum[1]);
    BoxOnPlaneSideClassify(&frustum[2]);
    BoxOnPlaneSideClassify(&frustum[3]);
#endif

#ifndef FASTBOPS
  }
#endif
}



/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
  int       edgecount;
  vrect_t     vrect;
  float     w, h;


  r_mod_maxdist = (ULONG)r_model_maxdist.value;
  r_mod_maxdist *= r_mod_maxdist;

// don't allow cheats in multiplayer
  if (cl.maxclients > 1)
//    Cvar_Set ("r_fullbright", "0");
  r_fullbright.value = 0;

//r_waterwarp switch enable by SuRgEoN
r_dowarp = (r_waterwarp.value != 0);

  R_AnimateLight ();

  r_framecount++;

// build the transformation matrix for the given view angles
  VectorCopy (r_refdef.vieworg, r_origin);

  AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
  r_oldviewleaf = r_viewleaf;
  r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

  V_SetContentsColor (r_viewleaf->contents);
  V_CalcBlend ();

  r_cache_thrash = false;

  c_brush_polys = 0;
  c_alias_polys = 0;

}


void MYgluPerspective( GLdouble fovy, GLdouble aspect,
         GLdouble zNear, GLdouble zFar )
{
   GLdouble xmin, xmax, ymin, ymax;

//   ymax = zNear * tan( fovy * M_PI / 360.0 );

//  if(fovy != 90)
   ymax = zNear * tan( fovy * MPIDIV);
/*
  else
   ymax = zNear;
*/
   ymin = -ymax;
/*
   xmin = ymin * aspect;
   xmax = ymax * aspect;
*/
   xmax = ymax * aspect;
   xmin = -xmax; 

   glFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
}


/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
  float screenaspect;
  float yfov;
  int   i;
  extern  int glwidth, glheight;
  int   x, x2, y2, y, w, h;

//surgeon: constant optimization
  int glhght,glwdth; //note these will typically be constant

  //
  // set up viewpoint
  //
  glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();

glhght = glheight/vid.height;
glwdth = glwidth/vid.width;

  x = r_refdef.vrect.x * glwdth;
  x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwdth;
  y = (vid.height-r_refdef.vrect.y) * glhght;
  y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glhght;

/*
  x = r_refdef.vrect.x * glwidth/vid.width;
  x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/vid.width;
  y = (vid.height-r_refdef.vrect.y) * glheight/vid.height;
  y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/vid.height;
*/

  // fudge around because of frac screen scale
  if (x > 0)
    x--;
  if (x2 < glwidth)
    x2++;
  if (y2 < 0)
    y2--;
  if (y < glheight)
    y++;

  w = x2 - x;
  h = y - y2;

  if (envmap)
  {
    x = y2 = 0;
    w = h = 256;
  }

/*
  if ((h + y2) == vid.height)
  {
	 y2 = 0;   //  30/01/2000 added: M.Tretene
  }

Surgeon: this causes offset to always be 0 for viewsizes less than fullscreen! - causes the console/sbar bug!
*/

  glViewport ((glx + x), (gly + y2), w, h);

//Surgeon: take vid.aspect into account
    screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height*vid.aspect;


//  yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/M_PI;

    MYgluPerspective (r_refdef.fov_y,  screenaspect,  4,  4096);

  if (mirror)
  {
    if (mirror_plane->normal[2])
      glScalef (1, -1, 1);
    else
      glScalef (-1, 1, 1);
    glCullFace(GL_BACK);
  }
  else
    glCullFace(GL_FRONT);

  glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();
#ifdef AMIGA
    glRotatefEXTs(-1.f, 0.f, GLROT_100);
    glRotatefEXTs( 1.f, 0.f, GLROT_001);
    glRotatefEXT (-r_refdef.viewangles[2], GLROT_100);
    glRotatefEXT (-r_refdef.viewangles[0], GLROT_010);
    glRotatefEXT (-r_refdef.viewangles[1], GLROT_001);
#else
    glRotatef (-90,  1, 0, 0);      // put Z going up
    glRotatef (90,  0, 0, 1);     // put Z going up
    glRotatef (-r_refdef.viewangles[2],  1, 0, 0);
    glRotatef (-r_refdef.viewangles[0],  0, 1, 0);
    glRotatef (-r_refdef.viewangles[1],  0, 0, 1);
#endif
    glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);

  glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

  //
  // set drawing parms
  //
  if (gl_cull.value)
    glEnable(GL_CULL_FACE);
  else
    glDisable(GL_CULL_FACE);

  glDisable(GL_BLEND);
  glDisable(GL_ALPHA_TEST);
  glEnable(GL_DEPTH_TEST);


#if defined(AMIGA) && !defined(MINIGL_DISPATCH_CLIENT)
  glEnable(MGL_PERSPECTIVE_MAPPING);
#endif
}



/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene (void)
{
  R_SetupFrame ();

  R_SetFrustum ();

  R_SetupGL ();

  R_MarkLeaves ();  // done here so we know if we're in water


//clamp triangle elimination area: (vertexarrays)
  if(r_elim_areasize.value > 10.0)
	Cvar_SetValue("r_elim_areasize", 10.0);
  else if (r_elim_areasize.value < 0.5)
	Cvar_SetValue("r_elim_areasize", 0.5);

#ifdef AMIGA
  mglMinTriArea(r_elim_areasize.value);
#endif

     if(!r_cullworld.value && !r_novis.value)
     glDisable(GL_CULL_FACE);

     glShadeModel (GL_FLAT); //SuRgEoN

     glColor4f(1,1,1,1); //brushmodels, world and sprites are drawn before alias models

     { unsigned long fp_t = FP_Enter (FP_ENTBRUSH);
       R_DrawEntitiesOnList (1); //brushmodels
#ifdef MGL68K_RESIDENT
       { extern void R_FlushBrushDeferred (void); R_FlushBrushDeferred (); }
#endif
       FP_Exit (FP_ENTBRUSH, fp_t); }

     { unsigned long fp_t = FP_Enter (FP_WORLD);
       R_DrawWorld ();   // adds static entities to the list
       FP_Exit (FP_WORLD, fp_t); }

     S_ExtraUpdate (); // don't let sound get messed up if going slow
 
     if(gl_cull.value)
     glEnable(GL_CULL_FACE);

     { unsigned long fp_t = FP_Enter (FP_ENTALIAS);
       R_DrawEntitiesOnList (2);
       FP_Exit (FP_ENTALIAS, fp_t); }

  glShadeModel (GL_SMOOTH); //SuRgEoN

  #ifdef GLOWEFFECTS
	if(gl_glows.value)
	{ unsigned long fp_t = FP_Enter (FP_GLOW);
	  R_RenderGlows ();
	  FP_Exit (FP_GLOW, fp_t); }
  #endif

  { unsigned long fp_t = FP_Enter (FP_GLOW);
    R_RenderDlights ();
    FP_Exit (FP_GLOW, fp_t); }

  glDisable(GL_CULL_FACE); //Surgeon: particle cullung is a waste of time

  { unsigned long fp_t = FP_Enter (FP_PARTICLES);
    R_DrawParticles ();
    FP_Exit (FP_PARTICLES, fp_t); }

if(gl_cull.value)
  glEnable(GL_CULL_FACE);

#ifdef GLTEST
  Test_Draw ();
#endif

}


/*
=============
R_Clear
=============
*/

void R_Clear (void)
{

glClearColor(0,0,0,1);
	//Surgeon - minimize gl_keeptjunctions 0 errors

  if (r_mirroralpha.value != 1.0)
  {
    if (gl_clear.value)
      glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    else
      glClear (GL_DEPTH_BUFFER_BIT);
    gldepthmin = 0;
    gldepthmax = 0.5;
    glDepthFunc (GL_LEQUAL);
  }
  else

 if (gl_ztrick.value)
  {
    static int trickframe;

    if (gl_clear.value)
      glClear (GL_COLOR_BUFFER_BIT);

    trickframe++;
    if (trickframe & 1)
    {
      gldepthmin = 0;
      gldepthmax = 0.49999;
      glDepthFunc (GL_LEQUAL);
    }
    else
    {
      gldepthmin = 1;
      gldepthmax = 0.5;
      glDepthFunc (GL_GEQUAL);
    }
  }
  else
  {
    if (gl_clear.value)
      glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    else
      glClear (GL_DEPTH_BUFFER_BIT);

    gldepthmin = 0;
    gldepthmax = 1;
    glDepthFunc (GL_LEQUAL);
  }

  glDepthRange (gldepthmin, gldepthmax);
}

/*
=============
R_Mirror
=============
*/
void R_Mirror (void)
{
  float   d;
  msurface_t  *s;
  entity_t  *ent;

  if (!mirror)
    return;

  memcpy (r_base_world_matrix, r_world_matrix, sizeof(r_base_world_matrix));

  d = DotProduct (r_refdef.vieworg, mirror_plane->normal) - mirror_plane->dist;
  VectorMA (r_refdef.vieworg, -2*d, mirror_plane->normal, r_refdef.vieworg);

  d = DotProduct (vpn, mirror_plane->normal);
  VectorMA (vpn, -2*d, mirror_plane->normal, vpn);

  r_refdef.viewangles[0] = -asin (vpn[2])/M_PI*180;
  r_refdef.viewangles[1] = atan2 (vpn[1], vpn[0])/M_PI*180;
  r_refdef.viewangles[2] = -r_refdef.viewangles[2];

  ent = &cl_entities[cl.viewentity];
  if (cl_numvisedicts < MAX_VISEDICTS)
  {
    cl_visedicts[cl_numvisedicts] = ent;
    cl_numvisedicts++;
  }

  gldepthmin = 0.5;
  gldepthmax = 1;
  glDepthRange (gldepthmin, gldepthmax);
  glDepthFunc (GL_LEQUAL);

  R_RenderScene ();
  R_DrawWaterSurfaces ();

  gldepthmin = 0;
  gldepthmax = 0.5;
  glDepthRange (gldepthmin, gldepthmax);
  glDepthFunc (GL_LEQUAL);

  // blend on top
  glEnable (GL_BLEND);
  //glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);//surgeon

  glMatrixMode(GL_PROJECTION);
  if (mirror_plane->normal[2])
    glScalef (1,-1,1);
  else
    glScalef (-1,1,1);

  glCullFace(GL_FRONT);
  glMatrixMode(GL_MODELVIEW);

  glLoadMatrixf (r_base_world_matrix);

  glColor4f (1,1,1,r_mirroralpha.value);

  s = cl.worldmodel->textures[mirrortexturenum]->texturechain;
  for ( ; s ; s=s->texturechain)
    R_RenderBrushPoly (s);
  cl.worldmodel->textures[mirrortexturenum]->texturechain = NULL;

  glDisable (GL_BLEND);
//  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);//surgeon
  glColor4f (1,1,1,1);
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
  double  time1, time2;
  //GLfloat colors[4] = {(GLfloat) 0.0, (GLfloat) 0.0, (GLfloat) 1, (GLfloat) 0.20};       // 30/01/2000 modified: M.Tretene
  static GLfloat colors[4] = {(GLfloat) 0.3, (GLfloat) 0.3, (GLfloat) 0.3, (GLfloat) 0.10};

  if (r_norefresh.value)
    return;

  if (!r_worldentity.model || !cl.worldmodel)
    Sys_Error ("R_RenderView: NULL worldmodel");

  if (r_speeds.value)
  {
    glFinish ();
    time1 = Sys_FloatTime ();
    c_brush_polys = 0;
    c_alias_polys = 0;
  }

  mirror = false;

  if (gl_finish.value)
    glFinish ();

  { unsigned long fp_t = FP_Enter (FP_CLEAR);
    R_Clear ();
    FP_Exit (FP_CLEAR, fp_t); }

  // render normal view
  
/***** Experimental silly looking fog ******
****** Use r_fullbright if you enable ******/

//Surgeon: not so silly looking anymore:

  if (gl_fog.value) {
     glFogi(GL_FOG_MODE, GL_LINEAR);

     glFogfv(GL_FOG_COLOR, colors);

     glFogf(GL_FOG_START, 256.0);
     glFogf(GL_FOG_END, 2048.0);

     glFogf(GL_FOG_DENSITY, 0.2); //surgeon

     glEnable(GL_FOG);
  }   
/********************************************/


  { unsigned long fp_t = FP_Enter (FP_SCENE);
    R_RenderScene ();
    FP_Exit (FP_SCENE, fp_t); }

  { unsigned long fp_t = FP_Enter (FP_WATER);
    R_DrawWaterSurfaces ();
    FP_Exit (FP_WATER, fp_t); }

//  More fog code right here :)
  if (gl_fog.value) glDisable(GL_FOG);
//  End of all fog code...

  // render mirror view
  { unsigned long fp_t = FP_Enter (FP_MIRROR);
    R_Mirror ();
    FP_Exit (FP_MIRROR, fp_t); }

  { unsigned long fp_t = FP_Enter (FP_BLEND);
    R_PolyBlend ();
    FP_Exit (FP_BLEND, fp_t); }

  if (r_speeds.value)
  {
//    glFinish ();
    time2 = Sys_FloatTime ();
    Con_Printf ("%3i ms  %4i wpoly %4i epoly\n", (int)((time2-time1)*1000), c_brush_polys, c_alias_polys); 
  }
}
