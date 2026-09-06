// The battle HUD. Cockpit_SetView is the game's own way in: its mode 0 hides the gauges, mode 1
// shows them, so the view field is written rather than the draw hooked.

#pragma once

namespace BattleCockpit
{
	bool IsHidden();

	void SetHidden(bool hidden);

	bool Reached();

	void Update();
}
