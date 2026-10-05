#include "Game/Customize/PortraitPainter.h"

#include "Core/Formats/ImageOps.h"
#include "Game/Customize/PortraitPlacement.h"
#include "Screens/PatEdit.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr const char* kArt = "\x83\x4c\x83\x83\x83\x89\x8a\x47";
constexpr const char* kShadow = "\x83\x4c\x83\x83\x83\x89\x89\x65";
constexpr const char* kWinner = "win";
constexpr const char* kFigure = "00";
constexpr const char* kSpecial = "sp00";
constexpr const char* kFace = "00_\x8a\xe7";
constexpr const char* kCloseup = "00_vs";
constexpr const char* kCardName = "icon_%02d";
constexpr int kCardWidth = 48;
constexpr int kCardHeight = 128;
constexpr int kCellBorder = 1;
constexpr uint8_t kBackdropTop = 136;
constexpr uint8_t kBackdropBottom = 92;
constexpr uint8_t kOpaque = 255;
constexpr uint32_t kClear = 0;
constexpr uint8_t kSolid = 127;
constexpr int kEdgeBand = 2;
constexpr int kTouchPixels = 8;
constexpr double kFadeShare = 0.08;
constexpr double kSelectFadeStart = 0.84375;
constexpr int kShadowBlur = 2;

constexpr PortraitFrames::Frame kMainFrames[PortraitPainter::Screen_Count] = {
	PortraitFrames::Frame_Select,
	PortraitFrames::Frame_Versus,
	PortraitFrames::Frame_Winner,
	PortraitFrames::Frame_Menu,
};

enum Role
{
	Role_None,
	Role_Clear,
	Role_Shadow,
	Role_Closeup,
	Role_Main,
};

struct Paint
{
	int atlas;
	ImageOps::Rect rect;
	Role role;
};

struct Sides
{
	bool left;
	bool right;
	bool top;
	bool bottom;
};

bool StartsWith(const std::string& text, const char* prefix)
{
	return text.compare(0, strlen(prefix), prefix) == 0;
}

std::string Unprefixed(const std::string& name)
{
	return StartsWith(name, kWinner) ? name.substr(strlen(kWinner)) : name;
}

Role ShadowRole(const std::string& rest)
{
	return rest == kFigure || rest == kSpecial ? Role_Shadow : Role_Clear;
}

Role ArtRole(const std::string& rest)
{
	if (rest == kFigure || rest == kSpecial || rest == kFace)
		return Role_Main;

	return rest == kCloseup ? Role_Closeup : Role_Clear;
}

Role RoleOf(const std::string& partName)
{
	const std::string name = Unprefixed(partName);

	if (StartsWith(name, kShadow))
		return ShadowRole(name.substr(strlen(kShadow)));

	if (StartsWith(name, kArt))
		return ArtRole(name.substr(strlen(kArt)));

	return Role_None;
}

bool SameRect(const ImageOps::Rect& one, const ImageOps::Rect& other)
{
	return one.x == other.x && one.y == other.y && one.width == other.width && one.height == other.height;
}

void Keep(std::vector<Paint>& paints, const Paint& paint)
{
	for (Paint& kept : paints)
	{
		if (kept.atlas != paint.atlas || !SameRect(kept.rect, paint.rect))
			continue;

		kept.role = (std::max)(kept.role, paint.role);
		return;
	}

	paints.push_back(paint);
}

std::vector<Paint> PaintsOf(const PatEdit::Layout& layout)
{
	std::vector<Paint> paints;

	for (const PatEdit::Part& part : layout.parts)
	{
		const Role role = RoleOf(part.name);
		const PatEdit::Atlas* const atlas = role == Role_None ? nullptr : PatEdit::FindAtlas(layout, part.atlas);

		if (atlas == nullptr)
			continue;

		Keep(paints, Paint{ atlas->id, PatEdit::PartRect(part, atlas->width, atlas->height), role });
	}

	return paints;
}

const Paint* MainOf(const std::vector<Paint>& paints)
{
	for (const Paint& paint : paints)
	{
		if (paint.role == Role_Main)
			return &paint;
	}

	return nullptr;
}

bool Valid(const PortraitPainter::Figure& figure)
{
	return figure.art != nullptr && figure.art->width > 0 && figure.entry != nullptr && figure.frames != nullptr;
}

ImageOps::Rect Detailed(const ImageOps::Rect& rect, int detail = PortraitPainter::kDetail)
{
	return ImageOps::Rect{ rect.x * detail, rect.y * detail, rect.width * detail, rect.height * detail };
}

int DetailOf(const std::vector<Paint>& paints, int atlas)
{
	for (const Paint& paint : paints)
	{
		if (paint.atlas == atlas && paint.role != Role_Clear)
			return PortraitPainter::kDetail;
	}

	return 1;
}

ImageOps::Affine PlaceOn(const PortraitPainter::Figure& figure, PortraitFrames::Frame frame, double zoom)
{
	return PortraitPlacement::Place(*figure.entry, figure.art->width, *figure.frames, frame, zoom);
}

bool Solid(const DdsImage::Image& image, int x, int y)
{
	return image.pixels[(static_cast<size_t>(y) * image.width + x) * 4 + 3] > kSolid;
}

