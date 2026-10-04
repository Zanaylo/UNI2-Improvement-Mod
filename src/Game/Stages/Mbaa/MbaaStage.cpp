#include "Game/Stages/Mbaa/MbaaStage.h"

#include "Core/Formats/DdsImage.h"
#include "Core/Formats/ImageOps.h"
#include "Core/logger.h"
#include "Game/Stages/Bbtag/BbtagStage.h"
#include "Game/Stages/FbxExWriter.h"
#include "Game/Stages/Mbaa/MbaaAtlas.h"
#include "Game/Stages/Mbaa/MbaaBg.h"
#include "Game/Stages/Mbaa/MbaaCg.h"
#include "Game/Stages/Mbaa/MbaaGeometry.h"
#include "Game/Stages/Mbaa/MbaaTimeline.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <map>
#include <utility>

namespace {

constexpr int kBlockAlign = 4;
constexpr int kWideSheet = 960;
constexpr int kCardWidth = 180;
constexpr int kCardHeight = 480;
constexpr int kCardTop = -300;
constexpr uint32_t kCardBackdrop = 0xff000000u;
constexpr const char* kTextureName = "mbaa%03d.dds";
constexpr const char* kLaneTextureName = "mbaa_lane%02d.dds";
constexpr int kMostLanes = 96;
constexpr int kMostSheet = 4096;
constexpr int kLampArea = 256 * 256;
constexpr int kLevels = 15;
constexpr float kOpaque = 255.0f;

constexpr float kMaterial[FbxExWriter::kMaterialValues] = {
	1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 20.0f,
};

const char* const kBlock =
	"\tViewGrid = 0,\r\n"
	"\tIsFog = 0,\r\n"
	"\tFogStart = 0.0,\r\n"
	"\tFogEnd = 100.0,\r\n"
	"\tFogColor = [ 0.0, 0.0, 0.0, 0.0 ],\r\n"
	"\tMSAA = 4,\r\n"
	"\tStageW = 4096,\r\n"
	"\tIsBloom = 0,\r\n"
	"\tShadowScale = 0.6,\r\n"
	"\tShadowAlpha = 0.7,\r\n"
	"\tBGBloomEnable = 0,\r\n"
	"\tBGBloomBlightness = 0.8,\r\n"
	"\tBGBloomPower = 2.0,\r\n"
	"\tBGBloomBiassR = 1.0,\r\n"
	"\tBGBloomBiassG = 1.0,\r\n"
	"\tBGBloomBiassB = 1.0,\r\n"
	"\tBGBloomBlurRadius = 1.0,\r\n"
	"\tBGBloomTextureSize = 256,\r\n"
	"\tBGBloomAlpha = 0.0,\r\n"
	"\tBGTinyFXAAEnable = 0,\r\n"
	"\tBGTinyFXAAThreshold = 0.2,\r\n"
	"\tBGTinyFXAALerpT = 0.5,\r\n";

using MbaaTimeline::Sample;
using MbaaTimeline::Unit;

struct Piece
{
	int priority;
	int object;
	int order;
	FbxExWriter::Node node;
	std::vector<float> anime;
};

struct StaticSheet
{
	int texture;
	int left;
	int top;
	int width;
	int height;
	int paddedWidth;
	int paddedHeight;
};

struct Canvas
{
	int left = 0;
	int top = 0;
	int width = 0;
	int height = 0;
};

typedef std::pair<int, int> CellKey;
typedef std::map<int, MbaaCg::Picture> Pictures;

int Padded(int size)
{
	int side = kBlockAlign;

	while (side < size)
		side *= 2;

	return side;
}

bool SameLook(const Sample& one, const Sample& other)
{
	return one.image == other.image && one.x == other.x && one.y == other.y && one.alpha == other.alpha;
}

bool IsStatic(const Unit& unit, int period)
{
	if (static_cast<int>(unit.samples.size()) != period)
		return false;

	return std::all_of(unit.samples.begin(), unit.samples.end(),
		[&unit](const Sample& sample) { return SameLook(sample, unit.samples.front()); });
}

bool IsAdditive(int blend)
{
	return blend == MbaaBg::Blend_Add || blend == MbaaBg::Blend_AddStrong;
}

int Level(int alpha)
{
	return (alpha * kLevels + static_cast<int>(kOpaque) / 2) / static_cast<int>(kOpaque);
}

MbaaGeometry::Box Shifted(const MbaaGeometry::Box& box, float by)
{
	MbaaGeometry::Box out = box;
	out.left += by;
	out.right += by;
	return out;
}

void PushWide(FbxExWriter::Node& node, const MbaaGeometry::Box& box, float alpha, const MbaaGeometry::Corners& corners,
	bool wide)
{
	MbaaGeometry::PushQuad(node, box, alpha, corners);

	if (!wide)
		return;

	const float width = box.right - box.left;
	MbaaGeometry::PushQuad(node, Shifted(box, -width), alpha, MbaaGeometry::Mirrored(corners));
	MbaaGeometry::PushQuad(node, Shifted(box, width), alpha, MbaaGeometry::Mirrored(corners));
}

FbxExWriter::Node Leaf(int blend, int material)
{
	FbxExWriter::Node node = FbxExWriter::Leaf();
	node.blendmode = IsAdditive(blend) ? 1 : 0;
	node.submeshes.push_back(FbxExWriter::Submesh{ material, {} });

	return node;
}

void PaintCard(DdsImage::Image& card, const MbaaCg::Picture& picture, float x, float y, int blend, int alpha)
{
	const int left = static_cast<int>(lroundf(x)) + picture.left - MbaaGeometry::kOriginX + kCardWidth / 2;
	const int top = static_cast<int>(lroundf(y)) + picture.top - kCardTop;
	const float opacity = alpha / kOpaque;

	if (IsAdditive(blend))
	{
		ImageOps::Add(card, picture.image, left, top, opacity);
		return;
	}

	DdsImage::Image faded = picture.image;

	if (opacity < 1.0f)
		ImageOps::Faded(faded, opacity);

	ImageOps::Over(card, faded, left, top);
}

std::vector<float> Filled(const std::vector<std::vector<float>>& matrices)
{
	const size_t count = matrices.size();
	size_t first = 0;

	while (first < count && matrices[first].empty())
		++first;

	std::vector<float> out;

	if (first == count)
		return out;

	const std::vector<float>* held = &matrices[first];

	for (size_t i = 0; i < count; ++i)
	{
		if (!matrices[i].empty())
			held = &matrices[i];

		out.insert(out.end(), held->begin(), held->end());
	}

	return out;
}

class Conversion
{
public:
	Conversion(const MbaaBg::File& file, const MbaaCg& cg, MbaaStage::Result& out)
		: m_file(file)
		, m_cg(cg)
		, m_out(out)
	{
	}

