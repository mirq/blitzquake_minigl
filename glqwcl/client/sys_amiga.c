/*
** sys_amiga.c
**
** QuakeWorld Client for Amiga M68k and Amiga PowerPC
**
** Written by Frank Wille <frank@phoenix.owl.de>
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
  #ifdef __GNUC__
  #include <powerpc/powerpc_protos.h>
  #else
  #include <clib/powerpc_protos.h>
  #endif
#else
#include <powerup/ppclib/time.h>
#include <powerup/gcclib/powerup_protos.h>
#endif
#endif
#pragma default-align

/* Quake includes */
#include "quakedef.h"
#if defined(__PPC__) && !defined(WOS)
#include "sys_timer.h"  /* GetSysTimePPC() emulation for PowerUp */
#endif

#define CLOCK_PAL       3546895
#define CLOCK_NTSC      3579545
#define MAX_HANDLES     10


#ifdef __STORM__
#define CreatePort(x,y) CreateMsgPort()
#define CreateExtIO(p,s) CreateIORequest(p,s)
#define DeletePort(p) DeleteMsgPort(p)
#define DeleteExtIO(p) DeleteIORequest(p)
extern struct Library *TimerBase;
extern struct Library *UtilityBase;
#else
  struct Device *TimerBase = NULL;
  struct Library *UtilityBase = NULL;
#endif
struct Library *LowLevelBase = NULL;
#ifdef __PPC__
#ifdef __STORM__
extern struct Library *MathIeeeDoubBasBase = NULL;
#else
struct Library *MathIeeeDoubBasBase;
#endif

qboolean no68kFPU = false; /* for LC040/LC060 systems */
#endif

#ifndef GLQUAKE
unsigned long __stack = 0x80000;  /* program requires 512k stack */
#else
unsigned long __stack = 0x100000;  /* program requires 1024k stack */
#endif

/* these are for snd_amiga.c */
long sysclock = CLOCK_PAL;
int FirstTime = 0;
int FirstTime2 = 0;

static quakeparms_t parms;

static struct timerequest *timerio;
static int membase_offs;
static int nostdout=0,coninput=0;
static BPTR amiga_stdin,amiga_stdout;
static BPTR sys_handles[MAX_HANDLES];

cvar_t  sys_linerefresh = {"sys_linerefresh","0"};// set for entity display


#ifndef __PPC__
extern void MMUHackOn(void);
extern void MMUHackOff(void);
#endif



// =======================================================================
// General routines
// =======================================================================

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

#ifdef GLQUAKE //surgeon
static qboolean prevent_crash = true;
#endif

static void cleanup(int rc)
{
  int i;

#ifdef GLQUAKE
  if(prevent_crash == true) //was not done in Sys_Error
#endif
  Host_Shutdown();

  if (coninput)
    SetMode(amiga_stdin,0);  /* put console back into normal CON mode */

  if (parms.membase)
    FreeMem((char *)parms.membase-membase_offs,parms.memsize+3*32);

#ifndef __PPC__
  if (COM_CheckParm("-hack"))
    MMUHackOff();
#endif

  if (TimerBase)
  {
    if (!CheckIO((struct IORequest *)timerio))
    {
      AbortIO((struct IORequest *)timerio);
      WaitIO((struct IORequest *)timerio);
    }
    CloseDevice((struct IORequest *)timerio);
    DeletePort(timerio->tr_node.io_Message.mn_ReplyPort);
    DeleteStdIO((struct IOStdReq *)timerio);
  }

#ifdef __PPC__
  if (no68kFPU) {
    if (MathIeeeDoubBasBase)
      CloseLibrary(MathIeeeDoubBasBase);
  }
#endif

  if (LowLevelBase)
    CloseLibrary(LowLevelBase);
  if (UtilityBase)
    CloseLibrary(UtilityBase);

  for (i=1; i<MAX_HANDLES; i++) {
    if (sys_handles[i])
      Close(sys_handles[i]);
  }
  exit(rc);
}


void Sys_DebugNumber(int y, int val)
{
}

