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

#include "quakedef.h"
#ifdef MGL68K_RESIDENT
extern int r68k_now;
#endif
#include "r_local.h"

#ifdef GLQUAKE
#ifndef MINIGL_DISPATCH_CLIENT
#include <mgl/mglmacros.h>
#endif
#endif

#define MAX_PARTICLES     2048  // default max # of particles at one
                    //  time
//#define ABSOLUTE_MIN_PARTICLES  512   // no fewer than this no matter what's
                    //  on the command line

//Surgeon: a performance killer in gl - can be disabled now
#define ABSOLUTE_MIN_PARTICLES  1
 
int   ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
int   ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
int   ramp3[8] = {0x6d, 0x6b, 6, 5, 4, 3};

particle_t  *active_particles = NULL;
particle_t  *free_particles   = NULL;
/* live particles are particles[0..part_n): a dead one is replaced by the
 * last, so the per-frame walk reads memory in order (the linked list
 * scattered them over the whole array: a cache miss or more each) */
int part_n;
particle_t  *particles        = NULL;

int     r_numparticles;

vec3_t      r_pright, r_pup, r_ppn;

/* Particle point size. 0 keeps the automatic resolution-scaled size
 * (0.5 + width/320, expanded to screen quads). The default 1 selects
 * MiniGL's hardware point path: one vertex per particle instead of a
 * CPU-expanded screen quad, roughly six times fewer record dwords during
 * explosions (measured ~4% faster in demo1 at 800x600). Values above 1.5
 * force the per-point screen-quad expansion; 3 restores the old 3 px look. */
cvar_t  r_particle_size = {"r_particle_size","1"};
cvar_t  r_partpoints = {"r_partpoints","2"};


/*
===============
R_InitParticles
===============
*/

void R_InitParticles (void)
{
  int   i;

  i = COM_CheckParm ("-particles");

  if (i)
  {
    r_numparticles = (int)(Q_atoi(com_argv[i+1]));

    if (r_numparticles < ABSOLUTE_MIN_PARTICLES)
      r_numparticles = ABSOLUTE_MIN_PARTICLES;
  }
  else
  {
    r_numparticles = MAX_PARTICLES;
  }
#ifdef GLQUAKE
  particles = (particle_t *) malloc(r_numparticles * sizeof(particle_t));
#else
  particles = (particle_t *)
      Hunk_AllocName (r_numparticles * sizeof(particle_t), "particles");
#endif
}

#ifdef GLQUAKE
void R_FreeParticles(void)
{
  if(particles) free(particles);
}
#endif

#ifdef QUAKE2
void R_DarkFieldParticles (entity_t *ent)
{
  int     i, j, k;
  particle_t  *p;
  float   vel;
  vec3_t    dir;
  vec3_t    org;

  org[0] = ent->origin[0];
  org[1] = ent->origin[1];
  org[2] = ent->origin[2];

  for (i=-16 ; i<16 ; i+=8)
    for (j=-16 ; j<16 ; j+=8)
      for (k=0 ; k<32 ; k+=8)
      {
        if (part_n >= r_numparticles)
          return;
        p = &particles[part_n++];
    
        p->die = cl.time + 0.2 + (rand()&7) * 0.02;
        p->color = 150 + rand()%6;
        p->type = pt_slowgrav;
        
        dir[0] = j*8;
        dir[1] = i*8;
        dir[2] = k*8;
  
        p->org[0] = org[0] + i + (rand()&3);
        p->org[1] = org[1] + j + (rand()&3);
        p->org[2] = org[2] + k + (rand()&3);
  
        VectorNormalize (dir);            
        vel = 50 + (rand()&63);
        VectorScale (dir, vel, p->vel);
      }
}
#endif


/*
===============
R_EntityParticles
===============
*/

#define NUMVERTEXNORMALS  162

#ifndef GLQUAKE
	extern float r_avertexnormals[NUMVERTEXNORMALS][3];
#else
	static float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};
#endif

vec3_t  avelocities[NUMVERTEXNORMALS];
float beamlength = 16;
vec3_t  avelocity = {23, 7, 3};
float partstep = 0.01;
float timescale = 0.01;

