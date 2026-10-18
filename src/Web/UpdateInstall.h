#pragma once

namespace UpdateInstall
{
	bool IsAvailable();

	void Start();

	void OnFrame();

	const char* GetError();
}
