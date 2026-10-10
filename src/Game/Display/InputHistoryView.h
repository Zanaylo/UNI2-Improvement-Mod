#pragma once

namespace InputHistoryView
{
	constexpr int kGameRows = 16;
	constexpr int kMostRows = 48;

	bool Install();
	void OnFrame();

	bool IsBehind();
	void SetBehind(bool behind);

	int Rows();
	void SetRows(int rows);
	void SaveRows();
}