void R_EntityParticles (entity_t *ent)
{
  int     count;
  int     i;
  particle_t  *p;
  float   angle;
  float   sr, sp, sy, cr, cp, cy;
  vec3_t    forward;
  float   dist;
  
  dist = 64;
  count = 50;

if (!avelocities[0][0])
{
for (i=0 ; i<NUMVERTEXNORMALS*3 ; i++)
avelocities[0][i] = (rand()&255) * 0.01;
}


  for (i=0 ; i<NUMVERTEXNORMALS ; i++)
  {
    angle = cl.time * avelocities[i][0];
    sy = sin(angle);
    cy = cos(angle);
    angle = cl.time * avelocities[i][1];
    sp = sin(angle);
    cp = cos(angle);
    angle = cl.time * avelocities[i][2];
    sr = sin(angle);
    cr = cos(angle);
  
    forward[0] = cp*cy;
    forward[1] = cp*sy;
    forward[2] = -sp;

    if (part_n >= r_numparticles)
      return;
    p = &particles[part_n++];

    p->die = cl.time + 0.01;
    p->color = 0x6f;
    p->type = pt_explode;
    
    p->org[0] = ent->origin[0] + r_avertexnormals[i][0]*dist + forward[0]*beamlength;     
    p->org[1] = ent->origin[1] + r_avertexnormals[i][1]*dist + forward[1]*beamlength;     
    p->org[2] = ent->origin[2] + r_avertexnormals[i][2]*dist + forward[2]*beamlength;     
  }
}


/*
===============
R_ClearParticles
===============
*/
void R_ClearParticles (void)
{
  int   i;
  
  free_particles = &particles[0];
  active_particles = NULL;
  part_n = 0;

  for (i=0 ;i<r_numparticles-1 ; i++)
  {
    particles[i].next = &particles[i+1];
  }
  particles[r_numparticles-1].next = NULL;
}


void R_ReadPointFile_f (void)
{
  FILE  *f;
  vec3_t  org;
  int   r;
  int   c;
  particle_t  *p;
  char  name[MAX_OSPATH];
  
  sprintf (name,"maps/%s.pts", sv.name);

  COM_FOpenFile (name, &f);
  if (!f)
  {
    Con_Printf ("couldn't open %s\n", name);
    return;
  }
  
  Con_Printf ("Reading %s...\n", name);
  c = 0;
  for ( ;; )
  {
    r = fscanf (f,"%f %f %f\n", &org[0], &org[1], &org[2]);
    if (r != 3)
      break;
    c++;
    
    if (part_n >= r_numparticles)
    {
      Con_Printf ("Not enough free particles\n");
      break;
    }
    p = &particles[part_n++];
    
    p->die = 99999;
    p->color = (-c)&15;
    p->type = pt_static;
    VectorCopy (vec3_origin, p->vel);
    VectorCopy (org, p->org);
  }

  fclose (f);
  Con_Printf ("%i points read\n", c);
}

/*
===============
R_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void R_ParseParticleEffect (void)
{
  vec3_t    org, dir;
  int     i, count, msgcount, color;
  
  for (i=0 ; i<3 ; i++)
    org[i] = MSG_ReadCoord ();
  for (i=0 ; i<3 ; i++)
    dir[i] = MSG_ReadChar () * (1.0/16);
  msgcount = MSG_ReadByte ();
  color = MSG_ReadByte ();

if (msgcount == 255)
  count = 1024;
else
  count = msgcount;
  
  R_RunParticleEffect (org, dir, color, count);
}
  
/*
===============
R_ParticleExplosion

===============
*/
void R_ParticleExplosion (vec3_t org)
{
  int     i, j;
  particle_t  *p;
  
  for (i=0 ; i<1024 ; i++)
  {
    if (part_n >= r_numparticles)
      return;
    p = &particles[part_n++];

    p->die = cl.time + 5;
    p->color = ramp1[0];
    p->ramp = rand()&3;
    if (i & 1)
    {
      p->type = pt_explode;
      for (j=0 ; j<3 ; j++)
      {
        p->org[j] = org[j] + ((rand()%32)-16);
        p->vel[j] = (rand()%512)-256;
      }
    }
    else
    {
      p->type = pt_explode2;
      for (j=0 ; j<3 ; j++)
      {
        p->org[j] = org[j] + ((rand()%32)-16);
        p->vel[j] = (rand()%512)-256;
      }
    }
  }
}

/*
===============
R_ParticleExplosion2

===============
*/
void R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength)
{
  int     i, j;
  particle_t  *p;
  int     colorMod = 0;

  for (i=0; i<512; i++)
  {
    if (part_n >= r_numparticles)
      return;
    p = &particles[part_n++];

    p->die = cl.time + 0.3;
    p->color = colorStart + (colorMod % colorLength);
    colorMod++;

    p->type = pt_blob;
    for (j=0 ; j<3 ; j++)
    {
      p->org[j] = org[j] + ((rand()%32)-16);
      p->vel[j] = (rand()%512)-256;
    }
  }
}

