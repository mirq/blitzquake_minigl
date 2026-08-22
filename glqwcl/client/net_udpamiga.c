/*
 * net_udpamiga.c
 * AMIGA Quake World network support
 * Written by Frank Wille <frank@phoenix.owl.de>
 */

#include "quakedef.h"

#if defined(__GNUC__) && defined(__PPC__)

#include <exec/types.h>
#include <exec/libraries.h>
        #ifdef __PPC__
        #include <powerup/ppcproto/exec.h>
        #else
        #include <proto/exec.h>
        #endif
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <arpa/inet.h>

#ifdef __PPC__
#include <proto/socket.h>
#else
#include <inline/socket.h>
#endif
#include <amitcp/socketbasetags.h>

#else

/* Amiga includes */
#pragma amiga-align
#include <exec/types.h>
#include <exec/libraries.h>
#include <amitcp/socketbasetags.h>
#include <proto/exec.h>
#include <proto/socket.h>
#pragma default-align

/* AmiTCP SDK includes */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <errno.h>

#endif

struct Library *SocketBase = NULL;  /* bsdsocket.library */
#define SOCKETVERSION 4             /* required version */

netadr_t  net_local_adr;

netadr_t  net_from;
sizebuf_t net_message;
int net_socket = -1;

#define MAX_UDP_PACKET  8192
byte    net_message_buffer[MAX_UDP_PACKET];


//=============================================================================

void NetadrToSockadr (netadr_t *a, struct sockaddr_in *s)
{
  memset (s, 0, sizeof(*s));
  s->sin_family = AF_INET;

  *(int *)&s->sin_addr = *(int *)&a->ip;
  s->sin_port = a->port;
}

void SockadrToNetadr (struct sockaddr_in *s, netadr_t *a)
{
  *(int *)&a->ip = *(int *)&s->sin_addr;
  a->port = s->sin_port;
}

qboolean  NET_CompareBaseAdr (netadr_t a, netadr_t b)
{
  if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3])
    return true;
  return false;
}


qboolean  NET_CompareAdr (netadr_t a, netadr_t b)
{
  if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3] && a.port == b.port)
    return true;
  return false;
}

char  *NET_AdrToString (netadr_t a)
{
  static  char  s[64];
  
  sprintf (s, "%i.%i.%i.%i:%i", (int)a.ip[0], (int)a.ip[1],
           (int)a.ip[2], (int)a.ip[3], (int)ntohs(a.port));

  return s;
}

char  *NET_BaseAdrToString (netadr_t a)
{
  static  char  s[64];
  
  sprintf (s, "%i.%i.%i.%i", (int)a.ip[0], (int)a.ip[1],
           (int)a.ip[2], (int)a.ip[3]);

  return s;
}

