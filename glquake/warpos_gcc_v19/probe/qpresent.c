/* Quake presentation regression probe, no PAK files or engine required.
 * make -f Makefile.GCCAmigaWOS_probe PROBE=qpresent
 * Default: fullscreen 800x600x32, 3 buffers, sync off, six frames/case.
 * -case 0..13 selects one case; -window and -finish are independent A/Bs.
 * Stops at the first GL error; logs before any potentially blocking call.
 * No perf counters, no per-vertex logging, no allocation in the frame loop.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/minigl.h>

static unsigned char rgba[64*64*4], lum[128*128];
static int testcase, frame;
static const char *names[] = {"flatfan", "polygon", "lightmap",
    "multitexture", "alias", "particles", "texsub", "stress",
    "state-churn", "mixed-2d-3d", "serial-unit0", "serial-unit1",
    "expanded-batch", "sliding-console"};

static void mark(const char *step, unsigned error)
{
    struct DateStamp ds;
    char line[192];
    BPTR f;
    DateStamp(&ds);
    sprintf(line, "case=%d frame=%d tick=%ld:%ld %s err=%u\n",
        testcase, frame, (long)ds.ds_Minute, (long)ds.ds_Tick, step, error);
    f = Open("DH2:wosbuild/qpresent.log", MODE_OLDFILE);
    if (!f) f = Open("DH2:wosbuild/qpresent.log", MODE_NEWFILE);
    if (f) {
        Seek(f, 0, OFFSET_END);
        Write(f, (STRPTR)line, strlen(line));
        Close(f);
    }
}

static int check(const char *step)
{
    GLenum error = glGetError();
    mark(step, (unsigned)error);
    return error == GL_NO_ERROR;
}

static void upload(GLuint id, GLenum format, int size, const void *pixels)
{
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, format, size, size, 0,
        format, GL_UNSIGNED_BYTE, pixels);
}

static void poly(GLenum mode, int dual)
{
    static const GLfloat verts[4][7] = {
        {-1,-1,-3, 0,0, 0,0}, {1,-1,-3, 1,0, 1,0},
        {1,1,-3, 1,1, 1,1}, {-1,1,-3, 0,1, 0,1}
    };
    int i, v;
    glBegin(mode);
    for (i = 0; i < 4; ++i) {
        v = (mode == GL_TRIANGLE_STRIP && i >= 2) ? 5-i : i;
        if (dual) {
            glMultiTexCoord2fARB(GL_TEXTURE0_ARB, verts[v][3], verts[v][4]);
            glMultiTexCoord2fARB(GL_TEXTURE1_ARB, verts[v][5], verts[v][6]);
        } else glTexCoord2f(verts[v][3], verts[v][4]);
        glVertex3f(verts[v][0], verts[v][1], verts[v][2]);
    }
    glEnd();
}

static void draw_case(void)
{
    int i;
    glViewport(0, 0, 800, 600);
    glDrawBuffer(GL_BACK);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glDepthRange(0, 1);
    glShadeModel(GL_FLAT);
    glColor4f(1, 1, 1, 1);
    glActiveTextureARB(GL_TEXTURE1_ARB);
    glDisable(GL_TEXTURE_2D);
    glActiveTextureARB(GL_TEXTURE0_ARB);
    glEnable(GL_TEXTURE_2D);
    if (testcase == 10 || testcase == 11) {
        glBindTexture(GL_TEXTURE_2D, 4);
        /* Frame 0 makes the texture resident. Updating a nonzero mip level
         * preserves residency and exercises the content-serial protocol;
         * TexSubImage can instead re-import under a new surface address. */
        if (frame) {
            rgba[0] = (unsigned char)(frame * 40);
            glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 32, 32, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        }
    }
    glBindTexture(GL_TEXTURE_2D, testcase == 10 ? 4 : 1);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glClearColor((frame & 1) ? .3f : .05f, .1f, .2f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (!testcase || testcase == 13) glOrtho(0, 800, 600, 0, -999, 999);
    else glFrustum(-1.333333, 1.333333, -1, 1, 1, 100);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_DEPTH_TEST);
    if (testcase == 13) {
        GLfloat top = (GLfloat)(-30 - frame*60);
        glDisable(GL_DEPTH_TEST);
        glBegin(MGL_FLATFAN);
        glTexCoord2f(0,0); glVertex2f(0,top);
        glTexCoord2f(1,0); glVertex2f(800,top);
        glTexCoord2f(1,1); glVertex2f(800,top+600);
        glTexCoord2f(0,1); glVertex2f(0,top+600);
        glEnd();
    } else if (!testcase) {
        glDisable(GL_DEPTH_TEST);
        glBegin(MGL_FLATFAN);
        glTexCoord2f(0,0); glVertex2f(100,100);
        glTexCoord2f(1,0); glVertex2f(700,100);
        glTexCoord2f(1,1); glVertex2f(700,500);
        glTexCoord2f(0,1); glVertex2f(100,500);
        glEnd();
    } else if (testcase == 3 || testcase == 11) {
        glActiveTextureARB(GL_TEXTURE1_ARB);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, testcase == 11 ? 4 : 2);
        poly(GL_POLYGON, 1);
        glDisable(GL_TEXTURE_2D);
        glActiveTextureARB(GL_TEXTURE0_ARB);
        mglDrawMultitexBuffer(GL_ZERO, GL_SRC_COLOR, GL_REPLACE);
    } else if (testcase == 4) {
        glShadeModel(GL_SMOOTH);
        poly(GL_TRIANGLE_FAN, 0);
        glTranslatef(.5f, 0, -.5f);
        poly(GL_TRIANGLE_STRIP, 0);
    } else if (testcase == 5) {
        poly(GL_QUADS, 0);
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_POINTS);
        for (i=0; i<256; ++i)
            glVertex3f((i%16)/8.0f-1, (i/16)/8.0f-1, -2);
        glEnd();
    } else if (testcase == 12) {
        /* Arm state-mismatch expansion, accumulate a large shared-header
         * batch, then change state. FAN forces individual flush records. */
        for (i=0; i<5; ++i) {
            glBindTexture(GL_TEXTURE_2D, (i & 1) ? 3 : 1);
            poly(GL_TRIANGLE_FAN, 0);
        }
        for (i=0; i<180; ++i)
            poly(GL_TRIANGLE_FAN, 0);
        for (i=0; i<24; ++i) {
            glBindTexture(GL_TEXTURE_2D, (i & 1) ? 1 : 3);
            poly(GL_TRIANGLE_FAN, 0);
        }
    } else {
        for (i=0; i < (testcase >= 7 && testcase <= 9 ? 1000 : 1); ++i) {
            if (testcase == 8 || testcase == 9)
                glBindTexture(GL_TEXTURE_2D, (i & 1) ? 3 : 1);
            poly(GL_POLYGON, 0);
            if (testcase == 9 && !(i % 17)) {
                /* Quake alternates transformed draws and screen-space
                 * effects: exercise both commit and inline slot shapes. */
                glDisable(GL_DEPTH_TEST);
                glBegin(MGL_FLATFAN);
                glTexCoord2f(0,0); glVertex2f(10,10);
                glTexCoord2f(1,0); glVertex2f(50,10);
                glTexCoord2f(1,1); glVertex2f(50,50);
                glTexCoord2f(0,1); glVertex2f(10,50);
                glEnd();
                glEnable(GL_DEPTH_TEST);
            }
        }
        if (testcase == 2 || testcase == 6) {
            glBindTexture(GL_TEXTURE_2D, 2);
            if (testcase == 6)
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, frame, 128, 1,
                    GL_LUMINANCE, GL_UNSIGNED_BYTE, lum);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ZERO, GL_SRC_COLOR);
            glDepthMask(GL_FALSE);
            poly(GL_POLYGON, 0);
        }
    }
}

