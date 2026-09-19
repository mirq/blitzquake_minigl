// frame_profile.c -- WarpOS PPC frame profiler (plan 2026-09-20 phase 1).
// See frame_profile.h for the contract. All timing is mftb-based; the only
// slow call happens once in FP_Init (a 0.5 s Delay calibration) and the
// one-shot FP_AutoDump after the measured interval.

#include "quakedef.h"
#include "frame_profile.h"

#if defined(WOS) && defined(__PPC__)

#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

#define FP_MAX_DEPTH   8
#define FP_WORST_SLOTS 16
#define FP_NAME_MAX    12

typedef struct {
	int     frame;
	ULONG   total;                      // whole-frame ticks
	ULONG   secs[FP_SECTIONS];          // per-section ticks this frame
	unsigned lm_calls;
	unsigned lm_bytes;
	unsigned overlays;
} fpslot_t;

static ULONG FP_Clock (void)
{
	ULONG value;
	__asm__ volatile ("mftb %0" : "=r"(value));
	return value;
}

static qboolean fp_enabled;
static char     fp_path[256];
static ULONG    fp_clock_hz;

static int   fp_depth;
static struct { int section; unsigned long start; } fp_stack[FP_MAX_DEPTH];
static ULONG fp_secs[FP_SECTIONS];
static unsigned fp_lm_calls, fp_lm_bytes;
static unsigned fp_overlays, fp_overlay_first;
static qboolean fp_frame_open;
static ULONG    fp_frame_start;
static int      fp_frame_no;

static unsigned long long fp_run_secs[FP_SECTIONS];
static unsigned long long fp_run_total;
static unsigned long long fp_run_lm_calls, fp_run_lm_bytes;
static unsigned long long fp_run_overlays, fp_run_overlay_first;

static fpslot_t fp_worst[FP_WORST_SLOTS];
static int      fp_worst_count;
static qboolean fp_dumped;

static const char *fp_names[FP_SECTIONS] = {
	"SCREEN", "VIEW", "SCENE", "WORLD", "VIS", "CHAINS", "LMBLEND",
	"LMBUILD", "ENTBRUSH", "ENTALIAS", "PARTICLES", "WATER", "MIRROR",
	"GLOW", "CLEAR", "BLEND", "HUD", "PRESENT"
};

void FP_Init (void)
{
	int     i;
	ULONG   t0, t1;

	i = COM_CheckParm ("-frameprofile");
	if (!i)
		return;
	fp_enabled = true;
	// Optional path argument: -frameprofile <path> (skip if the next
	// argument is another -/+ option).
	if (i + 1 < com_argc && com_argv[i+1][0] != '-' && com_argv[i+1][0] != '+')
	{
		strncpy (fp_path, com_argv[i+1], sizeof(fp_path)-1);
		fp_path[sizeof(fp_path)-1] = 0;
	}
	if (!fp_path[0])
		strcpy (fp_path, "DH2:wosbuild/frameprof.txt");

	// One-time calibration: Delay(25) is 0.5 s of dos ticks.
	t0 = FP_Clock ();
	Delay (25);
	t1 = FP_Clock ();
	fp_clock_hz = t1 > t0 ? (t1 - t0) * 2UL : 0UL;
}

unsigned long FP_Enter (int section)
{
	if (!fp_enabled || fp_depth >= FP_MAX_DEPTH)
		return 0;
	fp_stack[fp_depth].section = section;
	fp_stack[fp_depth].start = FP_Clock ();
	fp_depth++;
	return fp_stack[fp_depth-1].start;
}

void FP_Exit (int section, unsigned long start)
{
	if (!start)
		return;                     // covers disabled and depth-overflow
	if (fp_depth <= 0 || fp_stack[fp_depth-1].section != section)
		return;                     // mismatch (e.g. longjmp): drop, don't skew
	fp_secs[section] += FP_Clock () - start;
	fp_depth--;
}

void FP_FrameBegin (void)
{
	if (!fp_enabled)
		return;
	if (fp_frame_open)
		fp_depth = 0;               // previous frame aborted (longjmp/host_abortserver)
	fp_frame_open = true;
	fp_frame_start = FP_Clock ();
	memset (fp_secs, 0, sizeof(fp_secs));
	fp_lm_calls = fp_lm_bytes = 0;
	fp_overlays = fp_overlay_first = 0;
}

