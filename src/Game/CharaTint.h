// A stage may ask for the characters standing on it to be tinted, the way the Judgement Hall wants
// them dark. The tint is the field SetCharaColor writes, so it is engine state and not a draw hook.

#pragma once

namespace CharaTint
{
	void Update();

	bool Applies();

	const char* StatusText();
}