int main(int argc, char **argv)
{
    int i, first=0, last=13, window=0, finish=0, ok=1;
    BPTR f = Open("DH2:wosbuild/qpresent.log", MODE_NEWFILE);
    if (f) Close(f);
    for (i=1; i<argc; ++i) {
        if (!strcmp(argv[i], "-window")) window=1;
        else if (!strcmp(argv[i], "-finish")) finish=1;
        else if (!strcmp(argv[i], "-case") && i+1<argc)
            first=last=atoi(argv[++i]);
        else return 20;
    }
    if (first<0 || first>13) return 20;
    mark("open enter", 0);
    if (!MiniGLOpen()) { mark("open failed", 0); return 20; }
    mglChooseWindowMode(window ? GL_TRUE : GL_FALSE);
    mglChoosePixelDepth(32);
    mglChooseNumberOfBuffers(3);
    mglChooseVertexBufferSize(3000);
    mglChooseMtexBufferSize(6000);
    mark("context enter", 0);
    if (!mglCreateContext(0,0,800,600)) {
        mark("context failed", 0); MiniGLClose(); return 20;
    }
    mglEnableSync(GL_FALSE);
    mglLockMode(MGL_LOCK_SMART);
    if (!check("context leave")) { ok=0; goto done; }
    for (i=0; i<64*64; ++i) {
        rgba[4*i]=((i/64)^i)&8 ? 255 : 32;
        rgba[4*i+1]=128; rgba[4*i+2]=64; rgba[4*i+3]=255;
    }
    memset(lum, 192, sizeof(lum));
    upload(1, GL_RGBA, 64, rgba);
    if (!check("RGBA upload")) { ok=0; goto done; }
    upload(2, GL_LUMINANCE, 128, lum);
    if (!check("LUMINANCE upload")) { ok=0; goto done; }
    upload(3, GL_RGBA, 64, rgba);
    if (!check("second RGBA upload")) { ok=0; goto done; }
    upload(4, GL_RGBA, 64, rgba);
    if (!check("serial texture upload")) { ok=0; goto done; }
    for (testcase=first; testcase<=last; ++testcase) {
        mark(names[testcase], 0);
        for (frame=0; frame<6; ++frame) {
            mark("lock enter", 0);
            mglLockDisplay();
            if (!check("lock leave")) { ok=0; goto done; }
            mark("draw enter", 0);
            draw_case();
            if (!check("draw leave")) { ok=0; goto done; }
            if (finish) {
                mark("finish enter", 0);
                glFinish();
                if (!check("finish leave")) { ok=0; goto done; }
            }
            mark("swap enter", 0);
            mglSwitchDisplay();
            if (!check("swap leave")) { ok=0; goto done; }
            Delay(5); /* visible alternation, not a speed measurement */
        }
    }
done:
    mark(ok ? "PASS cleanup enter" : "FAIL cleanup enter", 0);
    mglDeleteContext();
    MiniGLClose();
    mark("exit", 0);
    return ok ? 0 : 20;
}
