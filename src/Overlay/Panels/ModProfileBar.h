#pragma once

#include <string>
#include <vector>

class ModProfileBar
{
public:
	bool Draw();

private:
	void Refresh();
	bool DrawPicker();
	void DrawActions();
	void DrawNaming();
	int ActiveIndex() const;

	std::vector<std::string> m_names;
	std::string m_active;
	long m_revision = -1;
	bool m_matches = false;
	bool m_naming = false;
	char m_name[33] = {};
};
