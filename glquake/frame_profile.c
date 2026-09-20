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
#define FP_WINDOW_SLOTS 32
#define FP_UPLOAD_SLOTS 16

typedef struct {
	ULONG ticks;
	unsigned bytes;
	int atlas, top, rows, site;
} fpupload_t;

typedef struct {
	int     frame;
	int     host_frame, demo_frame;
	double  demo_time;                  // last server message time, not wall time
	unsigned items;
	int active_weapon, weapon_model, bonus_end;
	float blend_alpha_end;
	char weapon_name[32];
	ULONG   total;                      // whole-frame ticks
	ULONG   secs[FP_SECTIONS];          // per-section ticks this frame
	unsigned lm_calls;
	unsigned lm_bytes;
	unsigned overlays;
	unsigned upload_count, upload_dropped;
	fpupload_t uploads[FP_UPLOAD_SLOTS];
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
static int      fp_host_frame;
static fpupload_t fp_uploads[FP_UPLOAD_SLOTS];
static unsigned fp_upload_count, fp_upload_dropped;

static unsigned long long fp_run_secs[FP_SECTIONS];
static unsigned long long fp_run_total;
static unsigned long long fp_run_lm_calls, fp_run_lm_bytes;
static unsigned long long fp_run_overlays, fp_run_overlay_first;

static fpslot_t fp_worst[FP_WORST_SLOTS];
static int      fp_worst_count;
static qboolean fp_dumped;
static fpslot_t fp_window[FP_WINDOW_SLOTS];
static int fp_window_first, fp_window_last, fp_window_count;
static qboolean fp_window_requested, fp_window_valid;

static const char *fp_names[FP_SECTIONS] = {
	"SCREEN", "VIEW", "SCENE", "WORLD", "VIS", "CHAINS", "LMBLEND",
	"LMBUILD", "LMUPLOAD", "ENTBRUSH", "ENTALIAS", "PARTICLES", "WATER",
	"MIRROR", "GLOW", "CLEAR", "BLEND", "HUD", "PRESENT"
};

/* Startup-only, strict bounded parsing; no overflowing atoi/strtol result. */
static qboolean FP_ParseFrame (const char *text, int *value)
{
	unsigned n = 0, digit;
	if (!text || !*text)
		return false;
	while (*text)
	{
		if (*text < '0' || *text > '9')
			return false;
		digit = (unsigned)(*text++ - '0');
		if (n > (2147483647u - digit) / 10u)
			return false;
		n = n * 10u + digit;
	}
	if (!n)
		return false;
	*value = (int)n;
	return true;
}

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
	i = COM_CheckParm ("-framewindow");
	if (i)
	{
		int first, last;
		fp_window_requested = true;
		if (i + 2 < com_argc && FP_ParseFrame(com_argv[i+1], &first) &&
			FP_ParseFrame(com_argv[i+2], &last) && last >= first &&
			last - first < FP_WINDOW_SLOTS)
		{
			fp_window_first = first;
			fp_window_last = last;
			fp_window_valid = true;
		}
	}

	// One-time calibration: Delay(25) is 0.5 s of dos ticks.
	t0 = FP_Clock ();
	Delay (25);
	t1 = FP_Clock ();
	fp_clock_hz = (ULONG)(t1 - t0) * 2UL;
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

static ULONG FP_CloseSection (int section, unsigned long start)
{
	ULONG elapsed;
	if (!start)
		return 0;                   // covers disabled and depth-overflow
	if (fp_depth <= 0 || fp_stack[fp_depth-1].section != section)
		return 0;                   // mismatch (e.g. longjmp): drop, don't skew
	elapsed = (ULONG)(FP_Clock () - start);
	fp_secs[section] += elapsed;
	fp_depth--;
	return elapsed;
}

void FP_Exit (int section, unsigned long start)
{
	(void)FP_CloseSection (section, start);
}

void FP_ExitLMUpload (unsigned long start, int atlas, int top, int rows,
                       unsigned bytes, int site)
{
	ULONG elapsed = FP_CloseSection (FP_LMUPLOAD, start);
	fpupload_t *upload;
	if (!start || !fp_enabled || !fp_frame_open)
		return;
	if (fp_upload_count == FP_UPLOAD_SLOTS)
	{
		++fp_upload_dropped;
		return;
	}
	upload = &fp_uploads[fp_upload_count++];
	upload->ticks = elapsed;
	upload->atlas = atlas;
	upload->top = top;
	upload->rows = rows;
	upload->bytes = bytes;
	upload->site = site;
}

void FP_FrameBegin (void)
{
	if (!fp_enabled)
		return;
	if (fp_frame_open)
		fp_depth = 0;               // previous frame aborted (longjmp/host_abortserver)
	fp_frame_open = true;
	fp_frame_start = FP_Clock ();
	fp_host_frame = host_framecount;
	memset (fp_secs, 0, sizeof(fp_secs));
	fp_lm_calls = fp_lm_bytes = 0;
	fp_overlays = fp_overlay_first = 0;
	fp_upload_count = fp_upload_dropped = 0;
}

void FP_FrameEnd (void)
{
	static fpslot_t slot;              // bounded scratch, not a large CLI stack frame
	int      s, insert, move;

	if (!fp_enabled || !fp_frame_open)
		return;
	fp_frame_open = false;

	slot.frame = ++fp_frame_no;
	slot.host_frame = fp_host_frame;
	slot.demo_frame = cls.timedemo ? fp_host_frame - cls.td_startframe : -1;
	slot.demo_time = cl.mtime[0];
	slot.items = (unsigned)cl.items;
	slot.active_weapon = cl.stats[STAT_ACTIVEWEAPON];
	slot.weapon_model = cl.stats[STAT_WEAPON];
	slot.bonus_end = cl.cshifts[CSHIFT_BONUS].percent;
	slot.blend_alpha_end = v_blend[3];
	strncpy (slot.weapon_name, cl.viewent.model ? cl.viewent.model->name : "-",
			 sizeof(slot.weapon_name) - 1);
	slot.weapon_name[sizeof(slot.weapon_name) - 1] = 0;
	slot.total = FP_Clock () - fp_frame_start;
	for (s = 0 ; s < FP_SECTIONS ; s++)
		slot.secs[s] = fp_secs[s];
	slot.lm_calls = fp_lm_calls;
	slot.lm_bytes = fp_lm_bytes;
	slot.overlays = fp_overlays;
	slot.upload_count = fp_upload_count;
	slot.upload_dropped = fp_upload_dropped;
	for (s = 0; s < (int)fp_upload_count; ++s)
		slot.uploads[s] = fp_uploads[s];

	/* Retain the requested frames BEFORE worst-table insertion can return.
	 * This is a fixed window, not a rolling tail that loses frame 260. */
	if (fp_window_valid && slot.frame >= fp_window_first &&
		slot.frame <= fp_window_last && fp_window_count < FP_WINDOW_SLOTS)
		fp_window[fp_window_count++] = slot;

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

static void FP_DumpSlot (FILE *f, const char *kind, int rank,
                        const fpslot_t *w, double k)
{
	unsigned u;
	fprintf (f, "FP %s rank=%d frame=%d total_ms=%.3f\n",
			kind, rank, w->frame, w->total * k);
	fprintf (f, "FPW %d host_frame=%d demo_frame=%d demo_s=%.6f\n",
			rank, w->host_frame, w->demo_frame, w->demo_time);
	fprintf (f, "FPW %d items=%lu active_weapon=%d weapon_model=%d\n", rank,
			(unsigned long)w->items, w->active_weapon, w->weapon_model);
	fprintf (f, "FPW %d bonus_end=%d blend_alpha_end=%.6f\n", rank,
			w->bonus_end, (double)w->blend_alpha_end);
	fprintf (f, "FPW %d weapon_name=%s\n", rank, w->weapon_name);
	fprintf (f, "FPW %d screen=%.3f view=%.3f scene=%.3f world=%.3f\n",
			rank, w->secs[FP_SCREEN] * k, w->secs[FP_VIEW] * k,
			w->secs[FP_SCENE] * k, w->secs[FP_WORLD] * k);
	fprintf (f, "FPW %d ents=%.3f brush=%.3f alias=%.3f\n", rank,
			(w->secs[FP_ENTBRUSH] + w->secs[FP_ENTALIAS]) * k,
			w->secs[FP_ENTBRUSH] * k, w->secs[FP_ENTALIAS] * k);
	fprintf (f, "FPW %d parts=%.3f blend=%.3f lmbuild=%.3f\n", rank,
			w->secs[FP_PARTICLES] * k, w->secs[FP_BLEND] * k,
			w->secs[FP_LMBUILD] * k);
	fprintf (f, "FPW %d hud=%.3f present=%.3f lmup=%.3f\n", rank,
			w->secs[FP_HUD] * k, w->secs[FP_PRESENT] * k,
			w->secs[FP_LMUPLOAD] * k);
	fprintf (f, "FPW %d lm_calls=%u lm_kb=%u overlays=%u\n", rank,
			w->lm_calls, w->lm_bytes / 1024u, w->overlays);
	fprintf (f, "FPW %d upload_details=%u upload_dropped=%u\n", rank,
			w->upload_count, w->upload_dropped);
	for (u = 0; u < w->upload_count; ++u)
	{
		const fpupload_t *upload = &w->uploads[u];
		fprintf (f, "FPU %u atlas=%d site=%d ms=%.3f\n", u,
				upload->atlas, upload->site, upload->ticks * k);
		fprintf (f, "FPUR %u top=%d rows=%d bytes=%u\n", u,
				upload->top, upload->rows, upload->bytes);
	}
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
	fprintf (f, "FP VERSION 2 frames=%d clock_hz=%lu wall_s=%.3f fps=%.3f\n",
			fp_frame_no, (unsigned long)fp_clock_hz, wall,
			wall > 0 ? fp_frame_no / wall : 0);
	/* NOTE: this runtime's fprintf has no working %llu and chokes on long
	 * argument lists (see MINIGL_PROFILE.md) -- keep few args per call and
	 * print seconds as doubles instead of raw 64-bit ticks. */
	fprintf (f, "FP NOTE sections nest; totals overlap across levels; "
			"pct is vs whole FRAME\n");
	fprintf (f, "FP NOTE LMUPLOAD includes synchronous DLL work/waits; "
			"bytes are source rectangle bytes, not actual VRAM traffic\n");
	fprintf (f, "FP CONFIG r_dynamic=%.0f gl_texsort=%.0f\n",
			(double)r_dynamic.value, (double)gl_texsort.value);
	fprintf (f, "FP CONFIG width=%d height=%d\n", vid.width, vid.height);

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
		FP_DumpSlot (f, "WORST", i, &fp_worst[i], hz > 0 ? 1000.0 / hz : 0);
	if (fp_window_requested)
	{
		fprintf (f, "FP WINDOW_INFO first=%d last=%d valid=%d count=%d\n",
				fp_window_first, fp_window_last, fp_window_valid, fp_window_count);
		for (i = 0; i < fp_window_count; ++i)
			FP_DumpSlot (f, "WINDOW", i, &fp_window[i], hz > 0 ? 1000.0 / hz : 0);
	}
	fclose (f);
}

#endif // WOS && __PPC__
