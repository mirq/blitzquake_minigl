# Frame-260 capture evidence

See `../../../FRAME260_INVESTIGATION.md` for the interpretation, exact binary
identities and run limitations. `.fp` files include both the old worst-frame
table and an explicit 250-270 window. Do not count overlapping records twice.

- `a32`: normal dynamic lighting, 800x600x32, complete timedemo.
- `a16`: normal dynamic lighting, 640x480x16, complete timedemo after recovery.
- `health`: production-client 640x480x16 run after the first cold power cycle.
- `b32.failedhost` / `b16.failedhost`: failed dynamic-off startup attempts;
  neither produced a completed timedemo measurement.
- `d32_*` / `d16_*`: deep per-upload attribution (profiled DLL+host pair):
  `host.log` (HOST_LM records), `ppc_lm.txt` (PPC_LM records), `fp.txt`
  (client frame profile with the 250-270 window). Frame-260 group = seqs
  22-25. See FRAME260_INVESTIGATION.md "Deep attribution".

Capture all files with the bridge's CRC-verified pull operation. No screenshot
or timing from a failed run is an accepted benchmark result.
