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
#include "r_local.h"

#ifdef GLQUAKE
#include <mgl/mglmacros.h> 
#endif

#define MAX_PARTICLES     2048  // default max # of particles at one time

//#define ABSOLUTE_MIN_PARTICLES  512   // no fewer than this no matter what's
                    //  on the command line

//surgeon: performance killer in gl - can be disabled
#define ABSOLUTE_MIN_PARTICLES  0


int   ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
int   ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
int   ramp3[8] = {0x6d, 0x6b, 6, 5, 4, 3};

particle_t  *active_particles, *free_particles;

particle_t  *particles;
int     r_numparticles;

vec3_t      r_pright, r_pup, r_ppn;

// fenix@io.com: sort particles 
cvar_t  r_sort_particles = { "r_sort_particles", "0" }; 
cvar_t  r_particle_hack = {"r_particle_hack","0"};
 
typedef struct pbucket_s { 
   particle_t* first; 
   particle_t* last; 
} pbucket_t; 
 
pbucket_t* particle_buckets; 
// -- 

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

  if(r_numparticles)
  {
  particles = (particle_t *)
      Hunk_AllocName (r_numparticles * sizeof(particle_t), "particles");

	// fenix@io.com: sort particles 
    particle_buckets = (pbucket_t *) 
            Hunk_AllocName(r_numparticles * sizeof(pbucket_t), "particle buckets"); 
	// -- 
  } 
	// fenix@io.com: sort particles 
    Cvar_RegisterVariable (&r_sort_particles); 
    Cvar_RegisterVariable (&r_particle_hack); 
	// -- 
}

