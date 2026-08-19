// Every effect a character spawns, and the palette entries that effect paints, out of the game's
// own files. An HA6 frame spawns an effect by pattern number and draws its sprites by .pat id, and
// the .pat says which palette entry each of those is tinted from - so the list is complete before
// the character has moved, and nothing has to be seen on screen first.
//
// An effect with no entries draws untinted and cannot be recoloured. It is still listed, because a
// list that quietly dropped them would be answering a different question than the one it claims to.

#pragma once

namespace EffectTable
{
	struct Effect
	{
		// The pattern the effect is, in the character's own file.
		unsigned short pattern;

		// The effect's own code name, which most effects do not have.
		const char* code;

		// What the artists called it, UTF-8, usually Japanese. Empty for the unnamed ones.
		const char* name;

		// The code names of the moves that spawn it - "236A, 236B" - or empty when none is named.
		const char* spawnedBy;

		const unsigned char* entries;
		unsigned char count;
	};

	int GetCount(int chara);
	bool Get(int chara, int index, Effect& out);

	// How many effects paint this entry. An entry is shared, so this is what says how much else
	// moves when its colour is changed. Zero means no spawned effect claims it - Vatista's
	// permanently attached wings read zero, because they are sprite parts she draws rather than
	// objects anything spawns, and the editor lists those separately.
	int CountUsing(int chara, int entry);
}
