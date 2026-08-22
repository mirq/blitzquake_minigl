/*
**  snd_amiga.c
**
**  Paula sound driver
**
**  Written by Frank Wille <frank@phoenix.owl.de>
**
*/

#pragma amiga-align
#include <exec/memory.h>
#include <exec/tasks.h>
#include <exec/interrupts.h>
#include <exec/libraries.h>
#include <devices/audio.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <clib/alib_protos.h>

#pragma default-align

#include "quakedef.h"


#ifdef __STORM__
#ifndef PPC
#define PPC
#endif

#include <libraries/powerpc.h>
#include <ppcamiga.h>

static void BeginIOAudioPPC(struct IORequest *arg1)
{
    extern struct Library *AudioBase;
    ULONG regs[16];
    regs[9] = (ULONG) arg1;
    __CallLibrary(AudioBase,-30,regs);
}
#define BeginIO(x) BeginIOAudioPPC(x)

static struct Interrupt *SetIntVectorPPC(long arg1, struct Interrupt *arg2)
{
    extern struct Library *SysBase;
    ULONG regs[16];
    regs[0] = (ULONG) arg1;
    regs[9] = (ULONG) arg2;
    __CallLibrary(SysBase,-162,regs);
    return (struct Interrupt*) regs[0];
}
#define SetIntVector(x,y) SetIntVectorPPC(x,y)

#endif // __STORM__


#define NSAMPLES 0x4000

extern long sysclock;
extern int desired_speed;
extern int FirstTime2;
extern struct Library *TimerBase;

static UBYTE *dmabuf = NULL;
static UWORD period;
static BYTE audio_dev = -1;
static float speed;
static struct MsgPort *audioport1=NULL,*audioport2=NULL;
static struct IOAudio *audio1=NULL,*audio2=NULL;
static struct Interrupt AudioInt;
static struct Interrupt *OldInt;
static short OldINTENA = 0;
struct Library *AudioBase;

static struct {
  struct Library *TimerBase;
  int *FirstTime2;
  double *aud_start_time;
#ifdef __PPC__
  struct Library *MathIeeeDoubBasBase; /* offset 12 */
#endif
} IntData;

#ifdef __PPC__
extern struct Library *MathIeeeDoubBasBase;
extern qboolean no68kFPU; /* for LC040/LC060 systems */
extern void AudioIntCodeNoFPU(void);
#endif

extern void AudioIntCode(void);
void (*audintptr)(void) = AudioIntCode;


void SNDDMA_Shutdown(void)
{
  if (OldINTENA)
    *(short *)0xdff09a = (OldINTENA | 0x8000);
  if (OldInt)
    SetIntVector(7,OldInt);

  if (audio_dev == 0) {
    if (!CheckIO(&audio1->ioa_Request)) {
      AbortIO(&audio1->ioa_Request);
      WaitPort(audioport1);
      while (GetMsg(audioport1));
    }
    if (!CheckIO(&audio2->ioa_Request)) {
      AbortIO(&audio2->ioa_Request);
      WaitPort(audioport2);
      while (GetMsg(audioport2));
    }
    CloseDevice(&audio1->ioa_Request);
  }

  if (audio2)
    Sys_Free(audio2);

  if (audio1)
    DeleteIORequest(&audio1->ioa_Request);
  if (audioport2)
    DeleteMsgPort(audioport2);
  if (audioport1)
    DeleteMsgPort(audioport1);
  if (IntData.aud_start_time)
    FreeMem((void*)(IntData.aud_start_time),sizeof(double));

  if (shm)
    Sys_Free((void*)shm);

  if (dmabuf)
    FreeMem(dmabuf,NSAMPLES);
}


