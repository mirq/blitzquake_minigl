/*
 * gui.c -- GadTools window for BlitzQuakePPC MiniGL Launcher by Mirq.
 *
 * Three buttons (Run, Save Settings, Quit), an ASL screen mode picker for
 * the resolution and the launch options.  All strings live in the LqConfig
 * buffers; GadTools string gadgets edit them in place.
 */
#pragma amiga-align
#include <exec/types.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <libraries/asl.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/asl.h>
#include <proto/dos.h>
#pragma default-align

#include <string.h>
#include <stdio.h>

#include "launcher.h"

#define LQ_TITLE  "BlitzQuakePPC MiniGL Launcher by Mirq"
#define LQ_WIN_W  620
#define LQ_WIN_H  156

static struct Window *lq_win;
static APTR           lq_vi;
static struct Gadget *lq_glist;   /* dummy head from CreateContext()  */
static struct Gadget *lq_last;    /* last created gadget              */
static APTR           lq_smr;     /* ASL screen mode requester        */
static LqConfig      *lq_cfg;
static int            lq_done;

static struct Gadget *g_res, *g_select;
static struct Gadget *g_windowmode, *g_sound, *g_ahiid, *g_audspeed, *g_flush;
static struct Gadget *g_noudp, *g_nojoy, *g_nomouse, *g_condebug;
static struct Gadget *g_map, *g_extra, *g_initppc, *g_starthost;
static struct Gadget *g_run, *g_save, *g_quit, *g_status;

static struct TextAttr lq_font = { (STRPTR)"topaz.font", 8, 0, 0 };

static char lq_resbuf[64];
static char lq_status[160];

static STRPTR lq_sound_labels[] = { (STRPTR)"AHI", (STRPTR)"No sound", NULL };
static STRPTR lq_flush_labels[] = { (STRPTR)"native", (STRPTR)"exec", NULL };

static void lq_tag(struct TagItem *t, int *n, ULONG tag, ULONG data)
{
    t[*n].ti_Tag = tag;
    t[*n].ti_Data = data;
    (*n)++;
}

static struct Gadget *lq_gad(ULONG kind, WORD x, WORD y, WORD w, WORD h,
                             const char *text, ULONG flags, struct TagItem *tags)
{
    struct NewGadget ng;
    struct Gadget *g;

    memset(&ng, 0, sizeof(ng));
    ng.ng_LeftEdge = x;
    ng.ng_TopEdge = y;
    ng.ng_Width = w;
    ng.ng_Height = h;
    ng.ng_GadgetText = (CONST_STRPTR)text;
    ng.ng_TextAttr = &lq_font;
    ng.ng_Flags = flags;
    ng.ng_VisualInfo = lq_vi;

    g = CreateGadgetA(kind, lq_last, &ng, tags);
    if (g)
        lq_last = g;
    {
        char tb[40];
        sprintf(tb, "gui: gad %ld %s", (long)kind, g ? "ok" : "FAIL");
        LQ_TRACE_MSG(tb);
    }
    return g;
}

static struct Gadget *lq_label(WORD x, WORD y, WORD w, const char *text)
{
    struct TagItem t[4];
    int n = 0;

    lq_tag(t, &n, GTTX_Text, (ULONG)text);
    lq_tag(t, &n, GTTX_CopyText, TRUE);
    lq_tag(t, &n, TAG_DONE, 0);
    return lq_gad(TEXT_KIND, x, y, w, 12, text, PLACETEXT_IN, t);
}

static struct Gadget *lq_string(WORD x, WORD y, WORD w, char *buf, int max)
{
    struct TagItem t[4];
    int n = 0;

    lq_tag(t, &n, GTST_String, (ULONG)buf);
    lq_tag(t, &n, GTST_MaxChars, (ULONG)max);
    lq_tag(t, &n, TAG_DONE, 0);
    return lq_gad(STRING_KIND, x, y, w, 12, NULL, 0, t);
}

static struct Gadget *lq_checkbox(WORD x, WORD y, WORD w, const char *text, int checked)
{
    struct TagItem t[3];
    int n = 0;

