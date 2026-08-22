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
// r_light.c


#include "quakedef.h"
#ifndef MINIGL_DISPATCH_CLIENT
#include <mgl/mglmacros.h>
#endif

int r_dlightframecount;


/*
==================
R_AnimateLight
==================
*/
void R_AnimateLight (void)
{
  int     i,j,k;
  
//
// light animations
// 'm' is normal light, 'a' is no light, 'z' is double bright
  i = (int)(cl.time*10);
  for (j=0 ; j<MAX_LIGHTSTYLES ; j++)
  {
    if (!cl_lightstyle[j].length)
    {
      d_lightstylevalue[j] = 256;
      continue;
    }
    k = i % cl_lightstyle[j].length;
    k = cl_lightstyle[j].map[k] - 'a';
    k = k*22;
    d_lightstylevalue[j] = k;
  } 
}

/*
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING

=============================================================================
*/


void AddLightBlend (float r, float g, float b, float a2)
{
  float a;

  v_blend[3] = a = v_blend[3] + a2*(1-v_blend[3]);

  a2 = a2/a;

  v_blend[0] = v_blend[1]*(1-a2) + r*a2;
  v_blend[1] = v_blend[1]*(1-a2) + g*a2;
  v_blend[2] = v_blend[2]*(1-a2) + b*a2;
}


//Surgeon: ripped from glquakeworld

float bubble_sintable[17], bubble_costable[17];

void R_InitBubble()
{
  float a;
  int i;
  float *bub_sin, *bub_cos;

  bub_sin = bubble_sintable;
  bub_cos = bubble_costable;

  for (i=16 ; i>=0 ; i--)
  {
    a = i/16.0 * M_PI*2;
    *bub_sin++ = sin(a);
    *bub_cos++ = cos(a);
  }
}

void R_RenderDlight (dlight_t *light)
{
  int   i, j;
  vec3_t  v;
  vec3_t vp2; //dave - slight fix
  float rad;
  float *bub_sin, *bub_cos;

  bub_sin = bubble_sintable;
  bub_cos = bubble_costable;

  rad = light->radius * 0.35;

  VectorSubtract (light->origin, r_origin, v);
  if (Length (v) < rad)
  { // view is inside the dlight
    AddLightBlend (1, 0.5, 0, light->radius * 0.0003);
    return;
  }

  VectorSubtract (light->origin, r_origin, vp2); //dave
  VectorNormalize (vp2); //dave

  glBegin (GL_TRIANGLE_FAN);


  glColor4f (0.4,0.2,0.0,0.2); // glColor3f (0.2,0.1,0.0); 

//    v[0] = light->origin[0] - vpn[0]*rad;
//    v[1] = light->origin[1] - vpn[1]*rad;
//    v[2] = light->origin[2] - vpn[2]*rad;

    v[0] = light->origin[0] - vp2[0]*rad;
    v[1] = light->origin[1] - vp2[1]*rad;
    v[2] = light->origin[2] - vp2[2]*rad;

  glVertex3f (v[0], v[1], v[2]);

  glColor3f (0, 0, 0);

  for (i=16 ; i>=0 ; i--)
  {
      v[0] = light->origin[0] + (vright[0]*(*bub_cos) +
        + vup[0]*(*bub_sin)) * rad;
      v[1] = light->origin[1] + (vright[1]*(*bub_cos) +
        + vup[1]*(*bub_sin)) * rad;
      v[2] = light->origin[2] + (vright[2]*(*bub_cos) +
        + vup[2]*(*bub_sin)) * rad;

    bub_sin++; 
    bub_cos++;

    glVertex3f (v[0], v[1], v[2]);

  }
  glEnd ();
}


/*
=============
R_RenderDlights
=============
*/

extern cvar_t gl_fog; //surgeon

#ifdef GLOWEFFECTS

extern cvar_t gl_glows;

