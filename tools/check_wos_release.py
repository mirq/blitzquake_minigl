#!/usr/bin/env python3
"""Check release artifacts for diagnostic code markers and print identities."""
import hashlib
import pathlib
import sys
import zlib

if len(sys.argv) != 4:
    raise SystemExit("usage: check_wos_release.py client.warpelf dll.warpelf host")

checks = (
    ("client", sys.argv[1], (b"RAM:gqtrace", b"Sys_WOSTraceFrame",
                            b"GL before present", b"MGL_PERF_DERIVED")),
    ("DLL", sys.argv[2], (b"DH2:wosbuild/qorigin.log", b"R200PPCErrorAt")),
    ("host", sys.argv[3], (b"DH2:wosbuild/qhosterr.log",
                         b"DH2:wosbuild/qinline.bin", b"HostFailurePrintf",
                         b"host: dbeg nativefail")),
)
for name, filename, markers in checks:
    data = pathlib.Path(filename).read_bytes()
    present = [m.decode() for m in markers if m in data]
    if present:
        raise SystemExit(f"{name}: diagnostic markers remain: {present}")
    print(f"{name}: diagnostic markers absent; bytes={len(data)} "
          f"CRC32={zlib.crc32(data):08X} SHA256={hashlib.sha256(data).hexdigest()}")
