#pragma once

#include "Network/NetLink.h"

namespace MatchKind
{
	enum Kind
	{
		Kind_None,
		Kind_PlayerMatch,
		Kind_Other
	};

	void Tick(const NetLink::Snapshot& snapshot);
	Kind Classify();

	const char* Describe(Kind kind);
}
