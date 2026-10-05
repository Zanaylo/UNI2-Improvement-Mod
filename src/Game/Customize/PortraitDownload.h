#pragma once

#include <string>

namespace PortraitDownload
{
	bool HasLink();
	bool Fetch(std::string& outError);

	bool BeginAll();
	void Update();

	bool IsBusy();
	int Progress();
	std::string StatusText();
}
