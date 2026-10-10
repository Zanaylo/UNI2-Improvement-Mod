#pragma once

#include "Game/Display/HudLayers.h"

namespace HudOpacity
{
	bool Install();
	void OnFrame();

	int Percent(HudLayers::Element element);
	int Lowest(HudLayers::Element element);
	const char* Name(HudLayers::Element element);
	const char* Info(HudLayers::Element element);

	void SetPercent(HudLayers::Element element, int percent);
	void Save(HudLayers::Element element);

	class Scope
	{
	public:
		explicit Scope(HudLayers::Element element);
		~Scope();

		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;

	private:
		int m_previousOpacity;
		bool m_previousScoped;
		HudLayers::Element m_previousElement;
	};
}