int SolidIn(const DdsImage::Image& image, const ImageOps::Rect& area)
{
	int count = 0;

	for (int y = area.y; y < area.y + area.height; ++y)
	{
		for (int x = area.x; x < area.x + area.width; ++x)
			count += Solid(image, x, y) ? 1 : 0;
	}

	return count;
}

Sides CutSides(const DdsImage::Image& art)
{
	const int columns = (std::min)(kEdgeBand, art.width);
	const int rows = (std::min)(kEdgeBand, art.height);

	return Sides{
		SolidIn(art, ImageOps::Rect{ 0, 0, columns, art.height }) > kTouchPixels,
		SolidIn(art, ImageOps::Rect{ art.width - columns, 0, columns, art.height }) > kTouchPixels,
		SolidIn(art, ImageOps::Rect{ 0, 0, art.width, rows }) > kTouchPixels,
		SolidIn(art, ImageOps::Rect{ 0, art.height - rows, art.width, rows }) > kTouchPixels,
	};
}

double Ramp(double distance, double length)
{
	return (std::min)((std::max)(distance / length, 0.0), 1.0);
}

double EdgeShare(bool cut, double edge, double inward, double at, int size, double length)
{
	if (!cut || edge <= 0.0 || edge >= size)
		return 1.0;

	return Ramp((at - edge) * inward, length);
}

void FadeCuts(DdsImage::Image& painted, const DdsImage::Image& art, const ImageOps::Affine& place)
{
	const Sides cut = CutSides(art);

	if (!cut.left && !cut.right && !cut.top && !cut.bottom)
		return;

	const double length = kFadeShare * (std::max)(painted.width, painted.height);
	const double across = place.scaleX > 0.0 ? 1.0 : -1.0;
	const double left = place.x;
	const double right = place.x + place.scaleX * art.width;
	const double top = place.y;
	const double bottom = place.y + place.scaleY * art.height;

	for (int y = 0; y < painted.height; ++y)
	{
		const double row = (std::min)(EdgeShare(cut.top, top, 1.0, y + 0.5, painted.height, length),
			EdgeShare(cut.bottom, bottom, -1.0, y + 0.5, painted.height, length));

		for (int x = 0; x < painted.width; ++x)
		{
			const double share = (std::min)({ row,
				EdgeShare(cut.left, left, across, x + 0.5, painted.width, length),
				EdgeShare(cut.right, right, -across, x + 0.5, painted.width, length) });

			if (share >= 1.0)
				continue;

			uint8_t* const alpha = &painted.pixels[(static_cast<size_t>(y) * painted.width + x) * 4 + 3];
			*alpha = static_cast<uint8_t>(std::lround(*alpha * share));
		}
	}
}

double SelectFadeShare(double across)
{
	const double eased = across * across * (3.0 - 2.0 * across);
	return 1.0 - (across + eased) / 2.0;
}

void FadeRightEdge(DdsImage::Image& image)
{
	const double start = kSelectFadeStart * image.width;
	const int first = static_cast<int>(start);
	std::vector<double> shares;

	for (int x = first; x < image.width; ++x)
		shares.push_back(SelectFadeShare(Ramp(x + 0.5 - start, image.width - start)));

	for (int y = 0; y < image.height; ++y)
	{
		uint8_t* const row = &image.pixels[(static_cast<size_t>(y) * image.width + first) * 4];

		for (size_t column = 0; column < shares.size(); ++column)
			row[column * 4 + 3] = static_cast<uint8_t>(std::lround(row[column * 4 + 3] * shares[column]));
	}
}

DdsImage::Image Drawn(const PortraitPainter::Figure& figure, PortraitFrames::Frame frame, int width, int height,
	double zoom)
{
	const ImageOps::Affine place = PlaceOn(figure, frame, zoom);
	DdsImage::Image painted = ImageOps::Mapped(*figure.art, width, height, place);
	FadeCuts(painted, *figure.art, place);
	return painted;
}

bool Grown(const std::vector<uint8_t>& pat, const PatEdit::Layout& layout, int id, int detail,
	std::map<int, DdsImage::Image>& surfaces)
{
	if (surfaces.count(id) != 0)
		return true;

	const PatEdit::Atlas* const atlas = PatEdit::FindAtlas(layout, id);
	DdsImage::Image decoded;

	if (atlas == nullptr || !PatEdit::DecodeAtlas(pat, *atlas, decoded))
		return false;

	surfaces[id] = detail == 1 ? decoded : ImageOps::Resized(decoded, decoded.width * detail, decoded.height * detail);
	return true;
}

void Draw(DdsImage::Image& surface, const Paint& paint, const Paint& main, PortraitPainter::Screen screen,
	const PortraitPainter::Figure& figure)
{
	const ImageOps::Rect rect = Detailed(paint.rect);
	const double share = paint.role == Role_Shadow ? static_cast<double>(paint.rect.width) / main.rect.width : 1.0;
	const PortraitFrames::Frame frame = paint.role == Role_Closeup ? PortraitFrames::Frame_Closeup : kMainFrames[screen];

	const int blur = paint.role == Role_Shadow ? kShadowBlur * PortraitPainter::kDetail : 0;
	DdsImage::Image drawn = ImageOps::Blurred(
		Drawn(figure, frame, rect.width, rect.height, PortraitPainter::kDetail * share), blur);

	if (screen == PortraitPainter::Screen_Select)
		FadeRightEdge(drawn);

	ImageOps::Copy(surface, drawn, rect.x, rect.y);
}

