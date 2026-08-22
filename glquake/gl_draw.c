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


// draw.c -- this is the only file outside the refresh that touches the
// vid buffer

#include "quakedef.h"
#ifndef MINIGL_DISPATCH_CLIENT
#include <mgl/mglmacros.h>
#endif

#define GL_COLOR_INDEX8_EXT     0x80E5

extern unsigned char d_15to8table[65536];

cvar_t    gl_nobind = {"gl_nobind", "0"};
cvar_t    gl_max_size = {"gl_max_size", "256"};

cvar_t    gl_picmip = {"gl_picmip", "0"};

byte    *draw_chars;        // 8*8 graphic characters
qpic_t    *draw_disc;
qpic_t    *draw_backtile;

int     translate_texture;
int     char_texture;
extern int texture_extension_number;

typedef struct
{
  int   texnum;
  float sl, tl, sh, th;
} glpic_t;

byte    conback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
qpic_t    *conback = (qpic_t *)&conback_buffer;


int   gl_lightmap_format = GL_RGBA;
#if 0
//these are replaced by defines in glquake.h
//amiga-version uses GL_RGB5_A1 for alpha textures
int   gl_solid_format = 3;
int   gl_alpha_format = 4;
#endif


int   gl_filter_min = GL_LINEAR;
int   gl_filter_max = GL_LINEAR;

int   texels;

typedef struct
{
  int   texnum;
  char  identifier[64];
  int   width, height;
  qboolean  mipmap;
} gltexture_t;

#define MAX_GLTEXTURES  1024

gltexture_t gltextures[MAX_GLTEXTURES];

int numgltextures = 0; //surgeon: was not initialized to 0

void GL_Bind (int texnum)
{
//  if (gl_nobind.value)
//    texnum = char_texture;

  if (currenttexture == texnum)
    return;

  currenttexture = texnum;
#ifdef _WIN32
  bindTexFunc (GL_TEXTURE_2D, texnum);
#else
  glBindTexture(GL_TEXTURE_2D, texnum);
#endif
}

//#define USE_SCRAP 1

#ifdef USE_SCRAP

/*
=============================================================================

  scrap allocation

  Allocate all the little status bar obejcts into a single texture
  to crutch up stupid hardware / drivers

=============================================================================
*/

#define MAX_SCRAPS    2
#define BLOCK_WIDTH   256
#define BLOCK_HEIGHT  256

int     scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
byte    scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT];
int     scrap_texnum;
qboolean  scrap_dirty = false;

static qboolean scrap_initialized = false; //surgeon

// returns a texture number and the position inside it

int Scrap_AllocBlock (int w, int h, int *x, int *y, qboolean alpha)
{
  int   i, j;
  int   best, best2;
  int   bestx;
  int   texnum;

    if(scrap_initialized == false)
    {
	for(i=0; i<BLOCK_WIDTH; i++)
	{
	  scrap_allocated[0][i] = 0;
	  scrap_allocated[1][i] = 0;
	}

	scrap_initialized = true;
    }

    if(alpha == true) texnum = 0;
    else texnum = 1;

    best = BLOCK_HEIGHT;

    for (i=0 ; i<BLOCK_WIDTH-w ; i++)
    {
      best2 = 0;

      for (j=0 ; j<w ; j++)
      {
        if (scrap_allocated[texnum][i+j] >= best)
          break;

        if (scrap_allocated[texnum][i+j] > best2)
          best2 = scrap_allocated[texnum][i+j];
      }

      if (j == w)
      { // this is a valid spot
        *x = i;
        *y = best = best2;
      }
    }

    if (best + h > BLOCK_HEIGHT)
      return -1;

    for (i=0 ; i<w ; i++)
    {
      scrap_allocated[texnum][*x + i] = best + h;
    }

    return texnum;
}

int scrap_uploads;

void Scrap_Upload (void)
{
  int   texnum;

  scrap_uploads++;

    GL_Bind(scrap_texnum); //alpha texture
    GL_Upload8 (scrap_texels[texnum], BLOCK_WIDTH, BLOCK_HEIGHT, false, true);

    GL_Bind(scrap_texnum + 1); //noalpha texture
    GL_Upload8 (scrap_texels[texnum], BLOCK_WIDTH, BLOCK_HEIGHT, false, false);

  scrap_dirty = false;
}

#endif


//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
  char    name[MAX_QPATH];
  qpic_t    pic;
  byte    padding[32];  // for appended glpic
} cachepic_t;

#define MAX_CACHED_PICS   128
cachepic_t  menu_cachepics[MAX_CACHED_PICS];
int     menu_numcachepics;

byte    menuplyr_pixels[4096];

int   pic_texels;
int   pic_count;

