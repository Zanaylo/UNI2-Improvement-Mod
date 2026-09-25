#pragma once

namespace SoundpackTransfer
{
	bool Export(const char* zipPath);
	bool Import(const char* zipPath);

	void Update();

	bool IsBusy();

	const char* StatusText();
}
