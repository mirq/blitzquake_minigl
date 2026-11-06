/*
 * launchtest.c -- on-target test of the launcher's client start path.
 *
 * Calls the real lq_start_client() from launch.c with argdump as the
 * "client", so the CreateNewProcTags/NP_Arguments/NP_Cli mechanism, the
 * argument assembly and the stack size are validated without touching the
 * PPC client.  Results: T:launchtest.log (launcher side) and
 * T:argdump.log (started process).
 */
#pragma amiga-align
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#pragma default-align

#include <stdio.h>
#include <string.h>

#include "launcher.h"

struct DosLibrary *DOSBase;

int main(int argc, char **argv)
{
    LqConfig c;
    char err[256];
    int rc;
    FILE *f;

    (void)argc;
    (void)argv;

    f = fopen("T:launchtest.log", "w");
    if (!f)
        return 5;

    memset(&c, 0, sizeof(c));
    strcpy(c.gamedir, "Work:Games/Quake");
    strcpy(c.client, "Work:wosbuild/argdump");
    c.width = 800;
    c.height = 600;
    c.depth = 32;
    c.windowmode = 1;
    c.nosound = 1;
    c.noudp = 1;
    c.nojoy = 1;
    c.nomouse = 1;
    c.condebug = 1;
    strcpy(c.ahiid, "00020018");
    strcpy(c.audspeed, "22050");
    strcpy(c.sndflush, "exec");
    strcpy(c.map, "e1m1");
    strcpy(c.extra, "+host_speeds 1");
    c.stack = 1048576;

    err[0] = 0;
    rc = lq_start_client(&c, err, sizeof(err));
    fprintf(f, "rc=%d\nerr=[%s]\n", rc, err);
    fclose(f);

    Delay(50); /* let argdump write its log */
    return 0;
}
