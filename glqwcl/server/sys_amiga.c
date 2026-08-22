/*
** sys_amiga.c
**
** QuakeWorld Server for Amiga M68k and Amiga PowerPC
**
*/

#pragma amiga-align
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include <graphics/gfxbase.h>
#include <devices/timer.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#ifdef __PPC__
#ifdef WOS
#include <clib/powerpc_protos.h>
#else
#include <powerup/ppclib/time.h>
#include <powerup/gcclib/powerup_protos.h>
#endif
#endif
#pragma default-align

/* Quake includes */
#include "qwsvdef.h"

unsigned long __stack = 0x40000;  /* program requires 256k stack */

cvar_t sys_nostdout = {"sys_nostdout","0"};
cvar_t sys_extrasleep = {"sys_extrasleep","0"};
static quakeparms_t parms;
struct Library *TimerBase = NULL;

static struct timerequest *timerio;
static int membase_offs;
static BPTR amiga_stdin,amiga_stdout;

extern void NET_Select(unsigned long);


/*
 * AMIGA only functions
 */

#if defined(__PPC__) && !defined(WOS)
/*
 * This is a GetSysTimePPC-emulation for PowerUp, as calling
 * M68k-GetSysTime() via context switch needs too much time.
 */
static void *PowerUpTimer = NULL;
static ULONG TicksPerSec[2];


/*
================
InitSysTimePPC
================
*/
static void InitSysTimePPC(void)
{
  struct TagItem ti[2];
  ULONG r[2];

  ti[0].ti_Tag = PPCTIMERTAG_CPU;
  ti[0].ti_Data = TRUE;
  ti[1].ti_Tag = TAG_END;
  if (!(PowerUpTimer = PPCCreateTimerObject(ti)))
    Sys_Error("Can't create TimerObject");
  PPCGetTimerObject(PowerUpTimer,PPCTIMERTAG_TICKSPERSEC,TicksPerSec);
}

/*
================
ExitSysTimePPC
================
*/
static void ExitSysTimePPC(void)
{
  if (PowerUpTimer) {
    PPCDeleteTimerObject(PowerUpTimer);
    PowerUpTimer = NULL;
  }
}

/*
================
GetSysTimePPC
================
*/
void GetSysTimePPC(struct timeval *tv)
{
  static ULONG mill[2] = { 0,1000000 };
  ULONG r[2],secs[2];

  if (PowerUpTimer) {
    PPCGetTimerObject(PowerUpTimer,PPCTIMERTAG_CURRENTTICKS,r);
    secs[0] = r[0];
    secs[1] = r[1];
    PPCDivu64p((int *)secs,(int *)TicksPerSec);
    tv->tv_secs = secs[1];
    PPCModu64p((int *)r,(int *)TicksPerSec);
    PPCMulu64p((int *)r,(int *)mill);
    PPCDivu64p((int *)r,(int *)TicksPerSec);
    tv->tv_micro = r[1];
  }
}
#endif


/*
================
cleanup
================
*/
static void cleanup(int rc)
{
  SetMode(amiga_stdin,0);  /* put console back into normal CON mode */

  if (parms.membase)
    FreeMem((char *)parms.membase-membase_offs,parms.memsize+3*32);

  if (TimerBase) {
    if (!CheckIO((struct IORequest *)timerio)) {
      AbortIO((struct IORequest *)timerio);
      WaitIO((struct IORequest *)timerio);
    }
    CloseDevice((struct IORequest *)timerio);
    DeletePort(timerio->tr_node.io_Message.mn_ReplyPort);
    DeleteExtIO((struct IORequest *)timerio);
  }

#if defined(__PPC__) && !defined(WOS)
  /* cleanup GetSysTimePPC() emulation for PowerUp */
  ExitSysTimePPC();
#endif

  exit(rc);
}


/*
================
usleep
================
*/
void usleep(unsigned long timeout)
{
  timerio->tr_node.io_Command = TR_ADDREQUEST;
  timerio->tr_time.tv_secs = timeout / 1000000;
  timerio->tr_time.tv_micro = timeout % 1000000;
  SendIO((struct IORequest *)timerio);
  WaitIO((struct IORequest *)timerio);
}


/*
================
strcasecmp
================
*/
int strcasecmp(const char *str1, const char *str2)
{
  while(tolower((unsigned char)*str1) == tolower((unsigned char)*str2)) {
    if(!*str1) return(0);
    str1++;str2++;
  }
  return(tolower(*(unsigned char *)str1)-tolower(*(unsigned char *)str2));
}


/*
================
strncasecmp
================
*/
int strncasecmp(const char *str1, const char *str2, size_t n)
{
  if (n==0) return 0;
  while (--n && tolower((unsigned char)*str1)==tolower((unsigned char)*str2)){
    if (!*str1) return(0);
    str1++; str2++;
  }
  return(tolower(*(unsigned char *)str1)-tolower(*(unsigned char *)str2));
}


/*
================
bzero
================
*/
void bzero(void *p,size_t n)
{
  memset(p,0,n);
}


/*
===============================================================================

        REQUIRED SYS FUNCTIONS

===============================================================================
*/

/*
============
Sys_FileTime

returns -1 if not present
============
*/
int Sys_FileTime (char *path)
{
  BPTR lck;
  int  t = -1;

  if (lck = Lock(path,ACCESS_READ)) {
    t = 1;
    UnLock(lck);
  }

  return t;
}


/*
============
Sys_mkdir

============
*/
void Sys_mkdir (char *path)
{
  BPTR lck;

  if (lck = CreateDir(path))
    UnLock(lck);
}


