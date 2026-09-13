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


// gl_vidamiga.c - 19/01/2000 Massimiliano Tretene
// modified by SuRgEoN

#ifdef __GNUC__
//#include <sys/ioctl.h>
#endif

#pragma amiga-align
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <intuition/intuition.h>
#if defined (__GNUC__) && defined (__PPC__)
#include <powerup/ppcproto/exec.h>
#else
#include <proto/exec.h>
#endif
#include <proto/graphics.h>
#include <proto/intuition.h>
#pragma default-align

#include <stdarg.h>
#include <stdio.h>
#include "miniglclient.h"
#if defined(MINIGL_PERF_DIAGNOSTICS) || defined(MINIGL_DIAGNOSTICS)
#include <libraries/minigl_perf.h>
#endif

#include "quakedef.h"


#define WARP_WIDTH              320
#define WARP_HEIGHT             240

unsigned char rawkeyconv[] = {
  '`','1','2','3','4','5','6','7','8','9','0','-','=','\\',0,'0',
  'q','w','e','r','t','y','u','i','o','p','[',']',0,K_END,K_DOWNARROW,K_PGDN,
  'a','s','d','f','g','h','j','k','l',';','\'',K_F12,0,K_LEFTARROW,0,K_RIGHTARROW,
  K_F11,'z','x','c','v','b','n','m',',','.','/',0,0,K_HOME,K_UPARROW,K_PGUP,
  K_SPACE,K_BACKSPACE,K_TAB,K_ENTER,K_ENTER,K_ESCAPE,K_DEL,0,
  0,0,'-',0,K_UPARROW,K_DOWNARROW,K_RIGHTARROW,K_LEFTARROW,
  K_F1,K_F2,K_F3,K_F4,K_F5,K_F6,K_F7,K_F8,K_F9,K_F10,'(',')','/','*','+',K_PAUSE,
  K_SHIFT,K_SHIFT,0,K_CTRL,K_ALT,K_ALT,0,0,
  K_MOUSE1,K_MOUSE2,K_MOUSE3
};

int shutdown_keyboard = 0;

struct Window *QuakeWindow = NULL;

#ifdef WOS
/* MOS2WOS does not define the Intuition/Graphics library bases (the old
 * PowerUp toolchain libs used to provide them). Defined here and opened in
 * VID_Init; in_amigamouse/in_amigapsx/in_amigajoy/snd_amiga reference them. */
struct GfxBase *GfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
#endif

GLboolean zbuffer = GL_TRUE;

#ifdef NOMOUSEASM
int m_move[2] = {0,0};
int *mousemove = m_move;
#endif

GLfloat mouse_x = 0.0, mouse_y = 0.0, mouse_z = 0.0; GLint offset = 0;
GLfloat fov = 70.0;
GLfloat inf_w = 0.1;
GLfloat zback = 1000.0;
GLfloat alpha = 1.0;

int numb = 3; //triplebuffer
int bpp = 32;

GLboolean guardband = GL_FALSE; //surgeon

qboolean gl_warp = false;

unsigned    d_8to24table[256];
unsigned char d_15to8table[65536];

static qboolean gl_videosync = true;
static qboolean gl_dithering = true;
static int gl_lockmode = MGL_LOCK_MANUAL;
static int scr_width, scr_height;
static float width;
static qboolean UPDATE = FALSE;  
static GLfloat gl_polyoffset = 0.0;

void VID_MenuDraw (void);
void VID_MenuKey (int);

int	odd_display = 0; //surgeon

/*-----------------------------------------------------------------------*/


int   texture_mode = GL_LINEAR;

extern int   gl_filter_min;
extern int   gl_filter_max;
extern cvar_t gl_max_size;

int   texture_extension_number = 1;

float   gldepthmin, gldepthmax;

cvar_t  gl_ztrick = {"gl_ztrick","0"};
cvar_t  gl_width = {"gl_width","320"};

cvar_t _windowed_mouse = {"_windowed_mouse", "1", true}; 

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

static float vid_gamma = 1.0;

qboolean is8bit = false;
qboolean isPermedia = false;
qboolean gl_mtexable = false;

