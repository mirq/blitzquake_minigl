/*
Copyright (C) 2000 Peter McGavin.

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
// snd_amiga.c

#if defined (__GNUC__) && defined (__PPC__)
#pragma pack(push,2)
#else
#pragma amiga-align
#endif


#include <exec/exec.h>
#include <exec/execbase.h>      /* CACRF_ClearD */
#include <dos/dos.h>
#include <graphics/gfxbase.h>
#include <devices/audio.h>
#include <devices/ahi.h>
#include <clib/alib_protos.h>
#include <exec/interrupts.h>
#include <proto/exec.h>
#if defined(__VBCC__) && defined(__PPC__)
#include <clib/powerpc_protos.h>
#elif defined(__GNUC__) && defined(__PPC__)
#include <powerpc/powerpc_protos.h>
#ifndef WOS
/* MOS2WOS does not ship powerup/ppcinline/alib.h; prototypes come from
 * <proto/exec.h> + <clib/alib_protos.h> already included above. */
#include <powerup/ppcinline/alib.h>
#endif
#endif

#if defined (__GNUC__) && defined (__PPC__)
#pragma pack(pop)
#else
#pragma default-align
#endif


#if defined (__GNUC__) && defined (__PPC__)
#include <powerup/ppcproto/exec.h>
#include <powerup/ppcproto/dos.h>
#include <powerup/ppcproto/graphics.h>
#include <powerup/ppcproto/ahi.h>
#endif

extern struct GfxBase *GfxBase;

#include "quakedef.h"

#ifdef WOS
/* MOS2WOS headers are OS4-style (natural PPC alignment); 68k AHI needs the
   2-byte packed AmigaOS request layout. See wos_ahi_abi.h. */
#include "wos_ahi_abi.h"
typedef struct wos_AHIRequest ahi_request_t;
#else
typedef struct AHIRequest ahi_request_t;
#endif

/* Cache-flush strategy for the freshly painted AHI ring (see SNDDMA_Submit).
   1 = native PPC dcbf+sync over the painted range; 0 = exec CacheClearE via
   the emulated LP call. Selected with -sndflush exec|native (default native;
   the exec path remains as a fallback). */
static int snd_flush_native = 1;

/* Optional AHI audio-mode override (hex AudioID, 0 = AHI_DEFAULT_ID).
   Selected with -ahiid <hex>; lets a cheaper Paula mode be tested without
   touching the user's AHI preferences (the default "14 bit stereo++
   calibrated" mode costs several ms/frame in driver mixing on this 68060). */
static ULONG wos_ahi_id = 0;

#ifdef WOS
/* Native PPC data-cache push for a range. dcbf writes back dirty lines and
   invalidates them; sync orders the writes before the 68k AHI driver reads
   the buffer. Validated on hardware with the wos_cache probe: the 68k sees
   dcbf'd fast-RAM data and does not serve stale lines on the next read. */
static void SND_FlushRange (UBYTE *addr, int len)
{
  if (len <= 0)
    return;

  if (snd_flush_native) {
    unsigned long a = (unsigned long)addr & ~31UL;
    unsigned long end = ((unsigned long)addr + (unsigned long)len + 31UL)
                        & ~31UL;

    for (; a < end; a += 32)
      __asm__ __volatile__ ("dcbf 0,%0" : : "r" (a) : "memory");
    __asm__ __volatile__ ("sync" ::: "memory");
  }
  else {
    CacheClearE (addr, len, CACRF_ClearD);
  }
}
#else
#define SND_FlushRange(a, l) CacheClearE ((a), (l), CACRF_ClearD)
#endif

/**********************************************************************/

extern int desired_speed;
extern int desired_bits;
extern int paintedtime;         /* sample pairs, owned by snd_dma.c */
int using_ahi = FALSE;

/* Negotiated AHI sample format (S16S on this port). */
static ULONG ahi_type = AHIST_S16S;

/* On WarpOS a missing/broken AHI must not kill the game: run silent.
   The audio.device/Paula path is never used on WarpOS (its 68k interrupt
   handler trips "Async Run68K function not supported"). */
#ifdef WOS
#define USE_AHI_ALWAYS 1
#define AHI_FAIL(msg) do { Con_Printf ("AHI: %s\n", (msg)); \
                           SNDDMA_Shutdown (); return 0; } while (0)
