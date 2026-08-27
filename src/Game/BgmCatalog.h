#pragma once

namespace BgmCatalog
{
	void Load();
	void Save();

	int Count();
	int IdAt(int index);
	bool IsListed(int id);

	bool IsAllowed(int id);
	void SetAllowed(int id, bool allowed);
	void SetAllAllowed(bool allowed);
	int AllowedCount();

	bool ShuffleEnabled();
	void SetShuffleEnabled(bool enabled);

	int Pick(int avoid);
}