	bool Run()
	{
		if (!MbaaTimeline::Build(m_file, m_timeline))
			return false;

		std::map<std::pair<int, int>, std::vector<int>> groups;

		for (size_t i = 0; i < m_timeline.units.size(); ++i)
		{
			const Unit& unit = m_timeline.units[i];

			if (IsStatic(unit, m_timeline.period))
				AddStatic(unit, static_cast<int>(i));
			else
				groups[{ unit.object, unit.blend }].push_back(static_cast<int>(i));
		}

		AddGroups(groups);

		if (m_pieces.empty())
			return false;

		Card();
		Assemble();

		m_out.engineClock = !m_out.flips.empty() || !m_out.lamps.empty();
		return true;
	}

private:
	const MbaaBg::Layer& LayerOf(int object) const
	{
		static const MbaaBg::Layer none = { 0, MbaaBg::kFullParallax, 0, true, {}, {}, {} };
		const MbaaBg::Layer* const layer = MbaaBg::LayerOf(m_file, object);

		return layer != nullptr ? *layer : none;
	}

	int MaterialOf(int texture)
	{
		const std::map<int, int>::const_iterator known = m_materials.find(texture);

		if (known != m_materials.end())
			return known->second;

		FbxExWriter::Material material = {};
		material.filename = m_out.textures[static_cast<size_t>(texture)].name;
		material.textureIndex = texture;
		std::copy(kMaterial, kMaterial + FbxExWriter::kMaterialValues, material.value);

		const int index = static_cast<int>(m_model.materials.size());
		m_model.materials.push_back(material);
		m_materials[texture] = index;

		return index;
	}

