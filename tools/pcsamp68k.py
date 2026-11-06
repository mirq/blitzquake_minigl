#!/usr/bin/env python3
"""Resolve a 68k -pcsample dump (glquake/pcsamp68k.c) to functions.

usage: pcsamp68k.py pcs.bin glquake68k_060_perf.sym MINIGL_ROOT [OBJDIR]

Quake PCs resolve against the unstripped client (first code hunk = first
segment). Library PCs resolve against minigl.library globals plus the local
symbols of the r200_unity / radeon3d_emit objects, placed with a global of
each object; the library's text base comes from the dispatch table address.
"""
import bisect, collections, struct, subprocess, sys

NM = "/opt/amiga/bin/m68k-amigaos-nm"


def nm(path):
    out = subprocess.run([NM, "-n", path], capture_output=True, text=True).stdout
    syms = []
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3 and p[1] in "tT":
            syms.append((int(p[0], 16), p[2]))
    return syms


def main():
    dump, qsym, root = sys.argv[1:4]
    objdir = sys.argv[4] if len(sys.argv) > 4 else "obj_commit"
    data = open(dump, "rb").read()
    magic, count, nsegs, table = struct.unpack(">4I", data[:16])
    off = 16
    segs = [struct.unpack(">2I", data[off + 8 * i: off + 8 * i + 8]) for i in range(nsegs)]
    off += 8 * nsegs
    raw = struct.unpack(">%dI" % count, data[off: off + 4 * count])
    callers = []
    if magic == 0x50435332:
        pcs = raw[0::2]
        callers = raw[1::2]
        count = len(pcs)
    else:
        pcs = raw

    qsyms = nm(qsym)
    import glob, os
    lib = os.environ.get("PCS_LIB") or (root + "/bin/" + ("commit" if objdir == "obj_commit" else "cpemit") + "/minigl.library")
    lsyms = nm(lib)
    lglob = {n: a for a, n in lsyms}
    # header word 3 = JMP target of LVO -30 = LibGetDispatchTable, a local
    # of minigl_library.o, placed via that object's global _LibVectors.
    mlo = dict((n, a) for a, n in nm("%s/%s/minigl_library.o" % (root, objdir)))
    getdt = mlo["_LibGetDispatchTable"] - mlo["_LibVectors"] + lglob["_LibVectors"]
    libbase = table - getdt
    allsyms = list(lsyms)
    for objpath in glob.glob("%s/%s/*.o" % (root, objdir)):
        osyms = nm(objpath)
        anchor = next((n for a, n in osyms if n in lglob), None)
        if anchor is None:
            continue
        oaddr = dict((n, a) for a, n in osyms)[anchor]
        delta = lglob[anchor] - oaddr
        allsyms += [(a + delta, n) for a, n in osyms if n not in lglob]
    allsyms.sort()
    laddr = [a for a, n in allsyms]
    qaddr = [a for a, n in qsyms]
    libtext = lsyms[-1][0] + 0x1000

    hits = collections.Counter()
    where = collections.Counter()
    for pc in pcs:
        if segs and segs[0][0] <= pc < segs[0][0] + segs[0][1]:
            o = pc - segs[0][0]
            i = bisect.bisect_right(qaddr, o) - 1
            hits["Q " + (qsyms[i][1] if i >= 0 else "?")] += 1
            where["quake"] += 1
        elif libbase <= pc < libbase + libtext + 0x10000:
            o = pc - libbase
            i = bisect.bisect_right(laddr, o) - 1
            hits["L " + (allsyms[i][1] if i >= 0 else "?")] += 1
            where["minigl"] += 1
        elif pc >= 0xF80000 and pc < 0x1000000:
            hits["ROM %06x" % (pc & ~0xff)] += 1
            where["rom"] += 1
            if callers:
                c = callers[pcs.index(pc)] if False else None

        else:
            hits["other %08x" % (pc & ~0xfff)] += 1
            where["other"] += 1
    if callers:
        rc = collections.Counter()
        for pc, c in zip(pcs, callers):
            if not (0xF80000 <= pc < 0x1000000) or not c:
                continue
            if segs and segs[0][0] <= c < segs[0][0] + segs[0][1]:
                i = bisect.bisect_right(qaddr, c - segs[0][0]) - 1
                rc["Q " + (qsyms[i][1] if i >= 0 else "?")] += 1
            elif libbase <= c < libbase + libtext + 0x10000:
                i = bisect.bisect_right(laddr, c - libbase) - 1
                rc["L " + (allsyms[i][1] if i >= 0 else "?")] += 1
            else:
                rc["other %08x" % (c & ~0xfff)] += 1
        print("ROM samples by caller:")
        for k, v in rc.most_common(15):
            print("   %6d  %s" % (v, k))
    print("samples", count, "segments", [(hex(a), b) for a, b in segs],
          "libbase", hex(libbase))
    for k, v in where.most_common():
        print("%-8s %6d %5.1f%%" % (k, v, 100.0 * v / max(count, 1)))
    print()
    for k, v in hits.most_common(60):
        print("%6d %5.1f%%  %s" % (v, 100.0 * v / max(count, 1), k))


if __name__ == "__main__":
    main()
