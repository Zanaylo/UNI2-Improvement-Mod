# Performance

Its own window, opened from Config.

It fixes a real frame pacing problem in the engine. The game runs its message pump on one thread and
its frame on another. Every frame, the frame thread waits for a message that only the pump can answer,
while the pump is asleep. Windows also lowers the timer resolution while the game is in the
background, and an alt-tab leaves it that way.

There are three options, each with its trade-off written next to it, and two presets. The window shows
what is **actually** in effect, read back from the device, not what the mod asked for.

The **Metrics** tab measures: frame interval and its spread, a quarter-millisecond histogram around
the target, two-cluster detection for judder that a median cannot show, how long Present blocks, and a
summary you can paste into a bug report.

The mod does not touch your display unless you ask. An earlier build did: it added a second back
buffer for everyone and dropped high refresh monitors to 60 Hz in fullscreen, which cost a frame of
input lag for nothing.