typedef struct glow_s
{
	vec3_t origin;
	vec3_t v;
	float  radius;
	float  color[4];
} glow_t;

static glow_t glowchain[1024]; //should be more than enough
static unsigned numglows = 0;

void GL_RenderGlow(glow_t *glow);

void R_RenderGlows (void)
{
  int   i;


  if(!numglows)
	return;

  if (gl_fog.value) //surgeon
  { 
	glDisable(GL_FOG); 
  } 

  glDepthMask (0);
  glDisable (GL_TEXTURE_2D);
  glShadeModel (GL_SMOOTH);
  glEnable (GL_BLEND);
  glBlendFunc (GL_ONE, GL_ONE);

  for (i=0 ; i<numglows; i++)
  {
	GL_RenderGlow (&glowchain[i]);
  }

  numglows = 0;

  glColor3f (1,1,1);
  glDisable (GL_BLEND);
  glEnable (GL_TEXTURE_2D);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask (1);


	if (gl_fog.value) //surgeon
	{ 
		glEnable(GL_FOG); 
	} 
}

#endif

void R_RenderDlights (void)
{
  int   i;
  dlight_t  *l;

#ifdef GLOWEFFECTS
  if (!gl_flashblend.value && !gl_glows.value)
#else
  if (!gl_flashblend.value)
#endif
    return;

#ifdef GLOWEFFECTS
  if(gl_flashblend.value)
#endif
  r_dlightframecount = r_framecount + 1;  // because the count hasn't
                      //  advanced yet for this frame

  if (gl_fog.value) //surgeon
  { 
	glDisable(GL_FOG); 
  } 

  glDepthMask (0);
  glDisable (GL_TEXTURE_2D);
  glShadeModel (GL_SMOOTH);
  glEnable (GL_BLEND);
  glBlendFunc (GL_ONE, GL_ONE);

  l = cl_dlights;
  for (i=0 ; i<MAX_DLIGHTS ; i++, l++)
  {
    if (l->die < cl.time || !l->radius)
      continue;
    R_RenderDlight (l);
  }

  glColor3f (1,1,1);
  glDisable (GL_BLEND);
  glEnable (GL_TEXTURE_2D);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask (1);

	if (gl_fog.value) //surgeon
	{ 
		glEnable(GL_FOG); 
	} 
}

#ifdef GLOWEFFECTS

float glowcos[17] =  
{ 
	 1.000000f, 
	 0.923880f, 
	 0.707105f, 
	 0.382680f, 
	 0.000000f, 
	-0.382680f, 
	-0.707105f, 
	-0.923880f, 
	-1.000000f, 
	-0.923880f, 
	-0.707105f, 
	-0.382680f, 
	 0.000000f, 
	 0.382680f, 
	 0.707105f, 
	 0.923880f, 
	 1.000000f 
}; 
 
 
float glowsin[17] =  
{ 
	 0.000000f, 
	 0.382680f, 
	 0.707105f, 
	 0.923880f, 
	 1.000000f, 
	 0.923880f, 
	 0.707105f, 
	 0.382680f, 
	-0.000000f, 
	-0.382680f, 
	-0.707105f, 
	-0.923880f, 
	-1.000000f, 
	-0.923880f, 
	-0.707105f, 
	-0.382680f, 
	 0.000000f 
}; 


void GL_RenderGlow (glow_t *glow) //surgeon
{
	int i;
	vec3_t	v; 
	float radius;

	radius = glow->radius;

	glBegin(GL_TRIANGLE_FAN);

		glColor4f(glow->color[0], glow->color[1], glow->color[2], glow->color[3]);	

		glVertex3f(glow->v[0], glow->v[1], glow->v[2]);

		glColor3f (0, 0, 0);
 
		for (i=16; i>=0; i--)  
		{ 
			v[0] = glow->origin[0] + radius * (vright[0] * glowcos[i] + vup[0] * glowsin[i]); 
			v[1] = glow->origin[1] + radius * (vright[1] * glowcos[i] + vup[1] * glowsin[i]); 
			v[2] = glow->origin[2] + radius * (vright[2] * glowcos[i] + vup[2] * glowsin[i]);
 
		glVertex3f(v[0], v[1], v[2]); 
		} 

	glEnd(); 
}