/*-----------------------------------------------------------------------*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect (int x, int y, int width, int height)
{
}


void VID_Shutdown(void)
{
      mglDeleteContext();
      MGLTerm();
#ifdef WOS
      if (GfxBase) { CloseLibrary((struct Library *)GfxBase); GfxBase = NULL; }
      if (IntuitionBase) { CloseLibrary((struct Library *)IntuitionBase); IntuitionBase = NULL; }
#endif
}

static void VID_ConfigureInputWindow(void)
{
  ULONG idcmp;

#ifdef MINIGL_DISPATCH_CLIENT
  QuakeWindow = (struct Window *)mglGetInputWindowHandle();
  if (!QuakeWindow)
    QuakeWindow = (struct Window *)mglGetWindowHandle();
#else
  QuakeWindow = (struct Window *)mglGetWindowHandle();
#endif

  if (!QuakeWindow)
    return;

  idcmp = QuakeWindow->IDCMPFlags | IDCMP_CLOSEWINDOW |
          IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS;
#ifdef NOMOUSEASM
  idcmp |= IDCMP_MOUSEMOVE | IDCMP_DELTAMOVE;
#endif
  ModifyIDCMP(QuakeWindow, idcmp);
}

static void Check_Gamma (unsigned char *pal)
{
  float f, inf;
  static unsigned char palette[768];
  register int   i;
  //static qboolean firsttime = false;
  //if (firsttime) return;
  
  if ((i = COM_CheckParm("-gamma")) == 0) {
    if ((gl_renderer && strstr(gl_renderer, "Voodoo")) ||
      (gl_vendor && strstr(gl_vendor, "3Dfx")))
      vid_gamma = 1;
    else
      vid_gamma = 0.7; // default to 0.7 on non-3dfx hardware
  } else
    vid_gamma = Q_atof(com_argv[i+1]);

  //firsttime=true;

  for (i=0 ; i<768 ; i++)
  {
    f = pow ( (pal[i]+1)/256.0 , vid_gamma );
    inf = f*255 + 0.5;
    if (inf < 0)
      inf = 0;
    if (inf > 255)
      inf = 255;
    palette[i] = inf;
  }

  memcpy (pal, palette, sizeof(palette));
}

void VID_ShiftPalette(unsigned char *p)
{
//  VID_SetPalette(p);
}

void	VID_SetPalette (unsigned char *palette)
{
	register byte	*pal;
	register unsigned short r,g,b;
	register unsigned     v;
	register unsigned short i;
	register unsigned	*table;
	int     r1,g1,b1;
	int		k;
	FILE *f;
	char s[255];
	float dist, bestdist;
	static qboolean palflag = false;

//
// 8 8 8 encoding
//
	pal = palette;
	table = d_8to24table;
	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

		if (COM_CheckParm ("-hicontrast")) 
		{ 
			// add some contrast 
			r1 = r - 200; r1 = 1.05 * r1; r1 = 200 + r1; 
			g1 = g - 200; g1 = 1.05 * g1; g1 = 200 + g1; 
			b1 = b - 200; b1 = 1.05 * b1; b1 = 200 + b1; 
 
			if (r1 > 255) r1 = 255; if (r1 < 0) r1 = 0; 
			if (g1 > 255) g1 = 255; if (g1 < 0) g1 = 0; 
			if (b1 > 255) b1 = 255; if (b1 < 0) b1 = 0; 
		} 
		
//	v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
//	v = (255<<0) + (r<<24) + (g<<16) + (b<<8);
//    v = LittleLong((170<<24) + (r<<0) + (g<<8) + (b<<16));

if (COM_CheckParm ("-hicontrast")) 
    v = (170<<0) + (r1<<24) + (g1<<16) + (b1<<8);
else
    v = (170<<0) + (r<<24) + (g<<16) + (b<<8);
		*table++ = v;
	}

	d_8to24table[255] &= 0xffffff00;	// 255 is transparent

	// JACK: 3D distance calcs - k is last closest, l is the distance.
	// FIXME: Precalculate this and cache to disk.

	if (palflag)
		return;
	palflag = true;

	COM_FOpenFile("glquake/15to8.pal", &f);
	if (f) {
		fread(d_15to8table, 1<<15, 1, f);
		fclose(f);
	} else {
		for (i=0; i < (1<<15); i++) {
			/* Maps
 			000000000000000
 			000000000011111 = Red  = 0x1F
 			000001111100000 = Blue = 0x03E0
 			111110000000000 = Grn  = 0x7C00
 			*/

 			r = ((i & 0x1F) << 3)+4;
 			g = ((i & 0x03E0) >> 2)+4;
 			b = ((i & 0x7C00) >> 7)+4;

			pal = (unsigned char *)d_8to24table;

			for (v=0,k=0,bestdist=10000.0; v<256; v++, pal+=4) {
 				r1 = (int)r - (int)pal[0];
 				g1 = (int)g - (int)pal[1];
 				b1 = (int)b - (int)pal[2];
				dist = sqrt(((r1*r1)+(g1*g1)+(b1*b1)));
				if (dist < bestdist) {
					k=v;
					bestdist = dist;
				}
			}
			d_15to8table[i]=k;
		}
		sprintf(s, "%s/glquake", com_gamedir);
 		Sys_mkdir (s);
		sprintf(s, "%s/glquake/15to8.pal", com_gamedir);
		if ((f = fopen(s, "wb")) != NULL) {
			fwrite(d_15to8table, 1<<15, 1, f);
			fclose(f);
		}
	}
}


