#pragma once

namespace BattleCockpit
{
	bool IsHidden();

	void SetHidden(bool hidden, bool persist = true);

	bool Reached();

	void Update();
}
