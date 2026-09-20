/*
 * wos_net.c - WarpOS/PPC bsdsocket.library probe (pure __P* BSD layer).
 *
 * Purpose: prove on real hardware that a PPC task can use the MOS2WOS
 * libglosswos POSIX socket layer end to end:
 *   - lazy OpenLibrary("bsdsocket.library", 4) via __init_bsdsocket
 *   - socket/bind/getsockname/ioctl(FIONBIO)/sendto/recvfrom/close
 *   - the stack's errno numbering (AmiTCP/Roadshow, NOT newlib's)
 *   - UDP loopback round trip with a LittleLong payload (wire order)
 *   - SO_BROADCAST, gethostname/gethostbyname
 *   - optional: real NetQuake CCREQ_SERVER_INFO / CCREQ_CONNECT exchange
 *     against a server (e.g. an x86 QuakeWorld/NetQuake host)
 *
 * Usage (Amiga shell, after Elf2Exe2 conversion):
 *   wos_net                      ; local tests only
 *   wos_net 192.168.1.50         ; local tests + Quake server-info query
 *   wos_net 192.168.1.50 26000   ; ... on a non-default port
 *   wos_net 192.168.1.50 26000 connect   ; ... plus CCREQ_CONNECT
 *
 * Exit codes: 0 = all executed tests passed, 20 = a local test failed,
 *             1 = local tests passed but the server test got no reply.
 *
 * Build: make -f Makefile.GCCAmigaWOS_probe_io PROBE=wos_net
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <utility/tagitem.h>

/* MOS2WOS: BSD-style API from netinclude, implemented by libglosswos.
 * Do NOT include proto/socket.h or the raw LP macros here - mixing the
 * fd-table layer with bsdsocket handles is exactly what breaks. */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* libglosswos owns SocketBase; referencing it pulls in bsdsocket.o. */
extern struct Library *SocketBase;

/* AmiTCP/Roadshow errno values (the stack writes these through
 * SBTC_ERRNOLONGPTR; newlib's <errno.h> here is a different numbering). */
#define WOS_EWOULDBLOCK   35
#define WOS_ECONNREFUSED  61

/* Quake net.h constants (keep in sync with glquake/net.h) */
#define NETFLAG_LENGTH_MASK  0x0000ffff
#define NETFLAG_CTL          0x80000000
#define NET_PROTOCOL_VERSION 3
#define CCREQ_CONNECT        0x01
#define CCREQ_SERVER_INFO    0x02
#define CCREP_ACCEPT         0x81
#define CCREP_REJECT         0x82
#define CCREP_SERVER_INFO    0x83
#define MOD_PROQUAKE         0x01
#define QUAKE_DEFAULT_PORT   26000

static int bigendian;

static double probe_time (void)
{
  struct DateStamp ds;

  DateStamp (&ds);
  return (double)ds.ds_Days * 86400.0 + (double)ds.ds_Minute * 60.0
         + (double)ds.ds_Tick / 50.0;
}

static unsigned int swapl (unsigned int x)
{
  return ((x >> 24) & 0xff) | ((x >> 8) & 0xff00)
       | ((x << 8) & 0xff0000) | ((x << 24) & 0xff000000);
}

static unsigned short swaps (unsigned short x)
{
  return (unsigned short)((x >> 8) | (x << 8));
}

/* Same conversion policy as Quake's COM_Init(). */
static unsigned int little_long (unsigned int x)
{
  return bigendian ? swapl (x) : x;
}

static unsigned short little_short (unsigned short x)
{
  return bigendian ? swaps (x) : x;
}

/* ---------------- Quake protocol helpers ---------------- */