qpic_t *Draw_PicFromWad (char *name)
{
  qpic_t  *p;
  glpic_t *gl;

  p = W_GetLumpName (name);
  gl = (glpic_t *)p->data;

#ifdef USE_SCRAP
// load little ones into the scrap
//surgeeon: is there a bug somewhere in scrap allocation ?

  if (p->width < 64 && p->height < 64)
  {
    int   x, y;
    int   i, j, k;
    int   texnum;
    int s;
    qboolean alpha; //surgeon

    s = p->width * p->height;
    alpha = false;

    for(i=0; i<s; i++)
    {
	if(p->data[i] == 255)
	{
	  alpha = true;
	  break;
	}
    }

    texnum = Scrap_AllocBlock (p->width, p->height, &x, &y, alpha);

    if(texnum == -1)
	goto noscrap;

    scrap_dirty = true;

    k = 0;

    for (i=0 ; i<p->height ; i++)
    {
      for (j=0 ; j<p->width ; j++, k++)
      {
        scrap_texels[texnum][(y+i)*BLOCK_WIDTH + x + j] = p->data[k];
      }
    }

    gl->texnum = scrap_texnum + texnum;
    gl->sl = ((float)x+0.01)/(float)BLOCK_WIDTH;
    gl->sh = ((float)(x+p->width)-0.01)/(float)BLOCK_WIDTH;
    gl->tl = ((float)y+0.01)/(float)BLOCK_WIDTH;
    gl->th = ((float)(y+p->height)-0.01)/(float)BLOCK_WIDTH;

    pic_count++;
    pic_texels += p->width*p->height;

    return p;
  }
  else
#endif
  {
    noscrap:

    gl->texnum = GL_LoadPicTexture (p);
    gl->sl = 0.0;
    gl->sh = 1.0;
    gl->tl = 0.0;
    gl->th = 1.0;

    return p;
  }
}


/*
================
Draw_CachePic
================
*/
qpic_t  *Draw_CachePic (char *path)
{
  cachepic_t  *pic;
  int     i;
  qpic_t    *dat;
  glpic_t   *gl;


	for (pic=menu_cachepics, i=0 ; i<menu_numcachepics ; pic++, i++)
	{
	if (!strcmp (path, pic->name))
		return &pic->pic;
	}

  if (menu_numcachepics == MAX_CACHED_PICS)
    Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");
  menu_numcachepics++;
  strcpy (pic->name, path);

//
// load the pic from disk
//

  dat = (qpic_t *)COM_LoadTempFile (path);  
  if (!dat)
    Sys_Error ("Draw_CachePic: failed to load %s", path);
  SwapPic (dat);


  // HACK HACK HACK --- we need to keep the bytes for
  // the translatable player picture just for the menu
  // configuration dialog
  if (!strcmp (path, "gfx/menuplyr.lmp"))
    memcpy (menuplyr_pixels, dat->data, dat->width*dat->height);

  pic->pic.width = dat->width;
  pic->pic.height = dat->height;

  gl = (glpic_t *)pic->pic.data;
  gl->texnum = GL_LoadPicTexture (dat);
  gl->sl = 0;
  gl->sh = 1;
  gl->tl = 0;
  gl->th = 1;

  return &pic->pic;
}


void Draw_CharToConback (int num, byte *dest)
{
  int   row, col;
  byte  *source;
  int   drawline;
  int   x;

  row = num>>4;
  col = num&15;
  source = draw_chars + (row<<10) + (col<<3);

  drawline = 8;

  while (drawline--)
  {
    for (x=0 ; x<8 ; x++)
      if (source[x] != 255)
        dest[x] = 0x60 + source[x];
    source += 128;
    dest += 320;
  }

}

typedef struct
{
  char *name;
  int minimize, maximize;
} glmode_t;