#else
#define USE_AHI_ALWAYS 0
#define AHI_FAIL(msg) Sys_Error (msg)
#endif

void SNDDMA_Shutdown (void);

/**********************************************************************/

/* AHI */

#define AHIBUFFERSIZE 262144

struct ahi_channel_info {
  ahi_request_t *AHIio;
  double starttime;
  BOOL sound_in_progress;
};

struct Library *AHIBase = NULL;
static struct MsgPort *AHImp = NULL;
static struct ahi_channel_info ahi_channel_info[2] = {
  {NULL, 0.0, FALSE},
  {NULL, 0.0, FALSE}
};
static BYTE AHIDevice = -1;
static BOOL ahidevice_is_open = FALSE;
static double ahi_playtime;
static int which_buffer;


/* audio.device */

/* #define BUFFERSIZE 16384 */
#define BUFFERSIZE 4096

#define MAXNUMCHANNELS   4   /* max number of Amiga sound channels */

struct channel_info {
  struct MsgPort *audio_mp;
  struct IOAudio *audio_io;
  double starttime;
  BOOL sound_in_progress;
};

static struct channel_info channel_info[MAXNUMCHANNELS] = {
  {NULL, NULL, 0.0, FALSE},
  {NULL, NULL, 0.0, FALSE},
  {NULL, NULL, 0.0, FALSE},
  {NULL, NULL, 0.0, FALSE},
};

static int size;
static struct MsgPort *audio_mp = NULL;
static struct IOAudio *audio_io = NULL;
static BOOL audio_is_open = FALSE;
static ULONG clock_constant;   /* see Amiga Hardware Manual page 141 */
static UWORD period;
static double twice_real_speed;

/**********************************************************************/
static void stop_ahi_sound (struct ahi_channel_info *c)
{
  if (!ahidevice_is_open)
    return;
  if (c->sound_in_progress) {
    AbortIO ((struct IORequest *)c->AHIio);
    WaitPort (AHImp);
    GetMsg (AHImp);
    c->sound_in_progress = FALSE;
  }
}

/**********************************************************************/
// Starts an AHI sound in a particular sound channel.
// Use link for double-buffering.

static void start_ahi_sound (struct ahi_channel_info *c,
                             char *buffer, int length,
                             ahi_request_t *link)
{
  if (!ahidevice_is_open)
    return;
  stop_ahi_sound (c);
  c->AHIio->ahir_Std.io_Command = CMD_WRITE;
  c->AHIio->ahir_Std.io_Flags = 0;
  c->AHIio->ahir_Std.io_Message.mn_Node.ln_Pri = -50;   /* sound effects */
  c->AHIio->ahir_Std.io_Data = buffer;
  c->AHIio->ahir_Std.io_Length = length;
  c->AHIio->ahir_Type = ahi_type;
  c->AHIio->ahir_Frequency = shm->speed;
  c->AHIio->ahir_Volume = 0x10000;
  c->AHIio->ahir_Position = 0x8000;
  c->AHIio->ahir_Link = link;
  /* The AHI driver (68k side) reads this memory; the 68060 does not snoop
     the PPC data cache, so write dirty lines back before it does. */
  CacheClearE (buffer, length, CACRF_ClearD);
  SendIO ((struct IORequest *)c->AHIio);
  c->sound_in_progress = TRUE;
}

/**********************************************************************/
// Stops an audio.device sound channel.

static void stopsound (int cnum)
{
  if (!audio_is_open)
    return;
  if (channel_info[cnum].sound_in_progress) {
    AbortIO ((struct IORequest *)channel_info[cnum].audio_io);
    WaitPort (channel_info[cnum].audio_mp);
    GetMsg (channel_info[cnum].audio_mp);
    channel_info[cnum].sound_in_progress = FALSE;
  }
}

/**********************************************************************/
// Starts an audio.device sound in a particular sound channel.

static int startsound (int cnum, char *buffer, int length)
{
  struct channel_info *c;

  if (!audio_is_open)
    return 1;
  stopsound (cnum);
  c = &channel_info[cnum];
  c->audio_io->ioa_Request.io_Command = CMD_WRITE;
  c->audio_io->ioa_Request.io_Flags = ADIOF_PERVOL;
  c->audio_io->ioa_Data = buffer;
  c->audio_io->ioa_Length = length;
  c->audio_io->ioa_Period = period;
  c->audio_io->ioa_Volume = 64;
  c->audio_io->ioa_Cycles = 0;
  BeginIO ((struct IORequest *)c->audio_io);
  c->starttime = Sys_FloatTime ();
  c->sound_in_progress = TRUE;
  return cnum;
}