/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime (void)
{
  static ULONG secbase = 0;
  struct timeval tv;

#ifdef __PPC__
  GetSysTimePPC(&tv);
#else
  GetSysTime(&tv);
#endif
  if (!secbase) {
    secbase = tv.tv_secs;
    return (double)tv.tv_micro/1000000.0;
  }
  return (double)(tv.tv_secs-secbase) + (((double)tv.tv_micro) / 1000000.0);
}

/*
================
Sys_Error
================
*/
void Sys_Error (char *error, ...)
{
  va_list argptr;
  
  printf("Fatal error: ");
  va_start(argptr,error);
  vprintf(error,argptr);
  va_end(argptr);
  printf("\n");
  cleanup(1);
}

/*
================
Sys_Printf
================
*/
void Sys_Printf (char *fmt, ...)
{
  va_list argptr;
  static char text[2048];
  unsigned char *p;

  va_start(argptr,fmt);
  vsprintf(text,fmt,argptr);
  va_end(argptr);

  if (strlen(text) > sizeof(text))
    Sys_Error("memory overwrite in Sys_Printf");

  if (sys_nostdout.value)
    return;

  for (p = (unsigned char *)text; *p; p++) {
    *p &= 0x7f;
    if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
      printf("[%02x]", *p);
    else
      putc(*p, stdout);
  }
  fflush(stdout);
}


/*
================
Sys_Quit
================
*/
void Sys_Quit (void)
{
  cleanup(0);   // appkit isn't running
}


/*
================
Sys_ConsoleInput

Checks for a complete line of text typed in at the console, then forwards
it to the host command processor
================
*/
char *Sys_ConsoleInput (void)
{
  static const char *backspace = "\b \b";
  static char text[256];
  static int len = 0;
  char ch;

  while (WaitForChar(amiga_stdin,10) == DOSTRUE) {
    Read(amiga_stdin,&ch,1);  /* note: console is in RAW mode! */
    if (ch == '\r') {
      ch = '\n';
      Write(amiga_stdout,&ch,1);
      text[len] = 0;
      len = 0;
      return text;
    }
    else if (ch == '\b') {
      if (len > 0) {
        len--;
        Write(amiga_stdout,(char *)backspace,3);
      }
    }
    else {
      if (len < sizeof(text)-1) {
        Write(amiga_stdout,&ch,1);
        text[len++] = ch;
      }
    }
  }
  return NULL;
}

/*
=============
Sys_Init

Quake calls this so the system can register variables before host_hunklevel
is marked
=============
*/
void Sys_Init (void)
{
  Cvar_RegisterVariable (&sys_nostdout);
  Cvar_RegisterVariable (&sys_extrasleep);
}

/*
=============
main
=============
*/
main(int argc, char *argv[])
{
  char cwd[128];
  struct MsgPort *timerport;
  int i;
  double time, oldtime, newtime;

  memset(&parms,0,sizeof(parms));
  parms.memsize = 16*1024*1024;  /* 16MB is default */

  COM_InitArgv (argc, argv);  
  parms.argc = com_argc;
  parms.argv = com_argv;

  /* Amiga Init */
  amiga_stdin = Input();
  amiga_stdout = Output();
  if (!COM_CheckParm("-noconsole"))
    SetMode(amiga_stdin,1);  /* put console into RAW mode */

#if defined(__PPC__) && !defined(WOS)
  /* init GetSysTimePPC() emulation for PowerUp */
  InitSysTimePPC();
#endif

  /* open timer.device */
  if (timerport = CreatePort(NULL,0)) {
    if (timerio = (struct timerequest *)
                   CreateExtIO(timerport,sizeof(struct timerequest))) {
      if (OpenDevice(TIMERNAME,UNIT_MICROHZ,
                     (struct IORequest *)timerio,0) == 0) {
        TimerBase = (struct Library *)timerio->tr_node.io_Device;
      }
      else {
        DeleteExtIO((struct IORequest *)timerio);
        DeletePort(timerport);
      }
    }
    else
      DeletePort(timerport);
  }
  if (!TimerBase)
    Sys_Error("Can't open timer.device");
  usleep(1);  /* don't delete, otherwise we can't do timer.device cleanup */

  /* check arguments */
  if (i = COM_CheckParm("-mem"))
    parms.memsize = (int)(Q_atof(com_argv[i+1]) * 1024 * 1024);

  /* alloc 16-byte aligned quake memory */
  parms.memsize = (parms.memsize+15)&~15;
  if ((parms.membase = AllocMem((ULONG)parms.memsize,MEMF_FAST)) == NULL)
    Sys_Error("Can't allocate %ld bytes\n", parms.memsize);
  if ((ULONG)parms.membase & 8)
    membase_offs = 40;  /* guarantee 16-byte aligment */
  else
    membase_offs = 32;
  parms.membase = (char *)parms.membase + membase_offs;
  parms.memsize -= 3*32;

  /* get name of current directory */
  GetCurrentDirName(cwd,128);
  if (i = COM_CheckParm("-dir"))
  strcpy(cwd,com_argv[i+1]);
  else
  parms.basedir = cwd;

  SV_Init (&parms);

  /* run one frame immediately for first heartbeat */
  SV_Frame (0.1);

/*
 * main loop
 */
  oldtime = Sys_DoubleTime() - 0.1;
  while (1) {
    /* wait 100 usec for a pending network packet */
    NET_Select(100);

    /* find time passed since last cycle */
    newtime = Sys_DoubleTime();
    time = newtime - oldtime;
    oldtime = newtime;

    SV_Frame (time);    
    
    /* extrasleep is just a way to generate a fucked up connection on purpose */
    if (sys_extrasleep.value)
      usleep(sys_extrasleep.value);
  }
}
