#pragma once

namespace StageCards
{
	bool Initialize();

	void OnFrame();

	void Repaint();

	bool Reached();

	const char* StatusText();
}