/**********************************************************************/

static UWORD def_smp_speed;

/* One short synchronous CMD_WRITE: returns the driver's io_Error for the
   given sample format, so we can bail out before streaming garbage. */
static ULONG ahi_type_test (ULONG type)
{
  ahi_request_t *r = ahi_channel_info[0].AHIio;
  ULONG len = 4096;

  if (!ahidevice_is_open || r == NULL || shm == NULL || shm->buffer == NULL)
    return (ULONG)-1;
  if (len > AHIBUFFERSIZE)
    len = AHIBUFFERSIZE;
  r->ahir_Std.io_Command = CMD_WRITE;
  r->ahir_Std.io_Flags = 0;
  r->ahir_Std.io_Message.mn_Node.ln_Pri = 0;
  r->ahir_Std.io_Data = shm->buffer;
  r->ahir_Std.io_Length = len;
  r->ahir_Type = type;
  r->ahir_Frequency = shm->speed;
  r->ahir_Volume = 0x10000;
  r->ahir_Position = 0x8000;
  r->ahir_Link = NULL;
  CacheClearE (shm->buffer, len, CACRF_ClearD);
  SendIO ((struct IORequest *)r);
  WaitIO ((struct IORequest *)r);
  return r->ahir_Std.io_Error;
}

#ifdef WOS
/* ================== WarpOS: Q3-style high-level AHI ==================
 *
 * Ported 1:1 from quake3amiga/code/amiga/amiga_snd.c (Quake 2 code by Jarmo
 * Laakkonen and Hans-Joerg Frieden, via Cowcat's Q3 port).
 *
 * AHI does the playback scheduling and loops one dynamic sample; the play
 * cursor comes from an AHIET_CHANNELINFO effect hook. AHI calls the 68k code
 * blob below on every mix and it stores ahieci_Offset[0] into hook->h_Data,
 * which the PPC reads directly - so there are no per-frame OS calls at all.
 * (Each emulated Run68K LP call costs milliseconds on this stack and used to
 * cost ~30 ms/frame with the old low-level double-buffer scheme.)
 *
 * Difference from Q3: the DMA buffer is malloc'd (this runtime has
 * PowerPCBase == NULL, so AllocVecPPC is unavailable), therefore
 * SNDDMA_Submit still flushes the painted range with exec CacheClearE -
 * required for the 68k AHI driver to see the PPC's writes.
 */

#include <utility/tagitem.h>
#include <utility/hooks.h>

#define WOS_AHI_MINBUF (4 * 16384)

static struct AHIAudioCtrl *wos_actrl = NULL;
static ahi_request_t *wos_ahi_io = NULL;
static ULONG wos_ahi_rc = 1;
static UBYTE *wos_dmabuf = NULL;
static int wos_buflen = 0;

/* 68k callback blob (converted from a vbcc object by Cowcat; see the Q3
 * source for the equivalent C):
 *   move.l a0,-(sp) / move.l a1,a2 / move.l 12(a1),d0
 *   move.l d0,16(a2) / moveq #0,d0 / move.l (sp)+,a2 / rts
 * i.e. hook->h_Data = info->ahieci_Offset[0]; return 0; */
static unsigned short wos_ahi_callback[] = {
  0x48e7, 0x0020, 0x2448, 0x2029, 0x000c, 0x2540, 0x0010, 0x7000, 0x245f, 0x4e75
};

struct wos_ahi_channel_info {
  struct AHIEffChannelInfo cinfo;
  ULONG x[1];
};

static struct wos_ahi_channel_info wos_info;
static struct Hook wos_effhook = { {NULL, NULL},
                                   (HOOKFUNC)wos_ahi_callback, NULL, NULL };