    lq_tag(t, &n, GTCB_Checked, (ULONG)(checked != 0));
    lq_tag(t, &n, TAG_DONE, 0);
    return lq_gad(CHECKBOX_KIND, x, y, w, 12, text, PLACETEXT_RIGHT, t);
}

static struct Gadget *lq_cycle(WORD x, WORD y, WORD w, STRPTR *labels, int active)
{
    struct TagItem t[4];
    int n = 0;

    lq_tag(t, &n, GTCY_Labels, (ULONG)labels);
    lq_tag(t, &n, GTCY_Active, (ULONG)active);
    lq_tag(t, &n, TAG_DONE, 0);
    return lq_gad(CYCLE_KIND, x, y, w, 12, NULL, 0, t);
}

static struct Gadget *lq_button(WORD x, WORD y, WORD w, const char *text)
{
    struct TagItem t[2];
    int n = 0;

    lq_tag(t, &n, TAG_DONE, 0);
    return lq_gad(BUTTON_KIND, x, y, w, 13, text, PLACETEXT_IN, t);
}

static int lq_build_gadgets(void)
{
    lq_glist = NULL;
    if (!CreateContext(&lq_glist))
        return 0;
    lq_last = lq_glist;

    /* Dynamic text gadgets must have valid initial text in ng_GadgetText. */
    sprintf(lq_resbuf, "%ldx%ldx%ld",
            (long)lq_cfg->width, (long)lq_cfg->height, (long)lq_cfg->depth);
    strcpy(lq_status, "Ready.");

    /* Row 0: resolution, start map */
    if (!lq_label(4, 6, 96, "Resolution:")) return 0;
    {
        struct TagItem t[3];
        int n = 0;
        lq_tag(t, &n, GTTX_Text, (ULONG)lq_resbuf);
        lq_tag(t, &n, TAG_DONE, 0);
        if (!(g_res = lq_gad(TEXT_KIND, 104, 6, 96, 12, lq_resbuf, PLACETEXT_IN, t))) return 0;
    }
    if (!(g_select = lq_button(208, 6, 84, "Select..."))) return 0;
    if (!lq_label(300, 6, 88, "Start map:")) return 0;
    if (!(g_map = lq_string(392, 6, 188, lq_cfg->map, sizeof(lq_cfg->map) - 1))) return 0;

    /* Row 1: sound */
    if (!lq_label(4, 24, 56, "Sound:")) return 0;
    if (!(g_sound = lq_cycle(64, 24, 80, lq_sound_labels, lq_cfg->nosound ? 1 : 0))) return 0;
    if (!lq_label(152, 24, 64, "AHI ID:")) return 0;
    if (!(g_ahiid = lq_string(220, 24, 56, lq_cfg->ahiid, sizeof(lq_cfg->ahiid) - 1))) return 0;
    if (!lq_label(284, 24, 80, "Mix rate:")) return 0;
    if (!(g_audspeed = lq_string(368, 24, 56, lq_cfg->audspeed, sizeof(lq_cfg->audspeed) - 1))) return 0;
    if (!lq_label(432, 24, 56, "Flush:")) return 0;
    if (!(g_flush = lq_cycle(492, 24, 64, lq_flush_labels,
                             strcmp(lq_cfg->sndflush, "exec") == 0 ? 1 : 0))) return 0;

    /* Row 2: video / network */
    if (!(g_windowmode = lq_checkbox(4, 42, 196, "Windowed (-windowmode)", lq_cfg->windowmode))) return 0;
    if (!(g_noudp = lq_checkbox(208, 42, 212, "Disable network (-noudp)", lq_cfg->noudp))) return 0;

    /* Row 3: input / logging */
    if (!(g_nojoy = lq_checkbox(4, 60, 180, "No joystick (-nojoy)", lq_cfg->nojoy))) return 0;
    if (!(g_nomouse = lq_checkbox(192, 60, 172, "No mouse (-nomouse)", lq_cfg->nomouse))) return 0;
    if (!(g_condebug = lq_checkbox(372, 60, 240, "Console log (-condebug)", lq_cfg->condebug))) return 0;

    /* Row 4: session.  Client and host are looked up in the launcher's own
     * drawer, so there are no path fields. */
    if (!(g_initppc = lq_checkbox(4, 78, 180, "Run C:InitPPC first", lq_cfg->initppc))) return 0;
    if (!(g_starthost = lq_checkbox(192, 78, 160, "Start MiniGL host", lq_cfg->starthost))) return 0;
    if (!lq_label(360, 78, 200, "mglhost in this drawer")) return 0;

    /* Row 5: extra arguments */
    if (!lq_label(4, 96, 88, "Extra args:")) return 0;
    if (!(g_extra = lq_string(96, 96, 484, lq_cfg->extra, sizeof(lq_cfg->extra) - 1))) return 0;

    /* Row 6: buttons and status */
    if (!(g_run = lq_button(4, 118, 60, "Run"))) return 0;
    if (!(g_save = lq_button(72, 118, 120, "Save Settings"))) return 0;
    if (!(g_quit = lq_button(200, 118, 60, "Quit"))) return 0;
    {
        struct TagItem t[3];
        int n = 0;
        lq_tag(t, &n, GTTX_Text, (ULONG)lq_status);
        lq_tag(t, &n, TAG_DONE, 0);
        if (!(g_status = lq_gad(TEXT_KIND, 268, 118, 344, 14, lq_status, PLACETEXT_IN, t))) return 0;
    }

    return 1;
}

