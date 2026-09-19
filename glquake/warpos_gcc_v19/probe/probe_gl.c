/*
 * glqprobe5.c - MiniGL R200 diagnostic probe for GLQuake text path + perf
 *
 * Reproduces the exact GLQuake render operations and measures each class:
 *  - charset texture (GL_RGBA 8-bit->RGBA expansion, flatfan text quads)
 *  - lightmap textures (GL_LUMINANCE 128x128 + glTexSubImage2D updates)
 *  - world texture uploads (GL_RGBA 256x256)
 *  - draw-call and vertex dispatch cost (MGL_FLATFAN vs GL_QUADS)
 *  - clear cost
 * Framebuffer dumps via mglWriteShotPPM for pixel verification.
 * All stages marker-printed to stdout (markers survive a mid-probe crash).
 */

#include <stdio.h>
#include <string.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/minigl.h>

static double now_ds(void)
{
    struct DateStamp ds;
    DateStamp(&ds);
    return (double)ds.ds_Days * 86400.0 + (double)ds.ds_Minute * 60.0
         + (double)ds.ds_Tick / 50.0;
}

static void stage(const char *s) { printf("[%s]\n", s); fflush(stdout); }

static void rep(const char *tag, int n, double t0, double t1)
{
    printf("%-28s %5d it %8.0f ms total %10.3f ms/it  (ticks %.0f)\n",
           tag, n, (t1 - t0) * 1000.0, (t1 - t0) * 1000.0 / n,
           (t1 - t0) * 50.0);
    fflush(stdout);
}

static void chk(const char *where)
{
    GLenum e = glGetError();
    if (e) printf("GL_ERROR at %s: 0x%x\n", where, e);
    fflush(stdout);
}

/* stdio-independent marker */
static void mark(const char *s)
{
    BPTR f = Open("RAM:glp5_marker", MODE_OLDFILE);
    if (!f) f = Open("RAM:glp5_marker", MODE_NEWFILE);
    if (f) { Seek(f, 0, OFFSET_END); Write(f, (STRPTR)s, strlen(s)); Close(f); }
}

/* synthetic conchars-style 8-bit buffer: 0 = transparent, else bright */
static unsigned char charset8[128 * 128];
/* RGBA expansions */
static unsigned int  charset_rgba[128 * 128];   /* a=0 bg, a=255 white glyphs */
static unsigned int  grad_rgba[256 * 256];      /* opaque gradient */
static unsigned char lum128[128 * 128];         /* lightmap gradient */
static unsigned int  rgba256[256 * 256];        /* world texture */
static unsigned char lumsub[128 * 64];          /* subimage block */

static void build_patterns(void)
{
    int x, y, i;
    for (i = 0; i < 128 * 128; i++) charset8[i] = 0;
    /* 8x8 glyph cells with bit patterns, like text */
    for (y = 0; y < 128; y += 8)
        for (x = 0; x < 128; x += 8)
        {
            int cx, cy;
            for (cy = 0; cy < 8; cy++)
                for (cx = 0; cx < 8; cx++)
                {
                    int on = ((cx == 0) || (cy == 0) || (cx == 7 && cy > 2)
                              || ((cx + y) & 4));
                    charset8[(y + cy) * 128 + (x + cx)] = on ? 255 : 0;
                }
        }
    for (i = 0; i < 128 * 128; i++)
    {
        unsigned char b = charset8[i];
        charset_rgba[i] = b ? 0xFFFFFFFFu : 0x00000000u; /* white, alpha */
    }
    for (y = 0; y < 256; y++)
        for (x = 0; x < 256; x++)
        {
            int r = x, g = y, b = (x + y) / 2;
            grad_rgba[y * 256 + x] = 0xFF000000u | (b << 16) | (g << 8) | r;
        }
    for (y = 0; y < 128; y++)
        for (x = 0; x < 128; x++)
            lum128[y * 128 + x] = (unsigned char)((x * 2) ^ (y * 2));
    for (i = 0; i < 256 * 256; i++)
        rgba256[i] = 0xFF804020u ^ (i << 3);
    for (i = 0; i < 128 * 64; i++)
        lumsub[i] = (unsigned char)(i & 0xFF);
}

static void set2d(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -999, 999);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

static void draw_text_quad(float x, float y, float size)
{
    glBegin(MGL_FLATFAN);
    glTexCoord2f(0.0f, 0.0f);           glVertex2f(x,       y);
    glTexCoord2f(size, 0.0f);           glVertex2f(x + 8.0f, y);
    glTexCoord2f(size, size);           glVertex2f(x + 8.0f, y + 8.0f);
    glTexCoord2f(0.0f, size);           glVertex2f(x,       y + 8.0f);
    glEnd();
}