/*
===============
R_BlobExplosion

===============
*/
void R_BlobExplosion (vec3_t org)
{
  int     i, j;
  particle_t  *p;
  
  for (i=0 ; i<1024 ; i++)
  {
    if (part_n >= r_numparticles)
      return;
    p = &particles[part_n++];

    p->die = cl.time + 1 + (rand()&8)*0.05;

    if (i & 1)
    {
      p->type = pt_blob;
      p->color = 66 + rand()%6;
      for (j=0 ; j<3 ; j++)
      {
        p->org[j] = org[j] + ((rand()%32)-16);
        p->vel[j] = (rand()%512)-256;
      }
    }
    else
    {
      p->type = pt_blob2;
      p->color = 150 + rand()%6;
      for (j=0 ; j<3 ; j++)
      {
        p->org[j] = org[j] + ((rand()%32)-16);
        p->vel[j] = (rand()%512)-256;
      }
    }
  }
}

/*
===============
R_RunParticleEffect

===============
*/
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
  int     i, j;
  particle_t  *p;
  
  for (i=0 ; i<count ; i++)
  {
    if (part_n >= r_numparticles)
      return;
    p = &particles[part_n++];

    if (count == 1024)
    { // rocket explosion
      p->die = cl.time + 5;
      p->color = ramp1[0];
      p->ramp = rand()&3;
      if (i & 1)
      {
        p->type = pt_explode;
        for (j=0 ; j<3 ; j++)
        {
          p->org[j] = org[j] + ((rand()%32)-16);
          p->vel[j] = (rand()%512)-256;
        }
      }
      else
      {
        p->type = pt_explode2;
        for (j=0 ; j<3 ; j++)
        {
          p->org[j] = org[j] + ((rand()%32)-16);
          p->vel[j] = (rand()%512)-256;
        }
      }
    }
    else
    {
      p->die = cl.time + 0.1*(rand()%5);
      p->color = (color&~7) + (rand()&7);
      p->type = pt_slowgrav;
      for (j=0 ; j<3 ; j++)
      {
        p->org[j] = org[j] + ((rand()&15)-8);
        p->vel[j] = dir[j]*15;// + (rand()%300)-150;
      }
    }
  }
}


/*
===============
R_LavaSplash

===============
*/
void R_LavaSplash (vec3_t org)
{
  int     i, j, k;
  particle_t  *p;
  float   vel;
  vec3_t    dir;

  for (i=-16 ; i<16 ; i++)
    for (j=-16 ; j<16 ; j++)
      for (k=0 ; k<1 ; k++)
      {
        if (part_n >= r_numparticles)
          return;
        p = &particles[part_n++];
    
        p->die = cl.time + 2 + (rand()&31) * 0.02;
        p->color = 224 + (rand()&7);
        p->type = pt_slowgrav;
        
        dir[0] = j*8 + (rand()&7);
        dir[1] = i*8 + (rand()&7);
        dir[2] = 256;
  
        p->org[0] = org[0] + dir[0];
        p->org[1] = org[1] + dir[1];
        p->org[2] = org[2] + (rand()&63);
  
        VectorNormalize (dir);            
        vel = 50 + (rand()&63);
        VectorScale (dir, vel, p->vel);
      }
}

/*
===============
R_TeleportSplash

===============
*/
void R_TeleportSplash (vec3_t org)
{
  int     i, j, k;
  particle_t  *p;
  float   vel;
  vec3_t    dir;

  for (i=-16 ; i<16 ; i+=4)
    for (j=-16 ; j<16 ; j+=4)
      for (k=-24 ; k<32 ; k+=4)
      {
        if (part_n >= r_numparticles)
          return;
        p = &particles[part_n++];
    
        p->die = cl.time + 0.2 + (rand()&7) * 0.02;
        p->color = 7 + (rand()&7);
        p->type = pt_slowgrav;
        
        dir[0] = j*8;
        dir[1] = i*8;
        dir[2] = k*8;
  
        p->org[0] = org[0] + i + (rand()&3);
        p->org[1] = org[1] + j + (rand()&3);
        p->org[2] = org[2] + k + (rand()&3);
  
        VectorNormalize (dir);            
        vel = 50 + (rand()&63);
        VectorScale (dir, vel, p->vel);
      }
}

