/*
 * wos_ahimodes.c - enumerate AHI audio modes (IDs + attributes).
 *
 * Used to find a cheaper Paula mode than the default
 * "Paula:HiFi 14 bit stereo++ calibrated" (0x0002000d), whose per-sample
 * mixing + calibration load costs several ms/frame on this 68060.
 *
 * Build: make -f Makefile.GCCAmigaWOS_probe_io PROBE=wos_ahimodes
 */

#include <exec/types.h>
#include <exec/ports.h>
#include <exec/io.h>
#include <devices/ahi.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <powerup/ppcproto/ahi.h>

#include <stdio.h>
#include <string.h>

#include "../../wos_ahi_abi.h"

struct Library *AHIBase = NULL;

static struct MsgPort          *port;
static struct wos_AHIRequest   *req;

int main (void)
{
  ULONG id;
  int n = 0;

  setvbuf (stdout, NULL, _IONBF, 0);
  printf ("wos_ahimodes: AHI audio mode list\n");

  port = CreateMsgPort ();
  if (!port) { printf ("CreateMsgPort failed\n"); return 20; }
  req = (struct wos_AHIRequest *)CreateIORequest (port,
                                     sizeof (struct wos_AHIRequest));
  if (!req) { printf ("CreateIORequest failed\n"); return 20; }

  req->ahir_Version = 4;
  if (OpenDevice ("ahi.device", AHI_NO_UNIT, (struct IORequest *)req, 0)) {
    printf ("OpenDevice(ahi.device) failed\n");
    return 20;
  }
  AHIBase = (struct Library *)req->ahir_Std.io_Device;
  if (AHIBase->lib_IdString)
    printf ("driver: %s\n", (char *)AHIBase->lib_IdString);
  printf ("id        name                                     bits st hi "
          "minfreq maxfreq\n");

  id = AHI_INVALID_ID;
  while ((id = AHI_NextAudioID (id)) != AHI_INVALID_ID) {
    char  name[256];
    ULONG bits = 0, stereo = 0, hifi = 0, minf = 0, maxf = 0;

    name[0] = 0;
    AHI_GetAudioAttrs (id, NULL,
                       AHIDB_BufferLen, 256,
                       AHIDB_Name, (ULONG)name,
                       AHIDB_Bits, (ULONG)&bits,
                       AHIDB_Stereo, (ULONG)&stereo,
                       AHIDB_HiFi, (ULONG)&hifi,
                       AHIDB_MinMixFreq, (ULONG)&minf,
                       AHIDB_MaxMixFreq, (ULONG)&maxf,
                       TAG_END);
    printf ("%08lx  %-40s %4lu %2lu %2lu %7lu %7lu\n",
            (unsigned long)id, name, (unsigned long)bits,
            (unsigned long)stereo, (unsigned long)hifi,
            (unsigned long)minf, (unsigned long)maxf);
    if (++n >= 64)
      break;
  }

  if (n == 0)
    printf ("no modes returned\n");

  CloseDevice ((struct IORequest *)req);
  DeleteIORequest ((struct IORequest *)req);
  DeleteMsgPort (port);
  printf ("wos_ahimodes: %d mode(s)\n", n);
  return 0;
}