/* Gadget state getters use explicit tag lists (no varargs macros). */
static ULONG lq_checked(struct Gadget *g)
{
    struct TagItem t[2];
    ULONG v = 0;

    t[0].ti_Tag = GTCB_Checked;
    t[0].ti_Data = (ULONG)&v;
    t[1].ti_Tag = TAG_DONE;
    t[1].ti_Data = 0;
    GT_GetGadgetAttrsA(g, lq_win, NULL, t);
    return v;
}

static ULONG lq_active(struct Gadget *g)
{
    struct TagItem t[2];
    ULONG v = 0;

    t[0].ti_Tag = GTCY_Active;
    t[0].ti_Data = (ULONG)&v;
    t[1].ti_Tag = TAG_DONE;
    t[1].ti_Data = 0;
    GT_GetGadgetAttrsA(g, lq_win, NULL, t);
    return v;
}

static void lq_set_text(struct Gadget *g, const char *text)
{
    struct TagItem t[2];

    t[0].ti_Tag = GTTX_Text;
    t[0].ti_Data = (ULONG)text;
    t[1].ti_Tag = TAG_DONE;
    t[1].ti_Data = 0;
    GT_BeginRefresh(lq_win);
    GT_SetGadgetAttrsA(g, lq_win, NULL, t);
    GT_EndRefresh(lq_win, TRUE);
}

static void lq_set_status(const char *s)
{
    strncpy(lq_status, s, sizeof(lq_status) - 1);
    lq_status[sizeof(lq_status) - 1] = 0;
    if (lq_win && g_status)
        lq_set_text(g_status, lq_status);
}

static void lq_update_res(void)
{
    sprintf(lq_resbuf, "%ldx%ldx%ld",
            (long)lq_cfg->width, (long)lq_cfg->height, (long)lq_cfg->depth);
    if (lq_win && g_res)
        lq_set_text(g_res, lq_resbuf);
}

static void lq_easy(const char *msg)
{
    struct EasyStruct es;

    es.es_StructSize = sizeof(es);
    es.es_Flags = 0;
    es.es_Title = (CONST_STRPTR)LQ_TITLE;
    es.es_TextFormat = (CONST_STRPTR)msg;
    es.es_GadgetFormat = (CONST_STRPTR)"OK";
    EasyRequest(lq_win, &es, NULL);
}

static int lq_confirm(const char *msg)
{
    struct EasyStruct es;

    es.es_StructSize = sizeof(es);
    es.es_Flags = 0;
    es.es_Title = (CONST_STRPTR)LQ_TITLE;
    es.es_TextFormat = (CONST_STRPTR)msg;
    es.es_GadgetFormat = (CONST_STRPTR)"Continue|Cancel";
    return EasyRequest(lq_win, &es, NULL) == 1;
}

