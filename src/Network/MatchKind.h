#pragma once

namespace MatchKind
{
	enum Kind
	{
		Kind_None,
		Kind_PlayerMatch,
		Kind_Other
	};

	Kind Classify();

	const char* Describe(Kind kind);
}
