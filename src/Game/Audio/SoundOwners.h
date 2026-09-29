#pragma once

namespace SoundOwners
{
	int Count();
	int At(int index);
	int IndexOf(int owner);

	bool Has(int owner);
	bool IsFighter(int owner);

	const char* Name(int owner);
}
