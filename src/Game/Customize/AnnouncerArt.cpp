#include "Game/Customize/AnnouncerArt.h"

#include "Core/Formats/ImageOps.h"
#include "Screens/PatEdit.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

namespace {

constexpr const char* kSelectFace = "cselface00";
constexpr const char* kMenuArt = "\x83\x4c\x83\x83\x83\x89\x8a\x47" "00";
constexpr const char* kMenuShadow = "\x83\x4c\x83\x83\x83\x89\x89\x65" "00";

constexpr int kTemplateNumber = 100;
constexpr const char* kVariants[] = { "", "h", "c", "l" };
constexpr int kFrames = 4;

constexpr int kKinds = 3;
constexpr const char* kKindPrefixes[kKinds] = { "icon00_", "icon01_", "icon02_" };

constexpr int kIconWidth = 48;
constexpr int kIconHeight = 128;
constexpr int kAtlasSide = 512;
constexpr int kAtlasColumns = kAtlasSide / kIconWidth;
constexpr int kAtlasRows = kAtlasSide / kIconHeight;
constexpr int kUvUnits = 256;

constexpr float kHeadShare = 0.35f;
constexpr float kStripAspect = static_cast<float>(kIconWidth) / kIconHeight;
constexpr uint8_t kOpaqueEnough = 32;

constexpr uint32_t kTransparent = 0x00000000u;
constexpr uint32_t kCardGrey = 0xff646468u;
constexpr uint32_t kCardWhite = 0xffffffffu;
constexpr uint32_t kCardBlack = 0xff000000u;
constexpr uint32_t kCardYellow = 0xfffff00au;
constexpr uint32_t kWashYellow = 0x50ffdc00u;
constexpr float kDimmed = 0.62f;
constexpr uint32_t kSystemTint = 0xffff8c8cu;
constexpr float kMenuHeadX = 0.683f;
constexpr float kMenuFadeShare = 0.2f;

enum Kind
{
	Kind_Normal,
	Kind_Dimmed,
	Kind_Chosen,
};

int KindOf(const std::string& partName)
{
	for (int kind = 0; kind < kKinds; ++kind)
	{
		const std::string prefix = kKindPrefixes[kind];

		if (partName.compare(0, prefix.size(), prefix) == 0)
			return kind;
	}

	return -1;
}

DdsImage::Image Strip(const DdsImage::Image& portrait)
{
	const int height = portrait.height;
	const int width = std::min(portrait.width, static_cast<int>(height * kStripAspect));
	const float centre = ImageOps::OpaqueCentreX(portrait, 0, static_cast<int>(height * kHeadShare),
		kOpaqueEnough);
	const int left = std::clamp(static_cast<int>(centre) - width / 2, 0, portrait.width - width);

	return ImageOps::Resized(ImageOps::Crop(portrait, ImageOps::Rect{ left, 0, width, height }),
		kIconWidth, kIconHeight);
}

void Frame(DdsImage::Image& card, uint32_t colour, int thickness)
{
	ImageOps::Fill(card, ImageOps::Rect{ 0, 0, card.width, thickness }, colour);
	ImageOps::Fill(card, ImageOps::Rect{ 0, card.height - thickness, card.width, thickness }, colour);
	ImageOps::Fill(card, ImageOps::Rect{ 0, 0, thickness, card.height }, colour);
	ImageOps::Fill(card, ImageOps::Rect{ card.width - thickness, 0, thickness, card.height }, colour);
}

DdsImage::Image Card(const DdsImage::Image& strip, int kind)
{
	DdsImage::Image card = ImageOps::Blank(kIconWidth, kIconHeight, kCardGrey);
	ImageOps::Over(card, strip, 0, 0);

	if (kind == Kind_Dimmed)
	{
		ImageOps::Grey(card, kDimmed);
		Frame(card, kCardBlack, 1);
		return card;
	}

	if (kind == Kind_Chosen)
	{
		ImageOps::Over(card, ImageOps::Blank(kIconWidth, kIconHeight, kWashYellow), 0, 0);
		Frame(card, kCardYellow, 2);
		return card;
	}

	Frame(card, kCardWhite, 1);
	return card;
}

struct IconSheet
{
	int atlasId;
	DdsImage::Image atlas;
};

std::map<int, int> TemplateParts(const PatEdit::Layout& layout, const int (&newParts)[kKinds])
{
	std::map<int, int> map;
	char suffix[16] = {};
	sprintf_s(suffix, "%03d", kTemplateNumber);

	for (const PatEdit::Part& part : layout.parts)
	{
		const int kind = KindOf(part.name);

		if (kind < 0 || part.name.compare(strlen(kKindPrefixes[kind]), std::string::npos, suffix) != 0)
			continue;

		map[part.id] = newParts[kind];
	}

	return map;
}

std::string PatternName(int number, int frame, const char* variant)
{
	char name[48] = {};
	sprintf_s(name, "icon_chr%03d_%03d%s", number, frame, variant);
	return name;
}

bool ClonePatterns(const std::vector<uint8_t>& pat, const PatEdit::Layout& layout, int number,
	const std::map<int, int>& partMap, int& nextIndex, std::vector<uint8_t>& out)
{
	for (const char* variant : kVariants)
	{
		for (int frame = 0; frame < kFrames; ++frame)
		{
			const PatEdit::Pattern* const source =
				PatEdit::FindPattern(layout, PatternName(kTemplateNumber, frame, variant));

			if (source == nullptr)
				return false;

			const std::vector<uint8_t> clone = PatEdit::ClonePattern(pat, *source, nextIndex++,
				PatternName(number, frame, variant), partMap);
			out.insert(out.end(), clone.begin(), clone.end());
		}
	}

	return true;
}

struct Templates
{
	DdsImage::Image cards[kKinds];
};

bool ReadTemplates(const std::vector<uint8_t>& pat, const PatEdit::Layout& layout, Templates& out)
{
	char suffix[16] = {};
	sprintf_s(suffix, "%03d", kTemplateNumber);

	for (int kind = 0; kind < kKinds; ++kind)
	{
		const PatEdit::Part* const card = PatEdit::FindPartNamed(layout, std::string(kKindPrefixes[kind]) + suffix);

		if (card == nullptr || !PatEdit::CutPart(pat, layout, *card, out.cards[kind]))
			return false;
	}

	return true;
}

DdsImage::Image CardFor(const AnnouncerArt::Face& face, int kind, const Templates& templates)
{
	if (face.portrait.width > 0)
		return Card(Strip(face.portrait), kind);

	DdsImage::Image card = ImageOps::Resized(templates.cards[kind], kIconWidth, kIconHeight);
	ImageOps::Multiply(card, kSystemTint);
	return card;
}

void FadeInFrom(DdsImage::Image& image, double start, double length)
{
	if (start <= 0.0 || length <= 0.0)
		return;

	const int first = static_cast<int>(start);
	const int last = std::min(image.width, static_cast<int>(std::ceil(start + length)));

	for (int x = first; x < last; ++x)
	{
		const double share = std::clamp((x + 0.5 - start) / length, 0.0, 1.0);

		for (int y = 0; y < image.height; ++y)
		{
			uint8_t* const alpha = &image.pixels[(static_cast<size_t>(y) * image.width + x) * 4 + 3];
			*alpha = static_cast<uint8_t>(std::lround(*alpha * share));
		}
	}
}

DdsImage::Image MenuFigure(const DdsImage::Image& portrait, int width, int height)
{
	if (portrait.width <= 0 || portrait.height <= 0)
		return ImageOps::Blank(width, height);

	const double scale = std::max(static_cast<double>(width) / portrait.width,
		static_cast<double>(height) / portrait.height);
	const double head = ImageOps::OpaqueCentreX(portrait, 0, static_cast<int>(portrait.height * kHeadShare),
		kOpaqueEnough);
	const double left = kMenuHeadX * width - head * scale;

	DdsImage::Image figure = ImageOps::Mapped(portrait, width, height, ImageOps::Affine{ scale, scale, left, 0.0 });
	FadeInFrom(figure, left, kMenuFadeShare * width);
	return figure;
}

std::vector<uint8_t> IconPart(int id, int kind, const AnnouncerArt::Face& face, int cell, int atlasId)
{
	PatEdit::Part part;
	char name[32] = {};
	sprintf_s(name, "%s%03d", kKindPrefixes[kind], face.number);

	const int x = (cell % kAtlasColumns) * kIconWidth;
	const int y = (cell / kAtlasColumns) * kIconHeight;
	const int unit = kAtlasSide / kUvUnits;

	part.id = id;
	part.name = name;
	part.uv[0] = x / unit;
	part.uv[1] = y / unit;
	part.uv[2] = kIconWidth / unit;
	part.uv[3] = kIconHeight / unit;
	part.size[0] = kIconWidth;
	part.size[1] = kIconHeight;
	part.pivot[0] = kIconWidth / 2;
	part.pivot[1] = kIconHeight / 2;
	part.atlas = atlasId;

	return PatEdit::PartBlock(part, kAtlasSide, kAtlasSide);
}

}

