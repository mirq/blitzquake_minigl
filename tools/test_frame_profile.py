#!/usr/bin/env python3
"""Exercise the actual frame profiler with a mocked PPC clock, under sanitizers.

No Amiga or GL calls. Only headers and FP_Clock are replaced; window retention,
upload accounting, text export and the production analyzer run unchanged.
"""
import contextlib
import importlib.util
import io
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "glquake/frame_profile.c").read_text()
header = (ROOT / "glquake/frame_profile.h").read_text()
spec = importlib.util.spec_from_file_location("analyze_frame_profile", ROOT / "tools/analyze_frame_profile.py")
assert spec is not None and spec.loader is not None
analyzer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analyzer)

PREFIX = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint32_t ULONG;
typedef int qboolean;
#define true 1
#define false 0
static int com_argc, host_framecount;
static char **com_argv;
static struct { int timedemo, td_startframe; } cls;
enum { STAT_WEAPON = 2, STAT_ACTIVEWEAPON = 10, CSHIFT_BONUS = 2 };
static struct TestModel { char name[64]; } test_model;
static struct {
    double mtime[2]; unsigned items; int stats[32];
    struct { int percent; } cshifts[4];
    struct { struct TestModel *model; } viewent;
} cl;
static float v_blend[4];
static struct { int width, height; } vid;
static struct { float value; } r_dynamic, gl_texsort;
static ULONG now = 100;
static unsigned clock_reads, file_opens;
static int COM_CheckParm(const char *text) {
    int i;
    for (i = 1; i < com_argc; ++i)
        if (!strcmp(com_argv[i], text)) return i;
    return 0;
}
static void Delay(int ticks) { now += (ULONG)ticks * 500000u; }
static FILE *TestOpen(const char *path, const char *mode) {
    ++file_opens;
    return fopen(path, mode);
}
#define fopen TestOpen
'''

body = source.replace('#include "quakedef.h"', "").replace('#include "frame_profile.h"', "").replace("#include <proto/dos.h>", "")
body, replaced = re.subn(r"static ULONG FP_Clock \(void\)\s*\{.*?\n\}",
                        "static ULONG FP_Clock(void) { ++clock_reads; return now; }", body, count=1, flags=re.S)
assert replaced == 1

CHECKS = r'''
int main(int argc, char **argv) {
    char *args[] = { "test", "-frameprofile", NULL, "-framewindow", "258", "262" };
    const char *mode;
    unsigned reads;
    int frame, i;
    unsigned long start, outer;
    assert(argc == 3);
    mode = argv[1]; args[2] = argv[2];
    com_argc = 6; com_argv = args;
    cls.timedemo = 1; cls.td_startframe = 100;
    vid.width = 800; vid.height = 600;
    r_dynamic.value = gl_texsort.value = 1;
    strcpy(test_model.name, "progs/v_nail.mdl");
    cl.viewent.model = &test_model; cl.items = 7;
    cl.stats[STAT_ACTIVEWEAPON] = 4;
    assert(sizeof(fp_worst) + sizeof(fp_window) < 32768u);
    if (!strcmp(mode, "disabled")) com_argc = 1;
    if (!strcmp(mode, "invalid")) args[5] = "999999999999999999999";
    if (!strcmp(mode, "wrap")) now = 0xfffffff0u;
    FP_Init();
    if (!strcmp(mode, "disabled")) {
        reads = clock_reads;
        FP_FrameBegin(); start = FP_Enter(FP_LMUPLOAD);
        FP_ExitLMUpload(start, 2, 1, 4, 512, 2);
        FP_CountLMUpload(512); FP_CountOverlay(0); FP_FrameEnd(); FP_AutoDump();
        assert(!start && reads == clock_reads && !file_opens && !fp_frame_no);
        return 0;
    }
    assert(fp_clock_hz == 25000000u && !file_opens);
    if (!strcmp(mode, "wrap")) {
        now = 0xfffffff0u;
        FP_FrameBegin(); start = FP_Enter(FP_LMUPLOAD);
        now += 40u;
        FP_ExitLMUpload(start, 1, 0, 1, 128, 2);
        FP_CountLMUpload(128); FP_FrameEnd();
        assert(fp_worst[0].secs[FP_LMUPLOAD] == 40u);
        assert(fp_worst[0].uploads[0].ticks == 40u);
    } else if (!strcmp(mode, "overflow")) {
        FP_FrameBegin();
        for (i = 0; i < 19; ++i) {
            start = FP_Enter(FP_LMUPLOAD); now += 25u;
            FP_ExitLMUpload(start, i, 0, 1, 128, 2); FP_CountLMUpload(128);
        }
        FP_FrameEnd();
        assert(fp_worst[0].upload_count == FP_UPLOAD_SLOTS);
        assert(fp_worst[0].upload_dropped == 3u && fp_run_lm_calls == 19u);
        assert(fp_run_secs[FP_LMUPLOAD] == 19u * 25u);
    } else if (!strcmp(mode, "aborted")) {
        FP_FrameBegin(); (void)FP_Enter(FP_WORLD); now += 50u;
        FP_FrameBegin(); assert(fp_depth == 0);
        start = FP_Enter(FP_WORLD); now += 10u; FP_Exit(FP_WORLD, start);
        FP_FrameEnd();
        assert(fp_frame_no == 1 && fp_run_secs[FP_WORLD] == 10u);
    } else if (!strcmp(mode, "invalid")) {
        assert(fp_window_requested && !fp_window_valid);
        FP_FrameBegin(); now += 50u; FP_FrameEnd();
    } else {
        assert(!strcmp(mode, "window"));
        for (frame = 1; frame <= 300; ++frame) {
            host_framecount = 99 + frame;
            cl.mtime[0] = 1.0 + (frame - 1) * 0.1;
            FP_FrameBegin(); outer = FP_Enter(FP_WORLD); now += 100u;
            if (frame >= 10 && frame < 30) now += 5000000u;
            if (frame == 260) {
                for (i = 0; i < 4; ++i) {
                    start = FP_Enter(FP_LMUPLOAD); now += (ULONG)(i + 1) * 250000u;
                    FP_ExitLMUpload(start, i + 2, i * 3, i + 1, (i + 1) * 128u, 2);
                    FP_CountLMUpload((i + 1) * 128u);
                }
            }
            FP_Exit(FP_WORLD, outer); ++host_framecount; FP_FrameEnd();
        }
        assert(fp_window_count == 5 && fp_frame_no == 300);
        assert(fp_window[2].frame == 260 && fp_window[2].host_frame == 359);
        assert(fp_window[2].demo_frame == 259 && fp_window[2].lm_calls == 4);
        assert(fp_window[2].secs[FP_LMUPLOAD] == 2500000u);
        assert(fp_run_lm_calls == 4 && fp_run_lm_bytes == 1280);
        for (i = 0; i < fp_worst_count; ++i) assert(fp_worst[i].frame != 260);
    }
    assert(!file_opens);
    FP_AutoDump(); FP_AutoDump();
    assert(file_opens == 1);
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="quake-frame260-", dir="/tmp/opencode") as directory:
    path = Path(directory)
    cfile = path / "check.c"
    cfile.write_text(PREFIX + "\n" + header + "\n" + body + "\n" + CHECKS)
    binary = path / "check"
    subprocess.run(["cc", "-std=gnu99", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                    "-DWOS", "-D__PPC__", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", "-fno-pie", "-no-pie",
                    str(cfile), "-o", str(binary)], check=True)
    for mode in ("disabled", "window", "overflow", "wrap", "invalid", "aborted"):
        output = path / (mode + ".txt")
        subprocess.run([str(binary), mode, str(output)], check=True)
        if mode == "disabled":
            assert not output.exists()
            continue
        hz, wall, totals, counters, frames = analyzer.parse(output)
        assert hz == 25000000 and wall is not None
        if mode == "window":
            window = [f for f in frames if f["kind"] == "WINDOW"]
            assert [f["frame"] for f in window] == list(range(258, 263))
            hitch = window[2]
            assert hitch["demo_frame"] == 259 and hitch["host_frame"] == 359
            assert hitch["weapon_name"] == "progs/v_nail.mdl"
            assert hitch["active_weapon"] == 4 and hitch["items"] == 7
            assert abs(hitch["demo_s"] - 26.9) < 1e-6
            assert hitch["secs"]["lmup"] == 100.0
            assert [u["ms"] for u in hitch["uploads"].values()] == [10, 20, 30, 40]
            assert sum(u["bytes"] for u in hitch["uploads"].values()) == 1280
            assert counters["lm_upload_calls"] == 4
        elif mode == "overflow":
            assert frames[0]["upload_dropped"] == 3
            assert len(frames[0]["uploads"]) == 16
            assert counters["lm_upload_calls"] == 19
        elif mode == "invalid":
            assert counters["window_valid"] == 0
        with contextlib.redirect_stdout(io.StringIO()):
            analyzer.main([output])
        print("FRAME_PROFILE", mode, "PASS")

    # Legacy dump with no gameplay frame must not divide by zero.
    legacy = path / "legacy.txt"
    legacy.write_text("FP VERSION 1 frames=1 clock_hz=25000000 wall_s=2.000 fps=0.500\n"
                      "FP WORST rank=0 frame=1 total_ms=2000.000\n"
                      "FPW 0 world=0.5 lm_calls=0 overlays=0\n")
    with contextlib.redirect_stdout(io.StringIO()):
        analyzer.main([legacy])
    print("FRAME_PROFILE legacy PASS")

    # Non-PPC client: new calls must disappear without enum or link dependencies.
    stub = path / "noop.c"
    stub.write_text(header + "\nint main(void) { FP_Init(); FP_FrameBegin();\n"
                    "FP_ExitLMUpload(FP_Enter(FP_LMUPLOAD), 1, 0, 2, 256, 2);\n"
                    "FP_FrameEnd(); FP_AutoDump(); return 0; }\n")
    subprocess.run(["cc", "-std=gnu99", "-Wall", "-Wextra", "-Werror",
                    str(stub), "-o", str(path / "noop")], check=True)
    subprocess.run([str(path / "noop")], check=True)
    print("FRAME_PROFILE non-PPC no-op PASS")

surf = (ROOT / "glquake/gl_rsurf.c").read_text()
assert surf.count("FP_ExitLMUpload (") == 3
assert surf.count("FP_Enter (FP_LMBUILD)") == 4
print("FRAME_PROFILE upload/rebuild coverage PASS")
