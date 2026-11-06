/*
 * launch.c -- command line assembly and process launch for the launcher.
 *
 * The client is started in the background ("Run >NIL:" style) with an
 * explicit stack size and the game directory as its current directory
 * (required for minigl_ppc.dll and id1/).  The MiniGL host is started the
 * same way from the game directory.  Nothing here reads or writes binary
 * game data; arguments are plain ASCII.
 */
#pragma amiga-align
#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#pragma default-align

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "launcher.h"

/* Append a string to the argument buffer.  Returns the new end offset. */
static int lq_app(char *buf, int buflen, int pos, const char *s)
{
    int n = (int)strlen(s);

    if (pos + n >= buflen)
        return pos;
    memcpy(buf + pos, s, n);
    pos += n;
    buf[pos] = 0;
    return pos;
}

static int lq_appi(char *buf, int buflen, int pos, const char *fmt, long v)
{
    char tmp[32];

    sprintf(tmp, fmt, v);
    return lq_app(buf, buflen, pos, tmp);
}

/*
 * Assemble the client command line.  Only flags with a value are emitted as
 * flag+value pairs (several client sites read argv[i+1] without a guard).
 */
int lq_build_args(const LqConfig *c, char *buf, int buflen)
{
    int p = 0;

    if (buflen < 2)
        return 0;
    buf[0] = 0;

    p = lq_appi(buf, buflen, p, " -width %ld", (long)c->width);
    p = lq_appi(buf, buflen, p, " -height %ld", (long)c->height);
    p = lq_appi(buf, buflen, p, " -bpp %ld", (long)c->depth);
    if (c->windowmode)
        p = lq_app(buf, buflen, p, " -windowmode");
    if (c->nosound)
        p = lq_app(buf, buflen, p, " -nosound");
    else
        p = lq_app(buf, buflen, p, " -ahi");   /* match the validated command */
    if (c->ahiid[0])
    {
        p = lq_app(buf, buflen, p, " -ahiid ");
        p = lq_app(buf, buflen, p, c->ahiid);
    }
    if (c->audspeed[0])
    {
        p = lq_app(buf, buflen, p, " -audspeed ");
        p = lq_app(buf, buflen, p, c->audspeed);
    }
    /* native dcbf+sync is the client default: only request exec explicitly */
    if (!strcmp(c->sndflush, "exec"))
        p = lq_app(buf, buflen, p, " -sndflush exec");
    if (c->noudp)
        p = lq_app(buf, buflen, p, " -noudp");
    if (c->nojoy)
        p = lq_app(buf, buflen, p, " -nojoy");
    if (c->nomouse)
        p = lq_app(buf, buflen, p, " -nomouse");
    if (c->condebug)
        p = lq_app(buf, buflen, p, " -condebug");
    if (c->map[0])
    {
        p = lq_app(buf, buflen, p, " +map ");
        p = lq_app(buf, buflen, p, c->map);
    }
    if (c->extra[0])
    {
        p = lq_app(buf, buflen, p, " ");
        p = lq_app(buf, buflen, p, c->extra);
    }
    return p;
}

/* Build a full path: absolute names are used as-is, otherwise relative to dir. */
static int lq_resolve(const char *dir, const char *file, char *out, int outlen)
{
    if (strchr(file, ':') || strchr(file, '/'))
    {
        if ((int)strlen(file) >= outlen)
            return 0;
        strcpy(out, file);
        return 1;
    }
    if (!dir[0] || (int)strlen(dir) >= outlen)
        return 0;
    strcpy(out, dir);
    if (!AddPart((STRPTR)out, (CONST_STRPTR)file, outlen))
        return 0;
    return 1;
}

/* Append one line to stdout (when there is a console) and to launch.log in the
 * game drawer.  This is the exact-command trace used to debug a failed run. */
void lq_log(const LqConfig *c, const char *fmt, ...)
{
    char line[1600];
    char path[2 * LQ_STRLEN];
    va_list ap;
    BPTR fh, out;

    va_start(ap, fmt);
    vsprintf(line, fmt, ap);
    va_end(ap);

    out = Output();
    if (out)
    {
        Write(out, (APTR)line, (LONG)strlen(line));
        Write(out, (APTR)"\n", 1);
    }

    if (lq_resolve(c->gamedir, LQ_LAUNCHLOG, path, sizeof(path)))
    {
        fh = Open((STRPTR)path, MODE_READWRITE);
        if (!fh)
            fh = Open((STRPTR)path, MODE_NEWFILE);
        if (fh)
        {
            Seek(fh, 0, OFFSET_END);
            Write(fh, (APTR)line, (LONG)strlen(line));
            Write(fh, (APTR)"\n", 1);
            Close(fh);
        }
    }
}

int lq_host_port_present(const LqConfig *c)
{
    if (!c->hostport[0])
        return 0;
    return FindPort((STRPTR)c->hostport) != NULL;
}

int lq_client_running(void)
{
    return FindTask((STRPTR)"glquakeWOS") != NULL;
}

int lq_run_initppc(const LqConfig *c, char *err, int errlen)
{
    LONG rc;

    lq_log(c, "InitPPC: %s", c->initppcpath[0] ? c->initppcpath : "C:InitPPC");
    rc = SystemTagList((CONST_STRPTR)(c->initppcpath[0] ? c->initppcpath : "C:InitPPC"), NULL);
    if (rc != 0)
    {
        sprintf(err, "InitPPC returned %ld.  The PPC may already be initialised.", (long)rc);
        return 2;
    }
    return 1;
}

