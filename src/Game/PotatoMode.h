// One switch for a machine that cannot hold 60.
//
// It is a preset over levers that already exist and none of them reaches the simulation. The stage
// keeps drawing - emptying it is a separate switch, because a match with no background is a worse
// trade than a slightly soft one.

#pragma once

namespace PotatoMode
{
	void Apply(bool enabled);
	bool IsActive();

	const char* Describe();
}