//surgeon: batch process glows
//make this user-customizable

void R_AddGlow (entity_t *e) //adapted from ThomazQuake
{ 
	vec3_t	v;			// Vector to ent. 
	vec3_t  vp2;			// surgeon
	float	radius;			// Radius of ent flare. 
	float	distance;		// Vector distance to ent. 
	float	intensity;		// Intensity of torch flare. 

	int			i; 
	model_t		*clmodel; 
 
	clmodel = e->model; 
 
	// NOTE: this isn't centered on the model 

	VectorCopy(e->origin, glowchain[numglows].origin);	 
 
	// set radius based on what model we are doing here 

		if ( (!strcmp (clmodel->name, "progs/flame.mdl")) || (!strcmp (clmodel->name, "progs/flame2.mdl"))  || (!strcmp (clmodel->name, "progs/s_light.mdl")) )
		{
			radius = 30.0;
		}
		else if ( (!strcmp(clmodel->name, "progs/quaddama.mdl")) || (!strcmp(clmodel->name, "progs/invulner.mdl")) )
		{
			glowchain[numglows].origin[2] += 16;
			radius = 60.0;
		}
		else
		{
			radius = 15.0;
		}

	VectorSubtract(glowchain[numglows].origin, r_origin, v); 
 
	// See if view is outside the light. 

	distance = Length(v); 

	if (distance > radius) 
	{

		if ( (!strcmp (clmodel->name, "progs/flame.mdl")) || (!strcmp (clmodel->name, "progs/flame2.mdl")) )
		{	 
			glowchain[numglows].origin[2] += 12;
 
			intensity = (1 - ((1024.0f - distance) * 0.0009765625)) * 0.75; 
			if(intensity > 1.0)
				intensity = 1.0;

			glowchain[numglows].color[0] = 0.6*intensity;
			glowchain[numglows].color[1] = 0.6*intensity;
			glowchain[numglows].color[2] = 0.5*intensity;
			glowchain[numglows].color[3] = 1.f;

		}
		else if (!strcmp(clmodel->name, "progs/quaddama.mdl"))
		{
			glowchain[numglows].color[0] = 0.3;
			glowchain[numglows].color[1] = 0.3;
			glowchain[numglows].color[2] = 0.8;
			glowchain[numglows].color[3] = 0.7;

		}
		else if (!strcmp(clmodel->name, "progs/invulner.mdl"))
		{
			glowchain[numglows].color[0] = 0.8;
			glowchain[numglows].color[1] = 0.3;
			glowchain[numglows].color[2] = 0.3;
			glowchain[numglows].color[3] = 0.7;
		}
		else if (!strcmp (clmodel->name, "progs/w_spike.mdl"))
		{
			glowchain[numglows].color[0] = 0.1;
			glowchain[numglows].color[1] = 0.6;
			glowchain[numglows].color[2] = 0.1;
			glowchain[numglows].color[3] = 0.7;
		}
		else if (!strcmp (clmodel->name, "progs/s_light.mdl"))
		{
			glowchain[numglows].color[0] = 0.1;
			glowchain[numglows].color[1] = 0.1;
			glowchain[numglows].color[2] = 0.8;
			glowchain[numglows].color[3] = 0.7;

		}
		else //bolt
		{ 
			intensity = ((2048.0f - distance) * 0.0005) * 0.75; 
			if(intensity > 1.125)
				intensity = 1.125;

			glowchain[numglows].color[0] = 0.1*intensity;
			glowchain[numglows].color[1] = 0.1*intensity;
			glowchain[numglows].color[2] = 0.8*intensity;
			glowchain[numglows].color[3] = 0.7;

		} 

	 	VectorSubtract (glowchain[numglows].origin, r_origin, vp2);
	  	VectorNormalize (vp2);
 
		VectorScale (vp2, -radius, v);
		VectorAdd (v, glowchain[numglows].origin, v); 

		glowchain[numglows].radius = radius;
		glowchain[numglows].v[0] = v[0];
		glowchain[numglows].v[1] = v[1];
		glowchain[numglows].v[2] = v[2];

		numglows++;
	} 
}

