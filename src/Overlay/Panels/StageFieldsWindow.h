#pragma once

#include <string>
#include <vector>

class StageFieldsWindow
{
public:
	void Open(int id, int slot, const std::string& name);
	void Draw();

private:
	void Refresh();
	void DrawCamera();
	void DrawField(int index);
	void Commit(int index, const std::string& value);
	void DrawReset();

	int m_id = -1;
	int m_slot = -1;
	bool m_open = false;
	long m_revision = -1;
	std::string m_name;
	std::vector<std::string> m_values;
	std::vector<bool> m_forced;
	bool m_edited = false;
};
