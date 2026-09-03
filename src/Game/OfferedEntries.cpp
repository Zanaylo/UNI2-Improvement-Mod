#include "Game/OfferedEntries.h"

#include "Game/GameTables.h"

void OfferedEntries::Fill(int chara, bool* offered, int count)
{
	if (offered == nullptr || count <= 0)
		return;

	const int parts = GameTables::GetPartCount(chara);

	for (int part = 0; part < parts; ++part)
	{
		const char* name = nullptr;
		const unsigned char* entries = nullptr;
		int entryCount = 0;

		if (!GameTables::GetPart(chara, part, name, entries, entryCount))
			continue;

		for (int i = 0; i < entryCount; ++i)
		{
			if (entries[i] < count)
				offered[entries[i]] = true;
		}
	}
}
