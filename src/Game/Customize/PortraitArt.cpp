#include "Game/Customize/PortraitArt.h"

#include "Core/Formats/ImageOps.h"
#include "Screens/PatEdit.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr const char* kOldVersus = "\x83\x4c\x83\x83\x83\x89\x8a\x47\x8f\xac";
constexpr const char* kOldFace = "chara_";
constexpr const char* kOldWinner = "chara_gazou";
constexpr const char* kOldMenu = "\x83\x4c\x83\x83\x83\x89";

constexpr const char* kArt = "\x83\x4c\x83\x83\x83\x89\x8a\x47";
constexpr const char* kThumb = "\x83\x4c\x83\x83\x83\x89\x89\x65";
constexpr const char* kGlow = "\x83\x8a\x83\x74";
constexpr const char* kCloseup = "_vs";
constexpr const char* kWinnerPrefix = "win";

constexpr float kMostCropped = 0.35f;
constexpr float kHeadY = 0.0f;
constexpr float kFaceY = 0.35f;
constexpr float kThumbY = 0.25f;
constexpr float kCentre = 0.5f;
constexpr float kFloor = 1.0f;
constexpr uint32_t kTransparent = 0;
constexpr uint8_t kSolid = 127;
constexpr int kFloorSlack = 2;
constexpr double kCompanionShare = 0.3;

enum Role
{
	Role_None,
	Role_Art,
	Role_Thumb,
	Role_Strip,
	Role_Closeup,
	Role_Clear,
};

struct Paint
{
	ImageOps::Rect rect;
	Role role;
	DdsImage::Image theirs;
	double mass;
};

bool StartsWith(const std::string& text, const char* prefix)
{
	return text.compare(0, strlen(prefix), prefix) == 0;
}

bool Holds(const std::string& text, const char* part)
{
	return text.find(part) != std::string::npos;
}

Role RoleOf(std::string name, PortraitArt::Screen screen)
{
	if (screen == PortraitArt::Screen_Winner && StartsWith(name, kWinnerPrefix))
		name = name.substr(strlen(kWinnerPrefix));

	if (Holds(name, kGlow))
		return Role_Clear;

	if (StartsWith(name, kThumb))
		return Role_Thumb;

	if (!StartsWith(name, kArt))
		return Role_None;

	if (screen != PortraitArt::Screen_Versus)
		return Role_Art;

	return Holds(name, kCloseup) ? Role_Closeup : Role_Strip;
}

const DdsImage::Image& ArtFor(PortraitArt::Screen screen, const PortraitArt::Sources& sources)
{
	if (screen == PortraitArt::Screen_Winner)
		return sources.winner;

	if (screen == PortraitArt::Screen_Menu)
		return sources.menu;

	return sources.versus;
}

const DdsImage::Image& SourceFor(Role role, PortraitArt::Screen screen, const PortraitArt::Sources& sources)
{
	return role == Role_Closeup ? sources.face : ArtFor(screen, sources);
}

float CropLoss(const DdsImage::Image& source, int width, int height)
{
	const float sourceAspect = static_cast<float>(source.width) / source.height;
	const float targetAspect = static_cast<float>(width) / height;
	const float ratio = sourceAspect > targetAspect ? targetAspect / sourceAspect : sourceAspect / targetAspect;

	return 1.0f - ratio;
}

DdsImage::Image Covered(const DdsImage::Image& source, const ImageOps::Rect& rect, Role role)
{
	if (role == Role_Strip)
		return ImageOps::Covering(source, rect.width, rect.height, kCentre, kHeadY);

	if (role == Role_Thumb)
		return ImageOps::Covering(source, rect.width, rect.height, kCentre, kThumbY);

	if (CropLoss(source, rect.width, rect.height) > kMostCropped)
		return ImageOps::Contained(source, rect.width, rect.height, kCentre, kFloor);

	return ImageOps::Covering(source, rect.width, rect.height, kCentre, kHeadY);
}

DdsImage::Image Fitted(const DdsImage::Image& source, const Paint& paint)
{
	if (paint.role == Role_Closeup)
		return ImageOps::Covering(source, paint.rect.width, paint.rect.height, kCentre, kFaceY);

	const DdsImage::Image framed = PortraitArt::Framed(source, paint.theirs);

	if (framed.width > 0)
		return framed;

	return Covered(source, paint.rect, paint.role);
}

int Rounded(double value)
{
	return static_cast<int>(std::lround(value));
}

int Bottom(const ImageOps::Rect& rect)
{
	return rect.y + rect.height;
}

bool Take(const std::vector<uint8_t>& pat, const char* partName, DdsImage::Image& out)
{
	PatEdit::Layout layout;

	if (!PatEdit::Survey(pat, layout))
		return false;

	const PatEdit::Part* const part = PatEdit::FindPartNamed(layout, partName);

	return part != nullptr && PatEdit::CutPart(pat, layout, *part, out);
}

bool TakeFirst(const std::vector<uint8_t>& pat, const char* prefix, DdsImage::Image& out)
{
	PatEdit::Layout layout;

	if (!PatEdit::Survey(pat, layout))
		return false;

	for (const PatEdit::Part& part : layout.parts)
	{
		if (StartsWith(part.name, prefix) && PatEdit::CutPart(pat, layout, part, out))
			return true;
	}

	return false;
}