static qboolean GL_ExtensionSupported(const char *name)
{
  const char *where;
  const char *terminator;
  int namelen;

  if (!gl_extensions || !name || strchr(name, ' '))
    return false;

  namelen = strlen(name);
  where = gl_extensions;

  while ((where = strstr(where, name)) != NULL)
  {
    terminator = where + namelen;
    if ((where == gl_extensions || where[-1] == ' ') &&
        (*terminator == 0 || *terminator == ' '))
      return true;
    where = terminator;
  }

  return false;
}

void CheckMultiTextureExtensions (void)
{
  if (COM_CheckParm("-extensions") &&
      (GL_ExtensionSupported("GL_ARB_multitexture") ||
       GL_ExtensionSupported("GL_MGL_ARB_multitexture")))
  {
    Con_Printf("Multitexture extension enabled.\n");
    gl_mtexable = true;
  }
}


/*
===============
GL_Init
===============
*/
#ifdef MINIGL_DIAGNOSTICS
void GL_CheckErrors (char *where)
{
#ifdef MINIGL_DISPATCH_CLIENT
  static int reports;
  GLenum error;
  int drained = 0;
  char *name;

  while (drained < 16 && (error = glGetError()) != GL_NO_ERROR)
  {
    switch (error)
    {
    case GL_INVALID_ENUM:      name = "GL_INVALID_ENUM"; break;
    case GL_INVALID_VALUE:     name = "GL_INVALID_VALUE"; break;
    case GL_INVALID_OPERATION: name = "GL_INVALID_OPERATION"; break;
    case GL_OUT_OF_MEMORY:     name = "GL_OUT_OF_MEMORY"; break;
    case GL_STACK_OVERFLOW:    name = "GL_STACK_OVERFLOW"; break;
    case GL_STACK_UNDERFLOW:   name = "GL_STACK_UNDERFLOW"; break;
    default:                   name = "unknown"; break;
    }
    if (reports < 64)
      Con_Printf ("MiniGL error after %s: %s (%lu)\n", where, name,
                  (unsigned long)error);
    reports++;
    drained++;
  }
#endif
}
#endif

void GL_Init (void)
{
#ifdef WOS
  { extern void Sys_WOSTrace (const char *s); Sys_WOSTrace("g0: glstring\n"); }
#endif
  gl_vendor = glGetString (GL_VENDOR);
  Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
  gl_renderer = glGetString (GL_RENDERER);
  Con_Printf ("GL_RENDERER: %s\n", gl_renderer);
  if (strstr(gl_renderer, "(virge)")) gl_polyoffset = -0.0005;
  gl_version = glGetString (GL_VERSION);
  Con_Printf ("GL_VERSION: %s\n", gl_version);
  gl_extensions = glGetString (GL_EXTENSIONS);
  Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);
#ifdef WOS
  { extern void Sys_WOSTrace (const char *s); Sys_WOSTrace("g1: extcheck\n"); }
#endif

  CheckMultiTextureExtensions();
#ifdef WOS
  { extern void Sys_WOSTrace (const char *s); Sys_WOSTrace("g2: mtex\n"); }
#endif

#ifdef MINIGL_DISPATCH_CLIENT
  gl_lightmap_format = GL_LUMINANCE;
#else
  gl_lightmap_format = MGL_UNSIGNED_SHORT_4_4_4_4;
#endif

  glClearColor (1,0,0,0);

  glCullFace(GL_FRONT);
  glEnable(GL_TEXTURE_2D);

  glEnable(GL_ALPHA_TEST);
  glAlphaFunc(GL_GREATER, 0.5); //surgeon - was 0.666

  glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

  glShadeModel (GL_FLAT);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
#ifdef WOS
  { extern void Sys_WOSTrace (const char *s); Sys_WOSTrace("g3: state\n"); }
#endif

#ifdef MINIGL_PERF_DIAGNOSTICS
  /* Temporary Phase 1 diagnostics: reset and enable the R200 counters before
   * any game frame is submitted. */
  glHint(MGL_PERF_COUNTERS_HINT, GL_NICEST);
  GL_CheckErrors("GL_Init texture state");
#endif

}

/*
=================
GL_BeginRendering

=================
*/