void FP_FrameEnd (void)
{
	fpslot_t slot;
	int      s, insert, move;

	if (!fp_enabled || !fp_frame_open)
		return;
	fp_frame_open = false;

	slot.frame = ++fp_frame_no;
	slot.total = FP_Clock () - fp_frame_start;
	for (s = 0 ; s < FP_SECTIONS ; s++)
		slot.secs[s] = fp_secs[s];
	slot.lm_calls = fp_lm_calls;
	slot.lm_bytes = fp_lm_bytes;
	slot.overlays = fp_overlays;

	for (s = 0 ; s < FP_SECTIONS ; s++)
		fp_run_secs[s] += fp_secs[s];
	fp_run_total += slot.total;
	fp_run_lm_calls += fp_lm_calls;
	fp_run_lm_bytes += fp_lm_bytes;
	fp_run_overlays += fp_overlays;
	fp_run_overlay_first += fp_overlay_first;

	// Rank-ordered slowest-frame table (rank 0 = worst), MiniGL-style.
	insert = 0;
	while (insert < fp_worst_count && fp_worst[insert].total >= slot.total)
		insert++;
	if (insert >= FP_WORST_SLOTS)
		return;
	if (fp_worst_count < FP_WORST_SLOTS)
		fp_worst_count++;
	move = fp_worst_count - 1;
	while (move > insert)
	{
		fp_worst[move] = fp_worst[move-1];
		move--;
	}
	fp_worst[insert] = slot;
}

void FP_CountLMUpload (unsigned bytes)
{
	if (!fp_enabled || !fp_frame_open)
		return;
	fp_lm_calls++;
	fp_lm_bytes += bytes;
}

void FP_CountOverlay (int first_use)
{
	if (!fp_enabled || !fp_frame_open)
		return;
	fp_overlays++;
	if (first_use)
		fp_overlay_first++;
}

void FP_AutoDump (void)
{
	FILE   *f;
	double  wall, hz;
	int     i, s;
	unsigned long long other;

	if (!fp_enabled || fp_dumped || !fp_frame_no)
		return;
	fp_dumped = true;

	f = fopen (fp_path, "w");
	if (!f)
		return;

	hz = (double)fp_clock_hz;
	wall = hz > 0 ? (double)fp_run_total / hz : 0;
	fprintf (f, "FP VERSION 1 frames=%d clock_hz=%lu wall_s=%.3f fps=%.3f\n",
			fp_frame_no, (unsigned long)fp_clock_hz, wall,
			wall > 0 ? fp_frame_no / wall : 0);
	/* NOTE: this runtime's fprintf has no working %llu and chokes on long
	 * argument lists (see MINIGL_PROFILE.md) -- keep few args per call and
	 * print seconds as doubles instead of raw 64-bit ticks. */
	fprintf (f, "FP NOTE sections nest; totals overlap across levels; "
			"pct is vs whole FRAME\n");

	for (s = 0 ; s < FP_SECTIONS ; s++)
	{
		fprintf (f, "FP TOTAL %-10s s=%.3f ms=%.3f pct=%.2f\n",
				fp_names[s],
				hz > 0 ? (double)fp_run_secs[s] / hz : 0,
				hz > 0 ? fp_run_secs[s] * 1000.0 / hz : 0,
				fp_run_total ? 100.0 * fp_run_secs[s] / fp_run_total : 0);
	}
	other = fp_run_total > fp_run_secs[FP_SCREEN]
			? fp_run_total - fp_run_secs[FP_SCREEN] : 0;
	fprintf (f, "FP TOTAL %-10s s=%.3f ms=%.3f pct=%.2f\n",
			"host_other",
			hz > 0 ? (double)other / hz : 0,
			hz > 0 ? other * 1000.0 / hz : 0,
			fp_run_total ? 100.0 * other / fp_run_total : 0);

	fprintf (f, "FP COUNTERS lm_upload_calls=%lu lm_upload_kb=%lu\n",
			(unsigned long)fp_run_lm_calls,
			(unsigned long)(fp_run_lm_bytes / 1024u));
	fprintf (f, "FP COUNTERS overlay_draws=%lu overlay_first_use=%lu\n",
			(unsigned long)fp_run_overlays,
			(unsigned long)fp_run_overlay_first);

	for (i = 0 ; i < fp_worst_count ; i++)
	{
		const fpslot_t *w = &fp_worst[i];
		double k = hz > 0 ? 1000.0 / hz : 0;

		fprintf (f, "FP WORST rank=%d frame=%d total_ms=%.3f\n",
				i, w->frame, w->total * k);
		fprintf (f, "FPW %d screen=%.3f view=%.3f scene=%.3f world=%.3f\n",
				i,
				w->secs[FP_SCREEN] * k, w->secs[FP_VIEW] * k,
				w->secs[FP_SCENE] * k, w->secs[FP_WORLD] * k);
		fprintf (f, "FPW %d ents=%.3f(%.3f+%.3f) parts=%.3f blend=%.3f\n",
				i,
				(w->secs[FP_ENTBRUSH] + w->secs[FP_ENTALIAS]) * k,
				w->secs[FP_ENTBRUSH] * k, w->secs[FP_ENTALIAS] * k,
				w->secs[FP_PARTICLES] * k, w->secs[FP_BLEND] * k);
		fprintf (f, "FPW %d hud=%.3f present=%.3f lm_calls=%u lm_kb=%u "
				"overlays=%u\n",
				i,
				w->secs[FP_HUD] * k, w->secs[FP_PRESENT] * k,
				w->lm_calls, w->lm_bytes / 1024u, w->overlays);
	}
	fclose (f);
}

#endif // WOS && __PPC__
