/*
 * wos_ahi.c - WarpOS/PPC AHI low-level playback probe (staged + traced).
 *
 * Purpose: find out exactly how far a PPC task can drive ahi.device on the
 * MOS2WOS/ppclibemu WarpOS stack, and prove sample playback works:
 *   - CreateMsgPort/CreateIORequest
 *   - OpenDevice("ahi.device", unit, v4)
 *   - CMD_WRITE + WaitIO (interrupt-driven completion)
 *   - CMD_WRITE + CheckIO polling (no WaitIO)
 *   - linked double-buffer streaming (the game's scheme)
 *
 * Every stage writes a line to Work:wosbuild/wos_ahi_trace.log with its own
 * open/append/close, so if the machine wedges the last line names the hang.
 *
 * Usage (Amiga shell, after Elf2Exe2 conversion):
 *   wos_ahi             ; all stages, auto format
 *   wos_ahi -step 2     ; stop after the WaitIO stage
 *   wos_ahi 0           ; force AHI unit 0
 *
 * Exit codes: 0 = full stream OK, 1 = partial, 20 = AHI unusable.
 *
 * Build: make -f Makefile.GCCAmigaWOS_probe_io PROBE=wos_ahi
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/io.h>
#include <exec/execbase.h>      /* CACRF_ClearD */
#include <devices/ahi.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include "../../wos_ahi_abi.h"

#define PROBE_RATE      11025
#define PROBE_FRAMES    16384
#define PROBE_S16LEN    (PROBE_FRAMES * 4)
#define PROBE_SECONDS   3
#define TRACE_FILE      "Work:wosbuild/wos_ahi_trace.log"

static struct MsgPort    *AHImp;
static struct wos_AHIRequest *AHIreq[2];
static struct Library    *AHIBase;
static UBYTE             *tonebuf;
static ULONG              tonelen;
static ULONG              tonetype;
static int                max_step = 9;
static BOOL               ahidevice_open = FALSE;

/* ---------------- tracing (per-line, crash-durable) ---------------- */

static void trace (const char *s)
{
  BPTR f = Open ((STRPTR)TRACE_FILE, MODE_OLDFILE);

  if (!f)
    f = Open ((STRPTR)TRACE_FILE, MODE_NEWFILE);
  if (f) {
    Seek (f, 0, OFFSET_END);
    Write (f, (APTR)s, (LONG)strlen (s));
    Write (f, (APTR)"\n", 1);
    Close (f);
  }
}

static void tracef (const char *fmt, ...)
{
  char buf[160];
  va_list ap;

  va_start (ap, fmt);
  vsprintf (buf, fmt, ap);
  va_end (ap);
  trace (buf);
}

static void stage (int n, const char *what, LONG value)
{
  char buf[160];

  sprintf (buf, "stage %d: %s (%ld)", n, what, value);
  trace (buf);
  printf ("  %s\n", buf);
}

static double probe_time (void)
{
  struct DateStamp ds;

  DateStamp (&ds);
  return (double)ds.ds_Days * 86400.0 + (double)ds.ds_Minute * 60.0
         + (double)ds.ds_Tick / 50.0;
}

static const char *ahi_error_name (ULONG e)
{
  switch (e) {
    case AHIE_OK:            return "OK";
    case AHIE_NOMEM:         return "NOMEM";
    case AHIE_BADSOUNDTYPE:  return "BADSOUNDTYPE";
    case AHIE_BADSAMPLETYPE: return "BADSAMPLETYPE";
    case AHIE_ABORTED:       return "ABORTED";
    case AHIE_UNKNOWN:       return "UNKNOWN";
    case AHIE_HALFDUPLEX:    return "HALFDUPLEX";
    default:                 return "?";
  }
}

static void fill_tone (ULONG type)
{
  int i;
  const int cycles = 655;
  const double step = 2.0 * 3.14159265358979323846 * (double)cycles
                      / (double)PROBE_FRAMES;

  if (type == AHIST_S16S) {
    WORD *p = (WORD *)tonebuf;

    for (i = 0; i < PROBE_FRAMES; i++) {
      WORD v = (WORD)(sin (step * (double)i) * 12000.0);
      *p++ = v;
      *p++ = v;
    }
  }
  else {
    BYTE *p = (BYTE *)tonebuf;

    for (i = 0; i < PROBE_FRAMES; i++) {
      BYTE v = (BYTE)(sin (step * (double)i) * 100.0);
      *p++ = v;
      *p++ = v;
    }
  }
}