void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
  static qboolean first_begin = true;
  if (first_begin)
  {
#ifdef WOS
    { extern void Sys_WOSTrace (const char *s); Sys_WOSTrace("r0: first begin\n"); }
#endif
    first_begin = false;
  }
  *x = *y = 0;
  *width = scr_width;
  *height = scr_height;

  mglLockDisplay();
}


void GL_EndRendering (void)
{
#ifdef MINIGL_DIAGNOSTICS
   static int diagnostic_frames;
   static int diagnostic_limit;
   GLint flushes, textured, untextured, drawrect, tex0, tex0mips;
   GLint tex1, tex1mips, degenerate, bound0;
#endif

#ifdef WOS
   { extern int host_framecount;
     extern void Sys_WOSTraceFrame (const char *tag, int n);
     if (host_framecount < 64) Sys_WOSTraceFrame ("pre-swap", host_framecount); }
#endif
   mglSwitchDisplay(); //performs unlock
#ifdef WOS
   { extern int host_framecount;
     extern void Sys_WOSTraceFrame (const char *tag, int n);
     if (host_framecount < 64) Sys_WOSTraceFrame ("post-swap", host_framecount); }
   { static int first_end = 1; if (first_end) { extern void Sys_WOSTrace (const char *s); Sys_WOSTrace("r1: first end\n"); first_end = 0; } }
#endif

#ifdef MINIGL_DIAGNOSTICS
   /* -diag keeps the per-frame MiniGL error drain alive for the whole run
    * (reporting every 256th frame) instead of only the first 16 frames, so
    * errors raised by mid-game effects stay visible in qconsole.log. */
   if (!diagnostic_limit)
      diagnostic_limit = COM_CheckParm("-diag") ? 0x40000000 : 16;
   if (diagnostic_frames < diagnostic_limit)
   {
      GL_CheckErrors("frame submission");
      diagnostic_frames++;
      if (diagnostic_frames == 1 || diagnostic_frames == 8 ||
          diagnostic_frames == 16 ||
          (diagnostic_frames > 16 && (diagnostic_frames & 255) == 0))
      {
         glGetIntegerv(MGL_PERF_FLUSH_CALLS, &flushes);
         glGetIntegerv(MGL_PERF_FLUSH_TEXTURED0, &textured);
         glGetIntegerv(MGL_PERF_FLUSH_UNTEXTURED0, &untextured);
         glGetIntegerv(MGL_PERF_DROP_DRAW_RECT, &drawrect);
         glGetIntegerv(MGL_PERF_DROP_TEXTURE0, &tex0);
         glGetIntegerv(MGL_PERF_DROP_TEXTURE0_MIPS, &tex0mips);
         glGetIntegerv(MGL_PERF_DROP_TEXTURE1, &tex1);
         glGetIntegerv(MGL_PERF_DROP_TEXTURE1_MIPS, &tex1mips);
         glGetIntegerv(MGL_PERF_DROP_FLAT_DEGENERATE, &degenerate);
         glGetIntegerv(MGL_PERF_LAST_BOUND_TEXTURE0, &bound0);
         Con_Printf("R200DIAG frame=%d flush=%ld tex=%ld plain=%ld rect=%ld "
                    "tex0=%ld mip0=%ld tex1=%ld mip1=%ld flatdeg=%ld bound0=%ld\n",
                    diagnostic_frames, (long)flushes, (long)textured,
                    (long)untextured, (long)drawrect, (long)tex0,
                    (long)tex0mips, (long)tex1, (long)tex1mips,
                    (long)degenerate, (long)bound0);
      }
   }
#endif

   if (UPDATE)
   {
      gl_width.value = width;
      Con_Printf("CHANGED ------------------------------ %4.0f\n",gl_width.value);
   
      scr_width = (int)gl_width.value;
      scr_height = scr_width * 3 / 4;
      vid.conwidth = scr_width;
      vid.conwidth &= 0xfff8; // make it a multiple of eight

      if (vid.conwidth < 320) vid.conwidth = 320;
      vid.conheight = vid.conwidth * 3 / 4;

	vid.width = vid.conwidth;
      vid.height = vid.conheight;
 
      mglResizeContext(vid.width, vid.height);

      VID_ConfigureInputWindow();
      vid.recalc_refdef = 1;
      if (gl_lockmode == MGL_LOCK_MANUAL)
		mglLockDisplay();
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	mglUnlockDisplay();
      
      UPDATE = FALSE;
   }   
}

qboolean VID_Is8bit(void)
{
  return is8bit;
}

