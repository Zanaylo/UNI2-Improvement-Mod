#include "Network/OnlineContext.h"

#include "Game/Audio/BgmControl.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/OnlineState.h"
#include "Network/NetLink.h"

bool OnlineContext::IsActive()
{
	return BgmControl::GetLastRequested() == GameOffsets::kBgmNetworkMenu ||
		NetLink::Lobby() != 0 || OnlineState::IsOnline();
}
