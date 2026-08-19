#pragma once

#include "Overlay/Window/IWindow.h"
#include "Overlay/WindowContainer/WindowType.h"

#include <map>
#include <memory>

class WindowContainer
{
public:
	WindowContainer();

	void UpdateAll();
	bool AnyWindowOpen() const;

	IWindow* GetWindow(WindowType type) const;

	template <typename T>
	T* GetWindow(WindowType type) const
	{
		return static_cast<T*>(GetWindow(type));
	}

private:
	std::map<WindowType, std::unique_ptr<IWindow>> m_windows;
};
