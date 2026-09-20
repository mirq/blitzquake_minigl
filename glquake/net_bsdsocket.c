/* 
**  net_bsdsocket.c
*/

/* Quake includes */
#include "quakedef.h"

#if defined(WOS) && defined(__GNUC__) && defined(__PPC__)

/* MOS2WOS: use the BSD-style socket API provided by libglosswos exactly as
 * it is meant to be used - no raw bsdsocket.library LP calls.
 *
 * Why: libglosswos returns fd-table indices from socket(); the Amiga-named
 * entry points (CloseSocket/IoctlSocket/Inet_NtoA/SocketBaseTagList) expect
 * raw bsdsocket handles, so mixing the two layers returns -1 for every call.
 * libglosswos also opens bsdsocket.library v4 lazily and wires
 * SBTC_ERRNOLONGPTR/SBTC_LOGTAGPTR itself, so no SocketBaseTags call is
 * needed here.
 *
 * Do NOT include proto/socket.h: its clib/bsdsocket_protos.h declares the
 * bsdsocket functions with AmigaOS LONG signatures, which conflict with the
 * netinclude sys/socket.h prototypes. */
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
#include <unistd.h>
#include <arpa/inet.h>
#include <amitcp/socketbasetags.h>

/* The bsdsocket stack writes AmiTCP/Roadshow errno values through
 * SBTC_ERRNOLONGPTR. newlib's <errno.h> in this SDK uses a different
 * numbering for some codes (ECONNREFUSED 111 vs 61, ENOTSOCK 108 vs 38),
 * so compare against the stack's values explicitly. EWOULDBLOCK matches by
 * luck (35 in both). */
#define NET_EWOULDBLOCK   35
#define NET_ECONNREFUSED  61

#elif defined(__GNUC__) && defined(__PPC__)

#else

/* Amiga includes */
#pragma amiga-align
#include <exec/types.h>
#include <exec/libraries.h>
#ifdef MIAMI_SDK
#include <bsdsocket/socketbasetags.h>
#else
#include <amitcp/socketbasetags.h>
#endif
#include <proto/exec.h>
#include <proto/socket.h>

/* standard net includes */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#pragma default-align
#include <errno.h>

#define NET_EWOULDBLOCK   EWOULDBLOCK
#define NET_ECONNREFUSED  ECONNREFUSED

#endif

#ifdef WOS
/* The libglosswos __P* stub layer defines SocketBase (auto-opened by its
 * __init_bsdsocket constructor); just reference it here. */
extern struct Library *SocketBase;
#else
struct Library *SocketBase = NULL;  /* bsdsocket.library */
#endif
#define SOCKETVERSION 4             /* required version */

extern cvar_t hostname;

static int net_acceptsocket = -1;   // socket for fielding new connections
static int net_controlsocket;
static int net_broadcastsocket = 0;
static struct qsockaddr broadcastaddr;

static unsigned long myAddr;

#include "net_udp.h"


//=============================================================================

#if !defined(WOS)
#ifndef inet_ntoa
char *inet_ntoa(struct in_addr addr)
{
  return Inet_NtoA(addr.s_addr);
}
#endif
#endif

//=============================================================================