void R_RocketTrail (vec3_t start, vec3_t end, int type)
{
  vec3_t    vec;
  float   len;
  int     j;
  particle_t  *p;
  int     dec;
  static int  tracercount;

  VectorSubtract (end, start, vec);
  len = VectorNormalize (vec);

  if (type < 128)
    dec = 3;
  else
  {
    dec = 1;
    type -= 128;
  }

  while (len > 0)
  {
    len -= dec;

    if (part_n >= r_numparticles)
      return;
    p = &particles[part_n++];
    
    VectorCopy (vec3_origin, p->vel);
    p->die = cl.time + 2;

    switch (type)
    {
      case 0: // rocket trail
        p->ramp = (rand()&3);
        p->color = ramp3[(int)p->ramp];
        p->type = pt_fire;
        for (j=0 ; j<3 ; j++)
          p->org[j] = start[j] + ((rand()%6)-3);
        break;

      case 1: // smoke smoke
        p->ramp = (rand()&3) + 2;
        p->color = ramp3[(int)p->ramp];
        p->type = pt_fire;
        for (j=0 ; j<3 ; j++)
          p->org[j] = start[j] + ((rand()%6)-3);
        break;

      case 2: // blood
        p->type = pt_grav;
        p->color = 67 + (rand()&3);
        for (j=0 ; j<3 ; j++)
          p->org[j] = start[j] + ((rand()%6)-3);
        break;

      case 3:
      case 5: // tracer
        p->die = cl.time + 0.5;
        p->type = pt_static;
        if (type == 3)
          p->color = 52 + ((tracercount&4)<<1);
        else
          p->color = 230 + ((tracercount&4)<<1);
      
        tracercount++;

        VectorCopy (start, p->org);
        if (tracercount & 1)
        {
          p->vel[0] = 30*vec[1];
          p->vel[1] = 30*-vec[0];
        }
        else
        {
          p->vel[0] = 30*-vec[1];
          p->vel[1] = 30*vec[0];
        }
        break;

      case 4: // slight blood
        p->type = pt_grav;
        p->color = 67 + (rand()&3);
        for (j=0 ; j<3 ; j++)
          p->org[j] = start[j] + ((rand()%6)-3);
        len -= 3;
        break;

      case 6: // voor trail
        p->color = 9*16 + 8 + (rand()&3);
        p->type = pt_static;
        p->die = cl.time + 0.3;
        for (j=0 ; j<3 ; j++)
          p->org[j] = start[j] + ((rand()&15)-8);
        break;
    }
    

    VectorAdd (start, vec, start);
  }
}


extern  cvar_t  sv_gravity;

cvar_t  r_particle_hack = {"r_particle_hack","0"};//surgeon

/*
===============
R_DrawParticles
===============
*/

#ifdef MGL68K_RESIDENT
#include <exec/types.h>
extern qboolean R_ResidentDynBegin (int numverts, volatile ULONG **dst,
  ULONG *handle, ULONG *offset);
extern qboolean R_ResidentDynDraw (ULONG handle, ULONG offset,
  const unsigned short *indices, int count);

#define PRES_CHUNK 1024
static unsigned short pres_indices[PRES_CHUNK * 3];
static qboolean pres_active;
static volatile ULONG *pres_dst;
static ULONG pres_handle, pres_offset;
static int pres_count;

static ULONG PartSwapLE (ULONG v)
{
  return (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24);
}

static qboolean PartResChunk (void)
{
  pres_count = 0;
  return R_ResidentDynBegin (PRES_CHUNK * 3, &pres_dst, &pres_handle,
                             &pres_offset);
}

static qboolean PartResBegin (void)
{
  int i;
  if (!pres_indices[1])
    for (i = 0; i < PRES_CHUNK * 3; i++)
      pres_indices[i] = i;
  return PartResChunk ();
}

extern qboolean R_ResidentPatternDraw (ULONG handle, ULONG offset,
  int pattern, int count);

extern qboolean R_ResidentPointsDraw (ULONG handle, ULONG offset, int count);
extern qboolean R_ResidentPointsOK (void);
static qboolean pres_points;   /* one vertex a particle, GL_POINTS */

static void PartResFlush (void)
{
  if (pres_points)
  {
    if (pres_count)
      (void)R_ResidentPointsDraw (pres_handle, pres_offset, pres_count);
    pres_count = 0;
    return;
  }
  if (pres_count &&
      !R_ResidentPatternDraw (pres_handle, pres_offset, 0, pres_count * 3))
    (void)R_ResidentDynDraw (pres_handle, pres_offset, pres_indices,
                             pres_count * 3);
  pres_count = 0;
}

