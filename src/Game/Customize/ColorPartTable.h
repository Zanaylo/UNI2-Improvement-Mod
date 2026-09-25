#pragma once

namespace ColorPartTable
{
	void Load();
	bool IsLoaded();

	int GetIndexCount(int chara, int part);
	const int* GetIndices(int chara, int part);

	int GetSampleCount(int chara, int part);
	const int* GetSamples(int chara, int part);

	bool GetDefault(int chara, int slot, int* outValues);
}
