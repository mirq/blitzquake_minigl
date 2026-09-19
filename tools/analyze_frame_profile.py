#!/usr/bin/env python3
"""Analyze GLQuake frame-profiler dumps (frameprof_*.txt).

Usage: python3 tools/analyze_frame_profile.py frameprof_A2.txt [more files...]

Parses the "FP " text format written by glquake/frame_profile.c, excludes
non-timedemo frames (map load etc.) and prints the frame budget, percentiles
and the slowest frames. Run on the host against files pulled from DH2:.
"""
import re
import sys


def parse(path):
    frames = []          # (frame_no, total_ms, {section: ms}, counters)
    totals = {}          # section -> ms (whole session incl. load frame)
    counters = {}
    clock_hz = None
    wall_s = None
    cur = None
    with open(path) as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("FP VERSION"):
                m = re.search(r"clock_hz=(\d+)", line)
                clock_hz = int(m.group(1)) if m else 0
            elif line.startswith("FP VERSION") or line.startswith("FP NOTE"):
                continue
            elif line.startswith("FP TOTAL"):
                m = re.match(r"FP TOTAL (\S+)\s+s=(\S+) ms=([\d.]+) pct=([\d.]+)", line)
                if m:
                    totals[m.group(1)] = float(m.group(3))
            elif line.startswith("FP COUNTERS"):
                for k, v in re.findall(r"(\w+)=(\d+)", line):
                    counters[k] = int(v)
            elif line.startswith("FP WORST"):
                m = re.match(r"FP WORST rank=(\d+) frame=(\d+) total_ms=([\d.]+)", line)
                if m:
                    cur = {"rank": int(m.group(1)), "frame": int(m.group(2)),
                           "total": float(m.group(3)), "secs": {}, "lm_calls": 0,
                           "lm_kb": 0, "overlays": 0}
                    frames.append(cur)
            elif line.startswith("FPW "):
                m = re.match(r"FPW (\d+) (.*)", line)
                if not m or cur is None:
                    continue
                rank = int(m.group(1))
                if frames and frames[-1]["rank"] == rank:
                    for k, v in re.findall(r"(\w+)=([\d.e+-]+)", m.group(2)):
                        if k in ("screen", "view", "scene", "world", "ents",
                                 "parts", "blend", "hud", "present"):
                            frames[-1]["secs"][k] = float(v)
                        elif k in ("lm_calls", "lm_kb", "overlays"):
                            frames[-1][k] = int(float(v))
    return clock_hz, wall_s, totals, counters, frames


def pct(sorted_vals, p):
    if not sorted_vals:
        return 0.0
    i = min(len(sorted_vals) - 1, int(p / 100.0 * len(sorted_vals)))
    return sorted_vals[i]


def main(paths):
    for path in paths:
        clock_hz, wall_s, totals, counters, frames = parse(path)
        if not frames:
            print(f"{path}: no frames parsed")
            continue
        # Exclude the loading/transition frame(s): anything >= 1 s.
        play = [f for f in frames if f["total"] < 1000.0]
        play.sort(key=lambda f: f["total"])
        totals_play = [f["total"] for f in play]
        n = len(play)
        mean = sum(totals_play) / n
        fps_from_mean = 1000.0 / mean if mean else 0
        print(f"== {path}")
        print(f"   clock_hz={clock_hz} frames_parsed={len(frames)} "
              f"play_frames={n} (excluded {len(frames)-n} load/transition frames)")
        print("   NOTE: the on-target dump keeps only a top-16 worst-frame ring;")
        print("   the mean/percentiles below describe THAT RING, not the demo.")
        print(f"   ring frame ms: mean={mean:.1f} p50={pct(totals_play,50):.1f} "
              f"p90={pct(totals_play,90):.1f} p95={pct(totals_play,95):.1f} "
              f"p99={pct(totals_play,99):.1f} max={totals_play[-1]:.1f}")
        print(f"   >100ms frames: {sum(1 for t in totals_play if t > 100)}  "
              f">150ms: {sum(1 for t in totals_play if t > 150)}  "
              f">200ms: {sum(1 for t in totals_play if t > 200)}")
        print("   whole-run section totals (ms, nested):")
        for sec, ms in totals.items():
            print(f"     {sec:<10} {ms:12.1f}")
        print(f"   counters: {counters}")
        print("   slowest play frames (rank order as recorded):")
        for f in sorted(frames, key=lambda f: -f["total"])[:10]:
            if f["total"] >= 1000.0:
                continue
            s = f["secs"]
            print(f"     frame {f['frame']:>4} total={f['total']:7.1f} "
                  f"world={s.get('world',0):6.1f} ents={s.get('ents',0):6.1f} "
                  f"(brush+alias) blend={s.get('blend',0):5.3f} "
                  f"present={s.get('present',0):5.1f} hud={s.get('hud',0):5.1f} "
                  f"lm={f['lm_calls']}/{f['lm_kb']}KB ov={f['overlays']}")
        print()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    main(sys.argv[1:])
