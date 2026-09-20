/*
 * wos_ahi_abi.h - 68k-compatible AHI device request layout for WarpOS/PPC.
 *
 * Why this file exists:
 *   The MOS2WOS os-includeppc headers are OS4-style: struct Message,
 *   IORequest, IOStdReq and AHIRequest get natural PPC alignment. The
 *   AmigaOS 68k device ABI (and every 68k driver, AHI included) uses the
 *   2-byte packed layout instead. Passing a naturally-aligned request to
 *   OpenDevice/SendIO makes the 68k driver read io_Message.mn_ReplyPort,
 *   io_Length, ahir_Type ... at the wrong offsets: the request never
 *   completes and WaitIO hangs the machine (this was the wos_ahi stage-3
 *   hang).
 *
 *   Verified against m68k-amigaos-gcc (AmigaOS 3.x headers):
 *     sizeof(Message)=20   mn_ReplyPort=14  mn_Length=18
 *     sizeof(IORequest)=32 io_Device=20     io_Command=28
 *     sizeof(IOStdReq)=48  io_Length=36     io_Data=40
 *     sizeof(AHIRequest)=80 ahir_Version=48 ahir_Type=60
 *                           ahir_Frequency=64 ahir_Link=76
 *
 * Only AHI device I/O needs this on WarpOS; the network structs come from
 * netinclude, which already forces pack(2) under __PPC__.
 */

#ifndef WOS_AHI_ABI_H
#define WOS_AHI_ABI_H

#include <exec/types.h>

struct wos_Node {
  UBYTE  ln_Type;
  BYTE   ln_Pri;
  char  *ln_Name;
  struct wos_Node *ln_Next;
  struct wos_Node *ln_Prev;
} __attribute__((packed));

struct wos_Message {
  struct wos_Node mn_Node;      /* 14 bytes */
  APTR   mn_ReplyPort;          /* struct MsgPort * */
  UWORD  mn_Length;
} __attribute__((packed));

struct wos_IORequest {
  struct wos_Message io_Message;
  APTR   io_Device;
  APTR   io_Unit;
  UWORD  io_Command;
  UBYTE  io_Flags;
  BYTE   io_Error;
} __attribute__((packed));

struct wos_IOStdReq {
  struct wos_Message io_Message;   /* the MOS2WOS IOStdReq inlines the     */
  APTR   io_Device;                /* IORequest fields (no io_Request      */
  APTR   io_Unit;                  /* member); the byte layout is the same */
  UWORD  io_Command;               /* as the 68k struct.                   */
  UBYTE  io_Flags;
  BYTE   io_Error;
  ULONG  io_Actual;
  ULONG  io_Length;
  APTR   io_Data;
  ULONG  io_Offset;
} __attribute__((packed));

struct wos_AHIRequest {
  struct wos_IOStdReq ahir_Std;
  UWORD  ahir_Version;
  UWORD  ahir_Pad1;
  ULONG  ahir_Private[2];
  ULONG  ahir_Type;
  ULONG  ahir_Frequency;
  LONG   ahir_Volume;           /* Fixed */
  LONG   ahir_Position;         /* Fixed */
  struct wos_AHIRequest *ahir_Link;
} __attribute__((packed));

/* 68k layout of struct Library (lib_Version=20, lib_IdString=24, size=34),
   for reading the device base returned in io_Device. */
struct wos_Library {
  UBYTE  ln_Type;
  BYTE   ln_Pri;
  char  *ln_Name;
  struct wos_Library *ln_Next;
  struct wos_Library *ln_Prev;
  UBYTE  lib_Flags;
  UBYTE  lib_Pad;
  UWORD  lib_NegSize;
  UWORD  lib_PosSize;
  UWORD  lib_Version;
  UWORD  lib_Revision;
  APTR   lib_IdString;
  ULONG  lib_Sum;
  UWORD  lib_OpenCnt;
} __attribute__((packed));

/* Compile-time ABI guards: fail the build if a header change shifts these. */
typedef char wos_abi_msg_size[sizeof(struct wos_Message) == 20 ? 1 : -1];
typedef char wos_abi_ioreq_size[sizeof(struct wos_IORequest) == 32 ? 1 : -1];
typedef char wos_abi_iostd_size[sizeof(struct wos_IOStdReq) == 48 ? 1 : -1];
typedef char wos_abi_ahireq_size[sizeof(struct wos_AHIRequest) == 80 ? 1 : -1];
typedef char wos_abi_library_size[sizeof(struct wos_Library) == 34 ? 1 : -1];

#endif /* WOS_AHI_ABI_H */