bool Painted(const std::vector<Paint>& paints, const Paint& paint)
{
	for (const Paint& done : paints)
	{
		if (done.role == paint.role && done.rect.x == paint.rect.x && done.rect.y == paint.rect.y &&
			done.rect.width == paint.rect.width && done.rect.height == paint.rect.height)
		{
			return true;
		}
	}

	return false;
}

std::map<int, std::vector<Paint>> PaintsByAtlas(const std::vector<uint8_t>& pat, const PatEdit::Layout& layout,
	PortraitArt::Screen screen, std::map<int, DdsImage::Image>& surfaces)
{
	std::map<int, std::vector<Paint>> out;

	for (const PatEdit::Part& part : layout.parts)
	{
		const Role role = RoleOf(part.name, screen);
		const PatEdit::Atlas* const atlas = role == Role_None ? nullptr : PatEdit::FindAtlas(layout, part.atlas);

		if (atlas == nullptr)
			continue;

		if (surfaces.count(atlas->id) == 0 && !PatEdit::DecodeAtlas(pat, *atlas, surfaces[atlas->id]))
		{
			surfaces.erase(atlas->id);
			continue;
		}

		const DdsImage::Image& surface = surfaces[atlas->id];
		Paint paint{ PatEdit::PartRect(part, surface.width, surface.height), role, DdsImage::Image(), 0.0 };

		if (Painted(out[atlas->id], paint))
			continue;

		paint.theirs = ImageOps::Crop(surface, paint.rect);
		paint.mass = ImageOps::FigureOf(paint.theirs, kSolid).mass;
		out[atlas->id].push_back(std::move(paint));
	}

	return out;
}

void ClearCompanions(std::map<int, std::vector<Paint>>& paints)
{
	std::map<Role, double> largest;

	for (const auto& atlas : paints)
	{
		for (const Paint& paint : atlas.second)
			largest[paint.role] = (std::max)(largest[paint.role], paint.mass);
	}

	for (auto& atlas : paints)
	{
		for (Paint& paint : atlas.second)
		{
			if (paint.mass < largest[paint.role] * kCompanionShare)
				paint.role = Role_Clear;
		}
	}
}

bool HoldsArt(const std::map<int, std::vector<Paint>>& paints)
{
	for (const auto& atlas : paints)
	{
		for (const Paint& paint : atlas.second)
		{
			if (paint.role == Role_Art || paint.role == Role_Strip)
				return true;
		}
	}

	return false;
}

}

bool PortraitArt::TakeVersus(const std::vector<uint8_t>& pat, Sources& out)
{
	return Take(pat, kOldVersus, out.versus);
}

bool PortraitArt::TakeFace(const std::vector<uint8_t>& pat, Sources& out)
{
	return TakeFirst(pat, kOldFace, out.face);
}

bool PortraitArt::TakeWinner(const std::vector<uint8_t>& pat, Sources& out)
{
	return Take(pat, kOldWinner, out.winner);
}

bool PortraitArt::TakeMenu(const std::vector<uint8_t>& pat, Sources& out)
{
	return Take(pat, kOldMenu, out.menu);
}

DdsImage::Image PortraitArt::Framed(const DdsImage::Image& ours, const DdsImage::Image& theirs)
{
	const ImageOps::Figure mine = ImageOps::FigureOf(ours, kSolid);
	const ImageOps::Figure target = ImageOps::FigureOf(theirs, kSolid);

	if (mine.mass <= 0.0 || target.mass <= 0.0)
		return DdsImage::Image();

	const double scale = std::sqrt(target.mass / mine.mass);
	const DdsImage::Image art = ImageOps::Resized(ours, Rounded(ours.width * scale), Rounded(ours.height * scale));
	const double left = target.x - mine.x * scale;
	double top = target.y - mine.y * scale;

	if (Bottom(target.bounds) >= theirs.height - kFloorSlack)
		top = (std::max)(top, theirs.height - Bottom(mine.bounds) * scale);

	DdsImage::Image out = ImageOps::Blank(theirs.width, theirs.height);
	ImageOps::Over(out, art, Rounded(left), Rounded(top));
	return out;
}

bool PortraitArt::Repaint(const std::vector<uint8_t>& pat, Screen screen, const Sources& sources,
	std::vector<uint8_t>& out)
{
	PatEdit::Layout layout;

	if (!PatEdit::Survey(pat, layout))
		return false;

	std::map<int, DdsImage::Image> surfaces;
	std::map<int, std::vector<Paint>> paints = PaintsByAtlas(pat, layout, screen, surfaces);

	if (!HoldsArt(paints))
		return false;

	ClearCompanions(paints);

	PatEdit::Additions additions;

	for (const auto& atlas : paints)
	{
		DdsImage::Image& surface = surfaces[atlas.first];

		for (const Paint& paint : atlas.second)
			ImageOps::Fill(surface, paint.rect, kTransparent);

		for (const Paint& paint : atlas.second)
		{
			const DdsImage::Image& source = SourceFor(paint.role, screen, sources);

			if (paint.role == Role_Clear || source.width == 0)
				continue;

			ImageOps::Over(surface, Fitted(source, paint), paint.rect.x, paint.rect.y);
		}

		const PatEdit::Atlas* const original = PatEdit::FindAtlas(layout, atlas.first);
		additions.replacedAtlases[atlas.first] = PatEdit::AtlasBlock(atlas.first, original->name, surface);
	}

	out = PatEdit::Rebuild(pat, layout, additions);
	return true;
}
