/*
 * main.c -- BlitzQuakePPC MiniGL Launcher by Mirq.
 *
 * A 68k AmigaOS front-end for the WarpOS/PPC MiniGL GLQuake client.  It
 * starts the MiniGL host and the client from the game directory and passes
 * the selected launch options as plain ASCII arguments.
 */
#pragma amiga-align
#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>
#pragma default-align

#include <string.h>

#include "launcher.h"

#ifdef LQ_TRACE
void lq_trace(const char *msg)
{
    BPTR fh;
    BPTR out = Output();

    if (out)
    {
        Write(out, (APTR)msg, (LONG)strlen(msg));
        Write(out, (APTR)"\n", 1);
    }

    fh = Open((STRPTR)"T:lq_trace.log", MODE_READWRITE);
    if (!fh)
        fh = Open((STRPTR)"T:lq_trace.log", MODE_NEWFILE);
    if (fh)
    {
        Seek(fh, 0, OFFSET_END);
        Write(fh, (APTR)msg, (LONG)strlen(msg));
        Write(fh, (APTR)"\n", 1);
        Close(fh);
    }
}
#endif

/*
 * Library bases.  Do NOT initialise these to NULL: libnix defines and opens
 * dos.library (and the other standard bases) itself, and an initialised
 * definition here would preempt libnix's, leaving DOSBase NULL and crashing
 * the first DOS call.  Declared without initialisers, the linker merges them
 * with libnix's definitions.  We still open the GUI libraries explicitly
 * below (a second open simply bumps the refcount).
 */
struct IntuitionBase *IntuitionBase;
struct GfxBase       *GfxBase;
struct Library       *GadToolsBase;
struct Library       *AslBase;
struct DosLibrary    *DOSBase;

#define LQ_APP_STACK (64 * 1024)

static void lq_fatal(const char *msg)
{
    if (IntuitionBase)
    {
        struct EasyStruct es;

        es.es_StructSize = sizeof(es);
        es.es_Flags = 0;
        es.es_Title = (CONST_STRPTR)"BlitzQuakePPC MiniGL Launcher";
        es.es_TextFormat = (CONST_STRPTR)msg;
        es.es_GadgetFormat = (CONST_STRPTR)"OK";
        EasyRequest(NULL, &es, NULL);
    }
}

int main(int argc, char **argv)
{
    static struct StackSwapStruct swap;
    static LqConfig cfg;   /* keep the large config out of the tiny WB stack */
    APTR mem;

    (void)argc;
    (void)argv;

    LQ_TRACE_MSG("main: enter");

    /* Workbench gives tools a small stack.  Move to our own before doing
     * anything heavy (GadTools/ASL).  The old stack memory stays valid for
     * the life of the process and nothing is freed before exit. */
    mem = AllocMem(LQ_APP_STACK, MEMF_ANY);
    if (mem)
    {
        swap.stk_Lower = mem;
        swap.stk_Upper = (ULONG)mem + LQ_APP_STACK;
        swap.stk_Pointer = (APTR)((ULONG)mem + LQ_APP_STACK - 16);
        StackSwap(&swap);
    }
    LQ_TRACE_MSG("main: stack swapped");

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase       = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 37);
    GadToolsBase  = OpenLibrary((STRPTR)"gadtools.library", 37);
    AslBase       = OpenLibrary((STRPTR)"asl.library", 38);
    LQ_TRACE_MSG("main: libs open");

    if (!IntuitionBase || !GfxBase || !GadToolsBase || !AslBase ||
        !DOSBase || DOSBase->dl_lib.lib_Version < 37)
    {
        lq_fatal("This launcher needs Kickstart 2.04+ (V37) with\n"
                 "intuition, graphics, gadtools and asl (V38).");
        return 20;
    }

    lq_config_defaults(&cfg);
    LQ_TRACE_MSG("main: defaults");
    lq_config_load(&cfg);
    /* The game drawer is always the drawer this launcher was started from:
     * glquakeWOS, mglhost, minigl_ppc.dll and id1/ must live there. */
    lq_config_program_dir(cfg.gamedir, LQ_STRLEN);
    LQ_TRACE_MSG("main: cfg loaded");
    lq_gui_run(&cfg);
    LQ_TRACE_MSG("main: gui done");

    CloseLibrary(AslBase);
    CloseLibrary(GadToolsBase);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
