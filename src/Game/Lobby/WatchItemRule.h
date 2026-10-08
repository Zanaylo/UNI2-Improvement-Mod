#pragma once

namespace WatchItemRule
{
	struct Room
	{
		bool enabled;
		bool inRoom;
		bool onRoomScreen;
		bool roomIdle;
		bool seated;
		bool matchOnOffer;
		bool watching;
	};

	bool ShouldOffer(const Room& room);
}

class WatchItemFlag
{
public:
	enum Write
	{
		Write_None,
		Write_Show,
		Write_Restore
	};

	Write Update(bool offer, bool hidden);
	bool IsShowing() const;

private:
	bool m_showing = false;
};