glmode_t modes[] = {
  {"GL_NEAREST", GL_NEAREST, GL_NEAREST},
  {"GL_LINEAR", GL_LINEAR, GL_LINEAR},
  {"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
  {"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
  {"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
  {"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
===============
Draw_TextureMode_f
===============
*/
void Draw_TextureMode_f (void)
{
  int   i;
  gltexture_t *glt;

  if (Cmd_Argc() == 1)
  {
    for (i=0 ; i< 6 ; i++)
      if (gl_filter_min == modes[i].minimize)
      {
        Con_Printf ("%s\n", modes[i].name);
        return;
      }
    Con_Printf ("current filter is unknown???\n");
    return;
  }

  for (i=0 ; i< 6 ; i++)
  {
    if (!Q_strcasecmp (modes[i].name, Cmd_Argv(1) ) )
      break;
  }
  if (i == 6)
  {
    Con_Printf ("bad filter name\n");
    return;
  }

  gl_filter_min = modes[i].minimize;
  gl_filter_max = modes[i].maximize;

  // change all the existing mipmap texture objects
  for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
  {
    if (glt->mipmap)
    {
      GL_Bind (glt->texnum);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
    }
  }
}

/*
===============
Draw_Init
===============
*/
void Draw_Init (void)
{
  int   i;
  qpic_t  *cb;
  byte  *dest, *src;
  int   x, y;
  char  ver[40];
  glpic_t *gl;
  int   start;
  byte  *ncdata;
  int   f, fstep;
  int    maxsize;

  Cvar_RegisterVariable (&gl_nobind);
  Cvar_RegisterVariable (&gl_max_size);
  Cvar_RegisterVariable (&gl_picmip);

  // 3dfx can only handle 256 wide textures
  if (!Q_strncasecmp ((char *)gl_renderer, "3dfx",4) ||
    strstr((char *)gl_renderer, "Glide"))
    Cvar_Set ("gl_max_size", "256");

  // texture_max_size

  if ((i = COM_CheckParm("-maxsize")) != 0) {
    maxsize = Q_atoi(com_argv[i+1]);
    maxsize &= 0xff80;
    Cvar_SetValue("gl_max_size", maxsize);
    //gl_max_size.value = Q_atof(com_argv[i+1]);
    //if (gl_max_size.value < 128)  gl_max_size.value = 128;
    //if (gl_max_size.value > 1024) gl_max_size.value = 1024;
    //Cvar_Set ("gl_max_size", com_argv[i+1]);
    //printf("MAXSIZE:%f\n",gl_max_size.value);
  }  

  Cmd_AddCommand ("gl_texturemode", &Draw_TextureMode_f);

  // load the console background and the charset
  // by hand, because we need to write the version
  // string into the background before turning
  // it into a texture
  draw_chars = W_GetLumpName ("conchars");
  for (i=0 ; i<256*64 ; i++)
    if (draw_chars[i] == 0)
      draw_chars[i] = 255;  // proper transparent color

  // now turn them into textures
  char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true);

  start = Hunk_LowMark();

  cb = (qpic_t *)COM_LoadTempFile ("gfx/conback.lmp");  
  if (!cb)
    Sys_Error ("Couldn't load gfx/conback.lmp");
  SwapPic (cb);

  // hack the version number directly into the pic
#if defined(__linux__)
  sprintf (ver, "(Linux %2.2f, gl %4.2f) %4.2f", (float)LINUX_VERSION, (float)GLQUAKE_VERSION, (float)VERSION);
#else
  sprintf (ver, "(gl %4.2f beta 1.0) %4.2f", (float)GLQUAKE_VERSION, (float)VERSION);
#endif
  dest = cb->data + 320*186 + 320 - 11 - 8*strlen(ver);
  y = strlen(ver);
  for (x=0 ; x<y ; x++)
    Draw_CharToConback (ver[x], dest+(x<<3));

#if 0
  conback->width = vid.conwidth;
  conback->height = vid.conheight;

  // scale console to vid size
  dest = ncdata = Hunk_AllocName(vid.conwidth * vid.conheight, "conback");
 
  for (y=0 ; y<vid.conheight ; y++, dest += vid.conwidth)
  {
    src = cb->data + cb->width * (y*cb->height/vid.conheight);
    if (vid.conwidth == cb->width)
      memcpy (dest, src, vid.conwidth);
    else
    {
      f = 0;
      fstep = cb->width*0x10000/vid.conwidth;
      for (x=0 ; x<vid.conwidth ; x+=4)
      {
        dest[x] = src[f>>16];
        f += fstep;
        dest[x+1] = src[f>>16];
        f += fstep;
        dest[x+2] = src[f>>16];
        f += fstep;
        dest[x+3] = src[f>>16];
        f += fstep;
      }
    }
  }
#else
  conback->width = cb->width;
  conback->height = cb->height;
  ncdata = cb->data;
#endif

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);  // 13/02/2000 changed: M.Tretene
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

  gl = (glpic_t *)conback->data;
  //gl->texnum = GL_LoadTexture ("conback", conback->width, conback->height, ncdata, false, false);     //  30/01/2000 modified: M.Tretene
  gl->texnum = GL_LoadTexture ("conback", conback->width, conback->height, ncdata, false, true);
  gl->sl = 0;
  gl->sh = 1;
  gl->tl = 0;
  gl->th = 1;
  conback->width = vid.width;
  conback->height = vid.height;

  // free loaded console
  Hunk_FreeToLowMark(start);

  // save a texture slot for translated picture
  translate_texture = texture_extension_number++;

#ifdef USE_SCRAP
  // save slot for scraps
  scrap_texnum = texture_extension_number;
  texture_extension_number += MAX_SCRAPS;
#endif

  //
  // get the other pics we need
  //

  draw_disc = Draw_PicFromWad ("disc");
  draw_backtile = Draw_PicFromWad ("backtile");
}



/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Character (int x, int y, int num)
{
  byte      *dest;
  byte      *source;
  unsigned short  *pusdest;
  int       drawline; 
  int       row, col;
  float     frow, fcol, size;

  if (num == 32)
    return;   // space

  num &= 255;
  
  if (y <= -8)
    return;     // totally off screen

  row = num>>4;
  col = num&15;

  frow = row*0.0625;
  fcol = col*0.0625;
  size = 0.0625;

  GL_Bind (char_texture);

#if !defined(AMIGA)
  glBegin (GL_QUADS);
#else
  glBegin (MGL_FLATFAN);
#endif

  glTexCoord2f (fcol, frow);
  glVertex2f (x, y);
  glTexCoord2f (fcol + size, frow);
  glVertex2f (x+8, y);
  glTexCoord2f (fcol + size, frow + size);
  glVertex2f (x+8, y+8);
  glTexCoord2f (fcol, frow + size);
  glVertex2f (x, y+8);
  glEnd ();

}

/*
================
Draw_String
================
*/
void Draw_String (int x, int y, char *str)
{
#if 0 //Surgeon: very inefficient switching blendmodes and binding same texture multiple times

  while (*str)
  {
    Draw_Character (x, y, *str);
    str++;
    x += 8;
  }
#else

  int       row, col;
  int num;
  float     frow, fcol;
  float fx,fy;
  const float size = 0.0625;

  if (y <= -8)
    return;     // totally off screen

  GL_Bind (char_texture);

  fy = (float)y;

  while (*str)
  {
  num = *str;

  if (num == 32)
    goto loc0;   // space

  num &= 255;
  
  row = num>>4;
  col = num&15;

  frow = (float)row*0.0625;
  fcol = (float)col*0.0625;

  fx = (float)x;

#if !defined(AMIGA)
  glBegin (GL_QUADS);
#else
  glBegin (MGL_FLATFAN);
#endif

  glTexCoord2f (fcol, frow);
  glVertex2f (fx, fy);
  glTexCoord2f (fcol + size, frow);
  glVertex2f (fx+8.f, fy);
  glTexCoord2f (fcol + size, frow + size);
  glVertex2f (fx+8.f, fy+8.f);
  glTexCoord2f (fcol, frow + size);
  glVertex2f (fx, fy+8.f);
  glEnd ();

  loc0:
    str++;
    x += 8;
  }

#endif
}

/*
================
Draw_DebugChar

Draws a single character directly to the upper right corner of the screen.
This is for debugging lockups by drawing different chars in different parts
of the code.
================
*/
void Draw_DebugChar (char num)
{
}

/*
=============
Draw_AlphaPic
=============
*/

void Draw_AlphaPic (int x, int y, qpic_t *pic, float alpha)
{
  byte      *dest, *source;
  unsigned short  *pusdest;
  int       v, u;
  glpic_t     *gl;

#ifdef USE_SCRAP
  if (scrap_dirty)
    Scrap_Upload ();
#endif

  gl = (glpic_t *)pic->data;

  glDisable(GL_ALPHA_TEST);
  glEnable (GL_BLEND);

  glColor4f (1,1,1,alpha);
  GL_Bind (gl->texnum);

#if !defined(AMIGA)
  glBegin (GL_QUADS);
#else
  glBegin (MGL_FLATFAN);
#endif

  glTexCoord2f (gl->sl, gl->tl);
  glVertex2f (x, y);
  glTexCoord2f (gl->sh, gl->tl);
  glVertex2f (x+pic->width, y);
  glTexCoord2f (gl->sh, gl->th);
  glVertex2f (x+pic->width, y+pic->height);
  glTexCoord2f (gl->sl, gl->th);
  glVertex2f (x, y+pic->height);
  glEnd ();

  glDisable (GL_BLEND);
  glEnable(GL_ALPHA_TEST);
  glColor4f (1,1,1,1);
}


/*
=============
Draw_Pic
=============
*/

void Draw_Pic (int x, int y, qpic_t *pic)
{
  byte      *dest, *source;
  unsigned short  *pusdest;
  int       v, u;
  glpic_t     *gl;


#ifdef USE_SCRAP
  if (scrap_dirty)
    Scrap_Upload ();
#endif

  gl = (glpic_t *)pic->data;

  GL_Bind (gl->texnum);

#if !defined(AMIGA)
  glBegin (GL_QUADS);
#else
  glBegin (MGL_FLATFAN);
#endif

  glTexCoord2f (gl->sl, gl->tl);
  glVertex2f (x, y);
  glTexCoord2f (gl->sh, gl->tl);
  glVertex2f (x+pic->width, y);
  glTexCoord2f (gl->sh, gl->th);
  glVertex2f (x+pic->width, y+pic->height);
  glTexCoord2f (gl->sl, gl->th);
  glVertex2f (x, y+pic->height);
  glEnd ();
}


/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic (int x, int y, qpic_t *pic)
{
  byte  *dest, *source, tbyte;
  unsigned short  *pusdest;
  int       v, u;
  glpic_t *gl; //surgeon

  if (x < 0 || (unsigned)(x + pic->width) > vid.width || y < 0 ||
     (unsigned)(y + pic->height) > vid.height)
  {
    Sys_Error ("Draw_TransPic: bad coordinates");
  }
    
/*
  Draw_Pic (x, y, pic);
*/

//surgeon: inlined
  gl = (glpic_t *)pic->data;
  GL_Bind (gl->texnum);

#if !defined(AMIGA)
  glBegin (GL_QUADS);
#else
  glBegin (MGL_FLATFAN);
#endif

  glTexCoord2f (gl->sl, gl->tl);
  glVertex2f (x, y);
  glTexCoord2f (gl->sh, gl->tl);
  glVertex2f (x+pic->width, y);
  glTexCoord2f (gl->sh, gl->th);
  glVertex2f (x+pic->width, y+pic->height);
  glTexCoord2f (gl->sl, gl->th);
  glVertex2f (x, y+pic->height);
  glEnd ();
}


/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/

void Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte *translation)
{
  int       v, u, c;
  unsigned    trans[64*64], *dest;
  byte      *src;
  int       p;

  GL_Bind (translate_texture);

  c = pic->width * pic->height;

  dest = trans;
  for (v=0 ; v<64 ; v++, dest += 64)
  {
    src = &menuplyr_pixels[ ((v*pic->height)>>6) *pic->width];
    for (u=0 ; u<64 ; u++)
    {
      p = src[(u*pic->width)>>6];
      if (p == 255)
        dest[u] = p;
      else
        dest[u] =  d_8to24table[translation[p]];
    }
  }

  glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


#if !defined(AMIGA)
  glBegin (GL_QUADS);
#else
  glBegin (MGL_FLATFAN);
#endif

  glTexCoord2f (0, 0);
  glVertex2f (x, y);
  glTexCoord2f (1, 0);
  glVertex2f (x+pic->width, y);
  glTexCoord2f (1, 1);
  glVertex2f (x+pic->width, y+pic->height);
  glTexCoord2f (0, 1);
  glVertex2f (x, y+pic->height);
  glEnd ();
}


/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
  int y = (vid.height * 3) >> 2;

  if (lines > y)
    Draw_Pic(0, lines - vid.height, conback);
  else
    Draw_AlphaPic (0, lines - vid.height, conback, (float)(1.2 * lines)/y);
}


/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h)
{
  GL_Bind (*(int *)draw_backtile->data);

#if !defined(AMIGA)
  glBegin (GL_QUADS);
#else
  glBegin (MGL_FLATFAN);
#endif

  glTexCoord2f (x/64.0, y/64.0);
  glVertex2f (x, y);
  glTexCoord2f ( (x+w)/64.0, y/64.0);
  glVertex2f (x+w, y);
  glTexCoord2f ( (x+w)/64.0, (y+h)/64.0);
  glVertex2f (x+w, y+h);
  glTexCoord2f ( x/64.0, (y+h)/64.0 );
  glVertex2f (x, y+h);
  glEnd ();
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
  glDisable (GL_TEXTURE_2D);

  glColor3f (host_basepal[c*3]/255.0,
    host_basepal[c*3+1]/255.0,
    host_basepal[c*3+2]/255.0);

#if !defined(AMIGA)
  glBegin (GL_QUADS);
#else
  glBegin (MGL_FLATFAN);
#endif

  glVertex2f (x,y);
  glVertex2f (x+w, y);
  glVertex2f (x+w, y+h);
  glVertex2f (x, y+h);

  glEnd ();
  glColor3f (1,1,1);
  glEnable (GL_TEXTURE_2D);
}
//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
#ifdef MINIGL_DISPATCH_CLIENT
  Draw_AlphaFill (0, 0, vid.width, vid.height, 0, 0, 0, 0.8f);