int UDP_Init (void)
{
  int p;
  struct hostent *local;
  char buff[MAXHOSTNAMELEN];
  struct qsockaddr addr;
  char *colon;
  int i; // JPG - for checking command line
  
  if (COM_CheckParm ("-noudp"))
    return (-1);

#ifdef WOS
  /* libglosswos owns SocketBase: it opens bsdsocket.library v4 lazily on the
     first socket call and wires SBTC_ERRNOLONGPTR/SBTC_LOGTAGPTR itself.
     Force the open now so a missing stack fails cleanly instead of at the
     first packet. */
  {
    int probe = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (probe < 0 || SocketBase == NULL) {
      if (probe >= 0)
        close (probe);
      return (-1);
    }
    close (probe);
  }
#else
  /* Amiga socket initialization */
  if (!(SocketBase = OpenLibrary("bsdsocket.library",SOCKETVERSION)))
    return (-1);
  if (SocketBaseTags(SBTM_SETVAL(SBTC_ERRNOPTR(sizeof(errno))),&errno,
                     SBTM_SETVAL(SBTC_LOGTAGPTR),"QuakeAmiga UDP",
                     TAG_END)) {
    CloseLibrary(SocketBase);
    return (-1);
  }
#endif

  p = COM_CheckParm ("-udpport");
  if (p == 0)
    net_hostport = DEFAULTnet_hostport;
  else if (p < com_argc-1)
    net_hostport = Q_atoi (com_argv[p+1]);
  else
    Sys_Error ("UDP_Init: you must specify a number after -udpport");

  // determine my name & address
  // JPG - check command line for -ip
  i = COM_CheckParm ("-ip");
  if (i)
  {
    if (i < com_argc-1)
    {
      myAddr = inet_addr(com_argv[i+1]);
      if (myAddr == INADDR_NONE)
        Sys_Error ("%s is not a valid IP address", com_argv[i+1]);
      strcpy(buff, com_argv[i+1]);
    }
    else
      Sys_Error ("NET_Init: you must specify an IP address after -ip");
  }
  else
  {
    gethostname(buff, MAXHOSTNAMELEN);
    if (local = gethostbyname(buff))
      myAddr = *(int *)local->h_addr_list[0];
    if (!local || !myAddr)
      Sys_Error ("NET_Init: you need a valid host name, or use -ip");
  }

  // if the quake hostname isn't set, set it to the machine name
  if (Q_strcmp(hostname.string, "UNNAMED") == 0)
  {
    buff[15] = 0;
    Cvar_Set ("hostname", buff);
  }

  if ((net_controlsocket = UDP_OpenSocket (0)) == -1)
    Sys_Error("UDP_Init: Unable to open control socket\n");

#ifdef WOS
  /* AmiTCP-style sockaddr_in has a leading sin_len byte; zero the whole
     thing and set it. */
  memset (&broadcastaddr, 0, sizeof (broadcastaddr));
  ((struct sockaddr_in *)&broadcastaddr)->sin_len =
      (UBYTE)sizeof (struct sockaddr_in);
#endif
  ((struct sockaddr_in *)&broadcastaddr)->sin_family = AF_INET;
  ((struct sockaddr_in *)&broadcastaddr)->sin_addr.s_addr = INADDR_BROADCAST;
  ((struct sockaddr_in *)&broadcastaddr)->sin_port = htons(net_hostport);

  UDP_GetSocketAddr (net_controlsocket, &addr);
  Q_strcpy(my_tcpip_address,  UDP_AddrToString (&addr));
  colon = Q_strrchr (my_tcpip_address, ':');
  if (colon)
    *colon = 0;

#ifdef WOS
  Con_Printf("bsdsocket.library TCP/IP (v4+, libglosswos)\n");
#else
  Con_Printf("bsdsocket.library V%d TCP/IP\n",SOCKETVERSION);
#endif
  tcpipAvailable = true;

  return net_controlsocket;
}

//=============================================================================

void UDP_Shutdown (void)
{
  if (SocketBase) {
    UDP_Listen (false);
    UDP_CloseSocket (net_controlsocket);
#ifndef WOS
    /* libglosswos owns the base on WOS and closes it in its .fini
       destructor; closing it here would double-close. */
    CloseLibrary(SocketBase);
#endif
  }
}

//=============================================================================

void UDP_Listen (qboolean state)
{
  // enable listening
  if (state)
  {
    if (net_acceptsocket != -1)
      return;
    if ((net_acceptsocket = UDP_OpenSocket (net_hostport)) == -1)
      Sys_Error ("UDP_Listen: Unable to open accept socket\n");
    return;
  }

  // disable listening
  if (net_acceptsocket == -1)
    return;
  UDP_CloseSocket (net_acceptsocket);
  net_acceptsocket = -1;
}

//=============================================================================

