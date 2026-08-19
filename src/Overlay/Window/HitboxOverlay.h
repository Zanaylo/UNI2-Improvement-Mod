// The hitbox viewer. ClassifyBox is the single place a box type is decided - no box number is a
// mechanic on its own, so a catch is only ever read from the move's own catch record.

#pragma once

#include "Overlay/Window/IWindow.h"

#include "Game/HitboxData.h"

class HitboxOverlay : public IWindow
{
public:

	enum BoxCategory
	{
		BoxCategory_Pushbox,
		BoxCategory_Hurtbox,
		BoxCategory_Hitbox,
		BoxCategory_Clash,
		BoxCategory_ProjectileClash,
		BoxCategory_GuardPoint,
		BoxCategory_CrouchBlock,
		BoxCategory_Pull,
		BoxCategory_Marker,
		BoxCategory_GrabPoint,
		BoxCategory_Inert,
		BoxCategory_Other,
		BoxCategory_COUNT
	};

	struct CategorySettings
	{
		bool enabled;
		float fillAlpha;
		float outlineAlpha;
	};

	HitboxOverlay(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

	CategorySettings& GetCategory(int category) { return m_categories[category]; }

	static const char* GetCategoryName(int category);

	static const char* GetCategorySummary(int category);
	static const char* GetCategoryDetail(int category);
	static unsigned int GetCategoryColor(int category);

	static int ClassifyBox(const HitboxData::Box& box, int catchBoxIndex);

	bool& GetShowOrigin() { return m_showOrigin; }

protected:
	void BeforeDraw() override;
	void AfterDraw() override;
	void Draw() override;

private:
	void DrawEntity(void* entity, bool isEffect);

	CategorySettings m_categories[BoxCategory_COUNT];
	bool m_showOrigin;
};