#endif


/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/


/*
=============
R_MarkLights
=============
*/
void R_MarkLights (dlight_t *light, int bit, mnode_t *node)
{
	mplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i; 
	// LordHavoc: .lit support begin (actually this is just a major lighting speedup, no relation to color :)
	float		l, maxdist; 
	int			j, s, t; 
	vec3_t		impact; 
loc0: 
	// LordHavoc: .lit support end 

	if (node->contents < 0)
		return;

	splitplane = node->plane; // LordHavoc: original code 
	// LordHavoc: .lit support (actually this is just a major lighting speedup, no relation to color :) 
	if (splitplane->type < 3)
		dist = light->origin[splitplane->type] - splitplane->dist; 
	else
		dist = DotProduct (light->origin, splitplane->normal) - splitplane->dist; // LordHavoc: original code 
	// LordHavoc: .lit support end 
	
	if (dist > light->radius)
	{ 
		// LordHavoc: .lit support begin (actually this is just a major lighting speedup, no relation to color :)
		//R_MarkLights (light, bit, node->children[0]); // LordHavoc: original code
		//return; // LordHavoc: original code 
		node = node->children[0]; 
		goto loc0; 
		// LordHavoc: .lit support end
	}
	if (dist < -light->radius)
	{
		// LordHavoc: .lit support begin (actually this is just a major lighting speedup, no relation to color :) 
		//R_MarkLights (light, bit, node->children[1]); // LordHavoc: original code 
		//return; // LordHavoc: original code 
		node = node->children[1]; 
		goto loc0; 
		// LordHavoc: .lit support end 
	}

	maxdist = light->radius*light->radius; // LordHavoc: .lit support (actually this is just a major lighting speedup, no relation to color :) 
 
// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{ 
		// LordHavoc: .lit support begin (actually this is just a major lighting speedup, no relation to color :) 
		/* LordHavoc: original code 
		if (surf->dlightframe != r_dlightframecount) 
		{ 
			surf->dlightbits = 0; 
			surf->dlightframe = r_dlightframecount; 
		} 
		surf->dlightbits |= bit; 
		*/ 
		// LordHavoc: MAJOR dynamic light speedup here, eliminates marking of surfaces that are too far away from light, thus preventing unnecessary renders and uploads 
		for (j=0 ; j<3 ; j++) 
			impact[j] = light->origin[j] - surf->plane->normal[j]*dist; 
 
		// clamp center of light to corner and check brightness 
		l = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0]; 
		s = l+0.5;if (s < 0) s = 0;else if (s > surf->extents[0]) s = surf->extents[0]; 
		s = l - s; 
		l = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1]; 
		t = l+0.5;if (t < 0) t = 0;else if (t > surf->extents[1]) t = surf->extents[1]; 
		t = l - t; 
		// compare to minimum light 
		if ((s*s+t*t+dist*dist) < maxdist) 
		{ 
			if (surf->dlightframe != r_dlightframecount) // not dynamic until now 
			{ 
				surf->dlightbits = bit; 
				surf->dlightframe = r_dlightframecount; 
			} 
			else // already dynamic 
				surf->dlightbits |= bit; 
		} 
		// LordHavoc: .lit support end
	}
