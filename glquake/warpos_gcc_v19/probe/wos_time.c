/*
 * wos_time.c - measure the cost of PPC->68k LP calls vs native PPC timing
 * and cache instructions on the MOS2WOS/ppclibemu WarpOS stack.
 *
 * Context: every powerup/ppcinline LP call sets IF_CACHEFLUSHALL, so each
 * OS call (DateStamp, CacheClearE, ...) also flushes the PPC caches. The
 * AHI path adds a few of those per frame; this probe quantifies them.
 *
 * Build: make -f Makefile.GCCAmigaWOS_probe_io PROBE=wos_time
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <exec/execbase.h>      /* CACRF_ClearD */
#include <proto/exec.h>
#include <proto/dos.h>

#include <stdio.h>
#include <string.h>

#define N_DATE   200
#define N_FLUSH  200
#define N_TB     1000000L
#define FLUSH_LEN 8192

static double probe_time (void)
{
  struct DateStamp ds;

  DateStamp (&ds);
  return (double)ds.ds_Days * 86400.0 + (double)ds.ds_Minute * 60.0
         + (double)ds.ds_Tick / 50.0;
}

static unsigned long long mftb (void)
{
  unsigned long hi, lo, hi2;

  do {
    __asm__ __volatile__ ("mftbu %0" : "=r" (hi));
    __asm__ __volatile__ ("mftb  %0" : "=r" (lo));
    __asm__ __volatile__ ("mftbu %0" : "=r" (hi2));
  } while (hi != hi2);
  return ((unsigned long long)hi << 32) | lo;
}

static void dcbf_range (void *addr, unsigned long len)
{
  unsigned long a = (unsigned long)addr & ~31UL;
  unsigned long end = ((unsigned long)addr + len + 31UL) & ~31UL;

  for (; a < end; a += 32)
    __asm__ __volatile__ ("dcbf 0,%0" : : "r" (a) : "memory");
  __asm__ __volatile__ ("sync" ::: "memory");
}

int main (void)
{
  struct DateStamp ds;
  unsigned char *buf;
  double t0, t1;
  unsigned long long tb0, tb1;
  int i;

  setvbuf (stdout, NULL, _IONBF, 0);
  printf ("wos_time: LP-call / cache / TB cost probe\n");

  /* 1. DateStamp via the LP stub (IF_CACHEFLUSHALL on every call) */
  t0 = probe_time ();
  for (i = 0; i < N_DATE; i++)
    DateStamp (&ds);
  t1 = probe_time ();
  printf ("  DateStamp (LP, flush-all):  %6.3f s / %d = %6.2f ms per call\n",
          t1 - t0, N_DATE, (t1 - t0) * 1000.0 / N_DATE);

  /* 2. native time base read */
  tb0 = mftb ();
  t0 = probe_time ();
  for (i = 0; i < N_TB; i++)
    (void)mftb ();
  t1 = probe_time ();
  tb1 = mftb ();
  printf ("  mftb (native):              %6.3f s / %ld = %6.3f us per call\n",
          t1 - t0, N_TB, (t1 - t0) * 1e6 / N_TB);

  /* TB frequency from this interval */
  if (t1 > t0)
    printf ("  time base frequency:        %.2f MHz\n",
            (double)(tb1 - tb0) / (t1 - t0) / 1e6);

  /* 3. native targeted dcbf over 8 KB */
  buf = (unsigned char *)AllocMem (FLUSH_LEN + 64, MEMF_PUBLIC | MEMF_CLEAR);
  if (buf) {
    t0 = probe_time ();
    for (i = 0; i < N_FLUSH; i++)
      dcbf_range (buf, FLUSH_LEN);
    t1 = probe_time ();
    printf ("  dcbf 8KB (native):          %6.3f s / %d = %6.3f ms per flush\n",
            t1 - t0, N_FLUSH, (t1 - t0) * 1000.0 / N_FLUSH);

    /* 4. CacheClearE via the LP stub (emulated; IF_CACHEFLUSHALL anyway) */
    t0 = probe_time ();
    for (i = 0; i < N_FLUSH; i++)
      CacheClearE (buf, FLUSH_LEN, CACRF_ClearD);
    t1 = probe_time ();
    printf ("  CacheClearE 8KB (LP):       %6.3f s / %d = %6.3f ms per call\n",
            t1 - t0, N_FLUSH, (t1 - t0) * 1000.0 / N_FLUSH);
    FreeMem (buf, FLUSH_LEN + 64);
  }

  printf ("wos_time: done\n");
  return 0;
}
