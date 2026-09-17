# Reporting a problem

Everything the mod writes is in **`UNI2-IM\Logs\`**, beside the game. Send the files from the run where the
problem happened, not a later one.

| File | What it is |
|---|---|
| `UNI2_IM_<date>_<time>.log` | the session log, one per run, the twenty newest kept |
| `UNI2_IM_NET_<date>_<time>.log` | the network log, the ten newest kept |
| `crash_<date>_<time>_<kind>.dmp` | a crash or hang dump, the ten newest kept |
| `UNI2_IM_meter_<date>_<time>_NN.csv` | frame meter traces, only with `[Debug] MeterTrace = 1` |

Both logs are off until you turn them on. The session log needs `[Debug] Logging = 1` in `UNI2_IM.ini`.
The network log has a tick box, **Write the network log**, in the Netplay window's Network log tab, or
`[Netplay] NetLog = 1` in the same file.

## A match felt worse with the mod

Turn the network log on, play the match that feels wrong, then send `UNI2_IM_NET_*.log`. Nothing is written
before you turn it on, so it has to be on for the run where it happened.

It holds one line per second of the match with the frame times, how long the mod
itself took each frame, the rollbacks, the ping and how many of your inputs the other side has not confirmed.
It also records every packet the mod sent, and the reason whenever it held one back.

That file is what separates the three possible answers: the mod costing you frames, the connection itself, or
the other player's machine.

If you want the whole picture, turn on **Include GGPO's own lines** in the Netplay window's Network log tab
first. That adds what the netcode says about syncing and lost packets, which the game normally throws away.
The lines it writes every frame are counted once a second instead of written out, so the file stays small.

## The game crashed

Send the `crash_*.dmp` from that run, with the session log. The log's last lines name the faulting module.

If there is no dump at all, the crash was one Windows handles on its own. Open **Event Viewer**, go to
**Windows Logs → Application**, find the **Application Error** entry for `uni2.exe`, and send the faulting
module name and offset from it.

## The game did not start, or hung with only a taskbar icon

Send the session log. Its last line says how far the mod got. Also say which other programs hook the game:
Special K, ReShade, RivaTuner or MSI Afterburner, Discord, OBS, the Steam overlay. The log lists every
non-Windows DLL in the process near the start and again at the first frame, but naming them helps.

## The frame meter or the hitbox viewer drew something wrong

Set `[Debug] MeterTrace = 1`, reproduce it, then send the `UNI2_IM_meter_*.csv` files with a screenshot. Each
row is one frame per player, with the pattern, the flags and what the meter drew, which is what settles a
disagreement with the game's own numbers.

For the hitbox viewer say your resolution and whether the game is fullscreen or windowed.
