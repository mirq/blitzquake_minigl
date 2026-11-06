/*
 * wos_cache.c - PPC/68k cache coherency probe for the AHI sample ring.
 *
 * Question this answers: after the PPC pushes sample data with native
 * dcbf+sync, does the 68k side (which runs the AHI driver) actually see it?
 * And does the 68060's own data cache then serve stale lines on the next
 * read (the suspected reason the native-flush experiment produced silence)?
 *
 * Method: the PPC writes a library-name string into a buffer, pushes it,
 * then calls exec.library/OpenLibrary(buffer, 0) over the LP path.
 * OpenLibrary runs on the 68k and reads the string through the 68k cache,
 * so the returned base says exactly what the 68k saw. No 68k writes are
 * involved (their visibility would need its own flush to be observed).
 *
 *   T1 cold visibility : first 68k read after dcbf+sync
 *   T2 stale read      : second read after the PPC rewrote + dcbf+sync
 *                        (returning the old library => 68k served stale)
 *   T3 control         : CacheClearE instead of dcbf (known-good path)
 *
 * Repeated for fast RAM and chip RAM so we know whether chip RAM is
 * cache-inhibited on the 68k side on this machine.
 *
 * Build: make -f Makefile.GCCAmigaWOS_probe_io PROBE=wos_cache
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>      /* CACRF_ClearD */
#include <proto/exec.h>
#include <proto/dos.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define STRBUF 64               /* two 32-byte cache lines */
#define TRACE_FILE "DH2:wosbuild/wos_cache_trace.log"

struct Library *UtilityBase = NULL;
struct Library *GraphicsBase = NULL;

/* per-line open/append/close: survives a wedged PPC task */
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

static const char *name_a = "utility.library";
static const char *name_b = "graphics.library";

static void dcbf_range (void *addr, unsigned long len)
{
  unsigned long a = (unsigned long)addr & ~31UL;
  unsigned long end = ((unsigned long)addr + len + 31UL) & ~31UL;

  for (; a < end; a += 32)
    __asm__ __volatile__ ("dcbf 0,%0" : : "r" (a) : "memory");
  __asm__ __volatile__ ("sync" ::: "memory");
}

static void dcbi_range (void *addr, unsigned long len)
{
  unsigned long a = (unsigned long)addr & ~31UL;
  unsigned long end = ((unsigned long)addr + len + 31UL) & ~31UL;

  for (; a < end; a += 32)
    __asm__ __volatile__ ("dcbi 0,%0" : : "r" (a) : "memory");
  __asm__ __volatile__ ("sync" ::: "memory");
}

/* Ask the 68k to read the buffer as a library name. */
static struct Library *open_via_68k (UBYTE *buf)
{
  struct Library *b;

  b = OpenLibrary ((STRPTR)buf, 0);
  return b;
}

static const char *identify (struct Library *b)
{
  if (b == NULL)
    return "NULL";
  if (b == UtilityBase)
    return "utility";
  if (b == GraphicsBase)
    return "graphics";
  return "other";
}

static void probe_buf (const char *what, UBYTE *buf)
{
  struct Library *b;

  printf ("--- %s @ %08lx ---\n", what, (unsigned long)buf);
  tracef ("--- %s @ %08lx ---", what, (unsigned long)buf);

  /* T1: cold 68k read after native dcbf */
  strcpy ((char *)buf, name_a);
  dcbf_range (buf, STRBUF);
  trace ("T1: calling OpenLibrary (dcbf'd name_a)");
  b = open_via_68k (buf);
  tracef ("T1: -> %s", identify (b));
  printf ("  T1 cold dcbf visible     : %s (%s)\n",
          b == UtilityBase ? "PASS" : "FAIL", identify (b));
  if (b)
    CloseLibrary (b);

  /* T2: the 68k has now cached that name; PPC rewrites + dcbf, read again.
     Getting "utility" back means the 68k served the stale line. */
  strcpy ((char *)buf, name_a);
  dcbf_range (buf, STRBUF);
  trace ("T2a: OpenLibrary (prime 68k cache)");
  b = open_via_68k (buf);               /* primes 68k cache with name_a */
  if (b)
    CloseLibrary (b);
  strcpy ((char *)buf, name_b);
  dcbf_range (buf, STRBUF);
  trace ("T2b: OpenLibrary (name_b after dcbf)");
  b = open_via_68k (buf);
  tracef ("T2: -> %s", identify (b));
  printf ("  T2 repeat read fresh     : %s (%s)\n",
          b == GraphicsBase ? "PASS" : "STALE", identify (b));
  if (b)
    CloseLibrary (b);

  /* T3: control, CacheClearE (the proven path) */
  strcpy ((char *)buf, name_a);
  CacheClearE (buf, STRBUF, CACRF_ClearD);
  trace ("T3: OpenLibrary (CacheClearE'd name_a)");
  b = open_via_68k (buf);
  tracef ("T3: -> %s", identify (b));
  printf ("  T3 CacheClearE control   : %s (%s)\n",
          b == UtilityBase ? "PASS" : "FAIL", identify (b));
  if (b)
    CloseLibrary (b);

  /* T4: after T3 the 68k cache was cleared for this range; rewrite with
     dcbf and read once more - the mixed sequence. */
  strcpy ((char *)buf, name_b);
  dcbf_range (buf, STRBUF);
  trace ("T4: OpenLibrary (dcbf after exec clear)");
  b = open_via_68k (buf);
  tracef ("T4: -> %s", identify (b));
  printf ("  T4 dcbf after exec clear : %s (%s)\n",
          b == GraphicsBase ? "PASS" : "FAIL", identify (b));
  if (b)
    CloseLibrary (b);
  trace ("probe_buf done");
}