	int AddTexture(const std::string& name, const DdsImage::Image& image)
	{
		m_out.textures.push_back(MbaaStage::Texture{ name, DdsImage::EncodeDxt(image) });

		return static_cast<int>(m_out.textures.size()) - 1;
	}

	bool StaticSheetOf(int image, StaticSheet& out)
	{
		const std::map<int, StaticSheet>::const_iterator known = m_statics.find(image);

		if (known != m_statics.end())
		{
			out = known->second;
			return true;
		}

		MbaaCg::Picture picture;

		if (!m_cg.Draw(image - MbaaBg::kSpriteBase, picture))
			return false;

		out = StaticSheet{ 0, picture.left, picture.top, picture.image.width, picture.image.height,
			Padded(picture.image.width), Padded(picture.image.height) };

		DdsImage::Image padded = ImageOps::Blank(out.paddedWidth, out.paddedHeight);
		ImageOps::Copy(padded, picture.image, 0, 0);
		ImageOps::Bleed(padded);

		char name[32] = {};
		sprintf_s(name, kTextureName, image - MbaaBg::kSpriteBase);
		out.texture = AddTexture(name, padded);
		m_statics[image] = out;

		return true;
	}

	void AddStatic(const Unit& unit, int order)
	{
		const Sample& look = unit.samples.front();
		StaticSheet sheet = {};

		if (!StaticSheetOf(look.image, sheet))
			return;

		const MbaaBg::Layer& layer = LayerOf(unit.object);
		const MbaaGeometry::Box box = MbaaGeometry::Place(layer.parallax, look.x + sheet.left, look.y + sheet.top,
			static_cast<float>(sheet.width), static_cast<float>(sheet.height));
		const float alpha = look.alpha / kOpaque;

		Piece piece = { layer.priority, unit.object, order, Leaf(unit.blend, MaterialOf(sheet.texture)), {} };
		PushWide(piece.node, box, alpha, MbaaGeometry::Sheet(static_cast<float>(sheet.width) / sheet.paddedWidth,
			static_cast<float>(sheet.height) / sheet.paddedHeight), sheet.width >= kWideSheet);

		m_out.fading = m_out.fading || alpha < 1.0f;
		m_pieces.push_back(piece);
	}

	std::vector<Unit> UnitsOf(const std::vector<int>& indices) const
	{
		std::vector<Unit> out;

		for (int index : indices)
			out.push_back(m_timeline.units[static_cast<size_t>(index)]);

		return out;
	}

	void AddGroups(const std::map<std::pair<int, int>, std::vector<int>>& groups)
	{
		std::map<std::pair<int, int>, int> needed;
		int total = 0;

		for (const auto& group : groups)
		{
			const int lanes = static_cast<int>(MbaaTimeline::Lanes(UnitsOf(group.second), m_timeline.period,
				static_cast<int>(group.second.size())).size());
			needed[group.first] = lanes;
			total += lanes;
		}

		for (const auto& group : groups)
		{
			const int wanted = needed[group.first];
			const int cap = total <= kMostLanes ? wanted : (std::max)(1, wanted * kMostLanes / total);

			AddGroup(group.first.first, group.first.second, UnitsOf(group.second), cap);
		}
	}

	bool Fixed(const std::vector<Unit>& units) const
	{
		std::map<int, std::pair<float, float>> placed;

		for (const Unit& unit : units)
		{
			for (const Sample& sample : unit.samples)
			{
				const auto known = placed.emplace(sample.image, std::make_pair(sample.x, sample.y));

				if (!known.second && known.first->second != std::make_pair(sample.x, sample.y))
					return false;
			}
		}

		return true;
	}

