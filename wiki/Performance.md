# Performance

Its own window, opened from Config. It exists because the engine's frame pacing has a specific,
findable problem: the game runs its message pump on one thread and its frame on another, and every
frame the frame thread blocks on a message that only the pump can answer while the pump is asleep.
Windows also takes back the millisecond timer resolution while the game sits in the background,
which is what an alt-tab leaves behind.

Three options, each with its real trade-off written next to it, and two presets. The window reports
what is **actually** in force, read back from the device rather than from what the mod asked for.

The **Metrics** tab measures. Frame interval with its spread, a quarter-millisecond histogram around
the target, two-cluster detection for the judder a median cannot see, how long Present blocks, and a
paste-ready summary for bug reports.

An earlier build of this mod made the game feel worse, and did so by default: it added a second back
buffer for everyone and dragged high-refresh monitors down to 60 Hz in fullscreen, which cost a
frame of input latency and bought nothing. It no longer touches the display unless asked.
