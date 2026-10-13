#pragma once

#include <cstdint>

class SceneExit
{
public:
	explicit SceneExit(uint32_t scene);

	bool Observe(uint32_t current);

private:
	uint32_t m_scene;
	bool m_inside = false;
};