/*
void VID_Init8bitPalette() 
{
  // Check for 8bit Extensions and initialize them.
  int i;

  char thePalette[256*3];
  char *oldPalette, *newPalette;

  if(COM_CheckParm("-no8bit"))
	return;

  Con_SafePrintf("8-bit colortable enabled.\n");
  oldPalette = (char *) d_8to24table; //d_8to24table3dfx;
  newPalette = thePalette;
  for (i=0;i<256;i++) {
    *newPalette++ = *oldPalette++;
    *newPalette++ = *oldPalette++;
    *newPalette++ = *oldPalette++;
    oldPalette++;
  }
  glColorTable(GL_COLOR_TABLE, GL_RGB, 256, GL_RGB, GL_UNSIGNED_BYTE, newPalette);
  is8bit = true;
}
*/

void ChangeVideoSync_f (void)
{
  if (Cmd_Argc() == 1)
  {
     Con_Printf ("GL_VIDEOSYNC %d\n", gl_videosync);
     return;
  }

  gl_videosync = Q_atoi(Cmd_Argv(1)); 
  mglEnableSync(gl_videosync);
}

void ChangeDithering_f (void)
{
  if (Cmd_Argc() == 1)
  {
     Con_Printf ("GL_DITHERING IS %d\n", gl_dithering);
     return;
  }

  gl_dithering = Q_atoi(Cmd_Argv(1)); 

#ifndef MINIGL_DISPATCH_CLIENT

  if(gl_dithering)
	glEnable(GL_DITHER);
  else
	glDisable(GL_DITHER);
#endif
}

void ChangeLockMode_f (void)
{
  if (Cmd_Argc() == 1)
  {
     if (gl_lockmode == MGL_LOCK_SMART) Con_Printf ("SMART\n");
     else if (gl_lockmode == MGL_LOCK_AUTOMATIC) Con_Printf ("AUTOMATIC\n");
     else if (gl_lockmode == MGL_LOCK_MANUAL) Con_Printf ("MANUAL\n");
     else Con_Printf ("UNKNOWN\n");
     return;
  }
  
  if (Q_strcasecmp(Cmd_Argv(1),"SMART") == 0) gl_lockmode = MGL_LOCK_SMART;
  if (Q_strcasecmp(Cmd_Argv(1),"AUTOMATIC") == 0) gl_lockmode = MGL_LOCK_AUTOMATIC;
  if (Q_strcasecmp(Cmd_Argv(1),"MANUAL") == 0) gl_lockmode = MGL_LOCK_MANUAL;

  mglLockMode(gl_lockmode);
}

void ChangeVideoMode_f (void)
{
  if (Cmd_Argc() == 1)
  {
     Con_Printf ("GL_WIDTH is %4.0f\n", gl_width.value);
     return;
  }

   width = Q_atof(Cmd_Argv(1)); 
   if (width != gl_width.value) UPDATE = TRUE;
}

void ChangePolyOffset_f (void)
{
  if (Cmd_Argc() == 1)
  {
     Con_Printf ("GL_POLYOFFSET is %f\n", gl_polyoffset);
     return;
  }

   gl_polyoffset = Q_atof(Cmd_Argv(1)); 

   mglSetZOffset(gl_polyoffset);
}

void ChangeWarp_f (void)
{
  if (Cmd_Argc() == 1)
  {
     Con_Printf ("gl_warp is %d\n", gl_warp);
     return;
  }

   gl_warp = Q_atof(Cmd_Argv(1)); 
}


void ShowLightmapFormat_f (void)
{
	  if (COM_CheckParm ("-lm_LUMINANCE"))
	    Con_Printf ("lightmapformat: GL_LUMINANCE\n");
	  else if (COM_CheckParm ("-lm_ALPHA"))
	    Con_Printf ("lightmapformat: GL_ALPHA\n");
	  else if (COM_CheckParm ("-lm_RGB"))
	    Con_Printf ("lightmapformat: MGL_UNSIGNED_SHORT_5_6_5\n");
	  else if (COM_CheckParm ("-lm_RGBA"))
	    Con_Printf ("lightmapformat: GL_RGBA\n");
	  else
	    Con_Printf ("lightmapformat: MGL_UNSIGNED_SHORT_4_4_4_4\n");
}

static qboolean firstinit = true;

void VID_Init(unsigned char *palette)
{
  int i;
  char gldir[MAX_OSPATH];

#ifdef WOS
  if (!(IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37)))
    Sys_Error ("VID_Init: cannot open intuition.library\n");
  if (!(GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 37)))
    Sys_Error ("VID_Init: cannot open graphics.library\n");
