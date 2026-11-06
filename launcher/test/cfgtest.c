/*
 * cfgtest.c -- headless test for the launcher's settings module.
 *
 * Saves a distinctive configuration to ENVARC:BlitzQuakePPC/Launcher.cfg,
 * reads it back into a fresh structure and dumps the result to
 * T:cfgtest.log.  Used to validate the Save Settings / persistence path
 * without the GUI (input injection is not available in all sessions).
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
    LqConfig a, b;
    FILE *f;

    (void)argc;
    (void)argv;

    f = fopen("T:cfgtest.log", "w");
    if (!f)
        return 5;

    lq_config_defaults(&a);
    a.width = 800;
    a.height = 600;
    a.depth = 32;
    a.windowmode = 1;
    a.nosound = 1;
    a.noudp = 1;
    a.nojoy = 1;
    a.nomouse = 1;
    a.condebug = 1;
    strcpy(a.ahiid, "00020018");
    strcpy(a.audspeed, "22050");
    strcpy(a.sndflush, "exec");
    strcpy(a.map, "e1m1");
    strcpy(a.extra, "+host_speeds 1");
    a.initppc = 1;
    a.starthost = 0;
    a.hostwait = 11;
    a.stack = 2097152;

    fprintf(f, "save=%d\n", lq_config_save(&a));

    lq_config_defaults(&b);
    b.width = 1;
    b.height = 1;
    b.depth = 1;
    b.windowmode = 0;
    b.nosound = 0;
    b.noudp = 0;
    b.nojoy = 0;
    b.nomouse = 0;
    b.condebug = 0;
    b.initppc = 0;
    b.starthost = 1;
    b.hostwait = 0;
    b.stack = 0;
    b.ahiid[0] = 0;
    b.audspeed[0] = 0;
    b.map[0] = 0;
    b.extra[0] = 0;

    fprintf(f, "load=%d\n", lq_config_load(&b));
    fprintf(f, "width=%d height=%d depth=%d win=%d snd=%d static=%s ahi=%s aud=%s\n",
            b.width, b.height, b.depth, b.windowmode, b.nosound,
            b.sndflush, b.ahiid, b.audspeed);
    fprintf(f, "noudp=%d nojoy=%d nomouse=%d condebug=%d initppc=%d starthost=%d wait=%d stack=%d\n",
            b.noudp, b.nojoy, b.nomouse, b.condebug, b.initppc,
            b.starthost, b.hostwait, b.stack);
    fprintf(f, "map=[%s] extra=[%s] host=[%s] client=[%s]\n",
            b.map, b.extra, b.host, b.client);

    fclose(f);
    return 0;
}