void Particle_Shutdown(void)
{
//hmmm...
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

  for (i=0 ;i<r_numparticles ; i++)
    particles[i].next = &particles[i+1];
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
  
// FIXME  sprintf (name,"maps/%s.pts", sv.name);

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
    
    if (!free_particles)
    {
      Con_Printf ("Not enough free particles\n");
      break;
    }
    p = free_particles;
    free_particles = p->next;
    p->next = active_particles;
    active_particles = p;
    
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
R_ParticleExplosion

===============
*/
void R_ParticleExplosion (vec3_t org)
{
  int     i, j;
  particle_t  *p;

#ifdef PROXY  
//surgeon start - sprite is handled in cl_tent.c
extern cvar_t r_explosions;

if (r_explosions.value == 0)
	return;
if (r_explosions.value == 1) //standard
//surgeon end
 for (i=0 ; i<1024 ; i++)
   {
    if (!free_particles)
      return;
    p = free_particles;
    free_particles = p->next;
    p->next = active_particles;
    active_particles = p;
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
//surgeon start

else //fast bloblike explosion, restricted
 for (i=0 ; i<50 ; i++)
   {
    if (!free_particles)
      return;
    p = free_particles;
    free_particles = p->next;
    p->next = active_particles;
    active_particles = p;
    p->die = cl.time + 5;
    p->color = ramp1[0];
    p->ramp = rand()&3;
    p->die = cl.time + 0.3;
      p->type = pt_blob2;
      for (j=0 ; j<3 ; j++)
      {
        p->org[j] = org[j] + ((rand()%32)-16);
        p->vel[j] = (rand()%512)-256;
      }
   }
//surgeon end
#else


  for (i=0 ; i<1024 ; i++)
  {
    if (!free_particles)
      return;
    p = free_particles;
    free_particles = p->next;
    p->next = active_particles;
    active_particles = p;

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
#endif
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
    if (!free_particles)
      return;
    p = free_particles;
    free_particles = p->next;
    p->next = active_particles;
    active_particles = p;

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
  int     scale;

  if (count > 130)
    scale = 3;
  else if (count > 20)
    scale = 2;
  else
    scale = 1;

  for (i=0 ; i<count ; i++)
  {
    if (!free_particles)
      return;
    p = free_particles;
    free_particles = p->next;
    p->next = active_particles;
    active_particles = p;

    p->die = cl.time + 0.1*(rand()%5);
    p->color = (color&~7) + (rand()&7);
    p->type = pt_grav;
    for (j=0 ; j<3 ; j++)
    {
      p->org[j] = org[j] + scale*((rand()&15)-8);
      p->vel[j] = dir[j]*15;// + (rand()%300)-150;
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
        if (!free_particles)
          return;
        p = free_particles;
        free_particles = p->next;
        p->next = active_particles;
        active_particles = p;
    
        p->die = cl.time + 2 + (rand()&31) * 0.02;
        p->color = 224 + (rand()&7);
        p->type = pt_grav;
        
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

#ifdef PROXY
extern cvar_t r_teleports;

if(!r_teleports.value)
	return;

if(r_teleports.value == 1)
  {
#endif

  for (i=-16 ; i<16 ; i+=4)
    for (j=-16 ; j<16 ; j+=4)
      for (k=-24 ; k<32 ; k+=4)
      {
        if (!free_particles)
          return;
        p = free_particles;
        free_particles = p->next;
        p->next = active_particles;
        active_particles = p;
    
        p->die = cl.time + 0.2 + (rand()&7) * 0.02;
        p->color = 7 + (rand()&7);
        p->type = pt_grav;
        
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
#ifdef PROXY
 }
else //small edition
 {
  for (i=-8 ; i<4 ; i+=4)
    for (j=-8 ; j<4 ; j+=4)
      for (k=-12 ; k<16 ; k+=4)
      {
        if (!free_particles)
          return;
        p = free_particles;
        free_particles = p->next;
        p->next = active_particles;
        active_particles = p;
    
        p->die = cl.time + 0.2 + (rand()&7) * 0.02;
        p->color = 7 + (rand()&7);
        p->type = pt_grav;
        
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
//surgeon end
#endif
}

void R_RocketTrail (vec3_t start, vec3_t end, int type)
{
  vec3_t  vec;
  float len;
  int     j;
  particle_t  *p;

#ifdef PROXY
extern cvar_t gibfilter;

  //gib-associated blood
  if (gibfilter.value)
  {
    if (type == 2 || type == 4)
	return;
  }
#endif

  VectorSubtract (end, start, vec);
  len = VectorNormalize (vec);
  while (len > 0)
  {
    len -= 3;

    if (!free_particles)
      return;
    p = free_particles;
    free_particles = p->next;
    p->next = active_particles;
    active_particles = p;
    
    VectorCopy (vec3_origin, p->vel);
    p->die = cl.time + 2;

    if (type == 4)
    { // slight blood
      p->type = pt_slowgrav;
      p->color = 67 + (rand()&3);
      for (j=0 ; j<3 ; j++)
        p->org[j] = start[j] + ((rand()%6)-3);
      len -= 3;
    }
    else if (type == 2)
    { // blood
      p->type = pt_slowgrav;
      p->color = 67 + (rand()&3);
      for (j=0 ; j<3 ; j++)
        p->org[j] = start[j] + ((rand()%6)-3);
    }
    else if (type == 6)
    { // voor trail
      p->color = 9*16 + 8 + (rand()&3);
      p->type = pt_static;
      p->die = cl.time + 0.3;
      for (j=0 ; j<3 ; j++)
        p->org[j] = start[j] + ((rand()&15)-8);
    }
    else if (type == 1)
    { // smoke smoke
      p->ramp = (rand()&3) + 2;
      p->color = ramp3[(int)p->ramp];
      p->type = pt_fire;
      for (j=0 ; j<3 ; j++)
        p->org[j] = start[j] + ((rand()%6)-3);
    }
    else if (type == 0)
    { // rocket trail
      p->ramp = (rand()&3);
      p->color = ramp3[(int)p->ramp];
      p->type = pt_fire;
      for (j=0 ; j<3 ; j++)
        p->org[j] = start[j] + ((rand()%6)-3);
    }
    else if (type == 3 || type == 5)
    { // tracer
      static int tracercount;

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
      
    }
    

    VectorAdd (start, vec, start);
  }
}


/* 
=============== 
R_SortParticles 
 
fenix@io.com: sort particles 
=============== 
*/ 
void R_SortParticles(void) 
{ 
    int i; 
    particle_t* p; 
 
    for (i = 0; i < r_numparticles; i++) 
    { 
       particle_buckets[i].last = NULL; 
    } 
 
    p = active_particles; 
 
	for (;;) 
	{ 
        int         index; 
        vec3_t      dvector; 
        float       dscalar; 
        particle_t* next; 
 
        if (!p) break; 
 
        VectorSubtract(p->org, r_origin, dvector); 
        dscalar = Length(dvector); 
        index = r_numparticles / (1.001 + (dscalar * (1.0f/256.0f))); 
 
        if (particle_buckets[index].last) 
        { 
           particle_buckets[index].last->next = p; 
           particle_buckets[index].last = p; 
        } 
        else 
        { 
           particle_buckets[index].first = p; 
           particle_buckets[index].last = p; 
        } 
 
        p = p->next; 
    } 
 
    active_particles = 0; 
 
    for (i = r_numparticles - 1; i >= 0; i--) 
    { 
        if (particle_buckets[i].last != NULL) 
        { 
            particle_buckets[i].last->next = active_particles; 
            active_particles = particle_buckets[i].first; 
        } 
    } 
} 



/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles (void)
{
  particle_t    *p, *kill;
  float     grav;
  int       i;
  float     time2, time3;
  float     time1;
  float     dvel;
  float     frametime;
#ifdef GLQUAKE
  unsigned char *at;
  unsigned char theAlpha;
  vec3_t      up, right;
  float     scale;
  float     prescale;
//LordHavoc:
  float     minparticledist;
  mleaf_t   *leaf;

if(!r_numparticles) //early out condition by Surgeon
	return;

//  alphaTestEnabled = glIsEnabled(GL_ALPHA_TEST);
//  if (alphaTestEnabled) //disabled for amiga port



  glDepthMask (0); //Surgeon
  glDisable(GL_ALPHA_TEST);

  glEnable (GL_BLEND);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

if(r_particle_hack.value)
  glDisable(GL_TEXTURE_2D);
else
  GL_Bind(particletexture);

if(r_particle_hack.value == 2)
  mglBegin (GL_LINES)
else
  mglBegin (GL_TRIANGLES);

  VectorScale (vup, 1.5, up);
  VectorScale (vright, 1.5, right);
#else
if(!r_numparticles) //early out condition by Surgeon
	return;

  D_StartParticles ();

  VectorScale (vright, xscaleshrink, r_pright);
  VectorScale (vup, yscaleshrink, r_pup);
  VectorCopy (vpn, r_ppn);
#endif

  frametime = host_frametime;
  time3 = frametime * 15;
  time2 = frametime * 10; // 15;
  time1 = frametime * 5;
  grav = frametime * 800 * 0.05;
  dvel = 4*frametime;
  
  for ( ;; ) 
  {
    kill = active_particles;
    if (kill && kill->die < cl.time)
    {
      active_particles = kill->next;
      kill->next = free_particles;
      free_particles = kill;
      continue;
    }
    break;
  }

//Surgeon: prescale factor for particles:
prescale = r_origin[0] * vpn[0] + r_origin[1] * vpn[1] + r_origin[2] * vpn[2];

//LordHavoc: //surgeon: use the prescale
minparticledist = prescale /*DotProduct(r_origin, vpn)*/ + 16.0f; 

  for (p=active_particles ; p ; p=p->next)
  {
    for ( ;; )
    {
      kill = p->next;
      if (kill && kill->die < cl.time)
      {
        p->next = kill->next;
        kill->next = free_particles;
        free_particles = kill;
        continue;
      }
      break;
    }

#ifdef GLQUAKE
 
		// LordHavoc: only render if not too close 
		if (DotProduct(p->org, vpn) < minparticledist) 			continue; 
/* 
		// LordHavoc: check if it's in a visible leaf 
		leaf = Mod_PointInLeaf(p->org, cl.worldmodel); 
		if (leaf->visframe != r_visframecount) 
			continue; 
*/

	scale = (p->org[0] * vpn[0]) + 
              (p->org[1] * vpn[1]) +  
              (p->org[2] * vpn[2]) - prescale; 

    if (scale < 20)
      scale = 1;
    else
      scale = 1 + scale * 0.004;
    at = (byte *)&d_8to24table[(int)p->color];

    if (p->type==pt_fire)
      theAlpha = 255*(6-p->ramp)/6;


//      theAlpha = 192;
//    else if (p->type==pt_explode || p->type==pt_explode2)
//      theAlpha = 255*(8-p->ramp)/8;

    else
      theAlpha = 255;


//    glColor3ubv (at);
//    glColor3ubv ((byte *)&d_8to24table[(int)p->color]);

#ifndef AMIGA
    glColor4ub (*at, *(at+1), *(at+2), theAlpha);
    glTexCoord2f (0,0);
    glVertex3fv (p->org);
    glTexCoord2f (1,0);
    glVertex3f (p->org[0] + up[0]*scale, p->org[1] + up[1]*scale, p->org[2] + up[2]*scale);
    glTexCoord2f (0,1);
    glVertex3f (p->org[0] + right[0]*scale, p->org[1] + right[1]*scale, p->org[2] + right[2]*scale);

#else

    mglColor4ub (*at, *(at+1), *(at+2), theAlpha);

if(!r_particle_hack.value)
{
    mglTexCoord2f (0,0);
    glVertex3f (p->org[0], p->org[1], p->org[2]);
    mglTexCoord2f (1,0);
    glVertex3f (p->org[0] + up[0]*scale, p->org[1] + up[1]*scale, p->org[2] + up[2]*scale);
    mglTexCoord2f (0,1);
    glVertex3f (p->org[0] + right[0]*scale, p->org[1] + right[1]*scale, p->org[2] + right[2]*scale);
}
else if(r_particle_hack.value < 2)
{
    glVertex3f (p->org[0], p->org[1], p->org[2]);
    glVertex3f (p->org[0] + up[0]*0.5, p->org[1] + up[1]*0.5, p->org[2] + up[2]*0.5);
    glVertex3f (p->org[0] + right[0]*0.5, p->org[1] + right[1]*0.5, p->org[2] + right[2]*0.5);
}
else
{
    glVertex3f (p->org[0], p->org[1], p->org[2]);
    glVertex3f (p->org[0] + up[0]*0.5, p->org[1] + up[1]*0.5, p->org[2] + up[2]*0.5);
}

#endif

#else
   D_DrawParticle (p);
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

    case pt_slowgrav:
    case pt_grav:
      p->vel[2] -= grav;
      break;
    }
  }

#ifdef GLQUAKE
  glEnd ();

if(r_particle_hack.value)
  glEnable(GL_TEXTURE_2D);

  glDisable (GL_BLEND);
  glDepthMask (1); 
//  if (alphaTestEnabled) //disabled for amiga port
//   glEnable(GL_ALPHA_TEST);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
#else
  D_EndParticles ();
#endif
}


