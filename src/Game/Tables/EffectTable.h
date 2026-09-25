#pragma once

namespace EffectTable
{
	struct Effect
	{
		unsigned short pattern;

		const char* code;

		const char* name;

		const char* spawnedBy;

		const unsigned char* entries;
		unsigned char count;
	};

	int GetCount(int chara);
	bool Get(int chara, int index, Effect& out);

	int CountUsing(int chara, int entry);
}