#else
  glEnable (GL_BLEND);
  glDisable (GL_TEXTURE_2D);
  glColor4f (0, 0, 0, 0.8);

#if !defined(AMIGA)
  glBegin (GL_QUADS);
#else
  glBegin (MGL_FLATFAN);
#endif

  glVertex2f (0,0);
  glVertex2f (vid.width, 0);
  glVertex2f (vid.width, vid.height);
  glVertex2f (0, vid.height);

  glEnd ();
  glColor4f (1,1,1,1);

  glEnable (GL_TEXTURE_2D);
  glDisable (GL_BLEND);
#endif

  Sbar_Changed();
}

#ifdef MINIGL_DISPATCH_CLIENT
/* Packed immediate-mode diffuse alpha currently reaches R200 as zero.
 * Texture alpha is correct. Cache sixteen white 1x1 textures, one per alpha
 * step, and modulate their RGB with diffuse color. This avoids reallocating a
 * resident Radeon texture on every frame of a damage/bonus flash. */
void Draw_AlphaFill (int x, int y, int w, int h,
                      float red, float green, float blue, float alpha)
{
  static int fill_texture[16];
  int level;
  byte pixel[4];

  level = (int)(alpha * 15.0f + 0.5f);
  if (level < 0) level = 0;
  if (level > 15) level = 15;
  if (!fill_texture[level])
  {
    fill_texture[level] = texture_extension_number++;
    pixel[0] = pixel[1] = pixel[2] = 255;
    pixel[3] = (byte)(level * 17);
    GL_Bind (fill_texture[level]);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
  else
    GL_Bind (fill_texture[level]);

  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glDisable (GL_ALPHA_TEST);
  glEnable (GL_TEXTURE_2D);
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColor4f (red, green, blue, 1);
  glBegin (MGL_FLATFAN);
  glTexCoord2f (0, 0); glVertex2f (x, y);
  glTexCoord2f (1, 0); glVertex2f (x+w, y);
  glTexCoord2f (1, 1); glVertex2f (x+w, y+h);
  glTexCoord2f (0, 1); glVertex2f (x, y+h);
  glEnd ();
  glDisable (GL_BLEND);
  glEnable (GL_ALPHA_TEST);
  glColor4f (1, 1, 1, 1);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}
#endif

//=============================================================================

/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void Draw_BeginDisc (void)
{
  if (!draw_disc)
    return;
  glDrawBuffer  (GL_FRONT);
  Draw_Pic (vid.width - 24, 0, draw_disc);
  glDrawBuffer  (GL_BACK);
}


/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void Draw_EndDisc (void)
{
}

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D (void)
{
#ifdef MINIGL_DISPATCH_CLIENT
  static qboolean checked_setup;
#endif
//  glViewport (glx, gly, glwidth, glheight);
    glViewport (0, 0, glwidth, glheight);
  
#if !defined(AMIGA)
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity ();
  glOrtho  (0, vid.width, vid.height, 0, -99999, 99999);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity ();
#endif

  glDisable (GL_DEPTH_TEST);
  glDisable (GL_CULL_FACE);
  glDisable (GL_BLEND);
  glEnable (GL_ALPHA_TEST);

#if defined(AMIGA) && !defined(MINIGL_DISPATCH_CLIENT)
  glDisable(MGL_PERSPECTIVE_MAPPING);
#endif

  glColor4f (1,1,1,1);

#ifdef MINIGL_DISPATCH_CLIENT
  /* Never inherit GL_MODULATE from the 3D pass: its zero diffuse alpha would
   * make alpha-tested characters and menu pictures disappear. */
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
#endif

#ifdef MINIGL_DISPATCH_CLIENT
  if (!checked_setup)
  {
    GL_CheckErrors ("2D setup");
    checked_setup = true;
  }
#endif
}

//====================================================================

/*
================
GL_FindTexture
================
*/
int GL_FindTexture (char *identifier)
{
  int   i;
  gltexture_t *glt;

  for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
  {
    if (!strcmp (identifier, glt->identifier))
      return gltextures[i].texnum;
  }

  return -1;
}

/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight)
{
  int   i, j;
  unsigned  *inrow;
  unsigned  frac, fracstep;

  fracstep = inwidth*0x10000/outwidth;
  for (i=0 ; i<outheight ; i++, out += outwidth)
  {
    inrow = in + inwidth*(i*inheight/outheight);
    frac = fracstep >> 1;
    for (j=0 ; j<outwidth ; j+=4)
    {
      out[j] = inrow[frac>>16];
      frac += fracstep;
      out[j+1] = inrow[frac>>16];
      frac += fracstep;
      out[j+2] = inrow[frac>>16];
      frac += fracstep;
      out[j+3] = inrow[frac>>16];
      frac += fracstep;
    }
  }
}

