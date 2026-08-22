#include "quakedef.h" 

#pragma amiga-align
//#include "twfsound_cd.h"
#include "twfsound_CD.h"
#include <devices/timer.h>
#include <proto/timer.h>
#ifdef __PPC__
#ifndef __GNUC__
#include <clib/powerpc_protos.h>
#else
#include <powerpc/powerpc_protos.h>
#endif
#endif
#pragma default-align

extern int FirstTime;  /* from sys_amiga.c */

static qboolean cdValid = false;
static qboolean cdPlaying = false;
static qboolean cdWasPlaying = false;
static qboolean cdInitialised = false;
static qboolean cdEnabled = false;
static byte     cdPlayTrack;
static byte     cdMaxTrack;
static int      cdEmulatedStart;
static int      cdEmulatedLength;

// CDAudio console interface -->
extern cvar_t bgmvolume;
static byte   remap[100];
static qboolean repeattrack = false;
// CDAudio console interface <--

struct TWFCDData *twfdata=0;


static int Milliseconds(void)
{
  struct timeval tv;
  int ms;

#ifdef __PPC__
  GetSysTimePPC(&tv);
#else
  GetSysTime(&tv);
#endif
  return (tv.tv_secs-FirstTime)*1000 + tv.tv_micro/1000;
}


int CDAudio_GetAudioDiskInfo()
{

  int err;
  cdValid = false;

  err=TWFCD_ReadTOC(twfdata);
  if(err==TWFCD_FAIL)
  {
    Con_Printf("CD Audio: Drive not ready\n");
    return(0);
  }

  cdMaxTrack=(twfdata->TWFCD_Table).cda_LastTrack;
  if ((twfdata->TWFCD_Table.cda_FirstAudio)==TWFCD_NOAUDIO)
  {
    Con_Printf("CD Audio: No music tracks\n");
    return(0);
  }
  cdValid = true;
  return(1);
}

void CDAudio_Play2(int realtrack, qboolean looping)
{
  int err;
  struct TagItem tags[]=
  {
    TWFCD_Track_Start,0,
    TWFCD_Track_End,0,
    TWFCD_Track_Count,0,
    TWFCD_Track_PlayMode,SCSI_CMD_PLAYAUDIO12,
    0,0
  };

  if (!cdEnabled) return;

  if ((realtrack < 1) || (realtrack > cdMaxTrack))
  {
    CDAudio_Stop();
    return;
  }

  if (!cdValid)
  {
    CDAudio_GetAudioDiskInfo();
    if (!cdValid) return;
  }

  if (!((twfdata->TWFCD_Table.cda_TrackData[realtrack]).cdt_Audio))
  {
    Con_Printf("CD Audio: Track %i is not audio\n", realtrack);
    return;
  }

  if(cdPlaying)
  {
    if(cdPlayTrack == realtrack) return;
    CDAudio_Stop();
  }

  tags[0].ti_Data=realtrack;
  tags[1].ti_Data=realtrack;
  tags[2].ti_Data=1;
  tags[3].ti_Data=SCSI_CMD_PLAYAUDIO12;
  err=TWFCD_PlayTracks(twfdata,tags);
  if (err!=TWFCD_OK)
  {
    tags[3].ti_Data=SCSI_CMD_PLAYAUDIO_TRACKINDEX;
    err=TWFCD_PlayTracks(twfdata,tags);
    if (err!=TWFCD_OK)
    {
      tags[3].ti_Data=SCSI_CMD_PLAYAUDIO10;
      err=TWFCD_PlayTracks(twfdata,tags);
    }
  }

  if (err!=TWFCD_OK)
  {
    Con_Printf("CD Audio: CD PLAY failed\n");
    return;
  }
  cdEmulatedLength = (((twfdata->TWFCD_Table.cda_TrackData[realtrack]).cdt_Length)/75)*1000;
  cdEmulatedStart = Milliseconds();

  cdPlayTrack = realtrack;
  cdPlaying = true;
}


void CDAudio_Play(byte track, qboolean looping) 
{ 
  CDAudio_Play2(track, looping);
} 
 
void CDAudio_Stop(void) 
{ 
  if (!cdEnabled) return;
  if (!cdPlaying) return;

  TWFCD_MotorControl(twfdata,TWFCD_MOTOR_STOP);
  cdWasPlaying = false;
  cdPlaying = false;
} 