static void ahi_prep (struct wos_AHIRequest *r, struct wos_AHIRequest *link,
                      ULONG type, ULONG len)
{
  r->ahir_Std.io_Command = CMD_WRITE;
  r->ahir_Std.io_Flags = 0;
  r->ahir_Std.io_Message.mn_Node.ln_Pri = 0;
  r->ahir_Std.io_Data = tonebuf;
  r->ahir_Std.io_Length = len;
  r->ahir_Type = type;
  r->ahir_Frequency = PROBE_RATE;
  r->ahir_Volume = 0x10000;
  r->ahir_Position = 0x8000;
  r->ahir_Link = link;
  CacheClearE (tonebuf, len, CACRF_ClearD);
}

static void ahi_submit (struct wos_AHIRequest *r, struct wos_AHIRequest *link,
                        ULONG type, ULONG len)
{
  ahi_prep (r, link, type, len);
  SendIO ((struct IORequest *)r);
}

/* ---------------- staged tests ---------------- */

static int stage1_port_and_io (void)
{
  int i;

  stage (0, "alive", 0);

  AHImp = CreateMsgPort ();
  tracef ("stage 1: CreateMsgPort -> %ld", (LONG)(AHImp != NULL));
  if (!AHImp)
    return -1;

  for (i = 0; i < 2; i++) {
    AHIreq[i] = (struct wos_AHIRequest *)CreateIORequest (AHImp,
                                        sizeof (struct wos_AHIRequest));
    tracef ("stage 1: CreateIORequest[%d] -> %ld", i, (LONG)(AHIreq[i] != NULL));
    if (!AHIreq[i])
      return -1;
  }
  stage (1, "port + io requests", 0);
  return 0;
}

static int stage2_open (int unit)
{
  AHIreq[0]->ahir_Version = 4;
  trace ("stage 2: calling OpenDevice(ahi.device, v4)");
  if (OpenDevice (AHINAME, unit, (struct IORequest *)AHIreq[0], 0)) {
    trace ("stage 2: OpenDevice FAILED");
    printf ("  OpenDevice(ahi.device) failed\n");
    return -1;
  }
  ahidevice_open = TRUE;
  AHIBase = (struct wos_Library *)AHIreq[0]->ahir_Std.io_Device;
  *AHIreq[1] = *AHIreq[0];
  tracef ("stage 2: OpenDevice OK, version %ld", (LONG)AHIBase->lib_Version);
  stage (2, "OpenDevice", (LONG)AHIBase->lib_Version);
  return 0;
}

static ULONG stage3_write_waitio (ULONG type)
{
  struct wos_AHIRequest *r = AHIreq[0];

  trace ("stage 3: SendIO + WaitIO (4096 bytes)");
  ahi_prep (r, NULL, type, 4096);
  SendIO ((struct IORequest *)r);
  trace ("stage 3: sent, calling WaitIO");
  WaitIO ((struct IORequest *)r);
  tracef ("stage 3: WaitIO returned, io_Error=%ld",
          (LONG)r->ahir_Std.io_Error);
  stage (3, "WaitIO io_Error", (LONG)r->ahir_Std.io_Error);
  return r->ahir_Std.io_Error;
}

static ULONG stage4_write_poll (ULONG type)
{
  struct wos_AHIRequest *r = AHIreq[0];
  double t0;

  trace ("stage 4: SendIO + CheckIO polling (no WaitIO)");
  ahi_prep (r, NULL, type, 4096);
  SendIO ((struct IORequest *)r);
  t0 = probe_time ();
  while (probe_time () - t0 < 5.0) {
    if (CheckIO ((struct IORequest *)r)) {
      GetMsg (AHImp);
      tracef ("stage 4: completed, io_Error=%ld", (LONG)r->ahir_Std.io_Error);
      stage (4, "poll io_Error", (LONG)r->ahir_Std.io_Error);
      return r->ahir_Std.io_Error;
    }
    Delay (1);
  }
  trace ("stage 4: TIMEOUT after 5 s, aborting");
  AbortIO ((struct IORequest *)r);
  WaitIO ((struct IORequest *)r);
  stage (4, "poll TIMEOUT", -1);
  return (ULONG)-1;
}