bool AnnouncerArt::Portrait(const std::vector<uint8_t>& selectPat, DdsImage::Image& out)
{
	PatEdit::Layout layout;

	if (!PatEdit::Survey(selectPat, layout))
		return false;

	const PatEdit::Part* const face = PatEdit::FindPartNamed(layout, kSelectFace);

	return face != nullptr && PatEdit::CutPart(selectPat, layout, *face, out);
}

bool AnnouncerArt::Icons(const std::vector<uint8_t>& iconPat, const std::vector<Face>& faces,
	std::vector<uint8_t>& out)
{
	PatEdit::Layout layout;

	if (faces.empty() || faces.size() > static_cast<size_t>(kAtlasColumns * kAtlasRows) ||
		!PatEdit::Survey(iconPat, layout))
	{
		return false;
	}

	IconSheet sheets[kKinds];
	const int firstAtlas = PatEdit::NextAtlasId(layout);

	for (int kind = 0; kind < kKinds; ++kind)
		sheets[kind] = IconSheet{ firstAtlas + kind, ImageOps::Blank(kAtlasSide, kAtlasSide, kTransparent) };

	Templates templates;

	if (!ReadTemplates(iconPat, layout, templates))
		return false;

	PatEdit::Additions additions;
	int nextPart = PatEdit::NextPartId(layout);
	int nextIndex = PatEdit::NextPatternIndex(layout);

	for (size_t cell = 0; cell < faces.size(); ++cell)
	{
		const Face& face = faces[cell];
		int parts[kKinds] = {};

		for (int kind = 0; kind < kKinds; ++kind)
		{
			parts[kind] = nextPart++;
			ImageOps::Copy(sheets[kind].atlas, CardFor(face, kind, templates),
				(static_cast<int>(cell) % kAtlasColumns) * kIconWidth,
				(static_cast<int>(cell) / kAtlasColumns) * kIconHeight);

			const std::vector<uint8_t> block =
				IconPart(parts[kind], kind, face, static_cast<int>(cell), sheets[kind].atlasId);
			additions.parts.insert(additions.parts.end(), block.begin(), block.end());
		}

		const std::map<int, int> partMap = TemplateParts(layout, parts);

		if (!ClonePatterns(iconPat, layout, face.number, partMap, nextIndex, additions.patterns))
		{
			return false;
		}
	}

	for (const IconSheet& sheet : sheets)
	{
		char name[32] = {};
		sprintf_s(name, "customize_iconmb%02d.dds", sheet.atlasId);

		const std::vector<uint8_t> block = PatEdit::AtlasBlock(sheet.atlasId, name, sheet.atlas);
		additions.atlases.insert(additions.atlases.end(), block.begin(), block.end());
	}

	out = PatEdit::Rebuild(iconPat, layout, additions);
	return true;
}

