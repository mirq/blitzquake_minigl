#ifndef __FRAME_PROFILE_H
#define __FRAME_PROFILE_H

// Frame-level profiler for the WarpOS PPC GLQuake client.
// Plan: PERFORMANCE_PLAN_2026-09-20_0024.md (phase 1).
//
// Runtime opt-in: launch with -frameprofile [path]. When disabled the cost
// is one flag test per enter/exit. Timing uses the PPC timebase (mftb),
// NOT Sys_FloatTime(): on WOS that is DateStamp (20 ms quantum) through the
// dos.library gateway. Calibration is one 0.5 s Delay(25) at FP_Init.
//
// The dump is written once, after the timing interval, by FP_AutoDump()
// (called from CL_FinishTimeDemo and cleanup()). Format is plain text with
// "FP " prefixed lines; nested sections overlap by construction, so totals
// must not be added across levels (SCREEN contains VIEW/HUD/PRESENT, SCENE
// contains WORLD/ENTBRUSH/ENTALIAS/PARTICLES, WORLD contains VIS/CHAINS/
// LMBLEND, and so on).
// -framewindow <first> <last> retains up to 32 consecutive FP frames even
// when an A/B change removes them from the worst-frame table. FP numbers keep
// their historical meaning; host/demo frame and server time are also logged.

#if defined(WOS) && defined(__PPC__)

enum {
	FP_SCREEN = 0,   // SCR_UpdateScreen total (contains VIEW, HUD, PRESENT)
	FP_VIEW,         // V_RenderView (3D scene)
	FP_SCENE,        // R_RenderScene (contains WORLD/ents/particles)
	FP_WORLD,        // R_DrawWorld (contains VIS/CHAINS/LMBLEND)
	FP_VIS,          // R_RecursiveWorldNode (BSP/PVS traversal)
	FP_CHAINS,       // DrawTextureChains
	FP_LMBLEND,      // R_BlendLightmaps
	FP_LMBUILD,      // R_BuildLightMap / Color CPU rebuilds
	FP_LMUPLOAD,     // elapsed glTexSubImage2D call, INCLUDING synchronous
	                 // DLL staging/upload and waits; not deferred GPU work
	FP_ENTBRUSH,     // R_DrawEntitiesOnList(1): brush models
	FP_ENTALIAS,     // R_DrawEntitiesOnList(2): alias models + sprites
	FP_PARTICLES,    // R_DrawParticles
	FP_WATER,        // R_DrawWaterSurfaces
	FP_MIRROR,       // R_Mirror
	FP_GLOW,         // R_RenderGlows + R_RenderDlights
	FP_CLEAR,        // R_Clear
	FP_BLEND,        // R_PolyBlend (pickup/damage/contents overlay)
	FP_HUD,          // 2D pass: tile/sbar/console/menus + V_UpdatePalette
	FP_PRESENT,      // GL_EndRendering -> mglSwitchDisplay
	FP_SECTIONS
};

void   FP_Init (void);              // after COM_InitArgv; enables + calibrates
unsigned long FP_Enter (int section);   // returns start tick (0 when disabled)
void   FP_Exit (int section, unsigned long start);
// Closes FP_LMUPLOAD and records atlas/source-rectangle details in RAM.
// site: 0 = sequential, 1 = underwater, 2 = texture-sorted blend pass.
void   FP_ExitLMUpload (unsigned long start, int atlas, int top, int rows,
                       unsigned bytes, int site);
void   FP_FrameBegin (void);
void   FP_FrameEnd (void);
void   FP_CountLMUpload (unsigned bytes);   // lightmap TexSubImage payload
void   FP_CountOverlay (int first_use);     // Draw_AlphaFill draws (level>0)
void   FP_AutoDump (void);          // write the report once

#else /* !WOS/PPC: compile out entirely */

#define FP_Init()                 ((void)0)
#define FP_Enter(s)               (0UL)
#define FP_Exit(s, t)             ((void)0)
#define FP_ExitLMUpload(t,a,y,h,b,s) ((void)0)
#define FP_FrameBegin()           ((void)0)
#define FP_FrameEnd()             ((void)0)
#define FP_CountLMUpload(b)       ((void)0)
#define FP_CountOverlay(f)        ((void)0)
#define FP_AutoDump()             ((void)0)

#endif
#endif // __FRAME_PROFILE_H
