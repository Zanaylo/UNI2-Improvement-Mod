#include "Game/Lobby/WatchItemRule.h"

bool WatchItemRule::ShouldOffer(const Room& room)
{
	return room.enabled && room.inRoom && room.onRoomScreen && room.roomIdle && !room.seated && room.matchOnOffer &&
		!room.watching;
}

WatchItemFlag::Write WatchItemFlag::Update(bool offer, bool hidden)
{
	if (offer && hidden)
	{
		m_showing = true;
		return Write_Show;
	}

	if (!offer && m_showing)
	{
		m_showing = false;
		return hidden ? Write_None : Write_Restore;
	}

	return Write_None;
}

bool WatchItemFlag::IsShowing() const
{
	return m_showing;
}
