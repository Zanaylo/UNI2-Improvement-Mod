#include "Overlay/Framework/FileDrop.h"

#include "Core/logger.h"

#include <shellapi.h>

#include <mutex>

namespace {

constexpr UINT kCountQuery = 0xFFFFFFFF;

HWND g_window = nullptr;
bool g_accepting = false;
std::mutex g_lock;
std::vector<std::string> g_dropped;

}

void FileDrop::Attach(HWND window)
{
	g_window = window;
}

void FileDrop::Accept(bool accept)
{
	if (g_window == nullptr || g_accepting == accept)
		return;

	g_accepting = accept;
	DragAcceptFiles(g_window, accept ? TRUE : FALSE);
}

bool FileDrop::Handle(UINT message, WPARAM wParam)
{
	if (message != WM_DROPFILES)
		return false;

	const HDROP drop = reinterpret_cast<HDROP>(wParam);
	const UINT count = DragQueryFileA(drop, kCountQuery, nullptr, 0);

	std::lock_guard<std::mutex> lock(g_lock);

	for (UINT i = 0; i < count; ++i)
	{
		char path[MAX_PATH] = {};

		if (DragQueryFileA(drop, i, path, MAX_PATH) != 0)
			g_dropped.push_back(path);
	}

	DragFinish(drop);
	LOG("FileDrop: %u item(s) dropped on the game window", count);
	return true;
}

bool FileDrop::Take(std::vector<std::string>& out)
{
	std::lock_guard<std::mutex> lock(g_lock);

	if (g_dropped.empty())
		return false;

	out.swap(g_dropped);
	g_dropped.clear();
	return true;
}
