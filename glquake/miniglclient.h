/*
 * miniglclient.h - MiniGL dispatch client compatibility shim
 *
 * BlitzQuake was written against the classic Hyperion MiniGL API
 * (mgl.library): global "mini_CurrentContext", MGLInit()/MGLTerm(),
 * and gl*() macros expanding to GL*(mini_CurrentContext, ...) calls.
 *
 * The target minigl.library (PiStorm SDK) is a dispatch-based client
 * library instead: the client opens it with MiniGLOpen(), then calls
 * functions through the MGLDispatchTable "MiniGLDispatch" pointer,
 * e.g. MiniGLDispatch->GLBegin(MGLD_CTX, mode).
 *
 * This header maps the classic API surface onto the dispatch client:
 *  - include <proto/minigl.h> which pulls in libraries/minigl_dispatch.h
 *    and redefines gl*()/mgl*() to MGLD_gl*()/MGLD_mgl*() wrappers that
 *    call through the dispatch table with MGLD_CTX
 *  - mini_CurrentContext maps to (*MiniGLDispatch->currentContext)
 *  - MGLInit()/MGLTerm() map to MiniGLOpen()/MiniGLClose()
 *
 * Note: must be included AFTER mgl/gl.h has been processed, because
 * gl.h declares "extern GLcontext mini_CurrentContext;" and function
 * prototypes for MGLInit()/MGLTerm() which would be clobbered by the
 * macros below.
 */

#ifndef MINIGLCLIENT_H
#define MINIGLCLIENT_H

#include <proto/minigl.h>

#define mini_CurrentContext		(*MiniGLDispatch->currentContext)
#define MGLInit()			MiniGLOpen()
#define MGLTerm()			MiniGLClose()

#endif /* MINIGLCLIENT_H */