static void PartResVertex (const float *v, ULONG c)
{
  union { float f; ULONG u; } x, y, z;
  x.f = v[0]; y.f = v[1]; z.f = v[2];
  pres_dst[0] = PartSwapLE (x.u);
  pres_dst[1] = PartSwapLE (y.u);
  pres_dst[2] = PartSwapLE (z.u);
  if (r68k_now)
  {
    pres_dst[3] = PartSwapLE (c);
    pres_dst += 4;
  }
  else
  {
    pres_dst[3] = 0x0000803f;
    pres_dst[4] = PartSwapLE (c);
    pres_dst[5] = 0;
    pres_dst[6] = 0;
    pres_dst += 7;
  }
}

static void PartResAdd (const float *org, const float *up,
  const float *right, ULONG c)
{
  float scale, v[3];

  if (!pres_active)
    return;
  if (pres_points)
  {
    if (pres_count == PRES_CHUNK * 3)
    {
      PartResFlush ();
      if (!PartResChunk ())
      {
        pres_active = false;
        return;
      }
    }
    PartResVertex (org, c);
    pres_count++;
    return;
  }
  if (pres_count == PRES_CHUNK)
  {
    PartResFlush ();
    if (!PartResChunk ())
    {
      pres_active = false;   /* ring full: skip the rest this frame */
      return;
    }
  }
  /* GLQuake's particle triangle, grown with distance */
  scale = (org[0] - r_origin[0]) * vpn[0] + (org[1] - r_origin[1]) * vpn[1] +
          (org[2] - r_origin[2]) * vpn[2];
  scale = scale < 20 ? 1 : 1 + scale * 0.004f;
  PartResVertex (org, c);
  v[0] = org[0] + up[0] * scale;
  v[1] = org[1] + up[1] * scale;
  v[2] = org[2] + up[2] * scale;
  PartResVertex (v, c);
  v[0] = org[0] + right[0] * scale;
  v[1] = org[1] + right[1] * scale;
  v[2] = org[2] + right[2] * scale;
  PartResVertex (v, c);
  pres_count++;
}

static void PartResEnd (void)
{
  if (pres_active)
    PartResFlush ();
  else
    pres_count = 0;
  pres_active = false;
}
#endif