std::vector<uint8_t> Rebuilt(const std::vector<uint8_t>& pat, const PatEdit::Layout& layout,
	const std::map<int, DdsImage::Image>& surfaces)
{
	PatEdit::Additions additions;

	for (const auto& surface : surfaces)
		additions.replacedAtlases[surface.first] =
			PatEdit::AtlasBlock(surface.first, PatEdit::FindAtlas(layout, surface.first)->name, surface.second);

	return PatEdit::Rebuild(pat, layout, additions);
}

DdsImage::Image Backdrop(int width, int height)
{
	DdsImage::Image out = ImageOps::Blank(width, height);

	for (int y = 0; y < height; ++y)
	{
		const double down = height > 1 ? static_cast<double>(y) / (height - 1) : 0.0;
		const uint8_t grey = static_cast<uint8_t>(std::lround(kBackdropTop + (kBackdropBottom - kBackdropTop) * down));

		for (int x = 0; x < width; ++x)
		{
			uint8_t* const pixel = &out.pixels[(static_cast<size_t>(y) * width + x) * 4];
			pixel[0] = pixel[1] = pixel[2] = grey;
			pixel[3] = kOpaque;
		}
	}

	return out;
}

const PatEdit::Part* CardPart(const PatEdit::Layout& layout, int chara)
{
	char prefix[16] = {};
	snprintf(prefix, sizeof(prefix), kCardName, chara);
	const size_t length = strlen(prefix);

	for (const PatEdit::Part& part : layout.parts)
	{
		if (part.name.compare(0, length, prefix) != 0)
			continue;

		if (part.name.size() == length || !isdigit(static_cast<unsigned char>(part.name[length])))
			return &part;
	}

	return nullptr;
}

void DressCell(DdsImage::Image& sheet, const ImageOps::Rect& cell, const DdsImage::Image& card)
{
	const int border = kCellBorder * PortraitPainter::kDetail;
	const ImageOps::Rect inside = { border, border, cell.width - border * 2, cell.height - border * 2 };

	if (inside.width <= 0 || inside.height <= 0 || card.width != cell.width || card.height != cell.height)
		return;

	ImageOps::Copy(sheet, ImageOps::Crop(card, inside), cell.x + border, cell.y + border);
}

}

bool PortraitPainter::Repaint(const std::vector<uint8_t>& pat, Screen screen, const Figure& figure,
	std::vector<uint8_t>& out)
{
	PatEdit::Layout layout;

	if (!Valid(figure) || screen < 0 || screen >= Screen_Count || !PatEdit::Survey(pat, layout))
		return false;

	const std::vector<Paint> paints = PaintsOf(layout);
	const Paint* const main = MainOf(paints);

	if (main == nullptr)
		return false;

	std::map<int, DdsImage::Image> surfaces;

	for (const Paint& paint : paints)
	{
		const int detail = DetailOf(paints, paint.atlas);

		if (Grown(pat, layout, paint.atlas, detail, surfaces))
			ImageOps::Fill(surfaces[paint.atlas], Detailed(paint.rect, detail), kClear);
	}

	for (const Paint& paint : paints)
	{
		if (paint.role != Role_Clear && surfaces.count(paint.atlas) != 0)
			Draw(surfaces[paint.atlas], paint, *main, screen, figure);
	}

	out = Rebuilt(pat, layout, surfaces);
	return true;
}

DdsImage::Image PortraitPainter::PaintCard(const Figure& figure)
{
	const int width = kCardWidth * kDetail;
	const int height = kCardHeight * kDetail;
	DdsImage::Image card = Backdrop(width, height);

	if (!Valid(figure))
		return card;

	ImageOps::Over(card, Drawn(figure, PortraitFrames::Frame_Card, width, height, kDetail), 0, 0);
	return card;
}

bool PortraitPainter::DressCards(const std::vector<uint8_t>& select, const std::vector<Card>& cards,
	std::vector<uint8_t>& out)
{
	PatEdit::Layout layout;

	if (cards.empty() || !PatEdit::Survey(select, layout))
		return false;

	std::map<int, DdsImage::Image> sheets;

	for (const Card& card : cards)
	{
		const PatEdit::Part* const part = CardPart(layout, card.chara);
		const PatEdit::Atlas* const atlas = part == nullptr ? nullptr : PatEdit::FindAtlas(layout, part->atlas);

		if (atlas == nullptr || !Grown(select, layout, atlas->id, kDetail, sheets))
			continue;

		DressCell(sheets[atlas->id], Detailed(PatEdit::PartRect(*part, atlas->width, atlas->height)), card.art);
	}

	if (sheets.empty())
		return false;

	out = Rebuilt(select, layout, sheets);
	return true;
}
