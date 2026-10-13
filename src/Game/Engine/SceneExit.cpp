#include "Game/Engine/SceneExit.h"

SceneExit::SceneExit(uint32_t scene)
	: m_scene(scene)
{
}

bool SceneExit::Observe(uint32_t current)
{
	const bool wasInside = m_inside;
	m_inside = current == m_scene;
	return wasInside && !m_inside;
}
