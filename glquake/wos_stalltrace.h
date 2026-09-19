/* Opt-in stall diagnosis. No GL queries, heap allocation or PPC timer calls.
 * Detail is truncated by the writer; non-WOS builds compile calls away. */
#ifndef WOS_STALLTRACE_H
#define WOS_STALLTRACE_H
#ifndef WOS_DIAGNOSTICS
#define WOS_DIAGNOSTICS 0
#endif
#if defined(WOS) && WOS_DIAGNOSTICS
void Sys_WOSTrace(const char *s);
void Sys_WOSTraceFrame(const char *tag, int frame);
void Sys_WOSStallTrace(const char *stage, const char *detail, int value);
#define WOS_STALL(stage, detail, value) Sys_WOSStallTrace(stage, detail, value)
#else
/* Real declarations keep legacy block-scope externs valid. The empty
 * bodies are visible to the optimizer, removing calls and their strings. */
#ifdef __GNUC__
#define WOS_DIAG_STUB static __inline__
#else
#define WOS_DIAG_STUB static
#endif
WOS_DIAG_STUB void Sys_WOSTrace(const char *s) { (void)s; }
WOS_DIAG_STUB void Sys_WOSTraceFrame(const char *tag, int frame)
{ (void)tag; (void)frame; }
WOS_DIAG_STUB void Sys_WOSStallTrace(const char *stage, const char *detail, int value)
{ (void)stage; (void)detail; (void)value; }
#undef WOS_DIAG_STUB
#define WOS_STALL(stage, detail, value) ((void)0)
#endif
#endif