static int q_packet (unsigned char *buf, int req, const char *name)
{
  int len = 4;                                  /* control header slot */
  unsigned int hdr;

  buf[len++] = (unsigned char)req;
  strcpy ((char *)buf + len, name);
  len += (int)strlen (name) + 1;
  buf[len++] = NET_PROTOCOL_VERSION;
  if (req == CCREQ_CONNECT)
    buf[len++] = MOD_PROQUAKE;

  hdr = NETFLAG_CTL | (unsigned int)len;        /* BigLong: big-endian */
  buf[0] = (unsigned char)(hdr >> 24);
  buf[1] = (unsigned char)(hdr >> 16);
  buf[2] = (unsigned char)(hdr >> 8);
  buf[3] = (unsigned char)(hdr);
  return len;
}

static unsigned char q_read_byte (const unsigned char *p, int *off)
{
  return p[(*off)++];
}

static unsigned int q_read_long (const unsigned char *p, int *off)
{
  unsigned int v = (unsigned int)p[*off]
                 | ((unsigned int)p[*off + 1] << 8)
                 | ((unsigned int)p[*off + 2] << 16)
                 | ((unsigned int)p[*off + 3] << 24);
  *off += 4;
  return v;
}

static const char *q_read_string (const unsigned char *p, int *off,
                                  char *out, int max)
{
  int n = 0;

  while (n < max - 1) {
    char c = (char)p[(*off)++];
    if (!c)
      break;
    out[n++] = c;
  }
  out[n] = 0;
  return out;
}

/* ---------------- tests ---------------- */

static void test_endian (void)
{
  unsigned short s = 1;

  bigendian = (*(unsigned char *)&s == 0);
  printf ("  host byte order : %s\n", bigendian ? "BIG (68k/PPC)" : "little");
  printf ("  LittleLong(0x12345678)  = 0x%08x (expect 0x78563412)\n",
          little_long (0x12345678u));
  printf ("  LittleShort(0x1234)     = 0x%04x (expect 0x3412)\n",
          (unsigned)little_short (0x1234));
  printf ("  htons(0x1234)           = 0x%04x (identity on big-endian)\n",
          (unsigned)htons (0x1234));
  printf ("  sizeof(sockaddr_in)=%d (expect 16)\n",
          (int)sizeof (struct sockaddr_in));
}

/* Local interface address the stack would use to reach `peer` (or the
   gateway by default), discovered with connect()+getsockname. 0 on failure. */
static unsigned int local_iface_addr (const char *peer)
{
  int s;
  struct sockaddr_in tgt, loc;
  socklen_t ll;
  unsigned int a = 0;

  s = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s < 0)
    return 0;
  memset (&tgt, 0, sizeof (tgt));
  tgt.sin_len = (unsigned char)sizeof (tgt);
  tgt.sin_family = AF_INET;
  tgt.sin_port = htons (9);
  tgt.sin_addr.s_addr = inet_addr (peer ? peer : "192.168.1.1");
  if (connect (s, (struct sockaddr *)&tgt, sizeof (tgt)) == 0) {
    memset (&loc, 0, sizeof (loc));
    ll = sizeof (loc);
    if (getsockname (s, (struct sockaddr *)&loc, &ll) == 0)
      a = loc.sin_addr.s_addr;
  }
  close (s);
  return a;
}