void CDAudio_Pause(void) 
{ 
  if(!cdEnabled) return;
  if(!cdPlaying) return;

  TWFCD_PausePlay(twfdata);
  cdWasPlaying = cdPlaying;
  cdPlaying = false;
} 
 
 
void CDAudio_Resume(void) 
{ 
  int err;
  if (!cdEnabled) return;
  if (!cdWasPlaying) return;
  if (!cdValid) return;

  err=TWFCD_PausePlay(twfdata);

  cdPlaying = true;
  if (err==TWFCD_FAIL) cdPlaying = false;
} 

 
void CDAudio_LoopTrack()
{
  cdPlaying=false;

//modified for CDAudio console interface -->

  if(repeattrack)
  {
  CDAudio_Play2(cdPlayTrack, true);
  return;
  }

  if (cdPlayTrack < cdMaxTrack) 
  cdPlayTrack+=1;
  else
  cdPlayTrack = 1;

  CDAudio_Play2(cdPlayTrack, false);

//modified for CDAudio console interface <--
}

void CDAudio_Update(void) //called from from cl_main.c
{ 
  if(!cdPlaying) return;

  if(Milliseconds() > (cdEmulatedStart + cdEmulatedLength))
  {
    CDAudio_LoopTrack();
  }
} 
 
 
int CDAudio_HardwareInit(void)
{
  char devname[255];
  char unitname[255];
  int unit;

  cdInitialised=true;

  if (0 >= GetVar("env:Quake1/cd_device", devname,255,0))
  {
      cdInitialised=false;
      return 0;
  }

  if (0 < GetVar("env:Quake1/cd_unit", unitname,255,0))
  {
      unit=atoi(unitname);
  }
  else
  {
      cdInitialised=false;
      return 0;
  }

  if (cdInitialised)
  {
    char test[1024];

    sprintf(test,"%s %i ",devname,unit);
    Con_Printf(test);

    if(twfdata=TWFCD_Setup(devname,unit))
      return(CDAudio_GetAudioDiskInfo());

    cdInitialised=false;
    Con_Printf("CD AUDIO: Init failed\n");
  }

  return 0;
}

//modified cd_linux.c interface -->

static void CDAudio_Eject(void)
{
  if (!cdEnabled)
    return; // no cd init'd

  TWFCD_MotorControl(twfdata,TWFCD_MOTOR_EJECT);
}


static void CDAudio_CloseDoor(void)
{
  if (!cdEnabled)
    return; // no cd init'd

  TWFCD_MotorControl(twfdata,TWFCD_MOTOR_LOAD);
}

static void CD_f (void)
{
  char  *command;
  int   ret;
  int   n;

  if (Cmd_Argc() < 2)
    return;

  command = Cmd_Argv (1);

  if (Q_strcasecmp(command, "on") == 0)
  {
    cdEnabled = true;
    return;
  }

  if (Q_strcasecmp(command, "off") == 0)
  {
    if (cdPlaying)
      CDAudio_Stop();
    cdEnabled = false;
    return;
  }

  if (Q_strcasecmp(command, "reset") == 0)
  {
    cdEnabled = true;
    if (cdPlaying)
      CDAudio_Stop();
    for (n = 0; n < 100; n++)
      remap[n] = n;
    CDAudio_GetAudioDiskInfo();
    return;
  }

  if (Q_strcasecmp(command, "remap") == 0)
  {
    ret = Cmd_Argc() - 2;
    if (ret <= 0)
    {
      for (n = 1; n < 100; n++)
        if (remap[n] != n)
          Con_Printf("  %u -> %u\n", n, remap[n]);
      return;
    }
    for (n = 1; n <= ret; n++)
      remap[n] = Q_atoi(Cmd_Argv (n+1));
    return;
  }


  if (Q_strcasecmp(command, "close") == 0)
  {
    CDAudio_CloseDoor();
    return;
  }


  if (!cdValid)
  {
    CDAudio_GetAudioDiskInfo();
    if (!cdValid)
    {
      Con_Printf("No CD in player.\n");
      return;
    }
  }

  if (Q_strcasecmp(command, "play") == 0)
  {
   repeattrack = false;
   CDAudio_Play((byte)Q_atoi(Cmd_Argv (2)), repeattrack);
    return;
  }

  if (Q_strcasecmp(command, "loop") == 0)
  {
    repeattrack = true;
    CDAudio_Play((byte)Q_atoi(Cmd_Argv (2)), repeattrack);
    return;
  }

  if (Q_strcasecmp(command, "stop") == 0)
  {
    CDAudio_Stop();
    return;
  }

  if (Q_strcasecmp(command, "pause") == 0)
  {
    CDAudio_Pause();
    return;
  }

  if (Q_strcasecmp(command, "resume") == 0)
  {
    CDAudio_Resume();
    return;
  }
//surgeon start - bgmvolume has no effect yet
  if (Q_strcasecmp(command, "volume") == 0)
  {
   static float cdvolume;
   cdvolume = atof(Cmd_Argv (2));
   if (cdvolume < 0)
   cdvolume = 0.0;
   if (cdvolume > 1)
   cdvolume = 1.0;

   Cvar_SetValue("bgmvolume", cdvolume);  
   return;
  }
//surgeon end

  if (Q_strcasecmp(command, "eject") == 0)
  {
    if (cdPlaying)
      CDAudio_Stop();
    CDAudio_Eject();
    cdValid = false;
    return;
  }


  if (Q_strcasecmp(command, "info") == 0)
  {
    Con_Printf("%u tracks\n", cdMaxTrack);
    if (cdPlaying)
      Con_Printf("Currently %s track %u\n", repeattrack ? "looping" : "playing", cdPlayTrack);
    else if (cdWasPlaying)
      Con_Printf("Paused %s track %u\n", repeattrack ? "looping" : "playing", cdPlayTrack);
    Con_Printf("Volume is %f\n", /*cdvolume*/bgmvolume.value);
    return;
  }
}

