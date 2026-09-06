#pragma once

namespace PlayerHealth
{
	struct Health
	{
		int current;
		int trailing;
		int max;
	};

	bool Read(void* playerData, Health& out);
}