#endif
  {
    extern void Sys_WOSTrace (const char *s);
#ifdef WOS
    Sys_WOSTrace("v0: vid_init enter\n");
#endif
  }

	if (firstinit)
	{
		S_Init();
      	CDAudio_Init ();
 
		firstinit = false;
	}


  Cvar_RegisterVariable (&gl_ztrick);

  Cmd_AddCommand ("gl_lightmapformat", &ShowLightmapFormat_f);
  Cmd_AddCommand ("gl_videosync", &ChangeVideoSync_f);
  Cmd_AddCommand ("gl_lockmode", &ChangeLockMode_f);
  Cmd_AddCommand ("gl_dithering", &ChangeDithering_f);
  Cmd_AddCommand ("gl_width", &ChangeVideoMode_f);
  Cmd_AddCommand ("gl_polyoffset", &ChangePolyOffset_f);
  Cmd_AddCommand ("gl_warp", &ChangeWarp_f);
  
  // texturemode

  if ((i = COM_CheckParm("-texturemode")) != 0) {
    if (Q_strcasecmp(com_argv[i+1],"GL_NEAREST") == 0) texture_mode = GL_NEAREST;
    if (Q_strcasecmp(com_argv[i+1],"GL_LINEAR") == 0) texture_mode = GL_LINEAR;
    if (Q_strcasecmp(com_argv[i+1],"GL_NEAREST_MIPMAP_NEAREST") == 0) texture_mode = GL_NEAREST_MIPMAP_NEAREST;
    if (Q_strcasecmp(com_argv[i+1],"GL_NEAREST_MIPMAP_LINEAR") == 0) texture_mode = GL_NEAREST_MIPMAP_LINEAR;
    if (Q_strcasecmp(com_argv[i+1],"GL_LINEAR_MIPMAP_NEAREST") == 0) texture_mode = GL_LINEAR_MIPMAP_NEAREST;
    if (Q_strcasecmp(com_argv[i+1],"GL_LINEAR_MIPMAP_LINEAR") == 0) texture_mode = GL_LINEAR_MIPMAP_LINEAR;

    gl_filter_min = texture_mode;
    gl_filter_max = texture_mode;
  }  
  
  // dither

  if ((i = COM_CheckParm("-dither")) != 0) {
    if (Q_strcasecmp(com_argv[i+1],"ON") == 0) gl_dithering = GL_TRUE;
    if (Q_strcasecmp(com_argv[i+1],"OFF") == 0) gl_dithering = GL_FALSE;
  }  
  
  // lockmode

  if ((i = COM_CheckParm("-lockmode")) != 0) {
    if (Q_strcasecmp(com_argv[i+1],"SMART") == 0) gl_lockmode = MGL_LOCK_SMART;
    if (Q_strcasecmp(com_argv[i+1],"AUTOMATIC") == 0) gl_lockmode = MGL_LOCK_AUTOMATIC;
    if (Q_strcasecmp(com_argv[i+1],"MANUAL") == 0) gl_lockmode = MGL_LOCK_MANUAL;
  }  
  
  // number of buffers
  
  if ((i = COM_CheckParm("-buf")) != 0)
    numb = Q_atoi(com_argv[i+1]);
  
  if (numb < 2)
	numb = 2;
  if (numb > 3)
	numb = 3;   
  
  if (numb == 2)
  {
	gl_videosync = true;
  }
  else
  {
	gl_videosync = false;
  }


  // width

  scr_width = 320; //gl_width.value;
  scr_height = scr_width * 3 / 4;
  
  if ((i = COM_CheckParm("-width")) != 0) {
    scr_width = Q_atoi(com_argv[i+1]);
    scr_height = scr_width * 3 / 4;
  }  

  // height

  if ((i = COM_CheckParm("-height")) != 0) {
    scr_height = Q_atoi(com_argv[i+1]);
  }  

  // bpp

  if ((i = COM_CheckParm("-bpp")) != 0) {
    bpp = Q_atoi(com_argv[i+1]);
    if (bpp < 15) bpp = 15;
    if (bpp > 32) bpp = 32;
  }  

  //guardband clipping
  if (COM_CheckParm("-guardband"))
  {
	guardband = GL_TRUE;
  }

  // set vid parameters

  vid.colormap = host_colormap;
  vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

  vid.conwidth = scr_width;
  vid.conwidth &= 0xfff8; // make it a multiple of eight

  if (vid.conwidth < 320)
    vid.conwidth = 320;

  // pick a conheight that matches with correct aspect
  
  vid.conheight = scr_height;


	vid.width = vid.conwidth;
	vid.height = vid.conheight;
  vid.maxwarpwidth = WARP_WIDTH;
  vid.maxwarpheight = WARP_HEIGHT;
  vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
	vid.numpages = 2 + numb - 2;
	gl_width.value = vid.width;

	Con_Printf("Calling MGLInit...\n");