/*
================
GL_Resample8BitTexture -- JACK
================
*/
void GL_Resample8BitTexture (unsigned char *in, int inwidth, int inheight, unsigned char *out,  int outwidth, int outheight)
{
  int   i, j;
  unsigned  char *inrow;
  unsigned  frac, fracstep;

  fracstep = inwidth*0x10000/outwidth;
  for (i=0 ; i<outheight ; i++, out += outwidth)
  {
    inrow = in + inwidth*(i*inheight/outheight);
    frac = fracstep >> 1;
    for (j=0 ; j<outwidth ; j+=4)
    {
      out[j] = inrow[frac>>16];
      frac += fracstep;
      out[j+1] = inrow[frac>>16];
      frac += fracstep;
      out[j+2] = inrow[frac>>16];
      frac += fracstep;
      out[j+3] = inrow[frac>>16];
      frac += fracstep;
    }
  }
}


/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GL_MipMap (byte *in, int width, int height)
{
  int   i, j;
  byte  *out;

  width <<=2;
  height >>= 1;
  out = in;
  
  for (i=0 ; i<height ; i++, in+=width)
  {
    for (j=0 ; j<width ; j+=8, out+=4, in+=8)
    {
      out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
      out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
      out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
      out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
    }
  }
}

/*
================
GL_MipMap8Bit

Mipping for 8 bit textures
================
*/
void GL_MipMap8Bit (byte *in, int width, int height)
{
  int   i, j;
  unsigned short     r,g,b;
  byte  *out, *at1, *at2, *at3, *at4;

//  width <<=2;
  height >>= 1;
  out = in;
  for (i=0 ; i<height ; i++, in+=width)
  {
    for (j=0 ; j<width ; j+=2, out+=1, in+=2)
    {
      at1 = (byte *) (d_8to24table + in[0]);
      at2 = (byte *) (d_8to24table + in[1]);
      at3 = (byte *) (d_8to24table + in[width+0]);
      at4 = (byte *) (d_8to24table + in[width+1]);

      r = (at1[0]+at2[0]+at3[0]+at4[0]); r>>=5;
      g = (at1[1]+at2[1]+at3[1]+at4[1]); g>>=5;
      b = (at1[2]+at2[2]+at3[2]+at4[2]); b>>=5;

      out[0] = d_15to8table[(r<<0) + (g<<5) + (b<<10)];
    }
  }
}