int main(void)
{
    double t0, t1;
    int i, j, k;
    GLuint tex = 1; /* game-style: sequential ids, no glGenTextures */

    setvbuf(stdout, NULL, _IONBF, 0);

    mark("m0: main entered\n");
    printf("[boot]\n");
    if (!MiniGLOpen()) { mark("m1: MiniGLOpen FAILED\n"); printf("MiniGLOpen failed\n"); return 20; }
    mark("m1: MiniGLOpen ok\n");
    mglChooseWindowMode(GL_TRUE);
    mglChoosePixelDepth(32);
    mglChooseVertexBufferSize(4000);
    if (!mglCreateContext(0, 0, 640, 480)) { mark("m2: CreateContext FAILED\n"); printf("CreateContext failed\n"); MiniGLClose(); return 20; }
    mark("m2: context ok\n");
    printf("context ok\n");
    set2d(640, 480);
    build_patterns();

    /* ---------- visual: charset texture + flatfan text ---------- */
    stage("S1 clear-black");
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    mglWriteShotPPM("RAM:probe_s1_black.ppm");
    printf("s1 ppm written\n");

    stage("S2 text render (charset rgba + flatfan)");
    tex++;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, charset_rgba);
    chk("S2 upload");
    glEnable(GL_TEXTURE_2D);
    glClear(GL_COLOR_BUFFER_BIT);
    /* 16x8 grid of glyphs = 128 glyphs, like a full console */
    for (j = 0; j < 8; j++)
        for (i = 0; i < 16; i++)
            draw_text_quad((float)(i * 40), (float)(j * 60), 1.0f / 16.0f);
    glFinish();
    chk("S2 draw");
    mglWriteShotPPM("RAM:probe_s2_text.ppm");
    printf("s2 ppm written\n");

    /* ---------- visual: lightmap GL_LUMINANCE blended quad ---------- */
    stage("S3 lightmap luminance blended");
    tex++;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 128, 128, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, lum128);
    chk("S3 lum upload");
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(MGL_FLATFAN);
    glTexCoord2f(0, 0);   glVertex2f(0, 0);
    glTexCoord2f(5.0f, 0);     glVertex2f(640, 0);
    glTexCoord2f(5.0f, 3.75f);  glVertex2f(640, 480);
    glTexCoord2f(0, 3.75f);    glVertex2f(0, 480);
    glEnd();
    glDisable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ZERO);
    glFinish();
    chk("S3 draw");
    mglWriteShotPPM("RAM:probe_s3_lum.ppm");
    printf("s3 ppm written\n");

    /* ---------- perf: texture uploads ---------- */
    stage("T1 upload rgba256 (submit+finish per iter)");
    tex++;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgba256);
    t0 = now_ds();
    for (i = 0; i < 20; i++)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA,
                        GL_UNSIGNED_BYTE, rgba256);
        glFinish();
    }
    t1 = now_ds();
    rep("texsub_rgba_256", 20, t0, t1);

    stage("T2 teximage lum128 (lightmap creation)");
    tex++;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 128, 128, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, lum128);
    t0 = now_ds();
    for (i = 0; i < 50; i++)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 128, 128, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, lum128);
        glFinish();
    }
    t1 = now_ds();
    rep("teximage_lum_128", 50, t0, t1);

    stage("T3 texsubimage lum 128x64 (lightmap update) <<< SUSPECT");
    t0 = now_ds();
    for (i = 0; i < 100; i++)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, (i & 63), 128, 64,
                        GL_LUMINANCE, GL_UNSIGNED_BYTE, lumsub);
        glFinish();
    }
    t1 = now_ds();
    rep("texsub_lum_128x64", 100, t0, t1);

    stage("T4 clear color+depth");
    glEnable(GL_DEPTH_TEST);
    t0 = now_ds();
    for (i = 0; i < 50; i++)
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glFinish();
    }
    t1 = now_ds();
    glDisable(GL_DEPTH_TEST);
    rep("clear_color_depth_320x240", 50, t0, t1);

    stage("T5 flatfan text-quad draw calls (submit only)");
    glBindTexture(GL_TEXTURE_2D, 1);
    t0 = now_ds();
    for (i = 0; i < 300; i++)
        draw_text_quad(8.0f, 8.0f, 1.0f / 16.0f);
    glFinish();
    t1 = now_ds();
    rep("flatfan_textquad", 300, t0, t1);

    stage("T6 vertex dispatch: 1 flatfan, 800 vertex pairs");
    t0 = now_ds();
    glBegin(MGL_FLATFAN);
    for (i = 0; i < 800; i++)
    {
        glTexCoord2f((float)(i & 63) / 64.0f, (float)(i % 32) / 32.0f);
        glVertex2f((float)(i & 255), (float)(i % 120));
    }
    glEnd();
    glFinish();
    t1 = now_ds();
    rep("flatfan_800verts", 800, t0, t1);

    stage("T7 GL_QUADS text-quad draw calls");
    t0 = now_ds();
    for (i = 0; i < 300; i++)
    {
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);       glVertex2f(8.0f, 8.0f);
        glTexCoord2f(0.0625f, 0.0f);    glVertex2f(16.0f, 8.0f);
        glTexCoord2f(0.0625f, 0.0625f); glVertex2f(16.0f, 16.0f);
        glTexCoord2f(0.0f, 0.0625f);    glVertex2f(8.0f, 16.0f);
        glEnd();
    }
    glFinish();
    t1 = now_ds();
    rep("glquads_textquad", 300, t0, t1);

    stage("T8 simulated console frame (clear + 128 glyphs + present)");
    t0 = now_ds();
    for (i = 0; i < 30; i++)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        for (j = 0; j < 8; j++)
            for (k = 0; k < 16; k++)
                draw_text_quad((float)(k * 20), (float)(j * 24), 1.0f / 16.0f);
        mglSwitchDisplay();
    }
    t1 = now_ds();
    rep("console_frame_full", 30, t0, t1);

    stage("F: internal-format matrix (upload err + texsub timing)");
    {
        /* game texture-id idiom */
        struct { const char *name; GLenum internal_; GLenum format; } fmts[] = {
            { "GL_RGBA/GL_RGBA",   GL_RGBA,   GL_RGBA   },
            { "GL_RGB/GL_RGBA",    GL_RGB,    GL_RGBA   },
            { "GL_RGB5_A1/GL_RGBA",GL_RGB5_A1,GL_RGBA   },
            { "GL_RGB/GL_RGB",     GL_RGB,    GL_RGB    },
            { "GL_LUM/GL_LUM",     GL_LUMINANCE, GL_LUMINANCE },
        };
        for (i = 0; i < 5; i++)
        {
            GLuint tid = (GLuint)(i + 1);
            GLenum e1, e2;
            double a, b;
            glBindTexture(GL_TEXTURE_2D, tid);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, fmts[i].internal_, 128, 128, 0,
                         fmts[i].format, GL_UNSIGNED_BYTE, charset_rgba);
            e1 = glGetError();
            a = now_ds();
            for (j = 0; j < 10; j++)
            {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 128, 128,
                                GL_RGBA, GL_UNSIGNED_BYTE, charset_rgba);
                glFinish();
            }
            b = now_ds();
            e2 = glGetError();
            printf("FMT %-20s teximg_err=0x%x  10x texsub128 %5.0f ms (err 0x%x)\n",
                   fmts[i].name, e1, (b - a) * 1000.0, e2);
            fflush(stdout);
            /* delete so the next id binding is clean */
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    stage("T9 full teximage re-upload timing (texsub workaround candidates)");
    {
        struct { const char *name; GLenum internal_; GLenum format; int bpp; } fmts[] = {
            { "teximg RGB5_A1 128", GL_RGB5_A1, GL_RGBA, 4 },
            { "teximg RGB     128", GL_RGB,     GL_RGBA, 4 },
            { "teximg RGB     256", GL_RGB,     GL_RGBA, 4 },
        };
        for (i = 0; i < 3; i++)
        {
            GLuint tid = (GLuint)(i + 20);
            int sz = (fmts[i].name[9] == '2') ? 256 : 128;
            unsigned int *src = (sz == 256) ? rgba256 : charset_rgba;
            glBindTexture(GL_TEXTURE_2D, tid);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            t0 = now_ds();
            for (j = 0; j < 30; j++)
            {
                glTexImage2D(GL_TEXTURE_2D, 0, fmts[i].internal_, sz, sz, 0,
                             fmts[i].format, GL_UNSIGNED_BYTE, src);
                glFinish();
            }
            t1 = now_ds();
            rep(fmts[i].name, 30, t0, t1);
            chk(fmts[i].name);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    stage("T10 MGL_UNSIGNED_SHORT_5_6_5 internal (game -lm_RGB path)");
    {
        GLuint tid = 30;
        glBindTexture(GL_TEXTURE_2D, tid);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, MGL_UNSIGNED_SHORT_5_6_5, 128, 128, 0,
                     MGL_UNSIGNED_SHORT_5_6_5, GL_UNSIGNED_BYTE, lum128);
        printf("5_6_5 teximg err=0x%x\n", glGetError());
        t0 = now_ds();
        for (i = 0; i < 20; i++)
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 128, 64,
                            MGL_UNSIGNED_SHORT_5_6_5, GL_UNSIGNED_BYTE, lumsub);
            glFinish();
        }
        t1 = now_ds();
        rep("texsub_565_128x64", 20, t0, t1);
        printf("5_6_5 texsub err=0x%x\n", glGetError());
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    stage("S4 final ppm");
    glClear(GL_COLOR_BUFFER_BIT);
    for (j = 0; j < 8; j++)
        for (i = 0; i < 16; i++)
            draw_text_quad((float)(i * 40), (float)(j * 60), 1.0f / 16.0f);
    glFinish();
    mglWriteShotPPM("RAM:probe_s4_final.ppm");
    printf("s4 ppm written\n");

    printf("[done] all stages completed\n");
    mglDeleteContext();
    MiniGLClose();
    return 0;
}
