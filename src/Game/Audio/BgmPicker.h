#pragma once

class FileIndex;

namespace BgmPicker
{
	bool Initialize();

	void Layer(FileIndex& into);

	bool Active();
	bool Widened();

	const char* StatusText();
}
