#pragma once

namespace TrainingHud
{
	class IDrawer
	{
	public:
		virtual ~IDrawer() = default;

		virtual void Draw(int layer) = 0;
	};

	bool Install(IDrawer* drawer);

	const char* StatusText();
}
