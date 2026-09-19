/* Read-only 68k Radeon3D rejection snapshot; no GPU submission or reset.
 * Build: /opt/amiga/bin/m68k-amigaos-gcc -noixemul -m68060 -O2
 *   -I/home/mirek/p96-driver/include qfail.c -o qfail
 * Uses interface 1 Open/GetInfo/Close, so newer headers also work with
 * installed interface 17 drivers. Check Size before reading V3 fields.
 */
#include <exec/libraries.h>
#include <proto/exec.h>
#include <proto/radeon3d.h>
#include <stdio.h>
#include <string.h>

struct Library *Radeon9200Base;

int main(void)
{
    struct Radeon3DInfo info;
    struct Radeon3DDevice *device;
    int result = 20;
    Radeon9200Base = OpenLibrary((CONST_STRPTR)"Radeon9200.chip", 3);
    if (!Radeon9200Base) { puts("qfail: no chip library"); return 20; }
    memset(&info, 0, sizeof(info));
    info.Size = sizeof(info);
    device = Radeon3DOpen(1, &info);
    if (device) {
        info.Size = sizeof(info);
        if (Radeon3DGetInfo(device, &info) &&
            info.Size >= RADEON3D_INFO_V3_SIZE) {
            printf("qfail: interface=%lu generation=%lu commit_stage=%lu (0x%08lx)\n",
                (unsigned long)info.Version, (unsigned long)info.Generation,
                (unsigned long)info.CommitFailStage,
                (unsigned long)info.CommitFailStage);
            result = 0;
        } else puts("qfail: GetInfo/V3 unavailable");
        Radeon3DClose(device);
    } else puts("qfail: no service");
    CloseLibrary(Radeon9200Base);
    return result;
}
