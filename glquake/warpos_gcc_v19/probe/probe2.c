#include <exec/types.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include <utility/tagitem.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <amitcp/socketbasetags.h>
#include <stdio.h>

extern struct Library *SocketBase;

#include <powerup/ppcinline/macros.h>
#ifndef BSDSOCKET_BASE_NAME
#define BSDSOCKET_BASE_NAME SocketBase
#endif
#define IoctlSocket(sock, req, argp) \
	LP3(0x72, LONG, IoctlSocket, LONG, sock, d0, ULONG, req, d1, APTR, argp, a0, \
	, BSDSOCKET_BASE_NAME, IF_CACHEFLUSHALL, NULL, 0, IF_CACHEFLUSHALL, NULL, 0)
#define CloseSocket(sock) \
	LP1(0x78, LONG, CloseSocket, LONG, sock, d0, \
	, BSDSOCKET_BASE_NAME, IF_CACHEFLUSHALL, NULL, 0, IF_CACHEFLUSHALL, NULL, 0)
#define SocketBaseTagList(tags) \
	LP1(0x126, LONG, SocketBaseTagList, struct TagItem *, tags, a0, \
	, BSDSOCKET_BASE_NAME, IF_CACHEFLUSHALL, NULL, 0, IF_CACHEFLUSHALL, NULL, 0)

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("probe2: alive\n");
    if (!(SocketBase = OpenLibrary("bsdsocket.library", 4))) {
        printf("no bsdsocket\n");
        return 20;
    }
    printf("bsdsocket opened\n");
    {
        struct TagItem socktags[3];
        socktags[0].ti_Tag  = SBTM_SETVAL(SBTC_ERRNOPTR(sizeof(errno)));
        socktags[0].ti_Data = (ULONG)&errno;
        socktags[1].ti_Tag  = TAG_END;
        socktags[1].ti_Data = 0;
        if (SocketBaseTagList(socktags)) { printf("tags failed\n"); return 20; }
    }
    printf("errno wired\n");
    {
        int s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
        printf("socket=%d errno=%d\n", s, errno);
        if (s >= 0) {
            LONG one = 1;
            LONG r = IoctlSocket(s, FIONBIO, (APTR)&one);
            printf("ioctl=%ld\n", r);
            printf("close=%ld\n", CloseSocket(s));
        }
    }
    printf("probe2: clean exit\n");
    return 0;
}