void Sys_Printf (char *fmt, ...)
{
  if(!nostdout) //adapted from normal quake
  {
  va_list argptr;
  static char text[2048];
  unsigned char *p;

  va_start(argptr,fmt);
  vsprintf(text,fmt,argptr);
  va_end(argptr);

  if (strlen(text) > sizeof(text))
    Sys_Error("memory overwrite in Sys_Printf");

//  if (nostdout)
//    return;

  for (p = (unsigned char *)text; *p; p++) {
    *p &= 0x7f;
    if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
      printf("[%02x]", *p);
    else
      putc(*p, stdout);
  }
  fflush(stdout);
  }
}

void Sys_Quit (void)
{
  cleanup(0);
}

void Sys_Init(void)
{
}

void Sys_Error (char *error, ...)
{
  va_list argptr;

#ifdef GLQUAKE //surgeon
  Host_Shutdown();
  prevent_crash = false;
#endif
  
  printf("Error: ");
  va_start(argptr,error);
  vprintf(error,argptr);
  va_end(argptr);
  printf("\n");
  cleanup(1);
}

void Sys_Warn (char *warning, ...)
{ 
  va_list argptr;
    
  printf("Warning: ");
  va_start(argptr,warning);
  vprintf(warning,argptr);
  va_end(argptr);
  printf("\n");
} 

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

void Sys_mkdir (char *path)
{
  BPTR lck;

  if (lck = CreateDir(path))
    UnLock(lck);
}


static int findhandle (void)
{
  int i;
  
  for (i=1 ; i<MAX_HANDLES ; i++)
    if (!sys_handles[i])
      return i;
  Sys_Error ("out of handles");
  return -1;
}

int Sys_FileOpenRead (char *path, int *handle)
{
  BPTR fh;
  struct FileInfoBlock *fib;
  int     i = -1;
  int flen = -1;
  
  *handle = -1;
  if (fib = AllocDosObjectTags(DOS_FIB,TAG_DONE)) {
    if (fh = Open(path,MODE_OLDFILE)) {
      if (ExamineFH(fh,fib)) {
        if ((i = findhandle()) > 0) {
          sys_handles[i] = fh;
          *handle = i;
          flen = (int)fib->fib_Size;
        }
        else
          Close(fh);
      }
      else
        Close(fh);
    }
    FreeDosObject(DOS_FIB,fib);
  }
  return flen;
}

int Sys_FileOpenWrite (char *path)
{
  BPTR fh;
  int  i;
  
  if ((i = findhandle ()) > 0) {
    if (fh = Open(path,MODE_NEWFILE)) {
      sys_handles[i] = fh;
    }
    else {
      char ebuf[80];
      Fault(IoErr(),"",ebuf,80);
      Sys_Error ("Error opening %s: %s", path, ebuf);
      i = -1;
    }
  }
  return i;
}

int Sys_FileWrite (int handle, void *src, int count)
{
  return (int)Write(sys_handles[handle],src,(LONG)count);
}

void Sys_FileClose (int handle)
{
  if (sys_handles[handle]) {
    Close(sys_handles[handle]);
    sys_handles[handle] = 0;
  }
}

void Sys_FileSeek (int handle, int position)
{
  Seek(sys_handles[handle],position,OFFSET_BEGINNING);
}

int Sys_FileRead (int handle, void *dest, int count)
{
   return (int)Read(sys_handles[handle],dest,(LONG)count);
}

void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
}

void Sys_DebugLog(char *file, char *fmt, ...)
{
  va_list argptr; 
  static char data[1024];
  BPTR fh;

  va_start(argptr, fmt);
  vsprintf(data, fmt, argptr);
  va_end(argptr);
  if (fh = Open(file,MODE_READWRITE)) {
    Seek(fh,0,OFFSET_END);
    Write(fh,data,(LONG)strlen(data));
    Close(fh);
  }
}

void Sys_EditFile(char *filename)
{
  char cmd[256];
  char *editor;

  editor = getenv("VISUAL");
  if (!editor)
    editor = getenv("EDITOR");
  if (!editor)
    editor = getenv("EDIT");
  if (!editor)
    editor = "ed";
  sprintf(cmd, "%s %s", editor, filename);
  system(cmd);
}