static int test_loopback (const char *peer, char *delivered, int dlen)
{
  int s1 = -1, s2 = -1;
  struct sockaddr_in a1, a2;
  socklen_t alen;
  char tx[64], rx[64];
  unsigned int sent, got;
  int i, r, one = 1, attempt;
  double t0;
  int ok = 0;

  (void)dlen;

  s1 = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  printf ("  socket#1 = %d (errno=%d) SocketBase=%p\n",
          s1, errno, (void *)SocketBase);
  if (s1 < 0)
    return -1;
  s2 = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  printf ("  socket#2 = %d (errno=%d)\n", s2, errno);
  if (s2 < 0)
    goto done;

  memset (&a1, 0, sizeof (a1));
  a1.sin_len = (unsigned char)sizeof (a1);
  a1.sin_family = AF_INET;
  a1.sin_addr.s_addr = htonl (INADDR_ANY);
  a1.sin_port = 0;
  if (bind (s1, (struct sockaddr *)&a1, sizeof (a1)) < 0) {
    printf ("  FAIL bind#1 errno=%d\n", errno);
    goto done;
  }
  alen = sizeof (a1);
  if (getsockname (s1, (struct sockaddr *)&a1, &alen) < 0) {
    printf ("  FAIL getsockname errno=%d\n", errno);
    goto done;
  }
  printf ("  bind#1 ok, port=%d\n", (int)ntohs (a1.sin_port));

  /* non-blocking + errno numbering on an empty socket */
  if (ioctl (s1, FIONBIO, &one) < 0) {
    printf ("  FAIL ioctl(FIONBIO) errno=%d\n", errno);
    goto done;
  }
  errno = 0;
  r = (int)recvfrom (s1, rx, sizeof (rx), 0, NULL, NULL);
  printf ("  empty non-blocking recvfrom: ret=%d errno=%d "
          "(stack EWOULDBLOCK=%d, newlib EWOULDBLOCK=%d)\n",
          r, errno, WOS_EWOULDBLOCK, EWOULDBLOCK);
  if (r != -1 || errno != WOS_EWOULDBLOCK) {
    printf ("  FAIL: expected -1/EWOULDBLOCK(35)\n");
    goto done;
  }

  for (i = 0; i < (int)sizeof (tx); i++)
    tx[i] = (char)(i * 7 + 1);
  sent = little_long (0x11223344u);
  memcpy (tx, &sent, 4);                     /* 0x44332211 on the wire */

  /* Deliver to ourselves: 127.0.0.1 first, then the machine's own interface
     address (this Roadshow install has no loopback interface). Delivery is
     informational - the server test is the real gate. */
  delivered[0] = 0;
  for (attempt = 0; attempt < 2 && delivered[0] == 0; attempt++) {
    memset (&a2, 0, sizeof (a2));
    a2.sin_len = (unsigned char)sizeof (a2);
    a2.sin_family = AF_INET;
    a2.sin_port = a1.sin_port;
    if (attempt == 0) {
      a2.sin_addr.s_addr = inet_addr ("127.0.0.1");
      strcpy (delivered, "127.0.0.1");
    } else {
      a2.sin_addr.s_addr = local_iface_addr (peer);
      if (a2.sin_addr.s_addr == 0)
        break;
      strcpy (delivered, inet_ntoa (a2.sin_addr));
    }

    r = (int)sendto (s2, tx, sizeof (tx), 0, (struct sockaddr *)&a2,
                     sizeof (a2));
    printf ("  sendto(%s) = %d (errno=%d)\n", delivered, r, errno);
    if (r != (int)sizeof (tx)) {
      delivered[0] = 0;
      continue;
    }

    t0 = probe_time ();
    r = -1;
    do {
      r = (int)recvfrom (s1, rx, sizeof (rx), 0, NULL, NULL);
      if (r > 0)
        break;
      Delay (1);
    } while (probe_time () - t0 < 2.0);

    if (r != (int)sizeof (tx)) {
      delivered[0] = 0;
      continue;
    }
    memcpy (&got, rx, 4);
    printf ("  recvfrom = %d bytes, first long=0x%08x (wire value of 0x11223344)\n",
            r, got);
    if (got != sent || memcmp (tx + 4, rx + 4, sizeof (tx) - 4) != 0) {
      printf ("  FAIL payload mismatch (sent 0x%08x)\n", sent);
      goto done;
    }
  }

  if (delivered[0])
    printf ("  UDP delivery OK via %s\n", delivered);
  else
    printf ("  no UDP delivery (no loopback interface; informational)\n");

  /* broadcast capability */
  errno = 0;
  r = setsockopt (s2, SOL_SOCKET, SO_BROADCAST, &one, sizeof (one));
  printf ("  setsockopt(SO_BROADCAST) = %d (errno=%d)\n", r, errno);

  ok = 1;

done:
  if (s1 >= 0)
    close (s1);
  if (s2 >= 0)
    close (s2);
  return ok ? 0 : -1;
}