/*
===============
GL_Upload32
===============
*/
void GL_Upload32 (unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
  int     samples;
static  unsigned  scaled[1024*512]; // [512*256];
  int     scaled_width, scaled_height;

  for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
    ;
  for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
    ;

  scaled_width >>= (int)gl_picmip.value;
  scaled_height >>= (int)gl_picmip.value;

  if (scaled_width > gl_max_size.value)
    scaled_width = gl_max_size.value;
  if (scaled_height > gl_max_size.value)
    scaled_height = gl_max_size.value;

  if (scaled_width * scaled_height > sizeof(scaled)/4)
    Sys_Error ("GL_LoadTexture: too big");

  samples = alpha ? gl_alpha_format : gl_solid_format;

#if 0
  if (mipmap)
    gluBuild2DMipmaps (GL_TEXTURE_2D, samples, width, height, GL_RGBA, GL_UNSIGNED_BYTE, trans);
  else if (scaled_width == width && scaled_height == height)
    glTexImage2D (GL_TEXTURE_2D, 0, samples, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
  else
  {
    gluScaleImage (GL_RGBA, width, height, GL_UNSIGNED_BYTE, trans,
      scaled_width, scaled_height, GL_UNSIGNED_BYTE, scaled);
    glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
  }
#else
texels += scaled_width * scaled_height;

  if (scaled_width == width && scaled_height == height)
  {
    if (!mipmap)
    {
      glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
      goto done;
    }
    memcpy (scaled, data, width*height*4);
  }
  else
    GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);

  glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
  if (mipmap)
  {
    int   miplevel;

    miplevel = 0;
    while (scaled_width > 1 || scaled_height > 1)
    {
      GL_MipMap ((byte *)scaled, scaled_width, scaled_height);
      scaled_width >>= 1;
      scaled_height >>= 1;
      if (scaled_width < 1)
        scaled_width = 1;
      if (scaled_height < 1)
        scaled_height = 1;
      miplevel++;
      glTexImage2D (GL_TEXTURE_2D, miplevel, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
    }
  }
done: ;
#endif


  if (mipmap)
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
  }
  else
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
  }
}