double Sys_DoubleTime (void)
{
  struct timeval tv;

#ifdef __PPC__
  GetSysTimePPC(&tv);
  return ((double)(tv.tv_secs-FirstTime) + (((double)tv.tv_micro) / 1000000.0));
#else
  GetSysTime(&tv);
  return ((double)(tv.tv_secs-FirstTime) + (((double)tv.tv_micro) / 1000000.0));
#endif
}




void Sys_LineRefresh(void)
{
}

char *Sys_ConsoleInput(void)
{
  if (coninput) {
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
  }
  return NULL;
}


int main(int argc, char *argv[])
{
  char cwd[128];
  struct MsgPort *timerport;
  int i;
  double time, oldtime, newtime;
  struct timeval tv;

  struct GfxBase *GfxBase;

  memset(&parms,0,sizeof(parms));
  parms.memsize = 16*1024*1024;  /* 16MB is default */

  COM_InitArgv (argc, argv);  
  parms.argc = com_argc;
  parms.argv = com_argv;

#ifndef __PPC__
  if (COM_CheckParm("-hack"))
    MMUHackOn();
#endif

  /* Amiga Init */
  if ((UtilityBase = OpenLibrary("utility.library",36)) == NULL ||
      (LowLevelBase = OpenLibrary("lowlevel.library",36)) == NULL)
    Sys_Error("OS2.0 required!");
#ifdef __PPC__
  no68kFPU = COM_CheckParm("-no68kfpu") != 0;
  if (no68kFPU) {
     if (!(MathIeeeDoubBasBase = OpenLibrary("mathieeedoubbas.library",36)))
       Sys_Error("mathieeedoubbas.library is missing");
  }
#endif
  amiga_stdin = Input();
  amiga_stdout = Output();

  nostdout = !(COM_CheckParm("-stdout"));
//modified by surgeon
  coninput = COM_CheckParm("-console");
  if (coninput)
    SetMode(amiga_stdin,1);  /* put console into RAW mode */

  /* open timer.device */
    if (timerport = CreatePort(NULL,0)) {
    if (timerio = (struct timerequest *)
                   CreateExtIO(timerport,sizeof(struct timerequest))) {
      if (OpenDevice(TIMERNAME,UNIT_MICROHZ,
                     (struct IORequest *)timerio,0) == 0) {
        TimerBase = (struct Device *)timerio->tr_node.io_Device;
      }
      else {
        DeleteStdIO((struct IOStdReq *)timerio);
        DeletePort(timerport);
      }
    }
    else
      DeletePort(timerport);
  }
  if (!TimerBase)
    Sys_Error("Can't open timer.device");
  usleep(1);  /* don't delete, otherwise we can't do timer.device cleanup */

#if defined(__PPC__) && !defined(WOS)
  /* init GetSysTimePPC() emulation for PowerUp */
  InitSysTimePPC();
#endif

#ifdef __PPC__
  GetSysTimePPC(&tv);
  FirstTime = tv.tv_secs;
  GetSysTime(&tv);
  FirstTime2 = tv.tv_secs;
#else //68k
  GetSysTime(&tv);
  FirstTime2 = FirstTime = tv.tv_secs;
#endif

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
  if (i = COM_CheckParm("-dir"))
  strcpy(cwd,com_argv[i+1]);
  else
  GetCurrentDirName(cwd,128);
  parms.basedir = cwd;

  /* set the clock constant */
  if (GfxBase = (struct GfxBase *)OpenLibrary("graphics.library",36))
  {
    if (GfxBase->DisplayFlags & PAL)
      sysclock = CLOCK_PAL;
    else
      sysclock = CLOCK_NTSC;

    CloseLibrary((struct Library *)GfxBase);
  }

  Sys_Init();

  Host_Init(&parms);

  oldtime = Sys_DoubleTime ();

  while (1) {
    /* find time spent rendering last frame */
    newtime = Sys_DoubleTime ();
    time = newtime - oldtime;

    Host_Frame(time);
    oldtime = newtime;
  }

  return 0;
}