static void lq_sync(void)
{
    lq_cfg->windowmode = lq_checked(g_windowmode) != 0;
    lq_cfg->nosound = lq_active(g_sound) == 1;
    lq_cfg->noudp = lq_checked(g_noudp) != 0;
    lq_cfg->nojoy = lq_checked(g_nojoy) != 0;
    lq_cfg->nomouse = lq_checked(g_nomouse) != 0;
    lq_cfg->condebug = lq_checked(g_condebug) != 0;
    lq_cfg->initppc = lq_checked(g_initppc) != 0;
    lq_cfg->starthost = lq_checked(g_starthost) != 0;
    strncpy(lq_cfg->sndflush, lq_active(g_flush) ? "exec" : "native",
            sizeof(lq_cfg->sndflush) - 1);
    lq_cfg->sndflush[sizeof(lq_cfg->sndflush) - 1] = 0;
}

static void lq_do_select(void)
{
    struct TagItem t[24];
    int n = 0;

    if (!lq_smr)
    {
        lq_easy("asl.library screen mode requester is not available.");
        return;
    }

    lq_tag(t, &n, ASLSM_Window, (ULONG)lq_win);
    lq_tag(t, &n, ASLSM_SleepWindow, TRUE);
    lq_tag(t, &n, ASLSM_PrivateIDCMP, TRUE);
    lq_tag(t, &n, ASLSM_TitleText, (ULONG)"Select screen mode");
    lq_tag(t, &n, ASLSM_InitialDisplayWidth, (ULONG)lq_cfg->width);
    lq_tag(t, &n, ASLSM_InitialDisplayHeight, (ULONG)lq_cfg->height);
    lq_tag(t, &n, ASLSM_InitialDisplayDepth, (ULONG)lq_cfg->depth);
    lq_tag(t, &n, ASLSM_DoWidth, TRUE);
    lq_tag(t, &n, ASLSM_DoHeight, TRUE);
    lq_tag(t, &n, ASLSM_DoDepth, TRUE);
    lq_tag(t, &n, ASLSM_MinWidth, 320);
    lq_tag(t, &n, ASLSM_MaxWidth, 1920);
    lq_tag(t, &n, ASLSM_MinHeight, 200);
    lq_tag(t, &n, ASLSM_MaxHeight, 1200);
    lq_tag(t, &n, ASLSM_MinDepth, 8);
    lq_tag(t, &n, ASLSM_MaxDepth, 32);
    lq_tag(t, &n, TAG_DONE, 0);

    if (AslRequest(lq_smr, t))
    {
        struct ScreenModeRequester *smr = (struct ScreenModeRequester *)lq_smr;
        int w = (int)smr->sm_DisplayWidth;
        int h = (int)smr->sm_DisplayHeight;
        int d = (int)smr->sm_DisplayDepth;

        if (w >= 320 && h >= 200)
        {
            if (d == 15) d = 16;    /* 15-bit modes are used as 16-bit */
            if (d < 8)   d = 16;
            lq_cfg->width = w;
            lq_cfg->height = h;
            lq_cfg->depth = d;
            lq_update_res();
        }
    }
}

static void lq_do_save(void)
{
    lq_sync();
    if (lq_config_save(lq_cfg))
        lq_set_status("Settings saved.");
    else
    {
        lq_set_status("Save failed!");
        lq_easy("Could not save settings to\nENVARC:BlitzQuakePPC/Launcher.cfg");
    }
}

static void lq_do_run(void)
{
    char err[320];
    int rc;

    lq_sync();

    lq_log(lq_cfg, "--- Run ---");

    if (!lq_cfg->gamedir[0])
    {
        lq_easy("Set the game directory first.");
        return;
    }

    if (lq_cfg->initppc)
    {
        lq_set_status("Running InitPPC...");
        rc = lq_run_initppc(lq_cfg, err, sizeof(err));
        if (rc != 1 && !lq_confirm(err))
        {
            lq_set_status("Cancelled.");
            return;
        }
    }

    if (lq_cfg->starthost)
    {
        lq_set_status("Starting MiniGL host, please wait...");
        rc = lq_start_host(lq_cfg, err, sizeof(err));
        if (rc != 1)
        {
            if (rc == 2 && lq_confirm(err))
            {
                /* user insists: fall through and start the client */
            }
            else
            {
                lq_set_status(rc == 2 ? "Cancelled." : "Host start failed.");
                if (rc != 2)
                    lq_easy(err);
                return;
            }
        }
    }

    lq_set_status("Starting client...");
    rc = lq_start_client(lq_cfg, err, sizeof(err));
    if (rc != 1)
    {
        lq_set_status(rc == 2 ? "Client already running." : "Client start failed.");
        lq_easy(err);
        return;
    }
    lq_set_status("Client started (background).");
}