static void test_names (void)
{
  char host[256];
  struct hostent *he;
  int r;

  host[0] = 0;
  r = gethostname (host, sizeof (host));
  printf ("  gethostname = %d \"%s\" (errno=%d)\n", r, host, errno);

  he = gethostbyname ("localhost");
  if (!he)
    he = gethostbyname (host);
  if (he) {
    struct in_addr in;
    memcpy (&in, he->h_addr_list[0], sizeof (in));
    printf ("  gethostbyname ok: %s -> %s\n", he->h_name, inet_ntoa (in));
  }
  else
    printf ("  gethostbyname failed (errno=%d) -- no name resolution?\n",
            errno);
}

/* Send a NetQuake control packet and wait for the reply. Returns the reply
 * length, or -1. */
static int q_exchange (int s, const struct sockaddr_in *dst,
                       const unsigned char *pkt, int plen,
                       unsigned char *rx, int rxlen, int wait_secs)
{
  double t0;
  int r;

  r = (int)sendto (s, pkt, plen, 0, (const struct sockaddr *)dst,
                   sizeof (*dst));
  if (r != plen) {
    printf ("    sendto = %d (errno=%d)\n", r, errno);
    return -1;
  }

  t0 = probe_time ();
  do {
    r = (int)recvfrom (s, rx, rxlen, 0, NULL, NULL);
    if (r > 0)
      return r;
    Delay (1);
  } while (probe_time () - t0 < (double)wait_secs);

  return -1;
}

