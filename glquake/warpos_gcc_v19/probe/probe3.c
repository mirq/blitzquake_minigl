#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <stdio.h>

struct Library *TimerBase = NULL;
static struct MsgPort *tp;
static struct timerequest *tr;

int main(void)
{
    struct timeval tv;
    struct DateStamp ds;
    struct EClockVal ec;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("p3: alive\n");

    if (!(tp = CreatePort(NULL, 0))) { printf("no port\n"); return 20; }
    if (!(tr = (struct timerequest *)CreateExtIO(tp, sizeof(struct timerequest)))) { printf("no io\n"); return 20; }
    if (OpenDevice(TIMERNAME, UNIT_MICROHZ, (struct IORequest *)tr, 0)) { printf("no timer\n"); return 20; }
    TimerBase = (struct Device *)tr->tr_node.io_Device;
    printf("timer open, base=%p\n", (void *)TimerBase);

    DateStamp(&ds);
    printf("DateStamp ok: %ld\n", (long)ds.ds_Days);

    GetSysTime(&tv);
    printf("GetSysTime ok: %ld.%06ld\n", (long)tv.tv_secs, (long)tv.tv_micro);

    printf("ReadEClock = %lu\n", (unsigned long)ReadEClock(&ec));
    printf("ec = %lu:%lu\n", (unsigned long)ec.ev_hi, (unsigned long)ec.ev_lo);
    printf("p3: clean exit\n");
    return 0;
}
