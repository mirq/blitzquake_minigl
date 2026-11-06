/*
 * argdump.c -- launch-mechanism smoke test for the launcher.
 *
 * Built for 68k and launched exactly like the game client (CreateNewProc
 * with NP_Arguments + NP_Cli, explicit stack, game directory as CWD).
 * Dumps argc/argv, the raw argument string, the process stack size and
 * whether a CLI structure was created to T:argdump.log.
 */
#pragma amiga-align
#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dosextens.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#pragma default-align

#include <stdio.h>

struct DosLibrary *DOSBase;

int main(int argc, char **argv)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    STRPTR argstr = GetArgStr();
    char dir[256];
    FILE *f;
    int i;

    f = fopen("T:argdump.log", "w");
    if (!f)
        return 5;

    fprintf(f, "argc=%d\n", argc);
    for (i = 0; i < argc; i++)
        fprintf(f, "argv[%d]=[%s]\n", i, argv[i]);
    fprintf(f, "argstr=[%s]\n", argstr ? (const char *)argstr : "(null)");
    fprintf(f, "stack=%ld\n", (long)me->pr_StackSize);
    fprintf(f, "cli=%s\n", me->pr_CLI ? "yes" : "no");
    fprintf(f, "name=%s\n", (const char *)me->pr_Task.tc_Node.ln_Name);

    dir[0] = 0;
    GetCurrentDirName(dir, sizeof(dir));
    fprintf(f, "cwd=[%s]\n", dir);

    {
        BPTR h = Open((STRPTR)"id1/pak0.pak", MODE_OLDFILE);
        fprintf(f, "open id1/pak0.pak=%ld (IoErr %ld)\n", (long)h, (long)IoErr());
        if (h)
            Close(h);
    }
    {
        BPTR h = Open((STRPTR)"id1", MODE_OLDFILE);
        fprintf(f, "open id1=%ld\n", (long)h);
        if (h)
            Close(h);
    }

    fclose(f);
    return 0;
}