void GL_Upload8_EXT (byte *data, int width, int height,  qboolean mipmap, qboolean alpha) 
{
  int     i, s;
  qboolean  noalpha;
  int     p;
  static unsigned j;
  int     samples;
    static  unsigned char scaled[1024*512]; // [512*256];
  int     scaled_width, scaled_height;

  s = width*height;
  // if there are no transparent pixels, make it a 3 component
  // texture even if it was specified as otherwise

/*
  if (alpha)
  {
    noalpha = true;
    for (i=0 ; i<s ; i++)
    {
      if (data[i] == 255)
        noalpha = false;
    }

    if (alpha && noalpha)
      alpha = false;
  }
*/

  for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
    ;
  for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
    ;

  scaled_width >>= (int)gl_picmip.value;
  scaled_height >>= (int)gl_picmip.value;

  if (scaled_width > gl_max_size.value)
    scaled_width = gl_max_size.value;
  if (scaled_height > gl_max_size.value)
    scaled_height = gl_max_size.value;

  if (scaled_width * scaled_height > sizeof(scaled))
    Sys_Error ("GL_LoadTexture: too big");

  samples = 1; // alpha ? gl_alpha_format : gl_solid_format;

  texels += scaled_width * scaled_height;

  if (scaled_width == width && scaled_height == height)
  {
    if (!mipmap)
    {
      glTexImage2D (GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX , GL_UNSIGNED_BYTE, data);
      goto done;
    }
    memcpy (scaled, data, width*height);
  }
  else
    GL_Resample8BitTexture (data, width, height, scaled, scaled_width, scaled_height);

  glTexImage2D (GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, scaled);
  if (mipmap)
  {
    int   miplevel;

    miplevel = 0;
    while (scaled_width > 1 || scaled_height > 1)
    {
      GL_MipMap8Bit ((byte *)scaled, scaled_width, scaled_height);
      scaled_width >>= 1;
      scaled_height >>= 1;
      if (scaled_width < 1)
        scaled_width = 1;
      if (scaled_height < 1)
        scaled_height = 1;
      miplevel++;
      glTexImage2D (GL_TEXTURE_2D, miplevel, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, scaled);
    }
  }
done: ;


  if (mipmap)
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
  }
  else
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
  }
}