void R_DrawParticles (void)
{
  particle_t    *p;
  int       pi;
  float     grav;
  int       i;
  float     time2, time3;
  float     time1;
  float     dvel;
  float     frametime;
  float minparticledist;
#ifdef MGL68K_RESIDENT
  float fd0, fd1, fd2, fd3;
#endif

  
#ifdef GLQUAKE
  int pcount;
  vec3_t      up, right;
  float     scale;
  float ntscale;
  unsigned char theAlpha; //surgeon
  unsigned char *at; //surgeon

//early out condition - Surgeon

if(r_numparticles < 2)
	return;

glDepthMask (GL_FALSE); //Surgeon
glDisable(GL_TEXTURE_2D);
glEnable(GL_BLEND);


  pcount = 0;

  VectorScale (vup, 1.5, up);
  VectorScale (vright, 1.5, right);

#ifdef MGL68K_RESIDENT
  /* minigl68k: one camera-facing triangle per particle (as GLQuake draws
   * them) written straight into the VRAM ring, 256 per draw. */
  /* r_partpoints n: n-pixel hardware point sprites from the ring;
   * -n: the same through glBegin (test) */
  pres_points = r_partpoints.value > 0 && r68k_now && R_ResidentPointsOK ();
  if (pres_points)
    glPointSize (r_partpoints.value);
  pres_active = r_partpoints.value < 0 ? false : PartResBegin ();
  if (!pres_active)
#endif
  {
  glPointSize (
#ifdef MGL68K_RESIDENT
                 r_partpoints.value ? fabs (r_partpoints.value) :
#endif
                 r_particle_size.value > 0.0f
                 ? r_particle_size.value
                 : 0.5f + (float)vid.width/320.f);
  glBegin (GL_POINTS);
  }

#else //soft renderer
//early out condition - Surgeon

if(r_numparticles < 2)
	return;

  D_StartParticles ();

  VectorScale (vright, xscaleshrink, r_pright);
  VectorScale (vup, yscaleshrink, r_pup);
  VectorCopy (vpn, r_ppn);
#endif


  frametime = cl.time - cl.oldtime;
  time3 = frametime * 15;
  time2 = frametime * 10; // 15;
  time1 = frametime * 5;
  grav = frametime * sv_gravity.value * 0.05;
  dvel = 4*frametime;
  
//LordHavoc:
  minparticledist = DotProduct(r_origin, vpn) + 16.0f; 
#ifdef MGL68K_RESIDENT
  fd0 = frustum[0].dist - 8; fd1 = frustum[1].dist - 8;
  fd2 = frustum[2].dist - 8; fd3 = frustum[3].dist - 8;
#endif

  for (pi = 0; pi < part_n; )
  {
    p = &particles[pi];
    if (p->die < cl.time)
    {
      *p = particles[--part_n];
      continue;
    }
    pi++;

#ifdef GLQUAKE

    // LordHavoc: only render if not too close 
    if (DotProduct(p->org, vpn) < minparticledist) 			continue; 


    at = (byte *)&d_8to24table[(int)p->color];

#ifdef MGL68K_RESIDENT
    if (pres_active)
    {
      ULONG a;
      /* outside the side planes: nothing to send over the bus */
      if (DotProduct (p->org, frustum[0].normal) < fd0 ||
          DotProduct (p->org, frustum[1].normal) < fd1 ||
          DotProduct (p->org, frustum[2].normal) < fd2 ||
          DotProduct (p->org, frustum[3].normal) < fd3)
        goto part_move;
      a = p->type == pt_fire ? (ULONG)(255*(6-p->ramp)/6) : 255;
      PartResAdd (p->org, up, right,
        (a << 24) | ((ULONG)at[0] << 16) | ((ULONG)at[1] << 8) | at[2]);
    }
    else
#endif
    {
    if (p->type==pt_fire)
    {
      theAlpha = 255*(6-p->ramp)/6;
      glColor4ub (at[0], at[1], at[2], theAlpha);
    }
    else
    {
      glColor3ubv (at);
    }

    glVertex3f (p->org[0], p->org[1], p->org[2]);
    pcount++;

	if(pcount == 256) //warp
	{
		glEnd();
		pcount = 0;
		glBegin (GL_POINTS);
	}
    }

#else

    D_DrawParticle (p);

#endif
#ifdef MGL68K_RESIDENT
part_move:
#endif

    p->org[0] += p->vel[0]*frametime;
    p->org[1] += p->vel[1]*frametime;
    p->org[2] += p->vel[2]*frametime;
    
    switch (p->type)
    {
    case pt_static:
      break;
    case pt_fire:
      p->ramp += time1;
      if (p->ramp >= 6)
        p->die = -1;
      else
        p->color = ramp3[(int)p->ramp];
      p->vel[2] += grav;
      break;

    case pt_explode:
      p->ramp += time2;
      if (p->ramp >=8)
        p->die = -1;
      else
        p->color = ramp1[(int)p->ramp];
      for (i=0 ; i<3 ; i++)
        p->vel[i] += p->vel[i]*dvel;
      p->vel[2] -= grav;
      break;

    case pt_explode2:
      p->ramp += time3;
      if (p->ramp >=8)
        p->die = -1;
      else
        p->color = ramp2[(int)p->ramp];
      for (i=0 ; i<3 ; i++)
        p->vel[i] -= p->vel[i]*frametime;
      p->vel[2] -= grav;
      break;

    case pt_blob:
      for (i=0 ; i<3 ; i++)
        p->vel[i] += p->vel[i]*dvel;
      p->vel[2] -= grav;
      break;

    case pt_blob2:
      for (i=0 ; i<2 ; i++)
        p->vel[i] -= p->vel[i]*dvel;
      p->vel[2] -= grav;
      break;

    case pt_grav:
#ifdef QUAKE2
      p->vel[2] -= grav * 20;
      break;
#endif
    case pt_slowgrav:
      p->vel[2] -= grav;
      break;
    }

  }

#ifdef GLQUAKE
#ifdef MGL68K_RESIDENT
  if (pres_active)
    PartResEnd ();
  else
#endif
  /* Always balance the glBegin above, including zero visible particles and
     exact 256-particle batches that leave a fresh empty batch open. */
  glEnd ();
#endif


#ifdef GLQUAKE
  glDisable (GL_BLEND);
  glEnable(GL_TEXTURE_2D);
  glDepthMask (GL_TRUE); //Surgeon
  glColor4f(1,1,1,1);
#else
  D_EndParticles ();
#endif
}
