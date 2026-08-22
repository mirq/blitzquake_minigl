// Main Module for Input Code for Amiga Version, can be easily expanded
// to support other Init-Devices

/*
            Portability
            ===========

            This file should compile fine under all Amiga-Based Kernels,
            including WarpUP, 68k and PowerUP.

            Steffen Haeuser (MagicSN@Birdland.es.bawue.de)
*/

#include "quakedef.h"
#include "in_amiga.h"

int psxused;
int mouseused;
int joyused;

static void Force_CenterView_f(void)
{
  cl.viewangles[PITCH] = 0;
}


void IN_Init (void)
{
  FILE *fil=fopen("devs:psxport.device","r");

  Cmd_AddCommand ("force_centerview", Force_CenterView_f);

  psxused=0;
  mouseused=0;
  joyused=0;

 if(!COM_CheckParm("-nomouse"))
 {
    mouseused=1;
    IN_InitMouse();
 }

 if(!COM_CheckParm("-nopsx"))
 {
  if (fil) {
    fclose(fil);
    psxused=1;
    IN_InitPsx();
  }  
 }
 if(!COM_CheckParm("-nojoy"))
 {
    joyused=1;
    IN_InitJoy();
 }
}


void IN_Shutdown (void)
{
  if (psxused)	IN_ShutdownPsx();
  if(joyused)		IN_ShutdownJoystick();
  if(mouseused)	IN_ShutdownMouse();
}

void IN_Commands (void)
{
  if (mouseused)	IN_MouseCommands();
}

void IN_Move (usercmd_t *cmd)
{
  if (mouseused)	IN_MouseMove(cmd);
  if (psxused)	IN_PsxMove(cmd);
  if (joyused)	IN_JoyMove(cmd);
}