#ifdef WOS
	{ extern void Sys_WOSTrace (const char *s); Sys_WOSTrace("v1: calling MGLInit\n"); }
#endif
	if (!MGLInit())
    Sys_Error("MiniGLOpen failed\n");
#ifdef WOS
	{ extern void Sys_WOSTrace (const char *s); Sys_WOSTrace("v2: MGLInit ok\n"); }
#endif

	mglChooseGuardBand(guardband);
	
	Con_Printf("Setting Vertex Buffer Size to 3000...\n");
	mglChooseVertexBufferSize(3000); //

	mglChooseMtexBufferSize(6000);

//	mglChooseTMU1Sort(1024, 1024+64); //1024 is the base texnum for lightmaps

	// windowmode
	Con_Printf("Setting pixel depth to %d...\n",bpp);
	mglChoosePixelDepth(bpp);

	if ((i = COM_CheckParm("-windowmode")) != 0)
	{
	Con_Printf("Window mode ON\n");
	mglChooseWindowMode(GL_TRUE);
	}   
	else
	{
	   mglChooseWindowMode(GL_FALSE);
	   Con_Printf("Setting number of buffers to %d...\n", numb);
	   mglChooseNumberOfBuffers(numb);

	   Con_Printf("gl_videosync is %d\n", gl_videosync);

	   if ((i = COM_CheckParm("-closewb")) != 0)
	   {
	   Con_Printf("Closing the Workbench...\n");
	   mglProposeCloseDesktop(GL_TRUE);
	   }   
	}   
  
	Con_Printf("Creating context...\n");
#ifdef WOS
	{ extern void Sys_WOSTrace (const char *s); Sys_WOSTrace("v3: calling CreateContext\n"); }
#endif

	if (mglCreateContext(0,0, vid.width, vid.height))
	{
#ifdef WOS
	{ extern void Sys_WOSTrace (const char *s); Sys_WOSTrace("v4: context ok\n"); }
#endif

	Con_Printf("Switching sync...\n");
	mglEnableSync(gl_videosync);	 

	mglLockMode(gl_lockmode);
#ifdef WOS
	{ extern void Sys_WOSTrace (const char *s); Sys_WOSTrace("v5: lockmode\n"); }
#endif

#ifndef MINIGL_DISPATCH_CLIENT
	glEnable(GL_DITHER);
#endif

	VID_ConfigureInputWindow();
#ifdef WOS
	{ extern void Sys_WOSTrace (const char *s); Sys_WOSTrace("v6: inputwin\n"); }
#endif
	GL_Init();
#ifdef WOS
	{ extern void Sys_WOSTrace (const char *s); Sys_WOSTrace("v7: gl_init\n"); }
#endif

	glHint(MGL_W_ONE_HINT, GL_FASTEST);
#ifdef WOS
	{ extern void Sys_WOSTrace (const char *s); Sys_WOSTrace("v8: hint\n"); }
#endif

//global setting for legacy MiniGL since there is no quality loss
#ifndef MINIGL_DISPATCH_CLIENT
	glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
#endif
     
	mglSetZOffset(gl_polyoffset);
#ifdef WOS
	{ extern void Sys_WOSTrace (const char *s); Sys_WOSTrace("v9: zoffset\n"); }
#endif
	GL_CheckErrors("video setup");

	sprintf (gldir, "%s/glquake", com_gamedir);
	Sys_mkdir (gldir);
#ifdef WOS
	{ extern void Sys_WOSTrace (const char *s); Sys_WOSTrace("vA: mkdir\n"); }
#endif

	Check_Gamma(palette);
	VID_SetPalette(palette);
#ifdef WOS
	{ extern void Sys_WOSTrace (const char *s); Sys_WOSTrace("vB: palette\n"); }
#endif

	Con_SafePrintf ("Video mode %dx%d initialized.\n", scr_width, scr_height);

	vid.recalc_refdef = 1;        // force a surface cache flush
	}	
	else Sys_Error("no context, exiting...\n");
}

void Force_CenterView_f (void)
{
  cl.viewangles[PITCH] = 0;
}