bool AnnouncerArt::Menu(const std::vector<uint8_t>& characterPat, const std::vector<uint8_t>& systemPat,
	const DdsImage::Image& portrait, std::vector<uint8_t>& out)
{
	PatEdit::Layout character;
	PatEdit::Layout system;

	if (!PatEdit::Survey(characterPat, character) || !PatEdit::Survey(systemPat, system))
		return false;

	const PatEdit::Part* const art = PatEdit::FindPartNamed(character, kMenuArt);
	const PatEdit::Part* const shadow = PatEdit::FindPartNamed(character, kMenuShadow);
	const PatEdit::Part* const systemArt = PatEdit::FindPartNamed(system, kMenuArt);

	if (art == nullptr || shadow == nullptr || systemArt == nullptr || shadow->atlas != art->atlas)
		return false;

	const PatEdit::Atlas* const target = PatEdit::FindAtlas(character, art->atlas);
	const PatEdit::Atlas* const source = PatEdit::FindAtlas(system, systemArt->atlas);
	DdsImage::Image canvas;

	if (target == nullptr || source == nullptr || !PatEdit::DecodeAtlas(systemPat, *source, canvas))
		return false;

	const ImageOps::Rect artRect = PatEdit::PartRect(*art, canvas.width, canvas.height);
	const ImageOps::Rect shadowRect = PatEdit::PartRect(*shadow, canvas.width, canvas.height);

	ImageOps::Fill(canvas, artRect, kTransparent);
	ImageOps::Fill(canvas, shadowRect, kTransparent);

	ImageOps::Over(canvas, MenuFigure(portrait, artRect.width, artRect.height), artRect.x, artRect.y);
	ImageOps::Over(canvas, MenuFigure(portrait, shadowRect.width, shadowRect.height), shadowRect.x, shadowRect.y);

	PatEdit::Additions additions;
	additions.replacedAtlases[target->id] = PatEdit::AtlasBlock(target->id, target->name, canvas);

	out = PatEdit::Rebuild(characterPat, character, additions);
	return true;
}