/* 
	// LordHavoc: .lit support begin (actually this is just a major lighting speedup, no relation to color :) 
	if (node->children[0]->contents >= 0)
		R_MarkLights (light, bit, node->children[0]); // LordHavoc: original code
	if (node->children[1]->contents >= 0) 
		R_MarkLights (light, bit, node->children[1]); // LordHavoc: original code 
	// LordHavoc: .lit support end
*/
	if (node->children[0]->contents >= 0) 
	{ 
		if (node->children[1]->contents >= 0) 
		{ 
			R_MarkLights (light, bit, node->children[0]); 
			node = node->children[1]; 
			goto loc0; 
		} 
		else 
		{ 
			node = node->children[0]; 
			goto loc0; 
		} 
	} 
	else if (node->children[1]->contents >= 0) 
	{ 
		node = node->children[1]; 
		goto loc0; 
	} 

}


/*
=============
R_PushDlights
=============
*/
void R_PushDlights (void)
{
  int   i;
  dlight_t  *l;

  if (gl_flashblend.value)
    return;

  r_dlightframecount = r_framecount + 1;  // because the count hasn't
                      //  advanced yet for this frame
  l = cl_dlights;

  for (i=0 ; i<MAX_DLIGHTS ; i++, l++)
  {
    if (l->die < cl.time || !l->radius)
      continue;
    R_MarkLights ( l, 1<<i, cl.worldmodel->nodes );
  }
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

mplane_t    *lightplane;
vec3_t      lightspot;
extern cvar_t r_shadows;


int RecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end)
{
  int     r;
  float   front, back, frac;
  int     side;
  mplane_t  *plane;
  vec3_t    mid;
  msurface_t  *surf;
  int     s, t, ds, dt;
  int     i;
  mtexinfo_t  *tex;
  byte    *lightmap;
  unsigned  scale;
  int     maps;


loc0:
  if (node->contents < 0)
  {
    return -1;    // didn't hit anything
  }
  
// calculate mid point

#if 0
// FIXME: optimize for axial
  plane = node->plane;
  front = DotProduct (start, plane->normal) - plane->dist;
  back = DotProduct (end, plane->normal) - plane->dist;
  side = front < 0;

#else
         if (node->plane->type < 3)
         {
                 front = start[node->plane->type] - node->plane->dist;
                 back = end[node->plane->type] - node->plane->dist;
         }
         else
         {
                 front = DotProduct(start, node->plane->normal) - node->plane->dist;
                 back = DotProduct(end, node->plane->normal) - node->plane->dist;
         }

	   side = front < 0;

#endif


  
#if 0
  
  if ( (back < 0) == side)
    return RecursiveLightPoint (node->children[side], start, end);
#else

	// LordHavoc: optimized recursion
	if ((back < 0) == (front < 0))
	{
		node = node->children[front < 0];
		goto loc0;
	}
#endif
  
  frac = front / (front-back);
  mid[0] = start[0] + (end[0] - start[0])*frac;
  mid[1] = start[1] + (end[1] - start[1])*frac;
  mid[2] = start[2] + (end[2] - start[2])*frac;
  
// go down front side 
  r = RecursiveLightPoint (node->children[side], start, mid);

if (r >= 0)
{
    return r;   // hit something
}
    
if ( (back < 0) == side )
{
    return -1;    // didn't hit anuthing
}
    
// check for impact on this node
if(r_shadows.value)
  VectorCopy (mid, lightspot);
  lightplane = plane;

  surf = cl.worldmodel->surfaces + node->firstsurface;
  for (i=0 ; i<node->numsurfaces ; i++, surf++)
  {
    if (surf->flags & SURF_DRAWTILED)
      continue; // no lightmaps

    tex = surf->texinfo;
    
    s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
    t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];;

    if (s < surf->texturemins[0] || t < surf->texturemins[1])
      continue;
    
    ds = s - surf->texturemins[0];
    dt = t - surf->texturemins[1];
    
    if ( ds > surf->extents[0] || dt > surf->extents[1] )
      continue;

    if (!surf->samples)
    {

      return 0;
    }

    ds >>= 4;
    dt >>= 4;

    lightmap = surf->samples;
    r = 0;

    if (lightmap)
    {

      lightmap += dt * ((surf->extents[0]>>4)+1) + ds;

      for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
          maps++)
      {
        scale = d_lightstylevalue[surf->styles[maps]];
        r += *lightmap * scale;
        lightmap += ((surf->extents[0]>>4)+1) *
            ((surf->extents[1]>>4)+1);
      }
      
      r >>= 8;
    }
    
    return r;
  }

