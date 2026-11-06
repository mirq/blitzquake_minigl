/*
 * config.c -- persistent settings for BlitzQuakePPC MiniGL Launcher.
 *
 * Plain ASCII key=value text, one setting per line.  Stored in
 * ENVARC:BlitzQuakePPC/Launcher.cfg so it survives reboots and is easy to
 * edit by hand.  Unknown keys are ignored and a version key is written for
 * forward compatibility.  No binary data, so no endian conversion applies.
 */
#pragma amiga-align
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#pragma default-align

#include <string.h>
#include <stdlib.h>

#include "launcher.h"

#define LQ_CFG_DIR  "ENVARC:BlitzQuakePPC"
#define LQ_CFG_FILE "ENVARC:BlitzQuakePPC/Launcher.cfg"
#define LQ_CFG_TMP  "ENVARC:BlitzQuakePPC/Launcher.cfg.new"

static int lq_clamp(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void lq_setstr(char *dst, const char *src, int max)
{
    strncpy(dst, src, max - 1);
    dst[max - 1] = 0;
}

void lq_config_program_dir(char *buf, int len)
{
    BPTR lock;

    buf[0] = 0;
    lock = GetProgramDir();
    if (lock)
        NameFromLock(lock, (STRPTR)buf, len);
    if (!buf[0])
        lq_setstr(buf, "Work:games/quake", len);
}

void lq_config_defaults(LqConfig *c)
{
    memset(c, 0, sizeof(*c));
    lq_config_program_dir(c->gamedir, LQ_STRLEN);
    lq_setstr(c->client, LQ_CLIENT, LQ_STRLEN);
    c->width = 640;
    c->height = 480;
    c->depth = 16;
    lq_setstr(c->sndflush, "native", sizeof(c->sndflush));
    lq_setstr(c->initppcpath, "C:InitPPC", LQ_STRLEN);
    c->starthost = 1;
    lq_setstr(c->host, LQ_HOST, LQ_STRLEN);
    c->hoststack = 65536;
    c->hostwait = 8;
    lq_setstr(c->hostport, "MiniGLPPC.R200.Host.1", sizeof(c->hostport));
    c->stack = 1048576;
}

/* Remove leading/trailing blanks, CRs and LFs (FGets keeps the newline). */
static void lq_trim(char *s)
{
    char *p = s;
    int n;

    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);

    n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                     s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = 0;
}

static void lq_apply(LqConfig *c, const char *k, const char *v)
{
    if (!strcmp(k, "width"))
        c->width = atoi(v);
    else if (!strcmp(k, "height"))
        c->height = atoi(v);
    else if (!strcmp(k, "depth"))
        c->depth = atoi(v);
    else if (!strcmp(k, "windowmode"))
        c->windowmode = atoi(v) != 0;
    else if (!strcmp(k, "nosound"))
        c->nosound = atoi(v) != 0;
    else if (!strcmp(k, "ahiid"))
        lq_setstr(c->ahiid, v, sizeof(c->ahiid));
    else if (!strcmp(k, "audspeed"))
        lq_setstr(c->audspeed, v, sizeof(c->audspeed));
    else if (!strcmp(k, "sndflush"))
        lq_setstr(c->sndflush, v, sizeof(c->sndflush));
    else if (!strcmp(k, "noudp"))
        c->noudp = atoi(v) != 0;
    else if (!strcmp(k, "nojoy"))
        c->nojoy = atoi(v) != 0;
    else if (!strcmp(k, "nomouse"))
        c->nomouse = atoi(v) != 0;
    else if (!strcmp(k, "condebug"))
        c->condebug = atoi(v) != 0;
    else if (!strcmp(k, "map"))
        lq_setstr(c->map, v, sizeof(c->map));
    else if (!strcmp(k, "extra"))
        lq_setstr(c->extra, v, sizeof(c->extra));
    else if (!strcmp(k, "initppc"))
        c->initppc = atoi(v) != 0;
    else if (!strcmp(k, "initppcpath"))
        lq_setstr(c->initppcpath, v, LQ_STRLEN);
    else if (!strcmp(k, "starthost"))
        c->starthost = atoi(v) != 0;
    else if (!strcmp(k, "hoststack"))
        c->hoststack = atoi(v);
    else if (!strcmp(k, "hostwait"))
        c->hostwait = atoi(v);
    else if (!strcmp(k, "hostport"))
        lq_setstr(c->hostport, v, sizeof(c->hostport));
    else if (!strcmp(k, "stack"))
        c->stack = atoi(v);
    /* anything else (including version=) is ignored */
}