int lq_start_host(const LqConfig *c, char *err, int errlen)
{
    char path[2 * LQ_STRLEN];
    char logpath[2 * LQ_STRLEN];
    BPTR seg, lock, log = 0;
    struct Process *p;

    if (lq_host_port_present(c))
    {
        strncpy(err, "A MiniGL host port is already present (stale host after a crash?).\n"
                     "Starting the client may hang.  Reboot is recommended.",
                errlen - 1);
        err[errlen - 1] = 0;
        return 2;
    }

    if (!lq_resolve(c->gamedir, c->host, path, sizeof(path)))
    {
        strncpy(err, "Cannot resolve the host path (game directory missing?).", errlen - 1);
        err[errlen - 1] = 0;
        return 0;
    }
    lq_log(c, "Host: %s  stack=%ld port=%s", path, (long)c->hoststack, c->hostport);

    seg = LoadSeg((STRPTR)path);
    if (!seg)
    {
        BPTR l = Lock((STRPTR)path, ACCESS_READ);

        if (!l)
        {
            sprintf(err, "MiniGL host not found in the game directory:\n%s", path);
            return 0;
        }
        UnLock(l);
        /* Not an executable: assume it is a script and run it via Execute. */
        {
            char cmd[2 * LQ_STRLEN + 32];

            sprintf(cmd, "Run >NIL: Execute \"%s\"", path);
            lq_log(c, "Host script: %s", cmd);
            if (SystemTagList((CONST_STRPTR)cmd, NULL) == -1)
            {
                sprintf(err, "Cannot start the host script:\n%s", path);
                return 0;
            }
        }
    }
    else
    {
        lock = Lock((STRPTR)c->gamedir, ACCESS_READ);
        if (!lock)
        {
            UnLoadSeg(seg);
            sprintf(err, "Cannot lock the game directory:\n%s", c->gamedir);
            return 0;
        }
        /* Keep the host's console output next to the game, as gqhostrun did. */
        if (lq_resolve(c->gamedir, LQ_HOSTLOG, logpath, sizeof(logpath)))
            log = Open((STRPTR)logpath, MODE_NEWFILE);

        p = CreateNewProcTags(
                NP_Seglist, (ULONG)seg,
                NP_CurrentDir, (ULONG)lock,
                NP_StackSize, (ULONG)c->hoststack,
                NP_Cli, TRUE,
                NP_CommandName, (ULONG)LQ_HOST,
                NP_Output, (ULONG)log,
                NP_CloseOutput, TRUE,
                NP_FreeSeglist, TRUE,
                NP_Name, (ULONG)"MiniGLHost",
                TAG_DONE);
        if (!p)
        {
            if (log)
                Close(log);
            UnLock(lock);
            UnLoadSeg(seg);
            strcpy(err, "Cannot start the MiniGL host (CreateNewProc failed).");
            return 0;
        }
        /* success: DOS owns the directory lock and the log handle */
    }

    Delay(c->hostwait * 50);
    if (!lq_host_port_present(c))
    {
        sprintf(err, "The MiniGL host did not come up: port %s not found after %ld s.",
                c->hostport, (long)c->hostwait);
        lq_log(c, "Host port MISSING: %s", c->hostport);
        return 0;
    }
    lq_log(c, "Host port up: %s", c->hostport);
    return 1;
}

int lq_start_client(const LqConfig *c, char *err, int errlen)
{
    char path[2 * LQ_STRLEN];
    char args[1024];
    BPTR seg, lock, log = 0;
    struct Process *p;

    if (lq_client_running())
    {
        strcpy(err, "The Quake client is already running.");
        return 2;
    }

    if (!lq_resolve(c->gamedir, c->client, path, sizeof(path)))
    {
        strncpy(err, "Cannot resolve the client path (game directory missing?).", errlen - 1);
        err[errlen - 1] = 0;
        return 0;
    }

    seg = LoadSeg((STRPTR)path);
    if (!seg)
    {
        sprintf(err, "Cannot load the client:\n%s\nIoErr %ld", path, (long)IoErr());
        return 0;
    }

    lq_build_args(c, args, sizeof(args));

    lq_log(c, "Client: %s", path);
    lq_log(c, "Args:%s", args);
    lq_log(c, "Cwd: %s", c->gamedir);

    lock = Lock((STRPTR)c->gamedir, ACCESS_READ);
    if (!lock)
    {
        UnLoadSeg(seg);
        sprintf(err, "Cannot lock the game directory:\n%s", c->gamedir);
        return 0;
    }

    /* The client's console output is not captured (-stdout is not passed);
     * diagnostics go to RAM:gqtrace inside the client. */
    log = Open((STRPTR)"NIL:", MODE_NEWFILE);

    p = CreateNewProcTags(
            NP_Seglist, (ULONG)seg,
            NP_CurrentDir, (ULONG)lock,
            NP_StackSize, (ULONG)c->stack,
            NP_Arguments, (ULONG)args,
            NP_Cli, TRUE,
            NP_CommandName, (ULONG)LQ_CLIENT,
            NP_Output, (ULONG)log,
            NP_CloseOutput, TRUE,
            NP_FreeSeglist, TRUE,
            NP_Name, (ULONG)LQ_CLIENT,
            TAG_DONE);

    if (!p)
    {
        UnLock(lock);
        if (log)
            Close(log);
        UnLoadSeg(seg);
        strcpy(err, "Cannot start the client (CreateNewProc failed).");
        return 0;
    }
    /* success: DOS owns the directory lock and the log handle */
    lq_log(c, "Client started (background).");
    return 1;
}
