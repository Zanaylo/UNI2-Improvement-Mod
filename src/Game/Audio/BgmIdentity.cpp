#include "Game/Audio/BgmIdentity.h"

#include "Game/Audio/BgmLibrary.h"

int BgmIdentity::LoadedSlot(const Binding& binding, int gameId, uint32_t stream)
{
	if (stream == 0 || gameId < 0)
		return kNone;

	if (binding.stream == stream && binding.gameId == gameId)
		return binding.slot;

	if (gameId >= BgmLibrary::kFirstId)
		return kNone;

	return gameId;
}
