#pragma once

#include <d3d9.h>

namespace Modules
{
	enum Group
	{
		Group_PresentBegin,
		Group_Frame,
		Group_Replay,
		Group_Hud,
		Group_Input,
		Group_Palette,
		Group_Share,
		Group_Tail,
		Group_COUNT
	};

	using Step = void (*)();

	void RunGuarded(const char* name, Step step);

	bool InstallGameHooks();

	void Run(Group group, IDirect3DDevice9* device);
}