#if 0
// go down back side
  return RecursiveLightPoint (node->children[!side], mid, end);
#else
	// LordHavoc: optimized recursion
	// go down back side 
	node = node->children[side ^ 1]; 
	start[2] = mid[2]; 
	goto loc0; 
#endif
}


int R_LightPoint (vec3_t p)
{
  vec3_t    end, temp;
  int     r;

  
  if (r_fullbright.value || !cl.worldmodel->lightdata)
	return 255;


//surgeon:
  VectorCopy(p, temp);
  
  end[0] = temp[0];
  end[1] = temp[1];
  end[2] = temp[2] - 2048;
  
  r = RecursiveLightPoint (cl.worldmodel->nodes, temp, end);
  
  if (r == -1)
    r = 0;

  return r;
}


#ifdef LITFILES

// LordHavoc: .lit support begin
// LordHavoc: original code replaced entirely
// Surgeon : tutorial was bugged - cleaned up the code


extern qboolean eyecandy;
vec3_t lightcolor; // LordHavoc: used by model rendering

int RecursiveLightPointColor (vec3_t color, mnode_t *node, vec3_t start, vec3_t end)
{
	int i, s, t, ds, dt;
	msurface_t *surf;
	mtexinfo_t  *tex;
	float           front, back, frac;
	vec3_t          mid;


 loc0:
         if (node->contents < 0)
                 return 0;	// didn't hit anything
         
 // calculate mid point
         if (node->plane->type < 3)
         {
                 front = start[node->plane->type] - node->plane->dist;
                 back = end[node->plane->type] - node->plane->dist;
         }
         else
         {
                 front = DotProduct(start, node->plane->normal) - node->plane->dist;
                 back = DotProduct(end, node->plane->normal) - node->plane->dist;
         }

         // LordHavoc: optimized recursion
         if ((back < 0) == (front < 0))
         {
                 node = node->children[front < 0];
                 goto loc0;
         }
         
         frac = front / (front-back);
         mid[0] = start[0] + (end[0] - start[0])*frac;
         mid[1] = start[1] + (end[1] - start[1])*frac;
         mid[2] = start[2] + (end[2] - start[2])*frac;
         
 // go down front side

         if (RecursiveLightPointColor (color, node->children[front < 0], start, mid))
                 return 1;    // hit something

         // check for impact on this node

	   if(r_shadows.value)
         VectorCopy (mid, lightspot);

         lightplane = node->plane;

         surf = cl.worldmodel->surfaces + node->firstsurface;

         for (i = 0;i < node->numsurfaces;i++, surf++)
         {
              if (surf->flags & SURF_DRAWTILED)
                  continue;       // no lightmaps

		tex = surf->texinfo;

		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];;

		if (s < surf->texturemins[0] || t < surf->texturemins[1])
			continue;
    
		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];
    
		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;


		 if(!surf->samples)
		 	return 0;

               else
               {
               // LordHavoc: enhanced to interpolate lighting
               float scale;
               int maps, line3;
		 int dsfrac, dtfrac;
#if 0
		 int r00,r01,r10,r11;
		 int g00,g01,g10,g11;
		 int b00,b01,b10,b11;
#else
		 float r00, g00, b00;
#endif
               byte *lightmap;

		 dsfrac = ds & 0x0F;
		 dtfrac = dt & 0x0F;
#if 1
		 r00 = 0.f; g00 = 0.f; b00 = 0.f;
#else
		 r00 = 0; g00 = 0; b00 = 0;
		 r01 = 0; g01 = 0; b01 = 0;
		 r10 = 0; g10 = 0; b10 = 0;
		 r11 = 0; g11 = 0; b11 = 0;
#endif
		 // LordHavoc: *3 for color:

               line3 = ((surf->extents[0]>>4)+1)*3;

               lightmap = surf->samples + ((dt>>4) * ((surf->extents[0]>>4)+1) + (ds>>4))*3;

			   for (maps = 0;maps < MAXLIGHTMAPS && surf->styles[maps] != 255;maps++)
			   {
				scale = (float)d_lightstylevalue[surf->styles[maps]] * 1.0 / 256.0;
#if 1
				r00 += (float)lightmap[0] * scale;
				g00 += (float)lightmap[1] * scale;
				b00 += (float)lightmap[2] * scale;

				lightmap += ((surf->extents[0]>>4)+1) * ((surf->extents[1]>>4)+1) * 3;
#else
				r00 += (float)lightmap[0] * scale;
				g00 += (float)lightmap[1] * scale;
				b00 += (float)lightmap[2] * scale;
				r01 += (float)lightmap[3] * scale;
				g01 += (float)lightmap[4] * scale;
				b01 += (float)lightmap[5] * scale;
				r10 += (float)lightmap[line3+0] * scale;
				g10 += (float)lightmap[line3+1] * scale;
				b10 += (float)lightmap[line3+2] * scale;

				r11 += (float)lightmap[line3+3] * scale;
				g11 += (float)lightmap[line3+4] * scale;
				b11 += (float)lightmap[line3+5] * scale;

lightmap += ((surf->extents[0]>>4)+1) * ((surf->extents[1]>>4)+1)*3; // LordHavoc: *3 for colored lighting
#endif
			   }
#if 1
		   color[0] += r00;
		   color[1] += g00;
		   color[2] += b00;
#else
		   color[0] += (float) ((int) ((((((((r11-r10) * dsfrac) >> 4) + r10)-((((r01-r00) * dsfrac) >> 4) + r00)) * dtfrac) >> 4) + ((((r01-r00) * dsfrac) >> 4) + r00)));
		   color[1] += (float) ((int) ((((((((g11-g10) * dsfrac) >> 4) + g10)-((((g01-g00) * dsfrac) >> 4) + g00)) * dtfrac) >> 4) + ((((g01-g00) * dsfrac) >> 4) + g00)));
		   color[2] += (float) ((int) ((((((((b11-b10) * dsfrac) >> 4) + b10)-((((b01-b00) * dsfrac) >> 4) + b00)) * dtfrac) >> 4) + ((((b01-b00) * dsfrac) >> 4) + b00)));
#endif
                 }
         return 1; // success
         }

      // go down back side

	// LordHavoc: optimized recursion
	// go down back side 
	node = node->children[(front < 0) ^ 1]; 
	start[2] = mid[2]; 
	goto loc0; 
}

int R_LightPointColor (vec3_t p)
{
	vec3_t          end;
	vec3_t		temp; //surgeon
	int r;
         
	if (r_fullbright.value || !cl.worldmodel->lightdata)
	{
	lightcolor[0] = lightcolor[1] = lightcolor[2] = 255;
      return 255;
      }

//surgeon:
	VectorCopy(p, temp);
         
      end[0] = temp[0];
      end[1] = temp[1];
      end[2] = temp[2] - 2048;

      lightcolor[0] = lightcolor[1] = lightcolor[2] = 0;

      r = RecursiveLightPointColor (lightcolor, cl.worldmodel->nodes, temp, end);

      return ((lightcolor[0] + lightcolor[1] + lightcolor[2]) * (1.0 / 3.0));
}

#endif
