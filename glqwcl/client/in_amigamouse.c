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

/* in_amigamouse.c -- AMIGA mouse driver */
/* Written by Frank Wille <frank@phoenix.owl.de> */

#include "quakedef.h"

#pragma amiga-align
#include <exec/memory.h>
#include <exec/interrupts.h>
#include <devices/input.h>
#include <intuition/intuitionbase.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#ifdef __PPC__
#ifdef WOS
  #ifndef __GNUC__
  #include <clib/powerpc_protos.h>
  #else
  #include <powerpc/powerpc_protos.h>
  #endif
#else
#include <powerup/gcclib/powerup_protos.h>
#endif
#endif
#pragma default-align

extern struct IntuitionBase *IntuitionBase;

#ifndef GLQUAKE
extern struct Screen *QuakeScreen;
#else
extern struct Window *QuakeWindow;
#endif


cvar_t  m_filter = {"m_filter","1"};
cvar_t  m_speed = {"m_speed","16"};

qboolean mouse_avail = false;
#ifndef GLQUAKE
float mouse_x, mouse_y;
#else
extern float mouse_x, mouse_y;
#endif

float old_mouse_x, old_mouse_y;

extern int psxused;

/* AMIGA specific */
static struct MsgPort *inputport=NULL;
static struct IOStdReq *input=NULL;
static byte input_dev = -1;
static struct Interrupt InputHandler;


struct InputIntDat
{
	int LeftButtonDown;
	int MidButtonDown;
	int RightButtonDown;
	int LeftButtonUp;
	int MidButtonUp;
	int RightButtonUp;
	int MouseX;
	int MouseY;
  int MouseSpeed;
};

static struct InputIntDat *InputIntData;

#ifdef NOMOUSEASM
extern int *mousemove;
#else
/* this is a 68k routine! */
extern void InputIntCode(void);
#endif

#ifdef __STORM__

/*
===========
IN_InitMouse
===========
*/