static qboolean wos_ahi_init (void)
{
  struct AHISampleInfo sample;
  ULONG mixfreq = 0, playsamples = 0, mode = 0;
  UBYTE name[256];
  int speed;
  int i;

  /* evaluate parameters */
  if (i = COM_CheckParm ("-audspeed"))
    def_smp_speed = (UWORD)Q_atoi (com_argv[i + 1]);
  else
    def_smp_speed = (UWORD)desired_speed;
  speed = def_smp_speed;

  /* cache-flush strategy for SNDDMA_Submit: -sndflush native|exec */
  if (i = COM_CheckParm ("-sndflush")) {
    if (i < com_argc - 1) {
      if (!strcmp (com_argv[i + 1], "exec"))
        snd_flush_native = 0;
      else if (!strcmp (com_argv[i + 1], "native"))
        snd_flush_native = 1;
    }
  }

  /* optional AHI mode override: -ahiid <hex AudioID> (0 = default) */
  if (i = COM_CheckParm ("-ahiid")) {
    if (i < com_argc - 1) {
      char *p = com_argv[i + 1];
      ULONG v = 0;

      while (*p) {
        int c = *p++;
        if (c >= '0' && c <= '9') v = (v << 4) | (c - '0');
        else if (c >= 'a' && c <= 'f') v = (v << 4) | (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v = (v << 4) | (c - 'A' + 10);
        else break;
      }
      wos_ahi_id = v;
    }
  }

  if ((shm = (dma_t *)malloc (sizeof (dma_t))) == NULL)
    Sys_Error ("malloc() failed");
  memset ((void *)shm, 0, sizeof (dma_t));

  wos_info.cinfo.ahieci_Channels = 1;
  wos_info.cinfo.ahieci_Func = &wos_effhook;
  wos_info.cinfo.ahie_Effect = AHIET_CHANNELINFO;
  wos_effhook.h_Data = NULL;

  Con_Printf ("AHI sound initialisation\n");
  Con_Printf ("...Frequency: %d Hz\n", speed);

  if ((AHImp = CreateMsgPort ()) == NULL)
    AHI_FAIL ("Can't create AHI message port");
  if ((wos_ahi_io = (ahi_request_t *)CreateIORequest (AHImp,
                                     sizeof (ahi_request_t))) == NULL)
    AHI_FAIL ("Can't create AHI io request");
  wos_ahi_io->ahir_Version = 4;
  if (OpenDevice ("ahi.device", AHI_NO_UNIT,
                  (struct IORequest *)wos_ahi_io, 0) != 0)
    AHI_FAIL ("Can't open ahi.device version 4");
  AHIDevice = 0;
  AHIBase = (struct Library *)wos_ahi_io->ahir_Std.io_Device;

  if ((wos_actrl = AHI_AllocAudio (AHIA_AudioID,
                                   wos_ahi_id ? wos_ahi_id : AHI_DEFAULT_ID,
                                   AHIA_MixFreq, speed,
                                   AHIA_Channels, 1,
                                   AHIA_Sounds, 1,
                                   TAG_END)) == NULL)
    AHI_FAIL ("Can't allocate audio");

  AHI_GetAudioAttrs (AHI_INVALID_ID, wos_actrl,
                     AHIDB_MaxPlaySamples, (ULONG)&playsamples,
                     AHIDB_BufferLen, 256,
                     AHIDB_Name, (ULONG)name,
                     AHIDB_AudioID, (ULONG)&mode,
                     TAG_END);
  AHI_ControlAudio (wos_actrl, AHIC_MixFreq_Query, (ULONG)&mixfreq, TAG_END);
  if (mixfreq == 0)
    mixfreq = speed;

  wos_buflen = playsamples * speed / mixfreq;
  if (wos_buflen < WOS_AHI_MINBUF)
    wos_buflen = WOS_AHI_MINBUF;

  if ((wos_dmabuf = (UBYTE *)malloc (wos_buflen)) == NULL)
    AHI_FAIL ("Can't allocate AHI dma buffer");
  memset (wos_dmabuf, 0, wos_buflen);

  shm->buffer = (unsigned char *)wos_dmabuf;
  shm->channels = 2;
  shm->speed = speed;
  shm->samplebits = 16;
  shm->samples = wos_buflen / (shm->samplebits / 8);
  shm->submission_chunk = 1;
  shm->samplepos = 0;

  sample.ahisi_Type = AHIST_S16S;
  sample.ahisi_Address = (APTR)wos_dmabuf;
  sample.ahisi_Length = wos_buflen / AHI_SampleFrameSize (AHIST_S16S);

  if ((wos_ahi_rc = AHI_LoadSound (0, AHIST_DYNAMICSAMPLE, &sample,
                                   wos_actrl)) != 0)
    AHI_FAIL ("Can't load sound");
  if (AHI_ControlAudio (wos_actrl, AHIC_Play, TRUE, TAG_END) != 0)
    AHI_FAIL ("Can't start playback");

  Con_Printf ("AHI audio initialized\n");
  Con_Printf ("AHI mode: %s (%08lx)\n", name, (unsigned long)mode);
  if (wos_ahi_id)
    Con_Printf ("AHI requested mode: %08lx\n", (unsigned long)wos_ahi_id);
  /* CPU-mixing Paula modes cost several ms/frame on the 68k (measured ~4 ms
     at 11025 Hz with "14 bit stereo++ calibrated"); the 68k also runs the
     MiniGL host, so that time lands directly in the frame. AHI DMA modes
     (e.g. Paula:DMA 8 bit stereo) measured ~0.1 ms. */
  if (!strstr ((char *)name, "DMA"))
    Con_Printf ("Note: select an AHI DMA mode for full speed "
                "(Paula mixing modes cost several ms/frame here)\n");
  Con_Printf ("Output: 16bit stereo, %d byte loop (%d Hz mix)\n",
              wos_buflen, (int)mixfreq);
  Con_Printf ("AHI cache flush: %s\n",
              snd_flush_native ? "native dcbf+sync" : "exec CacheClearE");

  AHI_Play (wos_actrl,
            AHIP_BeginChannel, 0,
            AHIP_Freq, speed,
            AHIP_Vol, 0x10000,
            AHIP_Pan, 0x8000,
            AHIP_Sound, 0,
            AHIP_EndChannel, NULL,
            TAG_END);

  AHI_SetEffect (&wos_info, wos_actrl);

  using_ahi = TRUE;
  return 1;
}
#endif /* WOS */

qboolean SNDDMA_Init (void)
{
#ifdef WOS
  return wos_ahi_init ();
#else
  int i, unit = 0;
  struct channel_info *c;
  UBYTE chans[1];

//  printf ("SNDDMA_Init()\n");


  if ((shm = (dma_t *)malloc (sizeof(dma_t))) == NULL)
    Sys_Error ("malloc() failed");
  memset((void*)shm, 0, sizeof(dma_t));

  /* evaluate parameters */
  if (i = COM_CheckParm("-audspeed")) {
    def_smp_speed = (UWORD)Q_atoi(com_argv[i+1]);
  }
  else
    def_smp_speed = (UWORD)(desired_speed);


  if (USE_AHI_ALWAYS || COM_CheckParm ("-ahi")) {
		if ((unit = COM_CheckParm ("-ahiunit"))) {
			if (unit < com_argc-1)
				unit = Q_atoi (com_argv[unit+1]);
			else
				Sys_Error ("You must specify a number after -unit");
		}
#ifdef WOS
    Con_Printf("Using AHI (WarpOS: audio.device path disabled).\n");
#else
    Con_Printf("Using AHI unit %d.\n", unit);
#endif
    using_ahi = TRUE;

    if ((shm->buffer = malloc (AHIBUFFERSIZE)) == NULL)
      AHI_FAIL ("malloc() failed");
    memset (shm->buffer, 0, AHIBUFFERSIZE);

    shm->channels = 2;
    shm->speed = def_smp_speed;
    shm->samplebits = 16;
    shm->samples = AHIBUFFERSIZE / (shm->samplebits / 8);
    shm->submission_chunk = 1;

    if ((AHImp = CreateMsgPort ()) == NULL)
      AHI_FAIL ("CreateMsgPort() failed");
    for (i = 0; i < 2; i++)
      if ((ahi_channel_info[i].AHIio = (ahi_request_t *)CreateIORequest
                                   (AHImp, sizeof(ahi_request_t))) == NULL)
        AHI_FAIL ("CreateIORequest() failed");
    ahi_channel_info[0].AHIio->ahir_Version = 4;
    if ((AHIDevice = OpenDevice (AHINAME, unit,
                                 (struct IORequest *)ahi_channel_info[0].AHIio,
                                 0)) != 0)
      AHI_FAIL ("OpenDevice(ahi.device) failed - is AHI installed?");
    ahidevice_is_open = TRUE;
    AHIBase = (struct Library *)ahi_channel_info[0].AHIio->ahir_Std.io_Device;
    *ahi_channel_info[1].AHIio = *ahi_channel_info[0].AHIio;

    /* Negotiate the sample format before streaming. */
    ahi_type = AHIST_S16S;
    if (ahi_type_test (ahi_type) != AHIE_OK) {
      Con_Printf ("AHI: AHIST_S16S rejected (io_Error=%d), sound disabled\n",
                  (int)ahi_channel_info[0].AHIio->ahir_Std.io_Error);
      SNDDMA_Shutdown ();
      return 0;
    }
    Con_Printf ("AHI audio initialized: unit %d, %d Hz, 16-bit stereo\n",
                unit, (int)shm->speed);

    twice_real_speed = 2.0 * (double)shm->speed;
    ahi_playtime = ((double)(AHIBUFFERSIZE >> 2)) / (double)shm->speed;
    //printf ("ahi_playtime = %f\n", ahi_playtime);

    ahi_channel_info[0].starttime = Sys_FloatTime ();
    start_ahi_sound (&ahi_channel_info[0], shm->buffer, AHIBUFFERSIZE, NULL);
    //printf ("start[0] = %f  %f\n", ahi_channel_info[0].starttime,
    //                               ahi_channel_info[0].starttime + ahi_playtime);
    ahi_channel_info[1].starttime = ahi_channel_info[0].starttime + ahi_playtime;
    start_ahi_sound (&ahi_channel_info[1], shm->buffer, AHIBUFFERSIZE,
                     ahi_channel_info[0].AHIio);
    //printf ("start[1] = %f  %f\n", ahi_channel_info[1].starttime,
    //                               ahi_channel_info[1].starttime + ahi_playtime);
    which_buffer = 0;

  } else { /* audio.device */

    Con_Printf("Using audio.device.\n");
    using_ahi = FALSE;

    if ((shm->buffer = AllocMem (BUFFERSIZE, MEMF_CHIP | MEMF_CLEAR)) == NULL)
      Sys_Error ("Out of CHIP memory for sound");
//    memset(shm->buffer, 0x80, BUFFERSIZE);

//    printf ("Sound buffer at 0x%08x\n", shm->buffer);

    shm->channels = 2;
//    shm->speed = desired_speed;
    shm->speed = def_smp_speed; 
    shm->samplebits = 8;
//    shm->samples = BUFFERSIZE / (shm->samplebits / 8);
    shm->samples = BUFFERSIZE;
    shm->submission_chunk = 1;

    if ((audio_mp = CreateMsgPort ()) == NULL ||
        (audio_io = (struct IOAudio *)AllocMem(sizeof(struct IOAudio),
                                             MEMF_PUBLIC | MEMF_CLEAR)) == NULL)
      Sys_Error ("CreateMsgPort() or AllocMem() failed");

    chans[0] = (1 << shm->channels) - 1; /* shm->channels Amiga audio channels */
    audio_io->ioa_Request.io_Message.mn_ReplyPort = audio_mp;
    audio_io->ioa_Request.io_Message.mn_Node.ln_Pri = 127;
    audio_io->ioa_AllocKey = 0;
    audio_io->ioa_Data = chans;
    audio_io->ioa_Length = sizeof(chans);

    if (OpenDevice (AUDIONAME, 0, (struct IORequest *)audio_io, 0) != 0)
      Sys_Error ("OpenDevice(\"audio.device\") failed");
    audio_is_open = TRUE;

    for (i = 0; i < shm->channels; i++) {
      c = &channel_info[i];
      if ((c->audio_mp = CreateMsgPort ()) == NULL ||
          (c->audio_io = (struct IOAudio *)AllocMem(sizeof(struct IOAudio),
                                             MEMF_PUBLIC | MEMF_CLEAR)) == NULL)
        Sys_Error ("CreateMsgPort() or AllocMem() failed");
      *c->audio_io = *audio_io;
      c->audio_io->ioa_Request.io_Message.mn_ReplyPort = c->audio_mp;
      c->audio_io->ioa_Request.io_Unit = (struct Unit *)(1 << i);
    }

    if (GfxBase->DisplayFlags & PAL)
      clock_constant = 3546895;   /* PAL */
    else
      clock_constant = 3579545;   /* NTSC */

    period = ((clock_constant << 1) + shm->speed) / ((shm->speed) << 1);

    twice_real_speed = 2.0 * ((double)clock_constant) / (double)period;

    startsound (0, shm->buffer, BUFFERSIZE >> 1);
    startsound (1, shm->buffer + (BUFFERSIZE >> 1), BUFFERSIZE >> 1);
  }

  return 1;
#endif /* WOS */
}

/**********************************************************************/

int SNDDMA_GetDMAPos (void)
{
#ifdef WOS
  if (shm == NULL)
    return 0;
  /* AHIET_CHANNELINFO hook value: current offset in sample frames. */
  return (shm->samplepos = (int)(wos_effhook.h_Data) * shm->channels);
#else
  if (shm == NULL || shm->buffer == NULL)
    return 0;

  if (using_ahi) {

    if (!ahi_channel_info[0].sound_in_progress)
      shm->samplepos = 0;
    else {
      struct ahi_channel_info *c, *c2;
      double now;

      now = Sys_FloatTime ();
      c = &ahi_channel_info[which_buffer];
      if (now >= c->starttime + ahi_playtime - 0.1 &&
          CheckIO ((struct IORequest *)c->AHIio)) {
        //printf ("finished[%d] = %f  %f\n", which_buffer, now,
        //                                   now - (c->starttime + ahi_playtime));
        c2 = &ahi_channel_info[1 - which_buffer];
        if (c2->starttime > now) {
          c2->starttime = now;
          //printf ("changed[%d] = %f  %f\n", 1 - which_buffer, c2->starttime,
          //                                  c2->starttime + ahi_playtime);
        }
        WaitPort (AHImp);
        GetMsg (AHImp);
        c->sound_in_progress = FALSE;
        if ((now = Sys_FloatTime ()) > (c->starttime =
                                        c2->starttime + ahi_playtime))
          c->starttime = now;
        start_ahi_sound (c, shm->buffer, AHIBUFFERSIZE,
                         ahi_channel_info[1 - which_buffer].AHIio);
        //printf ("start[%d] = %f  %f\n", which_buffer, c->starttime,
        //                                c->starttime + ahi_playtime);
        which_buffer = 1 - which_buffer;
        c = &ahi_channel_info[which_buffer];
      }
      shm->samplepos = ((int)((now + ahi_playtime - c->starttime) *
                              twice_real_speed + 0.5)) & (AHIBUFFERSIZE - 1);
    }

  } else {  /* audio.device */

    if (!channel_info[0].sound_in_progress)
      shm->samplepos = 0;
    else
      shm->samplepos = ((int)((Sys_FloatTime() - channel_info[0].starttime)
                              * twice_real_speed + 0.5))
                       & (BUFFERSIZE - 1);

  }

  return shm->samplepos;
#endif /* WOS */
}

/**********************************************************************/

void SNDDMA_Shutdown (void)
{
#ifdef WOS
  /* Q3 order: cancel effect, stop playback, unload sound, free buffer,
     free audio, close device, delete request, delete port. */
  if (wos_actrl) {
    wos_info.cinfo.ahie_Effect = AHIET_CHANNELINFO | AHIET_CANCEL;
    AHI_SetEffect (&wos_info, wos_actrl);
    AHI_ControlAudio (wos_actrl, AHIC_Play, FALSE, TAG_END);
  }
  if (wos_ahi_rc == 0 && wos_actrl) {
    AHI_UnloadSound (0, wos_actrl);
    wos_ahi_rc = 1;
  }
  if (wos_dmabuf) {
    free (wos_dmabuf);
    wos_dmabuf = NULL;
  }
  if (wos_actrl) {
    AHI_FreeAudio (wos_actrl);
    wos_actrl = NULL;
  }
  if (AHIDevice == 0) {
    CloseDevice ((struct IORequest *)wos_ahi_io);
    AHIDevice = -1;
  }
  if (wos_ahi_io) {
    DeleteIORequest ((struct IORequest *)wos_ahi_io);
    wos_ahi_io = NULL;
  }
  if (AHImp) {
    DeleteMsgPort (AHImp);
    AHImp = NULL;
  }
  if (shm) {
    free (shm);
    shm = NULL;
  }
  using_ahi = FALSE;
#else
  int i;

//  printf ("SNDDMA_Shutdown()\n");

  for (i = 0; i < 2; i++)
    stop_ahi_sound (&ahi_channel_info[i]);
  if (ahidevice_is_open) {
    CloseDevice ((struct IORequest *)ahi_channel_info[0].AHIio);
    ahidevice_is_open = FALSE;
  }
  for (i = 0; i < 2; i++) {
    if (ahi_channel_info[i].AHIio != NULL) {
      DeleteIORequest ((struct IORequest *)ahi_channel_info[i].AHIio);
      ahi_channel_info[i].AHIio = NULL;
    }
  }
  if (AHImp != NULL) {
    DeleteMsgPort (AHImp);
    AHImp = NULL;
  }
  if (audio_is_open) {
    if (shm != NULL) {
      for (i = 0; i < shm->channels; i++)
        stopsound (i);
      audio_io->ioa_Request.io_Unit = (struct Unit *)
                       ((1 << shm->channels) - 1);  /* free shm->channels channels */
    }
    CloseDevice ((struct IORequest *)audio_io);
    audio_is_open = FALSE;
  }
  for (i = 0; i < MAXNUMCHANNELS; i++) {
    if (channel_info[i].audio_io != NULL) {
      FreeMem (channel_info[i].audio_io, sizeof(struct IOAudio));
      channel_info[i].audio_io = NULL;
    }
    if (channel_info[i].audio_mp != NULL) {
      DeleteMsgPort (channel_info[i].audio_mp);
      channel_info[i].audio_mp = NULL;
    }
  }
  if (audio_io != NULL) {
    FreeMem (audio_io, sizeof(struct IOAudio));
    audio_io = NULL;
  }
  if (audio_mp != NULL) {
    DeleteMsgPort (audio_mp);
    audio_mp = NULL;
  }
  if (shm != NULL) {
    if (shm->buffer != NULL) {
      if (using_ahi)
//#if defined(__SASC) && defined(__PPC__)
//        PPCFreeMem (shm->buffer, AHIBUFFERSIZE);
//#else
        free (shm->buffer);
//#endif
      else
        FreeMem (shm->buffer, BUFFERSIZE);
      shm->buffer = NULL;
    }
    free ((char *)shm);
    shm = NULL;
  }
#endif /* WOS */
}

/**********************************************************************/

/* Called once per S_Update_ after S_PaintChannels() has mixed new audio
   into the ring. The AHI driver reads that memory from the 68k side; the
   68060 does not snoop the PPC data cache, so write the freshly painted
   range back to memory. Without this the driver can play stale samples
   (the classic "dma/cache sound issues"). */
void SNDDMA_Submit (void)
{
  static int flush_time;        /* paintedtime at the previous flush */
  int t0, t1, bpf, len, start, first, ringbytes;

  if (!using_ahi || shm == NULL || shm->buffer == NULL)
    return;

  t0 = flush_time;
  t1 = paintedtime;
  if (t1 == t0)
    return;
  if (t1 < t0) {                /* paintedtime reset (level change) */
    flush_time = t1;
    return;
  }

  bpf = shm->channels * (shm->samplebits >> 3);         /* bytes per frame */
  if (bpf <= 0)
    return;

  /* Ring size in bytes. The mixer wraps at shm->samples (16-bit units), so
     the byte ring is samples * bytes-per-sample. Do NOT use AHIBUFFERSIZE:
     that is the old low-level 256 KB constant while the WOS high-level ring
     is wos_buflen (>= 64 KB). Masking with the wrong size made the flush
     miss the painted range once t0*bpf passed wos_buflen (~1.5 s of
     painting); CacheClearE still worked because the LP stub flushes the
     whole PPC cache, but a native flush would have gone silent. */
  ringbytes = shm->samples * (shm->samplebits >> 3);
  if (ringbytes <= 0)
    return;

  len = (t1 - t0) * bpf;
  if (len > ringbytes)
    len = ringbytes;
  start = (t0 * bpf) & (ringbytes - 1);
  first = ringbytes - start;
  if (first > len)
    first = len;

  SND_FlushRange ((UBYTE *)shm->buffer + start, first);
  if (len > first)
    SND_FlushRange (shm->buffer, len - first);

  flush_time = t1;
}

/**********************************************************************/
