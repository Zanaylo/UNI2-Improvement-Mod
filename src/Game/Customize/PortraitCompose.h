#pragma once

#include <string>

namespace PortraitCompose
{
	std::string Folder();

	void Wear(int chara, const std::string& id);
	void Refresh();
	void Update();

	bool IsBusy();
	int Progress();
	std::string StatusText();
}