void IN_InitMouse (void)
{
	Cvar_RegisterVariable (&m_filter);
	Cvar_RegisterVariable (&m_speed);

  if ( COM_CheckParm ("-nomouse") )
    return;

  /* open input.device */
  if (inputport = CreateMsgPort()) {
    if (input = (struct IOStdReq *)CreateIORequest(inputport,sizeof(struct IOStdReq))) {
#ifdef __PPC__
#ifdef WOS
      if (InputIntData = AllocVecPPC(sizeof(struct InputIntDat),
          MEMF_CHIP|MEMF_PUBLIC|MEMF_CLEAR,0)) {
#else
      if (InputIntData = PPCAllocVec(sizeof(struct InputIntDat),
          MEMF_CHIP|MEMF_PUBLIC|MEMF_CLEAR)) {
#endif
#else
      if (InputIntData = AllocVec(sizeof(struct InputIntDat),
          MEMF_CHIP|MEMF_PUBLIC|MEMF_CLEAR)) {
#endif
        InputIntData->MouseSpeed = (int)m_speed.value;
        if (!(input_dev = OpenDevice("input.device",0,
            (struct IORequest *)input,0))) {
          InputHandler.is_Node.ln_Type = NT_INTERRUPT;
          InputHandler.is_Node.ln_Pri = 100;
          InputHandler.is_Data = InputIntData;
          InputHandler.is_Code = (void(*)())InputIntCode;
          input->io_Data = (void *)&InputHandler;
          input->io_Command = IND_ADDHANDLER;
          DoIO((struct IORequest *)input);
          mouse_avail = true;
        }
      }
    }
  }
  if (!mouse_avail) {
    Con_Printf("Couldn't open input.device\n");
    return;
  }
  Con_Printf("Amiga mouse available\n");
}


/*
===========
IN_ShutdownMouse
===========
*/
void IN_ShutdownMouse (void)
{
  if (mouse_avail) {
    input->io_Data=(void *)&InputHandler;
    input->io_Command=IND_REMHANDLER;
    DoIO((struct IORequest *)input);
  }
  if (!input_dev)
    CloseDevice((struct IORequest *)input);

#ifdef __PPC__
#ifdef WOS
  if (InputIntData)
    FreeVecPPC(InputIntData);
#else
  if (InputIntData)
    PPCFreeVec(InputIntData);
#endif
#else
  if (InputIntData)
    FreeVec(InputIntData);
#endif

  if (input)
    DeleteIORequest((struct IORequest *)input);
  if (inputport)
    DeleteMsgPort(inputport);
}
#else

/*
===========
IN_InitMouse
===========
*/


void IN_InitMouse (void)
{
	Cvar_RegisterVariable (&m_filter);
	Cvar_RegisterVariable (&m_speed);

  if ( COM_CheckParm ("-nomouse") )
    return;

#ifdef NOMOUSEASM
  mouse_avail = true;
#else

  /* open input.device */
  if (inputport = CreatePort(NULL,0)) {
    if (input = CreateStdIO(inputport)) {
#ifdef __PPC__
#ifdef WOS
      if (InputIntData = AllocVecPPC(sizeof(struct InputIntDat),
          MEMF_CHIP|MEMF_PUBLIC|MEMF_CLEAR,0)) {
#else
      if (InputIntData = PPCAllocVec(sizeof(struct InputIntDat),
          MEMF_CHIP|MEMF_PUBLIC|MEMF_CLEAR)) {
#endif
#else
      if (InputIntData = AllocVec(sizeof(struct InputIntDat),
          MEMF_CHIP|MEMF_PUBLIC|MEMF_CLEAR)) {
#endif
        InputIntData->MouseSpeed = (int)m_speed.value;
        if (!(input_dev = OpenDevice("input.device",0,
            (struct IORequest *)input,0))) {
          InputHandler.is_Node.ln_Type = NT_INTERRUPT;
          InputHandler.is_Node.ln_Pri = 100;
          InputHandler.is_Data = InputIntData;
          InputHandler.is_Code = (void(*)())InputIntCode;
          input->io_Data = (void *)&InputHandler;
          input->io_Command = IND_ADDHANDLER;
          DoIO((struct IORequest *)input);
          mouse_avail = true;
        }
      }
    }
  }
  if (!mouse_avail) {
    Con_Printf("Couldn't open input.device\n");
    return;
  }
  Con_Printf("Amiga mouse available\n");
#endif
}


/*
===========
IN_ShutdownMouse
===========
*/
void IN_ShutdownMouse (void)
{
#ifndef NOMOUSEASM
  if (mouse_avail) {
    input->io_Data=(void *)&InputHandler;
    input->io_Command=IND_REMHANDLER;
    DoIO((struct IORequest *)input);
  }
  if (!input_dev)
    CloseDevice((struct IORequest *)input);

#ifdef __PPC__
#ifdef WOS
  if (InputIntData)
    FreeVecPPC(InputIntData);
#else
  if (InputIntData)
    PPCFreeVec(InputIntData);
#endif
#else
  if (InputIntData)
    FreeVec(InputIntData);
#endif

  if (input)
    DeleteStdIO(input);
  if (inputport)
    DeletePort(inputport);

#endif
}
#endif

/*
===========
IN_MouseCommands
===========
*/
void IN_MouseCommands (void)
{
#ifndef NOMOUSEASM
	if (mouse_avail && psxused) {
    if (InputIntData->LeftButtonDown)
    	Key_Event (K_MOUSE1, TRUE);
    if (InputIntData->LeftButtonUp)
    	Key_Event (K_MOUSE1, FALSE);
    if (InputIntData->MidButtonDown)
    	Key_Event (K_MOUSE2, TRUE);
    if (InputIntData->MidButtonUp)
    	Key_Event (K_MOUSE2, FALSE);
    if (InputIntData->RightButtonDown)
    	Key_Event (K_MOUSE3, TRUE);
    if (InputIntData->RightButtonUp)
    	Key_Event (K_MOUSE3, FALSE);
	}
#endif
}


/*
===========
IN_MouseMove
===========
*/
void IN_MouseMove (usercmd_t *cmd)
{
	int mx, my;

	if (!mouse_avail)
		return;
#ifndef GLQUAKE
        if (IntuitionBase->FirstScreen != QuakeScreen)
          return;
#else
        if (IntuitionBase->ActiveWindow != QuakeWindow)
          return;
#endif


#ifdef NOMOUSEASM
	mx = (float)mousemove[0] * m_speed.value;
	my = (float)mousemove[1] * m_speed.value;

	mousemove[0] = 0;
	mousemove[1] = 0;
#else
	mx = (float)InputIntData->MouseX;
	my = (float)InputIntData->MouseY;
	InputIntData->MouseX = 0;
	InputIntData->MouseY = 0;
#endif

	if (m_filter.value) {
		mouse_x = (mx + old_mouse_x) * 0.5;
		mouse_y = (my + old_mouse_y) * 0.5;
	}
	else {
		mouse_x = mx;
		mouse_y = my;
	}
	old_mouse_x = mx;
	old_mouse_y = my;

	mouse_x *= sensitivity.value;
	mouse_y *= sensitivity.value;

  /* add mouse X/Y movement to cmd */
	if ( (in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1) ))
		cmd->sidemove += m_side.value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw.value * mouse_x;
	
	if (in_mlook.state & 1)
		V_StopPitchDrift ();
		
	if ( (in_mlook.state & 1) && !(in_strafe.state & 1)) {
		cl.viewangles[PITCH] += m_pitch.value * mouse_y;
		if (cl.viewangles[PITCH] > 80)
			cl.viewangles[PITCH] = 80;
		if (cl.viewangles[PITCH] < -70)
			cl.viewangles[PITCH] = -70;
	}
	else {
		if ((in_strafe.state & 1) && noclip_anglehack)
			cmd->upmove -= m_forward.value * mouse_y;
		else
			cmd->forwardmove -= m_forward.value * mouse_y;
	}

#ifndef NOMOUSEASM
  InputIntData->MouseSpeed = (int)m_speed.value;
#endif
}