int UDP_OpenSocket (int port)
{
  int newsocket;
  struct sockaddr_in address;
  LONG one = 1;

  if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) != -1) {
#ifdef WOS
    /* Pure libglosswos fd layer: ioctl()/close() are the correct calls for
       fds returned by socket() (IoctlSocket/CloseSocket take raw handles). */
    if (ioctl (newsocket, FIONBIO, &one) != -1) {
#else
    if (IoctlSocket (newsocket, FIONBIO, (char *)&one) != -1) {
#endif
      memset (&address, 0, sizeof (address));
#ifdef WOS
      address.sin_len = (UBYTE)sizeof (address);
#endif
      address.sin_family = AF_INET;
      address.sin_addr.s_addr = INADDR_ANY;
      address.sin_port = htons(port);
      if (bind (newsocket, (void *)&address, sizeof(address)) != -1)
        return newsocket;
    }
#ifdef WOS
    close (newsocket);
#else
    CloseSocket(newsocket);
#endif
  }
  return -1;
}

//=============================================================================

int UDP_CloseSocket (int socket)
{
  if (socket == net_broadcastsocket)
    net_broadcastsocket = 0;
#ifdef WOS
  return (close (socket));
#else
  return (CloseSocket(socket));
#endif
}


//=============================================================================
/*
============
PartialIPAddress

this lets you type only as much of the net address as required, using
the local network components to fill in the rest
============
*/
static int PartialIPAddress (char *in, struct qsockaddr *hostaddr)
{
  char buff[256];
  char *b;
  int addr;
  int num;
  int mask;
  
  buff[0] = '.';
  b = buff;
  strcpy(buff+1, in);
  if (buff[1] == '.') b++;

  addr = 0;
  mask=-1;
  while (*b == '.')
  {
    num = 0;
    if (*++b < '0' || *b > '9') return -1;
    while (!( *b < '0' || *b > '9'))
      num = num*10 + *(b++) - '0';
    mask<<=8;
    addr = (addr<<8) + num;
  }
  
  memset (hostaddr, 0, sizeof (struct sockaddr_in));
#ifdef WOS
  ((struct sockaddr_in *)hostaddr)->sin_len =
      (UBYTE)sizeof (struct sockaddr_in);
#endif
  hostaddr->sa_family = AF_INET;
  ((struct sockaddr_in *)hostaddr)->sin_port = htons(net_hostport); 
  ((struct sockaddr_in *)hostaddr)->sin_addr.s_addr = (myAddr & htonl(mask)) | htonl(addr);
  
  return 0;
}
//=============================================================================

int UDP_Connect (int socket, struct qsockaddr *addr)
{
  return 0;
}

//=============================================================================

int UDP_CheckNewConnections (void)
{
  char buf[4096];

  if (net_acceptsocket == -1)
    return -1;

  // JPG - fixed the zero-sized packet bug by changing > to >=
  if (recvfrom (net_acceptsocket, buf, sizeof(buf), MSG_PEEK, NULL, NULL) >= 0)
    return net_acceptsocket;
  return -1;
}

//=============================================================================

int UDP_Read (int socket, byte *buf, int len, struct qsockaddr *addr)
{
  LONG addrlen = sizeof (struct qsockaddr);
  int ret;

  ret = recvfrom (socket, buf, len, 0, (struct sockaddr *)addr, &addrlen);
  if (ret == -1 && (errno == NET_EWOULDBLOCK || errno == NET_ECONNREFUSED))
    return 0;
  return ret;
}

//=============================================================================

int UDP_MakeSocketBroadcastCapable (int socket)
{
  int       i = 1;

  // make this socket broadcast capable
  if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &i, sizeof(i)) < 0)
    return -1;
  net_broadcastsocket = socket;

  return 0;
}

//=============================================================================

int UDP_Broadcast (int socket, byte *buf, int len)
{
  int ret;

  if (socket != net_broadcastsocket)
  {
    if (net_broadcastsocket != 0)
      Sys_Error("Attempted to use multiple broadcasts sockets\n");
    ret = UDP_MakeSocketBroadcastCapable (socket);
    if (ret == -1)
    {
      Con_Printf("Unable to make socket broadcast capable\n");
      return ret;
    }
  }

  return UDP_Write (socket, buf, len, &broadcastaddr);
}

