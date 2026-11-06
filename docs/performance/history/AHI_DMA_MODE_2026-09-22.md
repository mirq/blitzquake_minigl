# AHI sound cost: flush bug + DMA mode (2026-09-22)

Session goal: remove the ~5 ms/frame AHI sound cost that kept the 640×480×16
fullscreen client at ~44 fps while `-nosound` ran at ~55 fps.

## Setup

- Physical A4000, 68060 + MPC7410 WarpOS, Prometheus + Radeon 9200,
  MiniGL V19 R200 stack. PAL.
- Client under test (final): `glqfinal` = `glquake_wos.warpelf`
  `CF1341FF` (1 091 568 B) → Elf2Exe2 `A44967F5` (739 068 B).
  Intermediate builds: `glqflush2`, `glqflush3`, `glqdiag`.
- DLL `Work:games/quake/minigl_ppc.dll` = `7FD5776D` (321 740 B);
  host `Work:ibdraw/mglhost` = `01C49B01` (48 504 B); fresh host per run.
- AHI mode during the first series: `Paula:HiFi 14 bit stereo++ calibrated`
  (`0002000d`, AHI's built-in default, no prefs file). Later switched by the
  operator to `Paula:DMA 8 bit stereo` (`00020018`).
- Command shape (one fresh host per run):
  `CD Work:games/quake` / `Stack 1048576` /
  `glq* -width 640 -height 480 -bpp 16 -ahi [-options] -benchmarkquit
   -benchmarklog DH2:wosbuild/<name>.txt +timedemo demo1`
- 969-frame demo1, fullscreen. Same boot for all runs unless noted. No
  `-condebug` in the benchmark runs (see below); no `MGLPPC_*` diagnostics.
- Renderer untouched by this change; parity counters were not instrumented
  for these audio runs (no GL path change).

## Measurements (fps / 969 frames, seconds)

| # | client + options | seconds | fps | vs `-nosound` |
|---|---|---:|---:|---:|
| 1 | `glqflush2 -nosound` | 17.52 | 55.308 | - |
| 2 | `glqflush2 -ahi` (native flush, Paula 14-bit calibrated) | 22.24 | 43.570 | +4.87 ms |
| 3 | `glqflush2 -ahi -sndflush exec` (old LP flush, same mode) | 23.32 | 41.552 | +5.99 ms |
| 4 | `glqflush2 -ahi -audspeed 4000` | 18.88 | 51.324 | +1.40 ms |
| 5 | `glqdiag -ahi -audspeed 22050` | 31.42 | 30.840 | +13.34 ms |
| 6 | `glqflush3 -ahi -ahiid 00020011` (Paula 8-bit HiFi) | 21.32 | 45.450 | +3.92 ms |
| 7 | `glqdiag -ahi -nomix` (driver only, no PPC mixing) | 22.06 | 43.926 | +3.98 ms |
| 8 | `glqdiag -ahi` (AHI switched to DMA mode) | 17.66 | 54.870 | +0.14 ms |
| 9 | `glqfinal -ahi` (DMA mode, run 1) | 17.50 | 55.371 | ~0 |
| 10 | `glqfinal -ahi` (DMA mode, run 2) | 17.78 | 54.499 | ~0 |
| 11 | `glqdiag -ahi -condebug` (DMA mode, diagnostic) | 19.00 | 51.000 | +4 fps I/O |

Notes:

- Run 7 is the decisive control: AHI keeps running (and the ring keeps
  playing) but `S_PaintChannels` is skipped. It still cost ~4 ms, so the PPC
  mixer is not the cost - the 68k AHI driver is.
- Runs 4/5 show the driver cost is almost exactly linear in the mix rate
  (4000 Hz → +1.4 ms, 11025 Hz → +4.9 ms, 22050 Hz → +13.3 ms).
- Run 6 shows the mode matters (8-bit mixing is cheaper than 14-bit), but
  only by ~1 ms; the DMA mode removes the cost entirely (run 8-10).
- Run 11: `-condebug` costs ~4 fps through console log I/O; never use it for
  benchmarks.

## Root cause of the old silent native-flush attempt

`SNDDMA_Submit` masked the flush range with `AHIBUFFERSIZE` (262 144 B, the
old low-level constant) while the high-level ring is `wos_buflen` (65 536 B
here). Once `paintedtime * 4` passed 64 KB (~1.5 s of painting) the native
flush targeted memory past the ring, so the painted samples were never
pushed to main memory and the 68k driver played silence. `CacheClearE` had
hidden the bug because every LP call flushes the whole PPC cache. Fixed by
computing the byte ring as `shm->samples * (shm->samplebits >> 3)`.

## Probe evidence

- `wos_time`: `DateStamp` LP call 1.10 ms; `CacheClearE` 8 KB via LP
  0.50-0.60 ms; native `dcbf` 8 KB < 0.05 ms; timebase 30.3 MHz.
- `wos_cache` (new): with `dcbf`+`sync` on fast RAM, the 68k sees the new
  data on the first read (T1 PASS) and does **not** serve a stale line on a
  repeated read (T2 PASS). The chip-RAM variant wedges the PPC task, so a
  chip-RAM ring is not viable in this runtime.
- `wos_ahimodes` (new): 26 audio modes; Paula IDs `00020001`-`00020012`,
  `00020018`; the only non-CPU-mixing mode is `Paula:DMA 8 bit stereo`
  (`00020018`, fixed 3 546 895 Hz mix-freq query). Names need
  `AHIDB_BufferLen` in the `AHI_GetAudioAttrs` tag list.

## Outcome and recommendation

- Client changes: native `dcbf`+`sync` flush over the correct ring range
  (default, `-sndflush exec` fallback), `-ahiid <hex>` mode override, a
  note when the selected mode is not a DMA mode, and the hidden Amiga
  pointer (`mglSetPointer()`).
- Configuration: AHI must use a **DMA** mode. With it, sound costs ~0 ms
  (54.9-55.4 fps vs 55.3 `-nosound`), i.e. the client is at the `-nosound`
  ceiling. The remaining gap to 60 fps is on the graphics/host path.
- Rollback: previous production client kept as `Work:games/quake/glqWOS.prefl`
  (`6B7B0938`).