//modified cd_linux.c interface <--

// CDaudio console interface by surgeon -->

void QCD_Info_f (void)
{

if (cdPlaying)
	Con_Printf ("playing track %d of %d %s\n", cdPlayTrack, cdMaxTrack, repeattrack ? "[repeat]" : "");

else if (cdValid)
	if (!cdWasPlaying)
  	Con_Printf ("%d tracks available\n", cdMaxTrack);
  	else 
  	Con_Printf ("paused at track %d of %d\n", cdPlayTrack, cdMaxTrack);

else
Con_Printf ("no valid audiotracks available\n");
}

void QCD_Skip_f (void)
{

if (!cdPlaying)
	{
	Con_Printf ("not playing\n");
	return;
	}
else
  	Con_Printf ("skipping track %d\n", cdPlayTrack);

cdPlaying = false;
if (cdPlayTrack < cdMaxTrack)
	CDAudio_Play2((cdPlayTrack +1), repeattrack);
else
	CDAudio_Play2(1, repeattrack);
}

void QCD_Play_f (void)
{
int nowplaying;
byte playtrack;
static char *looping = "";

if (Cmd_Argc() == 1)
{
	Con_Printf ("USAGE: cdplay <track> <r =repeat>\n");
	return;
}

if(cdPlaying)
cdPlaying = false;

playtrack = atoi(Cmd_Argv (1));
nowplaying = atoi(Cmd_Argv (1));

	if (Q_strcasecmp(Cmd_Argv(2),"r") != 0)
		repeattrack = false;
	else
		{
		repeattrack = true;
		looping = " [repeat]";
		}

if(cdEnabled && cdValid && ((twfdata->TWFCD_Table.cda_TrackData[nowplaying]).cdt_Audio))
	Con_Printf("Playing Track %i%s\n", playtrack, looping);

	CDAudio_Play(playtrack, repeattrack);
} 

void QCD_Pause_f (void)
{
	if(!cdEnabled) return;
	if(!cdPlaying) return;

	Con_Printf("playback paused\n");
	CDAudio_Pause();
}

void QCD_Resume_f (void)
{
	if (!cdEnabled) return;
	if (!cdWasPlaying) return;
	if (!cdValid) return;

	Con_Printf("playback resumed\n");
	CDAudio_Resume();
}

void QCD_Stop_f (void)
{
	if (!cdEnabled) return;
	if (!cdPlaying) return;

	CDAudio_Stop();
	Con_Printf("playback stopped\n");
}

// CDAudio console interface <--


int CDAudio_Init(void) 
{ 
static int firstinit = 1; //CDAudio console interface

  cdEnabled = false;
  cdInitialised = false;
  if (COM_CheckParm("-nocdaudio")) return -1;

  cdEnabled = true;

  if(!CDAudio_HardwareInit())
  {
    cdEnabled=false;
  }

  Con_Printf("CD Audio Initialized\n");

// CDAudio console interface -->
  if(firstinit) //only add cmds if cdaudio is enabled
  {
  Cmd_AddCommand("cd", CD_f); //modified linux interface
  Cmd_AddCommand("cdplay", QCD_Play_f);
  Cmd_AddCommand("cdskip", QCD_Skip_f);
  Cmd_AddCommand("cdpause", QCD_Pause_f);
  Cmd_AddCommand("cdresume", QCD_Resume_f);
  Cmd_AddCommand("cdstop", QCD_Stop_f);
  Cmd_AddCommand("cdinfo", QCD_Info_f);
  firstinit = 0;
  }
// CDAudio console interface <--
  return(0);
} 
 
 
void CDAudio_Shutdown(void) 
{ 
  if(!cdInitialised)
    return;
  CDAudio_Stop();
  TWFCD_Shutdown(twfdata);
}
