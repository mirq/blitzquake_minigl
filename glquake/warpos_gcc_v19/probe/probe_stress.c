/*
 * gqs - GLQuake-style frame-load stress probe for the MiniGL V19 WOS dispatch
 * stack (R200). Isolates the timedemo machine-starvation from the game:
 *
 *  - windowed 640x480x32 (game config), one 256x256 texture (GL_RGB,
 *    the known-good internal format), depth test on
 *  - per frame: glClear(color|depth), Q textured quads submitted as
 *    immediate-mode GL_QUADS with per-vertex glTexCoord2f/glVertex3f
 *    (exactly the game's world/alias submission shape), mglSwitchDisplay
 *  - quads escalate per frame (Q0 +QSTEP, capped) to find the load level
 *    where the 68k host saturates the machine
 *  - optional -batch: same quad count via vertex arrays + glDrawElements
 *    (validates the r_worldbatch path AND measures the batch/immediate ratio)
 *  - optional -finish: glFinish after submit (fenced) vs game-like unfenced
 *  - auto-exits after -frames N; per-frame line printed to stdout (redirect
 *    to a file!) and marker-appended to RAM:gqs_marker (stdio-independent)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/minigl.h>

static double now_ds(void)
{
    struct DateStamp ds;
    DateStamp(&ds);
    return (double)ds.ds_Days * 86400.0 + (double)ds.ds_Minute * 60.0
         + (double)ds.ds_Tick / 50.0;
}

static void mark(const char *s)
{
    BPTR f = Open("RAM:gqs_marker", MODE_OLDFILE);
    if (!f) f = Open("RAM:gqs_marker", MODE_NEWFILE);
    if (f) { Seek(f, 0, OFFSET_END); Write(f, (STRPTR)s, strlen(s)); Close(f); }
}

#define MAXVERT (4 * 4096)
static float verts[MAXVERT * 3];
static float texs[MAXVERT * 2];
static unsigned short idx[(MAXVERT / 4) * 6];

static void build_geom(int quads)
{
    int i;
    for (i = 0; i < quads; i++)
    {
        float x = (float)(i & 31) * 20.0f;
        float y = (float)((i >> 5) & 15) * 30.0f;
        float *v = verts + i * 12;
        float *t = texs + i * 8;
        v[0] = x;      v[1] = y;      v[2] = 0;
        v[3] = x + 18; v[4] = y;      v[5] = 0;
        v[6] = x + 18; v[7] = y + 28; v[8] = 0;
        v[9] = x;      v[10] = y + 28; v[11] = 0;
        t[0] = 0; t[1] = 0; t[2] = 1; t[3] = 0;
        t[4] = 1; t[5] = 1; t[6] = 0; t[7] = 1;
    }
}

static void submit_immediate(int quads)
{
    int i, j;
    glBegin(GL_QUADS);
    for (i = 0; i < quads; i++)
    {
        const float *v = verts + i * 12;
        const float *t = texs + i * 8;
        for (j = 0; j < 4; j++)
        {
            glTexCoord2f(t[j * 2], t[j * 2 + 1]);
            glVertex3f(v[j * 3], v[j * 3 + 1], v[j * 3 + 2]);
        }
    }
    glEnd();
}

static void submit_batched(int quads)
{
    int i, n = 0;
    for (i = 0; i < quads; i++)
    {
        int b = i * 4;
        idx[n++] = b;     idx[n++] = b + 1; idx[n++] = b + 2;
        idx[n++] = b;     idx[n++] = b + 2; idx[n++] = b + 3;
    }
    glVertexPointer(3, GL_FLOAT, 0, verts);
    glTexCoordPointer(2, GL_FLOAT, 0, texs);
    glDrawElements(GL_TRIANGLES, n, GL_UNSIGNED_SHORT, idx);
}

int main(int argc, char **argv)
{
    int frames = 12, q0 = 500, qstep = 250, qmax = 4000, batched = 0, fenced = 0;
    int i, a;
    GLuint tex = 1;
    static unsigned int rgba256[256 * 256];

    for (a = 1; a < argc; a++)
    {
        if (a + 1 < argc && !strcmp(argv[a], "-frames"))  frames = atoi(argv[++a]);
        else if (a + 1 < argc && !strcmp(argv[a], "-quads")) q0 = atoi(argv[++a]);
        else if (a + 1 < argc && !strcmp(argv[a], "-step"))  qstep = atoi(argv[++a]);
        else if (a + 1 < argc && !strcmp(argv[a], "-max"))   qmax = atoi(argv[++a]);
        else if (!strcmp(argv[a], "-batch"))   batched = 1;
        else if (!strcmp(argv[a], "-finish"))  fenced = 1;
    }

    setvbuf(stdout, NULL, _IONBF, 0);

    mark("s0: enter\n");
    printf("gqs: frames=%d q0=%d step=%d max=%d batch=%d finish=%d\n",
           frames, q0, qstep, qmax, batched, fenced);

    if (!MiniGLOpen()) { mark("s1: MiniGLOpen FAILED\n"); return 20; }
    mark("s1: dll ok\n");
    mglChooseWindowMode(GL_TRUE);
    mglChoosePixelDepth(32);
    mglChooseVertexBufferSize(8000);
    if (!mglCreateContext(0, 0, 640, 480)) { mark("s2: ctx FAILED\n"); MiniGLClose(); return 20; }
    mark("s2: ctx ok\n");

    glViewport(0, 0, 640, 480);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 640, 480, 0, -999, 999);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);

    for (i = 0; i < 256 * 256; i++)
        rgba256[i] = 0xFF804020u ^ (i << 3);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgba256);
    mark("s3: tex ok\n");
    if (batched)
    {
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
    }

    for (i = 0; i < frames; i++)
    {
        int quads = q0 + qstep * i;
        double t0, t1;
        char line[96];
        if (quads > qmax) quads = qmax;
        if (quads > MAXVERT / 4) quads = MAXVERT / 4;
        build_geom(quads);

        t0 = now_ds();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (batched)
            submit_batched(quads);
        else
            submit_immediate(quads);
        if (fenced)
            glFinish();
        mglSwitchDisplay();
        t1 = now_ds();

        sprintf(line, "f%02d quads=%4d %.1f ms\n", i, quads, (t1 - t0) * 1000.0);
        mark(line);
        printf("gqs %s", line);
    }

    mark("s4: done\n");
    printf("gqs: clean exit\n");
    glFinish();
    mglDeleteContext();
    MiniGLClose();
    return 0;
}
