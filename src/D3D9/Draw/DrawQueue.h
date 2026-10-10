#pragma once

#include <cstdint>

namespace DrawQueue
{
	enum User
	{
		User_Probe = 1 << 0,
		User_HudOpacity = 1 << 1,
		User_InputHistory = 1 << 2,
	};

	class IObserver
	{
	public:
		virtual ~IObserver() = default;

		virtual uint32_t OnPush(uint32_t layer, void* command) = 0;
	};

	bool Install();
	void Want(User user, bool wanted);
	bool AddObserver(IObserver* observer);
}