	Canvas CanvasOf(const std::vector<Unit>& units, const Pictures& pictures) const
	{
		int left = INT_MAX;
		int top = INT_MAX;
		int right = INT_MIN;
		int bottom = INT_MIN;

		for (const Unit& unit : units)
		{
			for (const Sample& sample : unit.samples)
			{
				const MbaaCg::Picture& picture = pictures.at(sample.image);
				const int x = static_cast<int>(lroundf(sample.x)) + picture.left;
				const int y = static_cast<int>(lroundf(sample.y)) + picture.top;

				left = (std::min)(left, x);
				top = (std::min)(top, y);
				right = (std::max)(right, x + picture.image.width);
				bottom = (std::max)(bottom, y + picture.image.height);
			}
		}

		return Canvas{ left, top, right - left, bottom - top };
	}

	bool Draw(const std::vector<Unit>& units, Pictures& out) const
	{
		for (const Unit& unit : units)
		{
			for (const Sample& sample : unit.samples)
			{
				if (out.count(sample.image) != 0)
					continue;

				MbaaCg::Picture picture;

				if (!m_cg.Draw(sample.image - MbaaBg::kSpriteBase, picture))
					return false;

				out[sample.image] = picture;
			}
		}

		return !out.empty();
	}

	std::vector<int> AlphaOf(const std::vector<Unit>& lane) const
	{
		std::vector<int> alpha(static_cast<size_t>(m_timeline.period), -1);

		for (const Unit& unit : lane)
		{
			for (const Sample& sample : unit.samples)
				alpha[static_cast<size_t>(sample.tick)] = sample.alpha;
		}

		return alpha;
	}

	bool Lamps(const std::vector<std::vector<Unit>>& lanes, std::vector<BbtagScript::Lamp>& out) const
	{
		out.clear();

		if (m_out.lamps.size() + lanes.size() > static_cast<size_t>(BbtagStage::kLampSlots))
			return false;

		for (const std::vector<Unit>& lane : lanes)
		{
			BbtagScript::Lamp lamp = {};
			lamp.loop = m_timeline.period;

			if (!MbaaTimeline::Ramps(AlphaOf(lane), lamp.ramp))
				return false;

			out.push_back(lamp);
		}

		return true;
	}

	DdsImage::Image CellImage(const MbaaCg::Picture& picture, const Sample& look, int level, bool fixed,
		const Canvas& canvas) const
	{
		DdsImage::Image cell = picture.image;

		if (fixed)
		{
			cell = ImageOps::Blank(canvas.width, canvas.height);
			ImageOps::Copy(cell, picture.image, static_cast<int>(lroundf(look.x)) + picture.left - canvas.left,
				static_cast<int>(lroundf(look.y)) + picture.top - canvas.top);
		}

		if (level < kLevels)
			ImageOps::Faded(cell, static_cast<float>(level) / kLevels);

		return cell;
	}

	int Slot(const BbtagScript::Flip& flip)
	{
		for (size_t i = 0; i < m_out.flips.size(); ++i)
		{
			if (m_out.flips[i].rects == flip.rects && m_out.flips[i].frame == flip.frame)
				return static_cast<int>(i);
		}

		if (m_out.flips.size() >= static_cast<size_t>(BbtagStage::kFlipSlots))
			return -1;

		m_out.flips.push_back(flip);
		return static_cast<int>(m_out.flips.size()) - 1;
	}