static int lq_gui_open(void)
{
    LQ_TRACE_MSG("gui: open enter");
    lq_win = OpenWindowTags(NULL,
            WA_Left, 40,
            WA_Top, 24,
            WA_Width, LQ_WIN_W,
            WA_Height, LQ_WIN_H,
            WA_Title, (ULONG)LQ_TITLE,
            WA_Flags, WFLG_SMART_REFRESH | WFLG_ACTIVATE | WFLG_DRAGBAR |
                      WFLG_DEPTHGADGET | WFLG_CLOSEGADGET | WFLG_GIMMEZEROZERO,
            WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_GADGETUP | IDCMP_GADGETDOWN,
            TAG_DONE);
    if (!lq_win)
        return 0;
    LQ_TRACE_MSG("gui: window open");

    lq_vi = GetVisualInfoA(lq_win->WScreen, NULL);
    if (!lq_vi)
    {
        CloseWindow(lq_win);
        lq_win = NULL;
        return 0;
    }
    LQ_TRACE_MSG("gui: visual info");

    if (!lq_build_gadgets())
    {
        FreeVisualInfo(lq_vi);
        lq_vi = NULL;
        CloseWindow(lq_win);
        lq_win = NULL;
        return 0;
    }
    LQ_TRACE_MSG("gui: gadgets built");

    AddGList(lq_win, lq_glist, -1, -1, NULL);
    LQ_TRACE_MSG("gui: addglist");
    RefreshGList(lq_glist, lq_win, NULL, -1);
    LQ_TRACE_MSG("gui: refresh");

    lq_smr = AllocAslRequest(ASL_ScreenModeRequest, NULL);
    LQ_TRACE_MSG("gui: asl");
    return 1;
}

static void lq_gui_close(void)
{
    if (lq_smr)
    {
        FreeAslRequest(lq_smr);
        lq_smr = NULL;
    }
    if (lq_win)
    {
        if (lq_glist)
        {
            RemoveGList(lq_win, lq_glist, -1);
            FreeGadgets(lq_glist);
            lq_glist = NULL;
        }
        CloseWindow(lq_win);
        lq_win = NULL;
    }
    if (lq_vi)
    {
        FreeVisualInfo(lq_vi);
        lq_vi = NULL;
    }
}

void lq_gui_run(LqConfig *c)
{
    lq_cfg = c;
    lq_done = 0;

    if (!lq_gui_open())
    {
        lq_easy("Cannot open the launcher window.");
        return;
    }
    LQ_TRACE_MSG("gui: open ok");

    lq_update_res();
    lq_set_status("Ready.");
    LQ_TRACE_MSG("gui: loop enter");

    while (!lq_done)
    {
        struct IntuiMessage *im;

        WaitPort(lq_win->UserPort);
        while ((im = GT_GetIMsg(lq_win->UserPort)) != NULL)
        {
            ULONG cls = im->Class;
            APTR iaddr = im->IAddress;

            GT_ReplyIMsg(im);

            if (cls == IDCMP_CLOSEWINDOW)
                lq_done = 1;
            else if (cls == IDCMP_GADGETUP)
            {
                struct Gadget *g = (struct Gadget *)iaddr;

                if (g == g_run)
                    lq_do_run();
                else if (g == g_save)
                    lq_do_save();
                else if (g == g_quit)
                    lq_done = 1;
                else if (g == g_select)
                    lq_do_select();
            }
        }
    }

    lq_gui_close();
}