int lq_config_load(LqConfig *c)
{
    BPTR fh;
    char line[512];

    fh = Open((STRPTR)LQ_CFG_FILE, MODE_OLDFILE);
    if (!fh)
        return 0;

    while (FGets(fh, (STRPTR)line, sizeof(line) - 1))
    {
        char *eq;

        lq_trim(line);
        if (line[0] == 0 || line[0] == '#' || line[0] == ';')
            continue;
        eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = 0;
        lq_apply(c, line, eq + 1);
    }
    Close(fh);

    /* sanity ranges */
    c->width = lq_clamp(c->width, 320, 1920);
    c->height = lq_clamp(c->height, 200, 1200);
    c->depth = lq_clamp(c->depth, 8, 32);
    c->hostwait = lq_clamp(c->hostwait, 1, 60);
    c->hoststack = lq_clamp(c->hoststack, 8192, 1048576);
    c->stack = lq_clamp(c->stack, 65536, 8 * 1048576);
    return 1;
}

static void lq_ensure_dir(void)
{
    BPTR l;

    l = Lock((STRPTR)LQ_CFG_DIR, ACCESS_READ);
    if (l)
    {
        UnLock(l);
        return;
    }
    l = CreateDir((STRPTR)LQ_CFG_DIR);
    if (l)
        UnLock(l);
}

static int lq_wr(BPTR fh, const char *s)
{
    LONG n = (LONG)strlen(s);

    if (n > 0 && Write(fh, (APTR)s, n) != n)
        return 0;
    if (Write(fh, (APTR)"\n", 1) != 1)
        return 0;
    return 1;
}

int lq_config_save(const LqConfig *c)
{
    BPTR fh;
    char buf[600];

    lq_ensure_dir();
    fh = Open((STRPTR)LQ_CFG_TMP, MODE_NEWFILE);
    if (!fh)
        return 0;

    lq_wr(fh, "# BlitzQuakePPC MiniGL Launcher by Mirq -- settings");
    lq_wr(fh, "# The game drawer is the launcher's own drawer; do not add paths.");
    lq_wr(fh, "version=1");

    sprintf(buf, "width=%ld", (long)c->width);
    lq_wr(fh, buf);
    sprintf(buf, "height=%ld", (long)c->height);
    lq_wr(fh, buf);
    sprintf(buf, "depth=%ld", (long)c->depth);
    lq_wr(fh, buf);
    sprintf(buf, "windowmode=%ld", (long)(c->windowmode != 0));
    lq_wr(fh, buf);
    sprintf(buf, "nosound=%ld", (long)(c->nosound != 0));
    lq_wr(fh, buf);
    sprintf(buf, "ahiid=%s", c->ahiid);
    lq_wr(fh, buf);
    sprintf(buf, "audspeed=%s", c->audspeed);
    lq_wr(fh, buf);
    sprintf(buf, "sndflush=%s", c->sndflush);
    lq_wr(fh, buf);
    sprintf(buf, "noudp=%ld", (long)(c->noudp != 0));
    lq_wr(fh, buf);
    sprintf(buf, "nojoy=%ld", (long)(c->nojoy != 0));
    lq_wr(fh, buf);
    sprintf(buf, "nomouse=%ld", (long)(c->nomouse != 0));
    lq_wr(fh, buf);
    sprintf(buf, "condebug=%ld", (long)(c->condebug != 0));
    lq_wr(fh, buf);
    sprintf(buf, "map=%s", c->map);
    lq_wr(fh, buf);
    sprintf(buf, "extra=%s", c->extra);
    lq_wr(fh, buf);
    sprintf(buf, "initppc=%ld", (long)(c->initppc != 0));
    lq_wr(fh, buf);
    sprintf(buf, "initppcpath=%s", c->initppcpath);
    lq_wr(fh, buf);
    sprintf(buf, "starthost=%ld", (long)(c->starthost != 0));
    lq_wr(fh, buf);
    sprintf(buf, "hoststack=%ld", (long)c->hoststack);
    lq_wr(fh, buf);
    sprintf(buf, "hostwait=%ld", (long)c->hostwait);
    lq_wr(fh, buf);
    sprintf(buf, "hostport=%s", c->hostport);
    lq_wr(fh, buf);
    sprintf(buf, "stack=%ld", (long)c->stack);
    lq_wr(fh, buf);

    Close(fh);

    DeleteFile((STRPTR)LQ_CFG_FILE);
    if (!Rename((STRPTR)LQ_CFG_TMP, (STRPTR)LQ_CFG_FILE))
    {
        DeleteFile((STRPTR)LQ_CFG_TMP);
        return 0;
    }
    return 1;
}
