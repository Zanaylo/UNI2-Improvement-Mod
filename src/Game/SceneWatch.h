// Which scene the game is on, settled.
//
// The scene id at kSceneId carries transition steps that live for a frame or two, so a reader that
// believes the raw value flickers between screens. A value has to be held before it counts.

#pragma once

#include <cstdint>

namespace SceneWatch
{
	constexpr uint32_t kNone = 0xFFFFFFFF;

	void OnFrame();

	uint32_t Current();
	uint32_t Raw();
	unsigned HeldFrames();

	const char* StatusText();
}
