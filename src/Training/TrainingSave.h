// The game writes the training settings only when a training session returns to the main menu,
// so closing the game from inside offline training loses every change. This writes them when the
// training menu closes, the way the online lobby already does.

#pragma once

namespace TrainingSave
{
	void OnFrame();
}
