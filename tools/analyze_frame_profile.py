#!/usr/bin/env python3
"""Analyze GLQuake frame-profiler dumps (frameprof_*.txt).

Usage: python3 tools/analyze_frame_profile.py frameprof_A2.txt [more files...]

Parses v1/v2 dumps. Worst-table statistics are NOT whole-demo percentiles.
v2 additionally reports an explicitly retained frame window and per-atlas
upload timings. Run on the host against files pulled from DH2:.
"""
import re
import sys

NUMBER = r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?"


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
                m = re.search(r"wall_s=(" + NUMBER + ")", line)
                wall_s = float(m.group(1)) if m else None
            elif line.startswith("FP NOTE"):
                continue
            elif line.startswith("FP CONFIG") or line.startswith("FP WINDOW_INFO"):
                prefix = "config_" if line.startswith("FP CONFIG") else "window_"
                for k, v in re.findall(r"(\w+)=(" + NUMBER + ")", line):
                    counters[prefix + k] = float(v)
            elif line.startswith("FP TOTAL"):
                m = re.match(r"FP TOTAL (\S+)\s+s=(\S+) ms=([\d.]+) pct=([\d.]+)", line)
                if m:
                    totals[m.group(1)] = float(m.group(3))
            elif line.startswith("FP COUNTERS"):
                for k, v in re.findall(r"(\w+)=(\d+)", line):
                    counters[k] = int(v)
            elif line.startswith("FP WORST ") or line.startswith("FP WINDOW "):
                m = re.match(r"FP (WORST|WINDOW) rank=(\d+) frame=(\d+) total_ms=([\d.]+)", line)
                if m:
                    cur = {"kind": m.group(1), "rank": int(m.group(2)),
                           "frame": int(m.group(3)), "total": float(m.group(4)),
                           "secs": {}, "lm_calls": 0, "lm_kb": 0, "overlays": 0,
                           "uploads": {}}
                    frames.append(cur)
            elif line.startswith("FPW "):
                m = re.match(r"FPW (\d+) (.*)", line)
                if not m or cur is None:
                    continue
                rank = int(m.group(1))
                if frames and frames[-1]["rank"] == rank:
                    for k, v in re.findall(r"(\w+)=(" + NUMBER + ")", m.group(2)):
                        if k in ("screen", "view", "scene", "world", "ents",
                                 "brush", "alias", "parts", "blend", "hud",
                                 "present", "lmup", "lmbuild"):
                            frames[-1]["secs"][k] = float(v)
                        elif k in ("lm_calls", "lm_kb", "overlays", "host_frame",
                                   "demo_frame", "upload_details", "upload_dropped",
                                   "items", "active_weapon", "weapon_model", "bonus_end"):
                            frames[-1][k] = int(float(v))
                        elif k in ("demo_s", "blend_alpha_end"):
                            frames[-1][k] = float(v)
                    weapon = re.search(r"weapon_name=(\S+)", m.group(2))
                    if weapon:
                        frames[-1]["weapon_name"] = weapon.group(1)
            elif line.startswith("FPU ") or line.startswith("FPUR "):
                m = re.match(r"FPUR? (\d+) (.*)", line)
                if m and cur is not None:
                    upload = cur["uploads"].setdefault(int(m.group(1)), {})
                    for k, v in re.findall(r"(\w+)=(" + NUMBER + ")", m.group(2)):
                        upload[k] = float(v) if k == "ms" else int(v)
    return clock_hz, wall_s, totals, counters, frames


def pct(sorted_vals, p):
    if not sorted_vals:
        return 0.0
    i = min(len(sorted_vals) - 1, int(p / 100.0 * len(sorted_vals)))
    return sorted_vals[i]


def print_frame(frame):
    s = frame["secs"]
    print(f"     frame {frame['frame']:>4} demo={frame.get('demo_frame', '?')} "
          f"server_s={frame.get('demo_s', '?')} total={frame['total']:7.3f} "
          f"world={s.get('world', 0):7.3f} ents={s.get('ents', 0):7.3f} "
          f"lmup={s.get('lmup', 0):7.3f} lmbuild={s.get('lmbuild', 0):6.3f} "
          f"present={s.get('present', 0):6.3f} "
          f"lm={frame['lm_calls']}/{frame['lm_kb']}KB ov={frame['overlays']}")
    if "active_weapon" in frame:
        print(f"       weapon={frame.get('weapon_name', '?')} "
              f"active={frame['active_weapon']} items={frame.get('items')} "
              f"bonus_end={frame.get('bonus_end')} "
              f"blend_alpha_end={frame.get('blend_alpha_end')}")
    for index, upload in sorted(frame["uploads"].items()):
        print(f"       upload {index}: atlas={upload.get('atlas')} "
              f"site={upload.get('site')} top={upload.get('top')} "
              f"rows={upload.get('rows')} source_bytes={upload.get('bytes')} "
              f"elapsed_ms={upload.get('ms', 0):.3f}")
    if frame.get("upload_dropped", 0):
        print(f"       WARNING: {frame['upload_dropped']} upload details omitted")


def main(paths):
    for path in paths:
        clock_hz, wall_s, totals, counters, frames = parse(path)
        if not frames:
            print(f"{path}: no frames parsed")
            continue
        worst = [f for f in frames if f["kind"] == "WORST"]
        window = [f for f in frames if f["kind"] == "WINDOW"]
        # v2 has explicit demo position: never discard a real >1 s hitch.
        # v1 has no such marker; retain its historical, labelled heuristic.
        play = [f for f in worst if (f["demo_frame"] > 0 if "demo_frame" in f
                                    else f["total"] < 1000.0)]
        play.sort(key=lambda f: f["total"])
        totals_play = [f["total"] for f in play]
        n = len(play)
        mean = sum(totals_play) / n if n else 0
        print(f"== {path}")
        print(f"   clock_hz={clock_hz} session_wall_s={wall_s} "
              f"worst_frames={len(worst)} play_worst={n} window_frames={len(window)}")
        if any("demo_frame" not in f for f in worst):
            print("   Legacy dump: >=1 s exclusion is only a loading-frame heuristic.")
        print("   NOTE: the on-target dump keeps only a top-16 worst-frame ring;")
        print("   the mean/percentiles below describe THAT RING, not the demo.")
        print(f"   ring frame ms: mean={mean:.1f} p50={pct(totals_play,50):.1f} "
              f"p90={pct(totals_play,90):.1f} p95={pct(totals_play,95):.1f} "
               f"p99={pct(totals_play,99):.1f} max={max(totals_play, default=0):.1f}")
        print(f"   >100ms frames: {sum(1 for t in totals_play if t > 100)}  "
              f">150ms: {sum(1 for t in totals_play if t > 150)}  "
              f">200ms: {sum(1 for t in totals_play if t > 200)}")
        print("   whole-run section totals (ms, nested):")
        for sec, ms in totals.items():
            print(f"     {sec:<10} {ms:12.1f}")
        print(f"   counters: {counters}")
        print("   slowest play frames (rank order as recorded):")
        for f in sorted(play, key=lambda f: -f["total"])[:10]:
            print_frame(f)
        if counters.get("window_valid") == 0:
            print("   WARNING: requested window was invalid (maximum 32 frames).")
        if window:
            print("   retained window (also keeps frames made fast by the A/B change):")
            for f in sorted(window, key=lambda f: f["frame"]):
                print_frame(f)
        print()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    main(sys.argv[1:])
