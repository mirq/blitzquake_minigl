/* in_amigamouse.c -- AMIGA mouse driver */
/* Written by Frank Wille <frank@phoenix.owl.de> */

#pragma amiga-align
#include <exec/memory.h>
#include <exec/interrupts.h>
#include <devices/input.h>
#include <intuition/intuitionbase.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#pragma default-align
#include "quakedef.h"

#ifdef NOMOUSEASMASM
//#include <devices/inputevent.h>
#endif

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

#if 0
void InputIntCode(void)
{
	WORD axis;
	ULONG xy;
	UWORD code;
	int speed;
	struct InputEvent *event;

	/****************************
	move.l  a0,-(sp)
	move.l	iid_MouseSpeed(a1),d1
	****************************/

	event = &InputHandler;
	speed = InputIntData->MouseSpeed;

loop:
	if(event->ie_Class != IECLASS_RAWMOUSE)
	{
		goto next;
	}


	/**************************
	move.w  ie_Code(a0),d0
	btst    #7,d0
	bne.b   .up
	**************************/

	code = event->ie_Code;

	if(code != 7)
	{
		goto up;
	}

	if(code == IECODE_LBUTTON)
	{
		goto lmb_down;
	}
	else if(code == IECODE_MBUTTON)
	{
		goto mmb_down;
	}
	else if(code == IECODE_RBUTTON)
	{
		goto rmb_down;
	}
	else
	{
		goto move;
	}

up:
	code &= 0x007F;

	if(code == IECODE_LBUTTON)
	{
		goto lmb_up;
	}
	else if(code == IECODE_MBUTTON)
	{
		goto mmb_up;
	}
	else if(code == IECODE_RBUTTON)
	{
		goto rmb_up;
	}
	else
	{
		goto move;
	}

lmb_down:
	InputIntData->LeftButtonDown = -1;
	InputIntData->LeftButtonUp   =  0;
	goto move;

mmb_down:
	InputIntData->MidButtonDown = -1;
	InputIntData->MidButtonUp   =  0;
	goto move;

rmb_down:
	InputIntData->RightButtonDown = -1;
	InputIntData->RightButtonUp   =  0;
	goto move;

lmb_up:
	InputIntData->LeftButtonUp   = -1;
	InputIntData->LeftButtonDown =  0;
	goto move;

mmb_up:
	InputIntData->MidButtonUp   = -1;
	InputIntData->MidButtonDown =  0;
	goto move;

rmb_up:
	InputIntData->RightButtonUp   = -1;
	InputIntData->RightButtonDown =  0;

move:
	axis = event->ie_position.ie_xy.ie_x;
	axis *= speed;
	InputIntData->MouseX = axis;

	axis = event->ie_position.ie_xy.ie_y;
	axis *= speed;
	InputIntData->MouseY = axis;

next:
	event = event->ie_NextEvent;

	if(event != NULL)
	{
		goto loop;
	}

	return;
}
#endif

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

  if ( COM_CheckParm ("-nomouse") )
    return;

  /* open input.device */
  if (inputport = CreateMsgPort()) {
    if (input = (struct IOStdReq *)CreateIORequest(inputport,sizeof(struct IOStdReq))) {
      if (InputIntData = Sys_Alloc(sizeof(struct InputIntDat),
                                   MEMF_CHIP|MEMF_PUBLIC|MEMF_CLEAR)) {
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
  Sys_Free(InputIntData);
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
  if ( COM_CheckParm ("-nomouse") )
    return;

#ifdef NOMOUSEASM
  mouse_avail = true;
#else
  /* open input.device */
  if (inputport = CreatePort(NULL,0)) {
    if (input = CreateStdIO(inputport)) {
      if (InputIntData = Sys_Alloc(sizeof(struct InputIntDat),
                                   MEMF_CHIP|MEMF_PUBLIC|MEMF_CLEAR)) {
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
  Sys_Free(InputIntData);
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
	if (mouse_avail && psxused)
	{
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
	float mx, my;	//surgeon: was int
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
	{
		cmd->sidemove += m_side.value * mouse_x;
	}
	else
	{
		cl.viewangles[YAW] -= m_yaw.value * mouse_x;
	}

	if (in_mlook.state & 1)
		V_StopPitchDrift ();
		
	if ( (in_mlook.state & 1) && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch.value * mouse_y;
		if (cl.viewangles[PITCH] > 80)
			cl.viewangles[PITCH] = 80;
		if (cl.viewangles[PITCH] < -70)
			cl.viewangles[PITCH] = -70;
	}
	else
	{
		if ((in_strafe.state & 1) && noclip_anglehack)
			cmd->upmove -= m_forward.value * mouse_y;
		else
			cmd->forwardmove -= m_forward.value * mouse_y;
	}

#ifndef NOMOUSEASM
  InputIntData->MouseSpeed = (int)m_speed.value;
#endif
}
