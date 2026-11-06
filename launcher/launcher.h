/*
 * BlitzQuakePPC MiniGL Launcher by Mirq -- shared declarations.
 *
 * A 68k AmigaOS front-end for the WarpOS/PPC MiniGL GLQuake client
 * (glquakeWOS).  It never links against the game; it only assembles the
 * client command line, starts the 68k MiniGL host from the game directory
 * and launches the client in the background ("Run >NIL:" style).
 */
#ifndef LQ_LAUNCHER_H
#define LQ_LAUNCHER_H

#define LQ_STRLEN 256

/* One fixed name per role: the launcher only ever starts these files from its
 * own drawer.  The struct fields below keep the names so the on-target tests
 * can substitute their own binaries, but there is no config/UI override. */
#define LQ_CLIENT  "glquakeWOS"   /* PPC MiniGL client                    */
#define LQ_HOST    "mglhost"      /* 68k MiniGL host                      */
#define LQ_HOSTLOG "mglhost.log"  /* host stdout/stderr, overwritten/run  */
#define LQ_LAUNCHLOG "launch.log" /* exact commands, appended per Run     */
#define LQ_CLIENTLOG "client.log" /* client console output (-stdout)      */

typedef struct LqConfig {
    char gamedir[LQ_STRLEN];   /* drawer with client, host, DLL and id1/ */
    char client[LQ_STRLEN];    /* normally LQ_CLIENT (tests only change) */
    int  width, height, depth; /* -width/-height/-bpp (from ASL)          */
    int  windowmode;           /* -windowmode                             */
    int  nosound;              /* -nosound                                */
    char ahiid[32];            /* -ahiid <hex>                            */
    char audspeed[32];         /* -audspeed <hz>                          */
    char sndflush[16];         /* -sndflush native|exec                   */
    int  noudp;                /* -noudp                                  */
    int  nojoy;                /* -nojoy                                  */
    int  nomouse;              /* -nomouse                                */
    int  condebug;             /* -condebug                               */
    char map[64];              /* +map <name>                             */
    char extra[256];           /* raw extra arguments                     */
    int  initppc;              /* run initppcpath first                   */
    char initppcpath[LQ_STRLEN];
    int  starthost;            /* start the MiniGL host first             */
    char host[LQ_STRLEN];      /* normally LQ_HOST (tests only change)    */
    int  hoststack;            /* host stack size in bytes                */
    int  hostwait;             /* seconds to wait for the host port       */
    char hostport[64];         /* MiniGL host port name                   */
    int  stack;                /* client stack size in bytes              */
} LqConfig;

/* config.c */
void lq_config_defaults(LqConfig *c);
void lq_config_program_dir(char *buf, int len);
int  lq_config_load(LqConfig *c);
int  lq_config_save(const LqConfig *c);

/* launch.c */
void lq_log(const LqConfig *c, const char *fmt, ...);
int  lq_build_args(const LqConfig *c, char *buf, int buflen);
int  lq_host_port_present(const LqConfig *c);
int  lq_client_running(void);
int  lq_run_initppc(const LqConfig *c, char *err, int errlen);
int  lq_start_host(const LqConfig *c, char *err, int errlen);
int  lq_start_client(const LqConfig *c, char *err, int errlen);

/* Return codes used by the launch helpers:
 *   1 = success
 *   0 = hard failure (err holds a message)
 *   2 = warning / state needs confirmation (err holds a message)
 */

/* gui.c */
void lq_gui_run(LqConfig *c);

/* Debug tracing, enabled with -DLQ_TRACE; writes to T:lq_trace.log. */
#ifdef LQ_TRACE
void lq_trace(const char *msg);
#define LQ_TRACE_MSG(m) lq_trace(m)
#else
#define LQ_TRACE_MSG(m) ((void)0)
#endif

#endif /* LQ_LAUNCHER_H */