	void AddGroup(int object, int blend, const std::vector<Unit>& units, int cap)
	{
		const std::vector<std::vector<int>> laneIndices = MbaaTimeline::Lanes(units, m_timeline.period, cap);
		std::vector<std::vector<Unit>> lanes;
		std::vector<Unit> kept;

		for (const std::vector<int>& indices : laneIndices)
		{
			lanes.emplace_back();

			for (int index : indices)
			{
				lanes.back().push_back(units[static_cast<size_t>(index)]);
				kept.push_back(units[static_cast<size_t>(index)]);
			}
		}

		Pictures pictures;

		if (lanes.empty() || !Draw(kept, pictures))
			return;

		const MbaaBg::Layer& layer = LayerOf(object);
		const bool fixed = Fixed(kept);
		const Canvas canvas = fixed ? CanvasOf(kept, pictures) : Canvas();
		const int firstAlpha = kept.front().samples.front().alpha;
		const bool constant = std::all_of(kept.begin(), kept.end(), [firstAlpha](const Unit& unit) {
			return std::all_of(unit.samples.begin(), unit.samples.end(),
				[firstAlpha](const Sample& sample) { return sample.alpha == firstAlpha; });
		});

		std::vector<BbtagScript::Lamp> lamps;
		const bool lit = fixed && !constant && canvas.width * canvas.height >= kLampArea && Lamps(lanes, lamps);
		const bool baked = !constant && !lit;

		std::map<CellKey, int> cells;
		std::vector<std::pair<CellKey, Sample>> looks;

		for (const Unit& unit : kept)
		{
			for (const Sample& sample : unit.samples)
			{
				const CellKey key(sample.image, baked ? Level(sample.alpha) : kLevels);

				if (key.second == 0 || cells.count(key) != 0)
					continue;

				cells[key] = static_cast<int>(looks.size());
				looks.emplace_back(key, sample);
			}
		}

		std::vector<MbaaAtlas::Size> sizes;

		for (const std::pair<CellKey, Sample>& look : looks)
		{
			const MbaaCg::Picture& picture = pictures.at(look.first.first);
			sizes.push_back(fixed ? MbaaAtlas::Size{ canvas.width, canvas.height }
				: MbaaAtlas::Size{ picture.image.width, picture.image.height });
		}

		MbaaAtlas::Layout layout;

		if (!MbaaAtlas::Pack(sizes, kMostSheet, layout))
		{
			LOG("MbaaStage: object %d has more animation frames than one sheet holds", object);
			return;
		}

		DdsImage::Image sheet = ImageOps::Blank(layout.width, layout.height);
		std::vector<float> rects;

		for (size_t i = 0; i < looks.size(); ++i)
		{
			const ImageOps::Rect& rect = layout.rects[i];
			DdsImage::Image cell = CellImage(pictures.at(looks[i].first.first), looks[i].second, looks[i].first.second,
				fixed, canvas);

			if (cell.width != rect.width || cell.height != rect.height)
				cell = ImageOps::Resized(cell, rect.width, rect.height);

			ImageOps::Bleed(cell);

			ImageOps::Copy(sheet, cell, rect.x, rect.y);

			rects.push_back(static_cast<float>(rect.x) / layout.width);
			rects.push_back(static_cast<float>(rect.width) / layout.width);
			rects.push_back(static_cast<float>(rect.y) / layout.height);
			rects.push_back(static_cast<float>(rect.height) / layout.height);
		}

		char name[32] = {};
		sprintf_s(name, kLaneTextureName, m_lanesheets++);
		const int material = MaterialOf(AddTexture(name, sheet));
		const float vertexAlpha = constant ? firstAlpha / kOpaque : 1.0f;

		for (size_t l = 0; l < lanes.size(); ++l)
		{
			BbtagScript::Flip flip;
			flip.rects = rects;
			flip.frame.assign(static_cast<size_t>(m_timeline.period), -1);
			std::vector<std::vector<float>> matrices(static_cast<size_t>(m_timeline.period));

			for (const Unit& unit : lanes[l])
			{
				for (const Sample& sample : unit.samples)
				{
					const CellKey key(sample.image, baked ? Level(sample.alpha) : kLevels);
					const std::map<CellKey, int>::const_iterator cell = cells.find(key);

					if (cell == cells.end())
						continue;

					flip.frame[static_cast<size_t>(sample.tick)] = cell->second;

					if (fixed)
						continue;

					const MbaaCg::Picture& picture = pictures.at(sample.image);
					matrices[static_cast<size_t>(sample.tick)] = MbaaGeometry::Matrix(MbaaGeometry::Place(
						layer.parallax, sample.x + picture.left, sample.y + picture.top,
						static_cast<float>(picture.image.width), static_cast<float>(picture.image.height)));
				}
			}

			const int slot = Slot(flip);

			if (slot < 0)
			{
				LOG("MbaaStage: object %d lost a lane, the stage has no flipbook slot left", object);
				continue;
			}

			int lamp = -1;

			if (lit)
			{
				lamp = static_cast<int>(m_out.lamps.size());
				m_out.lamps.push_back(lamps[l]);
			}

			Piece piece = { layer.priority, object, m_order++, Leaf(blend, material), {} };
			const MbaaGeometry::Corners corners = MbaaGeometry::Flip(slot, lamp);

			if (fixed)
			{
				const MbaaGeometry::Box box = MbaaGeometry::Place(layer.parallax, static_cast<float>(canvas.left),
					static_cast<float>(canvas.top), static_cast<float>(canvas.width), static_cast<float>(canvas.height));
				PushWide(piece.node, box, vertexAlpha, corners, canvas.width >= kWideSheet);
			}
			else
			{
				MbaaGeometry::PushQuad(piece.node, MbaaGeometry::Unit(), vertexAlpha, corners);
				piece.anime = Filled(matrices);
			}

			m_out.fading = m_out.fading || vertexAlpha < 1.0f;
			m_pieces.push_back(piece);
		}
	}