void Sys_SendKeyEvents (void)
{
  struct IntuiMessage *imsg;
  ULONG msgclass;
  UWORD msgcode;
  int kn;
#ifdef NOMOUSEASM
  WORD mousex, mousey;
#endif
  qboolean close_requested = false;
  
  if (QuakeWindow)
  {
    while (imsg = (struct IntuiMessage *)GetMsg(QuakeWindow->UserPort))
    {
      msgclass = imsg->Class;
      msgcode = imsg->Code;
#ifdef NOMOUSEASM
      mousex = imsg->MouseX;
      mousey = imsg->MouseY;
#endif
      ReplyMsg((struct Message *)imsg);

      if (msgclass == IDCMP_CLOSEWINDOW)
      {
        close_requested = true;
        continue;
      }

      if (close_requested)
        continue;

#ifdef NOMOUSEASM
	if(msgclass==IDCMP_MOUSEMOVE)
	{
		m_move[0] = (int)mousex;
		m_move[1] = (int)mousey;
	}
      else
#endif
	if (msgclass & (IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS))
      {
        kn = (int)rawkeyconv[msgcode & 0x7f];

        if (msgcode & IECODE_UP_PREFIX)
        {
          Key_Event(kn,false);
        }
        else
        {
          Key_Event(kn,true);
        }

        if (shutdown_keyboard)
        {
          shutdown_keyboard=0;
          break;
        }
      }

    }

    if (close_requested)
      Sys_Quit();
  }
}

/*
================
VID_MenuDraw
================
*/

#define MAX_COLUMN_SIZE 11
#define MAX_MODEDESCS   33

typedef struct
{
  int     modenum;
  char    *desc;
  int     iscur;
} modedesc_t;

static modedesc_t   modedescs[33];
int numvidmodes=11;
static int vid_wmodes,vid_line;
int vid_column_size;

void VID_MenuDraw (void)
{
  qpic_t      *p;
  char        *ptr;
  int         nummodes, i, j, column, row, dup;
  char        temp[100];

  p = Draw_CachePic ("gfx/vidmodes.lmp");
  M_DrawPic ( (320-p->width)/2, 4, p);

  M_Print (9*8, 36 + MAX_COLUMN_SIZE * 8 + 8*3, "Press Enter to set mode");
  //ptr = VID_GetModeDescription (vid_modenum);
  sprintf (temp, "D to make %s the default", ptr);
  M_Print (6*8, 36 + MAX_COLUMN_SIZE * 8 + 8*5, temp);
  //ptr = VID_GetModeDescription ((int)_vid_default_mode.value);
/*
  if (ptr)
  {
    sprintf (temp, "Current default is %s", ptr);
    M_Print (7*8, 36 + MAX_COLUMN_SIZE * 8 + 8*6, temp);
  }
*/
  M_Print (15*8, 36 + MAX_COLUMN_SIZE * 8 + 8*8, "Esc to exit");

  row = 36 + (vid_line % vid_column_size) * 8;
  column = 8 + (vid_line / vid_column_size) * 13*8;

  M_DrawCharacter (column, row, 12+((int)(realtime*4)&1));
}

/*
================
VID_MenuKey
================
*/
void VID_MenuKey (int key)
{
  switch (key)
  {
    case K_ESCAPE:
      S_LocalSound ("misc/menu1.wav");
      M_Menu_Options_f ();
      break;
  
    case K_UPARROW:
      S_LocalSound ("misc/menu1.wav");
      vid_line--;
  
      if (vid_line < 0) vid_line = vid_wmodes - 1;
      break;
  
    case K_DOWNARROW:
      S_LocalSound ("misc/menu1.wav");
      vid_line++;

      if (vid_line >= vid_wmodes)
      vid_line = 0;
      break;
  
    case K_LEFTARROW:
      S_LocalSound ("misc/menu1.wav");
      vid_line -= vid_column_size;
  
      if (vid_line < 0)
      {
        vid_line += ((vid_wmodes + (vid_column_size - 1)) /
                     vid_column_size) * vid_column_size;
  
        while (vid_line >= vid_wmodes)
               vid_line -= vid_column_size;
      }
      break;
  
    case K_RIGHTARROW:
      S_LocalSound ("misc/menu1.wav");
      vid_line += vid_column_size;
  
      if (vid_line >= vid_wmodes)
      {
        vid_line -= ((vid_wmodes + (vid_column_size - 1)) /
                     vid_column_size) * vid_column_size;
  
        while (vid_line < 0)
          vid_line += vid_column_size;
      }
      break;
  
    case K_ENTER:
      S_LocalSound ("misc/menu1.wav");
      Cvar_SetValue("gl_width",modedescs[vid_line].modenum);
      //vid_modenum=modedescs[vid_line].modenum;
      //config_notify=1;
      break;
/*
    case 'D':
    case 'd':
      S_LocalSound ("misc/menu1.wav");
      Cvar_SetValue ("_vid_default_mode", modedescs[vid_line].modenum);
      vid_modenum=modedescs[vid_line].modenum;
      break;
*/
    default:
      break;
  }
}
