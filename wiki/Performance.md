# Performance

Its own window, opened from Config.

It exists because the engine's frame pacing has a specific, findable problem. The game runs its
message pump on one thread and its frame on another, and every frame the frame thread blocks on a
message only the pump can answer, while the pump is asleep. Windows also takes back the millisecond
timer resolution while the game sits in the background, which is what an alt-tab leaves behind.

Three options, each with its trade-off written next to it, and two presets. The window reports what
is **actually** in force, read back from the device, not from what the mod asked for.

The **Metrics** tab measures: frame interval and its spread, a quarter-millisecond histogram around
the target, two-cluster detection for judder a median cannot see, how long Present blocks, and a
paste-ready summary for bug reports.

An earlier build of this mod made the game worse by default. It added a second back buffer for
everyone and dragged high-refresh monitors down to 60 Hz in fullscreen, costing a frame of input
latency for nothing. It no longer touches the display unless you ask.
