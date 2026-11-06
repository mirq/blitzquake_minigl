/*
 * pcsamp68k.c -- statistical PC sampler for the 68k client.
 *
 * -pcsample <file>: a CIA-B timer interrupt (~1 kHz) records the PC the
 * level-6 interrupt took the CPU from, while the Quake task is the running
 * task, from the second timedemo frame until the timedemo ends. The dump
 * is binary, big-endian ULONGs:
 *   'PCS1', count, quake segment count, minigl dispatch table address,
 *   segments (start, size) x N, then count PCs.
 * Resolve offline with tools/pcsamp68k.py against unstripped binaries.
 */
#include "quakedef.h"

#if defined(AMIGA) && !defined(__PPC__)

#include <exec/interrupts.h>
#include <hardware/cia.h>
#include <resources/cia.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/cia.h>
#include <libraries/minigl_dispatch.h>

#define PCS_MAX 65536UL
#define PCS_MAX_SEGS 16

extern struct ExecBase *SysBase;

static volatile ULONG *pcs_buf;
static volatile ULONG pcs_count;
static volatile int pcs_on;
static struct Task *pcs_task;
static struct Library *pcs_cia;
static struct Interrupt pcs_int;
static int pcs_bit = -1;
static char pcs_path[128];

static ULONG pcs_handler (void)
{
	UWORD *sp;
	int i;

	__asm__ volatile ("move.l %%sp,%0" : "=a"(sp));
	if (!pcs_on || SysBase->ThisTask != pcs_task || pcs_count >= PCS_MAX)
		return 0;
	/* Find the level-6 autovector frame: format 0, vector offset 0x78. */
	for (i = 2; i < 160; i++)
	{
		if (sp[i] == 0x0078 && !(sp[i-3] & 0x2000))
		{
			ULONG pc = ((ULONG)sp[i-2] << 16) | sp[i-1];
			ULONG caller = 0, *usp;
			int k;
			/* a caller for samples outside our code (ROM, other
			 * libraries): the first user-stack word that points into
			 * fast-RAM code (rough but enough to attribute OS time) */
			__asm__ volatile ("move.l %%usp,%0" : "=a"(usp));
			if (pc >= 0x00f80000 && pc < 0x01000000)
				for (k = 0; k < 40; k++)
				{
					ULONG r = usp[k];
					const UWORD *w = (const UWORD *)r;
					if (r < 0x08000000 || r >= 0x18000000 || (r & 1))
						continue;
					/* a return address: a jsr/bsr just before it */
					if ((w[-1] & 0xfff8) == 0x4e90 ||
					    (w[-2] & 0xfff8) == 0x4ea8 ||
					    w[-3] == 0x4eb9 || w[-2] == 0x6100 ||
					    w[-3] == 0x61ff ||
					    ((w[-1] & 0xff00) == 0x6100 && (w[-1] & 0xff)))
					{
						caller = r;
						break;
					}
				}
			if (pcs_count + 2 <= PCS_MAX)
			{
				pcs_buf[pcs_count++] = pc;
				pcs_buf[pcs_count++] = caller;
			}
			break;
		}
	}
	return 0;
}

void PCS_Init (void)
{
	volatile struct CIA *ciab = (volatile struct CIA *)0xbfd000;
	int p = COM_CheckParm ("-pcsample");

	if (!p || p + 1 >= com_argc)
		return;
	Q_strncpy (pcs_path, com_argv[p+1], sizeof(pcs_path) - 1);
	pcs_buf = AllocMem (PCS_MAX * sizeof(ULONG), MEMF_PUBLIC);
	if (!pcs_buf)
		return;
	pcs_task = FindTask (NULL);
	pcs_cia = OpenResource (CIABNAME);
	if (!pcs_cia)
		return;
	pcs_int.is_Node.ln_Type = NT_INTERRUPT;
	pcs_int.is_Node.ln_Name = "pcsample";
	pcs_int.is_Code = (void (*)(void))pcs_handler;
	pcs_int.is_Data = NULL;
	if (!AddICRVector (pcs_cia, CIAICRB_TB, &pcs_int))
	{
		pcs_bit = CIAICRB_TB;
		/* 709379 Hz / 709 = ~1 kHz, continuous */
		ciab->ciacrb = 0;
		ciab->ciatblo = 709 & 0xff;
		ciab->ciatbhi = 709 >> 8;
		ciab->ciacrb = CIACRBF_LOAD | CIACRBF_START;
	}
	else if (!AddICRVector (pcs_cia, CIAICRB_TA, &pcs_int))
	{
		pcs_bit = CIAICRB_TA;
		ciab->ciacra = 0;
		ciab->ciatalo = 709 & 0xff;
		ciab->ciatahi = 709 >> 8;
		ciab->ciacra = CIACRAF_LOAD | CIACRAF_START;
	}
	Con_Printf ("pcsample: CIA-B timer %s\n",
		pcs_bit == CIAICRB_TB ? "B" : pcs_bit == CIAICRB_TA ? "A" : "none");
}

void PCS_Start (void)
{
	if (pcs_bit >= 0)
		pcs_on = 1;
}

void PCS_Stop (void)
{
	volatile struct CIA *ciab = (volatile struct CIA *)0xbfd000;
	struct Process *pr;
	struct CommandLineInterface *cli;
	BPTR seg = 0, fh;
	ULONG header[4], segs[2 * PCS_MAX_SEGS];
	ULONG nsegs = 0;

	if (pcs_bit < 0)
		return;
	pcs_on = 0;
	if (pcs_bit == CIAICRB_TB)
		ciab->ciacrb = 0;
	else
		ciab->ciacra = 0;
	RemICRVector (pcs_cia, pcs_bit, &pcs_int);
	pcs_bit = -1;

	pr = (struct Process *)pcs_task;
	cli = pr->pr_CLI ? (struct CommandLineInterface *)BADDR(pr->pr_CLI) : NULL;
	if (cli)
		seg = cli->cli_Module;
	if (!seg)
		seg = pr->pr_SegList ? ((BPTR *)BADDR(pr->pr_SegList))[3] : 0;
	while (seg && nsegs < PCS_MAX_SEGS)
	{
		ULONG *s = (ULONG *)BADDR(seg);
		segs[2*nsegs] = (ULONG)(s + 1);
		segs[2*nsegs+1] = s[-1] - 8;
		nsegs++;
		seg = (BPTR)s[0];
	}
	header[0] = 0x50435332;   /* 'PCS2': pc, caller pairs */
	header[1] = pcs_count;
	header[2] = nsegs;
	/* library text anchor: the JMP target of LVO -30 (LibGetDispatchTable) */
	{
		header[3] = MiniGLBase ? *(ULONG *)((UBYTE *)MiniGLBase - 28) : 0;
	}
	fh = Open (pcs_path, MODE_NEWFILE);
	if (fh)
	{
		Write (fh, header, sizeof(header));
		Write (fh, segs, nsegs * 2 * sizeof(ULONG));
		Write (fh, (APTR)pcs_buf, pcs_count * sizeof(ULONG));
		Close (fh);
	}
	Con_Printf ("pcsample: %lu samples -> %s\n", (unsigned long)pcs_count,
		pcs_path);
}

#else
void PCS_Init (void) {}
void PCS_Start (void) {}
void PCS_Stop (void) {}
#endif