static int test_server (const char *ipstr, int port, int do_connect)
{
  int s = -1, one = 1, r, off, len;
  struct sockaddr_in dst;
  unsigned char pkt[128], rx[2048];
  unsigned int control;
  char str1[256], str2[256];
  int ok = 0;

  s = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s < 0) {
    printf ("  FAIL socket errno=%d\n", errno);
    return -1;
  }
  ioctl (s, FIONBIO, &one);

  memset (&dst, 0, sizeof (dst));
  dst.sin_len = (unsigned char)sizeof (dst);
  dst.sin_family = AF_INET;
  dst.sin_addr.s_addr = inet_addr (ipstr);
  dst.sin_port = htons ((unsigned short)port);
  if (dst.sin_addr.s_addr == INADDR_NONE) {
    struct hostent *he = gethostbyname (ipstr);
    if (!he) {
      printf ("  FAIL: bad address \"%s\"\n", ipstr);
      close (s);
      return -1;
    }
    memcpy (&dst.sin_addr, he->h_addr_list[0], sizeof (dst.sin_addr));
  }

  /* ---- CCREQ_SERVER_INFO ---- */
  len = q_packet (pkt, CCREQ_SERVER_INFO, "QUAKE");
  printf ("  server-info query -> %s:%d (%d bytes)\n", ipstr, port, len);
  r = q_exchange (s, &dst, pkt, len, rx, sizeof (rx), 3);
  if (r < 0) {
    printf ("  no server-info reply\n");
    close (s);
    return -1;
  }

  printf ("  reply: %d bytes:", r);
  for (off = 0; off < r && off < 24; off++)
    printf (" %02x", rx[off]);
  printf ("%s\n", r > 24 ? " ..." : "");

  control = ((unsigned int)rx[0] << 24) | ((unsigned int)rx[1] << 16)
          | ((unsigned int)rx[2] << 8) | rx[3];
  off = 4;
  if ((control & ~NETFLAG_LENGTH_MASK) != NETFLAG_CTL
      || (int)(control & NETFLAG_LENGTH_MASK) != r) {
    printf ("  FAIL: bad control word 0x%08x (len %d)\n", control, r);
    close (s);
    return -1;
  }
  if (q_read_byte (rx, &off) != CCREP_SERVER_INFO) {
    printf ("  FAIL: not a CCREP_SERVER_INFO reply\n");
    close (s);
    return -1;
  }
  q_read_string (rx, &off, str1, sizeof (str1));      /* address */
  q_read_string (rx, &off, str2, sizeof (str2));      /* hostname */
  printf ("  SERVER: address=\"%s\" hostname=\"%s\"", str1, str2);
  if (off < r) {
    q_read_string (rx, &off, str1, sizeof (str1));    /* map */
    printf (" map=\"%s\"", str1);
  }
  if (off + 3 <= r) {
    int players = q_read_byte (rx, &off);
    int maxplayers = q_read_byte (rx, &off);
    int proto = q_read_byte (rx, &off);
    printf (" players=%d/%d protocol=%d", players, maxplayers, proto);
  }
  printf ("\n");
  ok = 1;

  /* ---- optional CCREQ_CONNECT ---- */
  if (do_connect) {
    len = q_packet (pkt, CCREQ_CONNECT, "QUAKE");
    printf ("  connect request -> %s:%d (%d bytes)\n", ipstr, port, len);
    r = q_exchange (s, &dst, pkt, len, rx, sizeof (rx), 3);
    if (r < 0) {
      printf ("  no connect reply\n");
      ok = 0;
    }
    else {
      off = 4;
      control = ((unsigned int)rx[0] << 24) | ((unsigned int)rx[1] << 16)
              | ((unsigned int)rx[2] << 8) | rx[3];
      r = q_read_byte (rx, &off);
      if (r == CCREP_ACCEPT) {
        unsigned int cport = q_read_long (rx, &off);
        printf ("  CONNECT ACCEPTED, play port=%u (LittleLong)\n", cport);
      }
      else if (r == CCREP_REJECT) {
        q_read_string (rx, &off, str1, sizeof (str1));
        printf ("  CONNECT REJECTED: \"%s\"\n", str1);
        ok = 0;
      }
      else {
        printf ("  CONNECT: unexpected reply byte 0x%02x\n", r);
        ok = 0;
      }
    }
  }

  close (s);
  return ok ? 0 : -1;
}

int main (int argc, char **argv)
{
  const char *server = NULL;
  int port = QUAKE_DEFAULT_PORT;
  int do_connect = 0;
  int local_ok, server_ok = 1;
  char delivered[64];

  setvbuf (stdout, NULL, _IONBF, 0);
  printf ("wos_net: bsdsocket.library / UDP probe (pure __P* layer)\n");

  if (argc > 1)
    server = argv[1];
  if (argc > 2)
    port = atoi (argv[2]);
  if (argc > 3 && !strcmp (argv[3], "connect"))
    do_connect = 1;

  printf ("[1] endian / ABI\n");
  test_endian ();

  printf ("[2] local socket tests\n");
  local_ok = test_loopback (server, delivered, sizeof (delivered));
  printf ("  socket API: %s\n", local_ok == 0 ? "PASS" : "FAIL");
  test_names ();

  if (local_ok != 0) {
    printf ("wos_net: FAIL (local socket tests)\n");
    return 20;
  }

  if (server) {
    printf ("[3] Quake server test against %s:%d%s\n", server, port,
            do_connect ? " (server-info + connect)" : " (server-info)");
    server_ok = test_server (server, port, do_connect);
    printf ("  server test: %s\n", server_ok == 0 ? "PASS" : "NO REPLY/FAIL");
  }
  else
    printf ("[3] Quake server test skipped (pass an IP to enable)\n");

  if (server && server_ok != 0) {
    printf ("wos_net: local PASS, server test FAILED\n");
    return 1;
  }
  printf ("wos_net: PASS\n");
  return 0;
}
