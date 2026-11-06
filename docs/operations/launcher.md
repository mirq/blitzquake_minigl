# BlitzQuakePPC MiniGL Launcher (68k)

`BlitzQuakePPC MiniGL Launcher by Mirq` is a small **68k AmigaOS front-end**
that starts the WarpOS/PPC MiniGL client (`glquakeWOS`). It is a plain
Intuition/GadTools application (`launcher/`, see
[building.md](../building.md#blitzquakeppc-minigl-launcher-68k)); it contains
no renderer code and never links the game.

**Everything lives in one drawer.** The launcher uses the drawer it was started
from; there are no path fields. That drawer must contain:

| File | Role |
|---|---|
| `BlitzLaunch` | this launcher |
| `glquakeWOS` | PPC MiniGL client (Elf2Exe2-converted) |
| `mglhost` | 68k MiniGL host, started by the launcher |
| `minigl_ppc.dll` | MiniGL V19 R200 DLL |
| `id1/` | game data |

The names are fixed: the launcher always starts `glquakeWOS` and `mglhost`
from its own drawer, and there is no GUI or config override for them. Run
diagnostics land in the drawer:

| Log | Content |
|---|---|
| `launch.log` | Exact commands (InitPPC, host, client + arguments), appended per Run |
| `mglhost.log` | Host console output (overwritten per Run) |
| `client.log` | Client console output; the launcher passes `-stdout` so the client writes its startup trace here (overwritten per Run) |

The client and the host are resolved relative to that drawer; the launcher has
no GUI for paths on purpose.

## Window

| Control | Client option |
|---|---|
| `Resolution` + `Select…` | `-width`, `-height`, `-bpp` (ASL screen mode requester) |
| `Sound` cycle | `AHI` (default, passes `-ahi`) or `No sound` (`-nosound`) |
| `AHI ID` | `-ahiid <hex>` (empty = AHI preferences) |
| `Mix rate` | `-audspeed <hz>` (empty = client default) |
| `Flush` cycle | `-sndflush exec` only when `exec` is selected; native `dcbf+sync` is the client default, so nothing is passed |
| `Windowed` | `-windowmode` (off = fullscreen) |
| `Disable network` | `-noudp` |
| `No joystick` / `No mouse` | `-nojoy` / `-nomouse` |
| `Console log` | `-condebug` (do **not** use for benchmarks, ~4 fps) |
| `Start map` | `+map <name>` |
| `Extra args` | appended verbatim (`-game`, `-listen`, `-port`, `-mem`, `-zone`, `+timedemo`, `-benchmarkquit`, …) |
| `Run C:InitPPC first` | runs `C:InitPPC` before the host (default off) |
| `Start MiniGL host` | starts `mglhost` from the drawer (default on) |

Buttons: **Run**, **Save Settings**, **Quit**. The status line reports each
step.

## Run sequence

1. If `Run C:InitPPC first` is set, `C:InitPPC` is run synchronously. Keep this
   **off** unless the machine has just booted: `InitPPC` must run exactly once
   per boot ([runbook](runbook.md)).
2. If `Start MiniGL host` is set:
   - if the host port `MiniGLPPC.R200.Host.1` already exists, the launcher warns
     that a **stale host** may be present (previous crash); rebooting is the
     recommended recovery;
   - otherwise `mglhost` is started asynchronously from the game drawer with a
     64 KB stack and its console output redirected to `mglhost.log`, the
     launcher waits 8 s and verifies the port. **The client is not started if
     the port is missing** — that launch would hang in `MiniGLOpen`.
3. The client is started asynchronously (`Run >NIL:` semantics) with a 1 MB
   stack, the game drawer as its current directory (required for
   `minigl_ppc.dll` and `id1/`), `-stdout` and the assembled argument list; its
   console output goes to `client.log`. The launcher stays open; quitting it
   does **not** stop the game.

Arguments are always emitted as flag/value pairs; the client reads several
values without bound checks.

## Settings

`ENVARC:BlitzQuakePPC/Launcher.cfg`, ASCII `key=value`, one per line, `#` and
`;` comments, unknown keys ignored. **Save Settings** writes the file;
the launcher loads it at startup (defaults when missing).

| Key | Default |
|---|---|
| `width` / `height` / `depth` | `640` / `480` / `16` |
| `windowmode`, `nosound`, `noudp`, `nojoy`, `nomouse`, `condebug` | `0` |
| `ahiid`, `audspeed` | empty |
| `sndflush` | `native` |
| `map`, `extra` | empty |
| `initppc` / `initppcpath` | `0` / `C:InitPPC` |
| `starthost` | `1` |
| `hoststack` / `hostwait` | `65536` / `8` |
| `hostport` | `MiniGLPPC.R200.Host.1` |
| `stack` | `1048576` |

## Testing and debug tracing

```sh
cd launcher
make            # build/BlitzLaunch
make test       # build/argdump + build/cfgtest + build/launchtest
```

- `argdump` writes its `argv`, stack size and CLI state to `T:argdump.log`.
- `launchtest` calls the real `lq_start_client()` with `argdump` as the client,
  validating the assembled arguments, the explicit stack and the background
  start without the PPC client. Deploy both binaries to the same drawer.
- `cfgtest` saves a distinctive configuration, reads it back and writes
  `T:cfgtest.log`; it validates persistence without GUI input.
- Build with `-DLQ_TRACE` for a line trace: markers go to `T:lq_trace.log`
  (and stdout when run from a CLI).

## Caveats

- Use an AHI **DMA** mode for full speed; CPU-mixing Paula modes cost several
  ms/frame ([warpos.md](../targets/warpos.md#ahi-on-warpos-use-the-q3-high-level-scheme-measured)).
- After any client crash the host stays ghosted: reboot before the next launch
  ([runbook](runbook.md)). The launcher's port check catches the stale host but
  cannot recover it.
- `-condebug` is for troubleshooting only; use `-benchmarkquit -benchmarklog`
  for measurements.
