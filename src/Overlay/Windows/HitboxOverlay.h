#pragma once

#include "Overlay/Framework/IWindow.h"

#include "Game/Engine/Camera.h"
#include "Game/Engine/HitboxData.h"

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

	static bool IsShown();
	static void SetShown(bool shown);

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
	void DrawEntity(const Camera::ScreenTransform& transform, void* entity, bool isEffect);

	CategorySettings m_categories[BoxCategory_COUNT];
	bool m_showOrigin;
};