static int stage5_stream (ULONG type, int seconds, ULONG *out_count)
{
  double t0;
  int n0 = 0, n1 = 0;

  tonetype = type;
  tonelen = (type == AHIST_S16S) ? PROBE_S16LEN : PROBE_S16LEN / 2;
  fill_tone (type);

  trace ("stage 5: streaming (linked double buffer)");
  ahi_submit (AHIreq[0], NULL, type, tonelen);
  ahi_submit (AHIreq[1], AHIreq[0], type, tonelen);

  t0 = probe_time ();
  while (probe_time () - t0 < (double)seconds) {
    if (CheckIO ((struct IORequest *)AHIreq[0])) {
      GetMsg (AHImp);
      ahi_submit (AHIreq[0], AHIreq[1], type, tonelen);
      n0++;
    }
    if (CheckIO ((struct IORequest *)AHIreq[1])) {
      GetMsg (AHImp);
      ahi_submit (AHIreq[1], AHIreq[0], type, tonelen);
      n1++;
    }
    Delay (1);
  }

  AbortIO ((struct IORequest *)AHIreq[0]);
  AbortIO ((struct IORequest *)AHIreq[1]);
  WaitIO ((struct IORequest *)AHIreq[0]);
  WaitIO ((struct IORequest *)AHIreq[1]);

  *out_count = (ULONG)(n0 + n1);
  tracef ("stage 5: done, completions=%ld", (LONG)*out_count);
  stage (5, "stream completions", (LONG)*out_count);
  return (n0 > 0 || n1 > 0) ? 0 : -1;
}

int main (int argc, char **argv)
{
  int unit = 0;
  int i;
  ULONG err;

  setvbuf (stdout, NULL, _IONBF, 0);
  trace ("=== wos_ahi probe start ===");
  printf ("wos_ahi: AHI staged probe (trace: %s)\n", TRACE_FILE);

  for (i = 1; i < argc; i++) {
    if (!strcmp (argv[i], "-step") && i + 1 < argc)
      max_step = atoi (argv[i + 1]);
    else if (argv[i][0] >= '0' && argv[i][0] <= '9')
      unit = atoi (argv[i]);
  }
  tracef ("max_step=%ld", (LONG)max_step);

  if (stage1_port_and_io () != 0) {
    trace ("FAIL: port/io setup");
    return 20;
  }
  if (max_step <= 1) { trace ("stopping after stage 1"); return 0; }

  if (stage2_open (unit) != 0) {
    trace ("FAIL: OpenDevice");
    return 20;
  }
  if (AHIBase->lib_IdString)
    printf ("  driver: %s\n", (char *)AHIBase->lib_IdString);
  if (max_step <= 2) {
    trace ("stopping after stage 2 (closing device)");
    CloseDevice ((struct IORequest *)AHIreq[0]);
    return 0;
  }

  tonebuf = (UBYTE *)AllocMem (PROBE_S16LEN, MEMF_PUBLIC | MEMF_CLEAR);
  if (!tonebuf) {
    trace ("FAIL: AllocMem");
    return 20;
  }
  tracef ("tone buffer allocated at %08lx", (LONG)(ULONG)tonebuf);

  err = stage3_write_waitio (AHIST_S16S);
  printf ("  WaitIO test: io_Error=%lu (%s)\n", err, ahi_error_name (err));
  if (max_step <= 3) {
    trace ("stopping after stage 3");
    CloseDevice ((struct IORequest *)AHIreq[0]);
    return 0;
  }

  err = stage4_write_poll (AHIST_S16S);
  printf ("  poll test: io_Error=%lu (%s)\n", err, ahi_error_name (err));
  if (max_step <= 4) {
    trace ("stopping after stage 4");
    CloseDevice ((struct IORequest *)AHIreq[0]);
    return 0;
  }

  if (err == AHIE_OK) {
    ULONG n = 0;
    printf ("  streaming for %d s (listen)...\n", PROBE_SECONDS);
    if (stage5_stream (AHIST_S16S, PROBE_SECONDS, &n) == 0) {
      printf ("  stream OK: %lu buffer completions\n", n);
      trace ("=== probe end: PASS ===");
      return 0;
    }
    printf ("  stream FAILED\n");
    trace ("=== probe end: stream failed ===");
    return 1;
  }

  trace ("=== probe end: mode not usable ===");
  printf ("wos_ahi: S16S not usable (io_Error=%lu)\n", err);
  return 20;
}