/*
=============
NET_StringToAdr

idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/
qboolean  NET_StringToAdr (char *s, netadr_t *a)
{
  struct hostent  *h;
  struct sockaddr_in sadr;
  char  *colon;
  char  copy[128];
  
  
  memset (&sadr, 0, sizeof(sadr));
  sadr.sin_family = AF_INET;
  
  sadr.sin_port = 0;

  strcpy (copy, s);
  // strip off a trailing :port if present
  for (colon = copy ; *colon ; colon++)
    if (*colon == ':')
    {
      *colon = 0;
      sadr.sin_port = htons(atoi(colon+1)); 
    }
  
  if (copy[0] >= '0' && copy[0] <= '9')
  {
    *(int *)&sadr.sin_addr = inet_addr(copy);
  }
  else
  {
    if (! (h = (struct hostent *)gethostbyname(copy)) )
      return 0;
    *(int *)&sadr.sin_addr = *(int *)h->h_addr_list[0];
  }
  
  SockadrToNetadr (&sadr, a);

  return true;
}

// Returns true if we can't bind the address locally--in other words, 
// the IP is NOT one of our interfaces.
qboolean NET_IsClientLegal(netadr_t *adr)
{
  return true;
}

char *inet_ntoa(struct in_addr addr)
{
  return Inet_NtoA(addr.s_addr);
}


//=============================================================================

qboolean NET_GetPacket (void)
{
  int   ret;
  struct sockaddr_in  from;
  LONG fromlen;

  fromlen = sizeof(from);
  ret = recvfrom (net_socket, (UBYTE *)net_message_buffer,
                  sizeof(net_message_buffer), 0,
                  (struct sockaddr *)&from, &fromlen);
  if (ret == -1) {
    if (errno == EWOULDBLOCK)
      return false;
    if (errno == ECONNREFUSED)
      return false;
    Sys_Printf ("NET_GetPacket: %s\n", strerror(errno));
    return false;
  }

  net_message.cursize = ret;
  SockadrToNetadr (&from, &net_from);

  return ret;
}

//=============================================================================

void NET_SendPacket (int length, void *data, netadr_t to)
{
  int ret;
  struct sockaddr_in  addr;

  NetadrToSockadr (&to, &addr);

  ret = sendto (net_socket, data, length, 0, (struct sockaddr *)&addr, sizeof(addr) );
  if (ret == -1) {
    if (errno == EWOULDBLOCK)
      return;
    if (errno == ECONNREFUSED)
      return;
    Sys_Printf ("NET_SendPacket: %s\n", strerror(errno));
  }
}

//=============================================================================

int UDP_OpenSocket (int port)
{
  int newsocket;
  struct sockaddr_in address;
  qboolean _true = true;
  int i;

  if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    Sys_Error ("UDP_OpenSocket: socket:", strerror(errno));
  if (IoctlSocket(newsocket, FIONBIO, (char *)&_true) == -1) {
    CloseSocket(newsocket);
    Sys_Error ("UDP_OpenSocket: ioctl FIONBIO:", strerror(errno));
  }
  address.sin_family = AF_INET;
//ZOID -- check for interface binding option
  if ((i = COM_CheckParm("-ip")) != 0 && i < com_argc) {
    address.sin_addr.s_addr = inet_addr(com_argv[i+1]);
    Con_Printf("Binding to IP Interface Address of %s\n",
        inet_ntoa(address.sin_addr));
  } else
    address.sin_addr.s_addr = INADDR_ANY;
  if (port == PORT_ANY)
    address.sin_port = 0;
  else
    address.sin_port = htons((short)port);
  if( bind (newsocket, (void *)&address, sizeof(address)) == -1) {
    CloseSocket(newsocket);
    Sys_Error ("UDP_OpenSocket: bind: %s", strerror(errno));
  }

  return newsocket;
}

void NET_GetLocalAddress (void)
{
  char  buff[MAXHOSTNAMELEN];
  struct sockaddr_in  address;
  LONG namelen;

  gethostname(buff, MAXHOSTNAMELEN);
  buff[MAXHOSTNAMELEN-1] = 0;

  NET_StringToAdr (buff, &net_local_adr);

  namelen = sizeof(address);
  if (getsockname (net_socket, (struct sockaddr *)&address, &namelen) == -1)
    Sys_Error ("NET_Init: getsockname:", strerror(errno));
  net_local_adr.port = address.sin_port;

  Con_Printf("IP address %s\n", NET_AdrToString (net_local_adr) );
}

/*
====================
NET_Init
====================
*/
void NET_Init (int port)
{
  /* Amiga socket initialization */
  if (!(SocketBase = OpenLibrary("bsdsocket.library",SOCKETVERSION)))
    Sys_Error ("NET_Init: Can't open bsdsocket.library V%d",SOCKETVERSION);
  if (SocketBaseTags(SBTM_SETVAL(SBTC_ERRNOPTR(sizeof(errno))),&errno,
          		       SBTM_SETVAL(SBTC_LOGTAGPTR),"QuakeWorldAmiga UDP",
          		       TAG_END)) {
    CloseLibrary(SocketBase);
    Sys_Error ("NET_Init: Can't set errno");
  }

  //
  // open the single socket to be used for all communications
  //
  net_socket = UDP_OpenSocket (port);

  //
  // init the message buffer
  //
  net_message.maxsize = sizeof(net_message_buffer);
  net_message.data = net_message_buffer;

  //
  // determine my name & address
  //
  NET_GetLocalAddress ();

  Con_Printf("UDP via bsdsocket.library V%d initialized\n",SOCKETVERSION);
}

/*
====================
NET_Shutdown
====================
*/
void  NET_Shutdown (void)
{
  if (SocketBase) {
    if (net_socket >= 0) {
      CloseSocket(net_socket);
      net_socket = -1;
    }
    CloseLibrary(SocketBase);
  }
}


/*
====================
NET_Select
====================
*/
void NET_Select (unsigned long usec)
{
  struct timeval timeout;
  fd_set fdset;

  FD_ZERO(&fdset);
  FD_SET(net_socket,&fdset);
  timeout.tv_sec = usec/1000000;
  timeout.tv_usec = usec%1000000;
  WaitSelect(net_socket+1, &fdset, NULL, NULL, &timeout, 0);
}