/* Timing: native dcbf vs LP CacheClearE over game-sized ranges. */
static void probe_timing (void)
{
  UBYTE *buf;
  struct DateStamp ds;
  double t0, t1;
  int i;
  const int n = 1000;

  buf = (UBYTE *)AllocMem (65536 + 64, MEMF_PUBLIC | MEMF_CLEAR);
  if (!buf)
    return;

  DateStamp (&ds);
  t0 = (double)ds.ds_Days * 86400.0 + (double)ds.ds_Minute * 60.0
       + (double)ds.ds_Tick / 50.0;
  for (i = 0; i < n; i++)
    dcbf_range (buf, 1024);
  DateStamp (&ds);
  t1 = (double)ds.ds_Days * 86400.0 + (double)ds.ds_Minute * 60.0
       + (double)ds.ds_Tick / 50.0;
  printf ("  dcbf   1KB x%d: %.3f ms per call\n", n, (t1 - t0) * 1000.0 / n);

  DateStamp (&ds);
  t0 = (double)ds.ds_Days * 86400.0 + (double)ds.ds_Minute * 60.0
       + (double)ds.ds_Tick / 50.0;
  for (i = 0; i < n; i++)
    CacheClearE (buf, 1024, CACRF_ClearD);
  DateStamp (&ds);
  t1 = (double)ds.ds_Days * 86400.0 + (double)ds.ds_Minute * 60.0
       + (double)ds.ds_Tick / 50.0;
  printf ("  exec   1KB x%d: %.3f ms per call\n", n, (t1 - t0) * 1000.0 / n);

  DateStamp (&ds);
  t0 = (double)ds.ds_Days * 86400.0 + (double)ds.ds_Minute * 60.0
       + (double)ds.ds_Tick / 50.0;
  for (i = 0; i < n; i++)
    dcbf_range (buf, 65536);
  DateStamp (&ds);
  t1 = (double)ds.ds_Days * 86400.0 + (double)ds.ds_Minute * 60.0
       + (double)ds.ds_Tick / 50.0;
  printf ("  dcbf  64KB x%d: %.3f ms per call\n", n, (t1 - t0) * 1000.0 / n);

  FreeMem (buf, 65536 + 64);
}

int main (void)
{
  UBYTE *fast, *chip;

  setvbuf (stdout, NULL, _IONBF, 0);
  trace ("=== wos_cache start ===");
  printf ("wos_cache: PPC/68k coherency probe (68k reader = OpenLibrary)\n");

  UtilityBase = OpenLibrary ("utility.library", 0);
  GraphicsBase = OpenLibrary ("graphics.library", 0);
  tracef ("bases: utility=%08lx graphics=%08lx",
          (unsigned long)UtilityBase, (unsigned long)GraphicsBase);
  printf ("bases: utility=%08lx graphics=%08lx\n",
          (unsigned long)UtilityBase, (unsigned long)GraphicsBase);
  if (!UtilityBase || !GraphicsBase) {
    printf ("cannot open the reference libraries - aborting\n");
    return 20;
  }

  trace ("alloc fast/chip");
  fast = (UBYTE *)AllocMem (256, MEMF_PUBLIC | MEMF_CLEAR);
  chip = (UBYTE *)AllocMem (256, MEMF_CHIP | MEMF_CLEAR);
  tracef ("fast=%08lx chip=%08lx", (unsigned long)fast, (unsigned long)chip);
  printf ("fast=%08lx chip=%08lx\n", (unsigned long)fast, (unsigned long)chip);

  if (fast)
    probe_buf ("FAST RAM (AllocMem MEMF_PUBLIC)", fast);
  if (chip)
    probe_buf ("CHIP RAM (AllocMem MEMF_CHIP)", chip);

  trace ("timing start");
  printf ("timing:\n");
  probe_timing ();
  trace ("=== wos_cache done ===");

  printf ("wos_cache: done\n");
  return 0;
}