/*
===============
GL_Upload8
===============
*/
void GL_Upload8 (byte *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
  static  unsigned  trans[640*480];   // FIXME, temporary
  int     i, s;
  qboolean  noalpha;
  int     p;

  s = width*height;
  // if there are no transparent pixels, make it a 3 component
  // texture even if it was specified as otherwise

  if (alpha)
  {
    noalpha = true;

    for (i=0 ; i<s ; i++)
    {
      p = data[i];
      if (p == 255) noalpha = false;

      trans[i] = d_8to24table[p];
    }

    if (alpha && noalpha) alpha = false;
  }
  else
  {
    if (s&3)
      Sys_Error ("GL_Upload8: s&3");

    for (i=0 ; i<s ; i+=4)
    {
      trans[i]   = d_8to24table[data[i]];
      trans[i+1] = d_8to24table[data[i+1]];
      trans[i+2] = d_8to24table[data[i+2]];
      trans[i+3] = d_8to24table[data[i+3]];
    }
  }

#ifndef AMIGA
  if (VID_Is8bit() && !alpha && (data != scrap_texels[0]))
  {
    GL_Upload8_EXT (data, width, height, mipmap, alpha);
    return;
  }
#endif

  GL_Upload32 (trans, width, height, mipmap, alpha);
}

/*
================
GL_LoadTexture
================
*/
int GL_LoadTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha)
{
  qboolean  noalpha;
  int     i, p, s;
  gltexture_t *glt;
  
  // see if the texture is allready present
  if (identifier != NULL)
  {
    for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
    {
      if (!strcmp (identifier, glt->identifier))
      {
        if (width != glt->width || height != glt->height)
          Sys_Error ("GL_LoadTexture: cache mismatch");

        return gltextures[i].texnum;
      }
    }
  }
  
  glt = &gltextures[numgltextures];
  numgltextures++;

  strcpy (glt->identifier, identifier);
  glt->texnum = texture_extension_number;
  glt->width = width;
  glt->height = height;
  glt->mipmap = mipmap;

  GL_Bind(texture_extension_number );
  GL_Upload8 (data, width, height, mipmap, alpha);

#ifdef MINIGL_DISPATCH_CLIENT
  GL_CheckErrors (identifier ? identifier : "unnamed texture upload");
#endif

  texture_extension_number++;

  return texture_extension_number-1;
}

/*
================
GL_LoadPicTexture
================
*/
int GL_LoadPicTexture (qpic_t *pic)
{
  return GL_LoadTexture (NULL, pic->width, pic->height, pic->data, false, true);
}

/****************************************/

static GLenum currenttextureunit = GL_TEXTURE0_ARB;

void GL_SelectTexture (GLenum target) 
{
  if (target != GL_TEXTURE0_ARB && target != GL_TEXTURE1_ARB)
    return;

  if (target == currenttextureunit)
    return;

  cnttextures[currenttextureunit - GL_TEXTURE0_ARB] = currenttexture;
  glActiveTextureARB (target);
  currenttexture = cnttextures[target - GL_TEXTURE0_ARB];
  currenttextureunit = target;
}