//=============================================================================

int UDP_Write (int socket, byte *buf, int len, struct qsockaddr *addr)
{
  int ret;

  ret = sendto (socket, buf, len, 0, (struct sockaddr *)addr, sizeof(struct qsockaddr));
  if (ret == -1 && errno == NET_EWOULDBLOCK)
    return 0;
  return ret;
}

//=============================================================================

char *UDP_AddrToString (struct qsockaddr *addr)
{
  static char buffer[22];
  int haddr;

  haddr = ntohl(((struct sockaddr_in *)addr)->sin_addr.s_addr);
  sprintf(buffer, "%d.%d.%d.%d:%d", (haddr >> 24) & 0xff,
          (haddr >> 16) & 0xff, (haddr >> 8) & 0xff, haddr & 0xff, 
          (int)ntohs(((struct sockaddr_in *)addr)->sin_port));
  return buffer;
}

//=============================================================================

int UDP_StringToAddr (char *string, struct qsockaddr *addr)
{
  int ha1, ha2, ha3, ha4, hp;
  int ipaddr;

  sscanf(string, "%d.%d.%d.%d:%d", &ha1, &ha2, &ha3, &ha4, &hp);
  ipaddr = (ha1 << 24) | (ha2 << 16) | (ha3 << 8) | ha4;

  addr->sa_family = AF_INET;
  ((struct sockaddr_in *)addr)->sin_addr.s_addr = htonl(ipaddr);
  ((struct sockaddr_in *)addr)->sin_port = htons(hp);
  return 0;
}

//=============================================================================

int UDP_GetSocketAddr (int socket, struct qsockaddr *addr)
{
  LONG addrlen = sizeof(struct qsockaddr);
  unsigned int a;

  Q_memset(addr, 0, sizeof(struct qsockaddr));
  getsockname(socket, (struct sockaddr *)addr, &addrlen);
  a = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
  if (a == 0 || a == inet_addr("127.0.0.1"))
    ((struct sockaddr_in *)addr)->sin_addr.s_addr = myAddr;

  return 0;
}

//=============================================================================

int UDP_GetNameFromAddr (struct qsockaddr *addr, char *name)
{
  struct hostent *hostentry;

  hostentry = (struct hostent *)gethostbyaddr ((char *)&((struct sockaddr_in *)addr)->sin_addr, sizeof(struct in_addr), AF_INET);
  if (hostentry)
  {
    Q_strncpy (name, (char *)hostentry->h_name, NET_NAMELEN - 1);
    return 0;
  }

  Q_strcpy (name, UDP_AddrToString (addr));
  return 0;
}

//=============================================================================

int UDP_GetAddrFromName(char *name, struct qsockaddr *addr)
{
  struct hostent *hostentry;

  if (name[0] >= '0' && name[0] <= '9')
    return PartialIPAddress (name, addr);
  
  hostentry = gethostbyname (name);
  if (!hostentry)
    return -1;

  addr->sa_family = AF_INET;
  ((struct sockaddr_in *)addr)->sin_port = htons(net_hostport); 
  ((struct sockaddr_in *)addr)->sin_addr.s_addr = *(int *)hostentry->h_addr_list[0];

  return 0;
}

//=============================================================================

int UDP_AddrCompare (struct qsockaddr *addr1, struct qsockaddr *addr2)
{
  if ((addr1->sa_family&0xff) != (addr2->sa_family&0xff))
    return -1;

  if (((struct sockaddr_in *)addr1)->sin_addr.s_addr != ((struct sockaddr_in *)addr2)->sin_addr.s_addr)
    return -1;

  if (((struct sockaddr_in *)addr1)->sin_port != ((struct sockaddr_in *)addr2)->sin_port)
    return 1;

  return 0;
}

//=============================================================================

int UDP_GetSocketPort (struct qsockaddr *addr)
{
  return ntohs(((struct sockaddr_in *)addr)->sin_port);
}


int UDP_SetSocketPort (struct qsockaddr *addr, int port)
{
  ((struct sockaddr_in *)addr)->sin_port = htons(port);
  return 0;
}

//=============================================================================
