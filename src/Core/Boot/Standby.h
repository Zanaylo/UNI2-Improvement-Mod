#pragma once

#include "Core/GameBuild.h"

namespace Standby
{
	void Decide();
	bool IsOn();
	GameBuild::Age GameAge();
	void OnFrame();
}
