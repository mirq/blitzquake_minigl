#!/usr/bin/env python3
"""Decode a T:cphang<n>.bin dump (minigl.library CpDumpStalledSlot).

Header: 'CPHG', record dwords, CP dwords, commit flag (big-endian), then
the records, then the CP stream in host order. Prints the record walk and
a packet walk of the CP stream with any structural anomaly flagged.
"""
import struct, sys

P3 = {0x10: "NOP", 0x2e: "3D_LOAD_VBPNTR", 0x33: "INDX_BUFFER",
      0x34: "3D_DRAW_VBUF_2", 0x35: "3D_DRAW_IMMD_2", 0x36: "3D_DRAW_INDX_2",
      0x28: "3D_DRAW_IMMD", 0x29: "3D_DRAW_VBUF", 0x2a: "3D_DRAW_INDX",
      0x2d: "3D_CLEAR_ZMASK", 0x39: "3D_CLEAR_HIZ", 0x92: "BITBLT_MULTI",
      0x9b: "CNTL_BITBLT", 0x91: "CNTL_PAINT", 0x13: "WAIT_FOR_IDLE",
      0x30: "3D_RNDR_GEN_INDX_PRIM", 0x31: "LOAD_PALETTE"}


def main():
    data = open(sys.argv[1], "rb").read()
    magic, nrec, ncp, commit = struct.unpack(">4I", data[:16])
    rec = struct.unpack(">%dI" % nrec, data[16:16 + 4 * nrec])
    cp = struct.unpack(">%dI" % ncp, data[16 + 4 * nrec:16 + 4 * (nrec + ncp)])
    print("records", nrec, "cp dwords", ncp, "commit", commit)
    w = 0
    while w + 1 < nrec:
        op, ln = rec[w], rec[w + 1]
        print("  rec @%d op=%d len=%d hdr=%s" % (w, op, ln,
              " ".join("%08x" % x for x in rec[w:w + min(ln, 12)])))
        if ln < 2:
            print("  !! bad record length")
            break
        w += ln
    i = 0
    counts = {}
    while i < ncp:
        h = cp[i]
        t = h >> 30
        if t == 0:
            n = ((h >> 16) & 0x3fff) + 1
            reg = (h & 0x7fff) << 2
            counts["P0"] = counts.get("P0", 0) + 1
            i += 1 + n
        elif t == 2:
            counts["P2"] = counts.get("P2", 0) + 1
            i += 1
        elif t == 3:
            n = ((h >> 16) & 0x3fff) + 1
            op = (h >> 8) & 0xff
            name = P3.get(op, "op%02x" % op)
            counts[name] = counts.get(name, 0) + 1
            if name.startswith("3D_DRAW") or op in (0x2e, 0x33):
                body = cp[i + 1:i + 1 + min(n, 4)]
                print("  @%d %s n=%d %s" % (i, name, n,
                      " ".join("%08x" % x for x in body)))
                if name == "3D_DRAW_IMMD_2" and len(body):
                    vc = body[0] >> 16
                    print("     vertices=%d dwords/vertex=%.2f" % (vc, (n - 1) / max(vc, 1)))
            i += 1 + n
        else:
            print("  !! type-1 packet at", i, "%08x" % h)
            i += 1
        if i > ncp:
            print("  !! packet runs past the end:", i, ">", ncp)
    print("packets", counts)


if __name__ == "__main__":
    main()
