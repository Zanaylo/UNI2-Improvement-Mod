// Which palette entries a character's own sprite parts are tinted from, out of its .pat file.
// An entry that is not this character's belongs to the other side and should not be offered.

#pragma once

namespace PartColourTable
{
	bool IsPartEntry(int chara, int entry);
	int GetPartCount(int chara, int entry);
}