qboolean SNDDMA_Init(void)
{
  /* first try ch. 0/1, then 2/3 */
  static const UBYTE channelalloc[2] = { 1|2, 4|8 };
  int channelnr, i;

  /* evaluate parameters */
  if (i = COM_CheckParm("-audspeed")) {
    period = (UWORD)(sysclock / Q_atoi(com_argv[i+1]));
  }
  else
    period = (UWORD)(sysclock / desired_speed);

#ifdef __PPC__
  if (no68kFPU)
    audintptr = AudioIntCodeNoFPU; /* use mathieeedoubbas.library */
#endif

  /* allocate dma buffer and sound structure */
  if (!(dmabuf = AllocMem(NSAMPLES,MEMF_CHIP|MEMF_PUBLIC|MEMF_CLEAR))) {
    Con_Printf("Can't allocate Paula DMA buffer\n");
    return (false);
  }
  if (!(shm = Sys_Alloc(sizeof(dma_t),MEMF_ANY|MEMF_CLEAR))) {
    Con_Printf("Failed to allocate shm\n");
    return (false);
  }
  if (!(IntData.aud_start_time = AllocMem(sizeof(double),MEMF_CHIP|MEMF_CLEAR))) {
    Con_Printf("Failed to allocate 8 bytes of CHIP-RAM buffer\n");
    return (false);
  }

  /* init shm */
  shm->buffer = (unsigned char *)dmabuf;
  shm->channels = 1;
  shm->speed = sysclock/(long)period;
  shm->samplebits = 8;
  shm->samples = NSAMPLES;
  shm->submission_chunk = 1;
  speed = (float)shm->speed;


  /* open audio.device */
  if (audioport1 = CreateMsgPort()) {
    if (audioport2 = CreateMsgPort()) {
        if (audio1 = (struct IOAudio *)CreateIORequest(audioport1,
                      sizeof(struct IOAudio))) {
          if (audio2 = (struct IOAudio *)Sys_Alloc(sizeof(struct IOAudio),
                                                   MEMF_PUBLIC)) {
              audio1->ioa_Request.io_Message.mn_Node.ln_Pri = ADALLOC_MAXPREC;
              audio1->ioa_Request.io_Command = ADCMD_ALLOCATE;
              audio1->ioa_Request.io_Flags = ADIOF_NOWAIT;
              audio1->ioa_AllocKey = 0;
              audio1->ioa_Data = (UBYTE *)channelalloc;
              audio1->ioa_Length = sizeof(channelalloc);
              audio_dev = OpenDevice(AUDIONAME,0,
                                     &audio1->ioa_Request,0);
          }
        }
    }
  }

  if (audio_dev == 0) {
    /* set up audio io blocks */
    AudioBase = (struct Library *)audio1->ioa_Request.io_Device;
    audio1->ioa_Request.io_Command = CMD_WRITE;
    audio1->ioa_Request.io_Flags = ADIOF_PERVOL;
    audio1->ioa_Data = dmabuf;
    audio1->ioa_Length = NSAMPLES;
    audio1->ioa_Period = period;
    audio1->ioa_Volume = 64;
    audio1->ioa_Cycles = 0;  /* loop forever */
    *audio2 = *audio1;
    audio2->ioa_Request.io_Message.mn_ReplyPort = audioport2;
    audio1->ioa_Request.io_Unit = (struct Unit *)
                                   ((ULONG)audio1->ioa_Request.io_Unit & 9);
    audio2->ioa_Request.io_Unit = (struct Unit *)
                                   ((ULONG)audio2->ioa_Request.io_Unit & 6);

  }
  else {
    Con_Printf("Couldn't open audio.device\n");
    return (false);
  }
  IntData.TimerBase = TimerBase;
  IntData.FirstTime2 = &FirstTime2;
#ifdef __PPC__
  IntData.MathIeeeDoubBasBase = MathIeeeDoubBasBase;
#endif
  AudioInt.is_Node.ln_Type = NT_INTERRUPT;
  AudioInt.is_Node.ln_Pri = 0;
  AudioInt.is_Data = &IntData;
  AudioInt.is_Code = (void(*)())audintptr;
  switch ((ULONG)audio1->ioa_Request.io_Unit) {
    case 1: channelnr = 0;break;
    case 2: channelnr = 1;break;
    case 4: channelnr = 2;break;
    case 8: channelnr = 3;break;
  }
  BeginIO(&audio1->ioa_Request);
  BeginIO(&audio2->ioa_Request);
  while (*(volatile short *)0xdff01c & (0x0080 << channelnr));
  OldInt = SetIntVector(channelnr+7,&AudioInt);
  OldINTENA = *(short *)0xdff01c;
  *IntData.aud_start_time = 0;
  *(short *)0xdff09a = OldINTENA | (0xc000 | (0x0080 << channelnr));
  return (true);
}


int SNDDMA_GetDMAPos(void)
{
  int pos;
#ifdef __PPC__
  static struct timeval tv;
  static int SyncCounter=0;
  static double timediff;
  double time;

  while (!(*IntData.aud_start_time));
  if (SyncCounter == 0)
  {
    GetSysTime(&tv);
    time = ((double)(tv.tv_secs-FirstTime2) + 
            (((double)tv.tv_micro) / 1000000.0));
    timediff = (Sys_FloatTime()) - time;
  }
  SyncCounter = (SyncCounter + 1) % 50;
  pos = (int)((Sys_FloatTime()-(*IntData.aud_start_time+timediff))*speed);

#else
  while (!(*IntData.aud_start_time));
  pos = (int)((Sys_FloatTime()-*IntData.aud_start_time)*speed);
#endif

  if (pos >= NSAMPLES)
        pos = 0;

  return (shm->samplepos = pos);
}


void SNDDMA_Submit(void)
{
}