	void Card()
	{
		DdsImage::Image card = ImageOps::Blank(kCardWidth, kCardHeight, kCardBackdrop);
		std::vector<std::pair<std::pair<int, int>, std::pair<const Unit*, const Sample*>>> shown;

		for (const Unit& unit : m_timeline.units)
		{
			if (unit.samples.empty() || unit.samples.front().tick != 0)
				continue;

			shown.push_back({ { LayerOf(unit.object).priority, unit.object }, { &unit, &unit.samples.front() } });
		}

		std::stable_sort(shown.begin(), shown.end(),
			[](const auto& one, const auto& other) { return one.first < other.first; });

		for (const auto& entry : shown)
		{
			MbaaCg::Picture picture;
			const Sample& sample = *entry.second.second;

			if (m_cg.Draw(sample.image - MbaaBg::kSpriteBase, picture))
				PaintCard(card, picture, sample.x, sample.y, entry.second.first->blend, sample.alpha);
		}

		m_out.card = DdsImage::EncodeArgb(card);
	}

	void Assemble()
	{
		std::stable_sort(m_pieces.begin(), m_pieces.end(), [](const Piece& one, const Piece& other) {
			if (one.priority != other.priority)
				return one.priority < other.priority;

			if (one.object != other.object)
				return one.object < other.object;

			return one.order < other.order;
		});

		for (const MbaaStage::Texture& texture : m_out.textures)
			m_model.textures.push_back(texture.name);

		FbxExWriter::Node root = FbxExWriter::Branch();
		root.child = 1;
		m_model.nodes.push_back(root);
		m_model.animes.push_back(MbaaGeometry::Rest());

		const int count = static_cast<int>(m_pieces.size());

		for (int i = 0; i < count; ++i)
		{
			Piece& piece = m_pieces[static_cast<size_t>(i)];
			piece.node.sibling = i + 1 < count ? i + 2 : -1;

			m_model.nodes.push_back(piece.node);
			m_model.animes.push_back(piece.anime.empty() ? MbaaGeometry::Rest() : piece.anime);
		}

		FbxExWriter::Build(m_model, m_out.model);
	}

	const MbaaBg::File& m_file;
	const MbaaCg& m_cg;
	MbaaStage::Result& m_out;
	MbaaTimeline::Timeline m_timeline;
	FbxExWriter::Model m_model;
	std::vector<Piece> m_pieces;
	std::map<int, StaticSheet> m_statics;
	std::map<int, int> m_materials;
	int m_lanesheets = 0;
	int m_order = 1 << 20;
};

}

bool MbaaStage::Convert(const std::vector<uint8_t>& dat, Result& out)
{
	out = Result();

	MbaaBg::File file;
	MbaaCg cg;

	if (!MbaaBg::Read(dat, file) || !cg.Open(dat.data() + file.cgAt, file.cgBytes))
		return false;

	Conversion conversion(file, cg, out);

	return conversion.Run();
}

std::string MbaaStage::Block()
{
	char camera[256] = {};
	sprintf_s(camera, "\tScale = [ 1.0, 1.0, 1.0 ],\r\n"
		"\tPosition = [ 0.0, 0.0, 0.0 ],\r\n"
		"\tFOV = %.1f,\r\n"
		"\tViewRotationX = 0.0,\r\n"
		"\tVanishingPoint = 0.0,\r\n", MbaaGeometry::kFov);

	return std::string(camera) + kBlock;
}
