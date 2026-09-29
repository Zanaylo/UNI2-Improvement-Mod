#include "Game/Stages/Bbtag/BbtagStage.h"

#include "Game/Stages/Bbtag/BbtagArt.h"
#include "Game/Stages/Bbtag/BbtagCamera.h"
#include "Game/Stages/Bbtag/BbtagDefaults.h"
#include "Game/Stages/Bbtag/BbtagMmot.h"
#include "Game/Stages/Bbtag/BbtagMua.h"
#include "Game/Stages/Bbtag/BbtagPac.h"
#include "Game/Stages/Bbtag/BbtagParticle.h"
#include "Game/Stages/Bbtag/BbtagParticleBake.h"
#include "Game/Stages/Bbtag/BbtagScript.h"
#include "Game/Stages/FbxExWriter.h"
#include "Game/Stages/StageCapture.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <set>

namespace {

constexpr double kCharacter = 213.0;
constexpr double kUni2Character = 0.132;

constexpr float kFlowMark = 128.0f;
constexpr int kFlowDown = 0;
constexpr int kFlowAcross = 1;
constexpr int kFlowBoth = 2;
constexpr float kLampMark = 128.0f;
constexpr double kFullRamp = 1000.0;
constexpr int kBlendOpaque = 0;
constexpr int kBlendAdd = 2;
constexpr int kBlendSubtract = 4;
constexpr int kBlendUnset = 0x7fffffff;

constexpr float kWindowCover = 0.9f;
constexpr float kWindowBand = 0.6f;
constexpr float kWindowMatch = 0.25f;
constexpr int kWindowBands = 8;
constexpr int kWindowHold = 180;
constexpr const char* kModelTake = "(the model's own)";
constexpr const char* kObjectList = "object.txt";

constexpr double kUni2EyeHeight = 280.0 / 360.0;
constexpr float kParked = -1000.0f;
constexpr size_t kLampRects = 4;
constexpr uint32_t kCullNone = 0x400;
constexpr uint32_t kNoDefaultTake = 0x10;
constexpr uint32_t kPivotMoves = 0x200;
constexpr uint32_t kShining = 0x100000;
constexpr double kSkyDriftY = 0.0005;
constexpr double kNearest = 1.0;
constexpr int kShineSplits = 3;
constexpr const char* kSparkSuffix = "_particles.dds";
constexpr const char* kKickSuffix = "_kicked.dds";
constexpr const char* kKickEffect = "efbg_sakura";
constexpr int kKickKind = 6;
constexpr int kKickBursts = 96;
constexpr uint32_t kDdsRawFlags = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000;
constexpr uint32_t kDdsRgbAlpha = 0x41;
constexpr uint32_t kDdsRawBits = 32;
constexpr uint32_t kDdsMasks[4] = { 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 };
constexpr size_t kDdsMasksAt = 92;
constexpr uint32_t kCardSide = 64;
constexpr uint32_t kCardBytes = kCardSide * kCardSide / 2;

struct CardKind
{
	const char* sheet;
	const char* texture;
	const char* stamp;
};

const CardKind kCards[] = {
	{ "capture", "uni2im_capture.dds", StageCapture::kStamp },
};

constexpr const char* kBannerSheet = "stagefont";
constexpr int kBannerLines = 2;
const char* const kUnculled[] = { "bg_town", "bg_garden", "bg_monolis" };

constexpr size_t kDdsSize = 128;
constexpr uint32_t kDdsHeader = 124;
constexpr uint32_t kDdsFlags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000;
constexpr size_t kDdsPixelFormatAt = 76;
constexpr uint32_t kDdsPixelFormat = 32;
constexpr uint32_t kDdsFourCc = 0x4;
constexpr size_t kDdsFourCcAt = 84;
constexpr size_t kDdsCapsAt = 108;
constexpr uint32_t kDdsTexture = 0x1000;

constexpr double kScale = kUni2Character / kCharacter;
constexpr bool kMirror = true;
constexpr bool kFlip = true;

constexpr int kFloats = FbxExWriter::kVertexFloats;
constexpr size_t kMatrix = FbxExWriter::kMatrixFloats;
constexpr int kNormalX = 3;
constexpr int kNormalY = 4;
constexpr int kU = 10;
constexpr size_t kMergedVertices = 65535;
constexpr int kV = 11;

const char* const kBlock =
	"\tIsFog = 0,\r\n"
	"\tFogStart = 0.0,\r\n"
	"\tFogEnd = 100.0,\r\n"
	"\tFogColor = [ 0.0, 0.0, 0.0, 0.0 ],\r\n"
	"\tMSAA = 4,\r\n"
	"\tStageW = 4096,\r\n"
	"\tIsBloom = 0,\r\n"
	"\tShadowLightType = 0,\r\n"
	"\tShadowReflexColor = [ 0.0, 0.0, 0.0, 0.0 ],\r\n"
	"\tShadowScale = 0.6,\r\n"
	"\tShadowAlpha = 0.7,\r\n"
	"\tBGBloomEnable = 1,\r\n"
	"\tBGBloomBlightness = 0.8,\r\n"
	"\tBGBloomPower = 2.0,\r\n"
	"\tBGBloomBiassR = 1.0,\r\n"
	"\tBGBloomBiassG = 1.0,\r\n"
	"\tBGBloomBiassB = 1.0,\r\n"
	"\tBGBloomBlurRadius = 1.0,\r\n"
	"\tBGBloomTextureSize = 256,\r\n"
	"\tBGBloomAlpha = 0.5,\r\n"
	"\tBGTinyFXAAEnable = 0,\r\n"
	"\tBGTinyFXAAThreshold = 0.2,\r\n"
	"\tBGTinyFXAALerpT = 0.5,\r\n";

struct Plate
{
	float span;
	float tall;
	float low;
};

struct Rank
{
	float first;
	float second;
};

bool Before(const Rank& one, const Rank& other)
{
	return one.first < other.first || (one.first == other.first && one.second < other.second);
}

struct Piece
{
	bool clear = false;
	bool solid = true;
	Rank rank = {};
	int bone = -1;
	int root = -1;
	FbxExWriter::Node node;
	std::vector<float> fixed;
	const BbtagScript::Run* run = nullptr;
	const std::vector<int>* frames = nullptr;
	int rect = -1;
};

std::string ScriptOf(const BbtagMua::Model& model, const BbtagMua::Mesh& mesh)
{
	const std::vector<BbtagMua::Skeleton>& skeletons = model.Skeletons();

	if (mesh.skeleton < 0 || mesh.skeleton >= static_cast<int>(skeletons.size()))
		return std::string();

	const int index = skeletons[mesh.skeleton].script;
	const std::vector<std::string>& scripts = model.Scripts();

	if (index < 0 || index >= static_cast<int>(scripts.size()))
		return std::string();

	const std::string& named = scripts[index];
	const size_t dot = named.rfind('.');

	return dot == std::string::npos ? named : named.substr(0, dot);
}

typedef std::map<std::pair<std::string, int>, BbtagScript::Played> PlayedScripts;

std::string Stem(const BbtagScript::Scripts& scripts, const std::string& bound)
{
	return scripts.find(bound) != scripts.end() ? bound : std::string();
}

const BbtagScript::Played& PlayedFor(const BbtagScript::Scripts& scripts, PlayedScripts& played,
	const std::string& stem, int skeleton)
{
	const std::pair<std::string, int> key(stem, skeleton);
	const PlayedScripts::const_iterator known = played.find(key);

	if (known != played.end())
		return known->second;

	BbtagScript::Played& fresh = played[key];
	BbtagScript::Play(scripts.at(stem), stem + " " + std::to_string(skeleton), fresh);

	return fresh;
}

template <typename Held>
const Held* Named(const std::map<int, Held>& held, int mesh)
{
	const typename std::map<int, Held>::const_iterator found = held.find(mesh);

	return found == held.end() ? nullptr : &found->second;
}

typedef std::map<int, std::vector<Plate> > Siblings;

struct Track
{
	BbtagMmot::Motion motion;
	std::vector<std::vector<float> > frames;
};

typedef std::map<std::string, Track> Takes;

bool Unseen(const BbtagScript::Played& run)
{
	const size_t first = run.sample.size() > 1 ? 1 : 0;

	for (size_t i = first; i < run.sample.size(); ++i)
	{
		if (run.sample[i].ramp > 0.0)
			return false;
	}

	for (const BbtagScript::Sample& sample : run.sample)
	{
		if (sample.lit)
			return false;
	}

	return true;
}

bool Specks(const BbtagScript::Sprite& sprite)
{
	return !sprite.rect.empty() && std::all_of(sprite.rect.begin(), sprite.rect.end(), BbtagScript::Speck);
}

double ConstantRamp(const BbtagScript::Played& run)
{
	if (run.sample.empty())
		return 0.0;

	const double level = run.sample[0].ramp;

	if (level <= 0.0 || level >= kFullRamp)
		return 0.0;

	for (const BbtagScript::Sample& sample : run.sample)
	{
		if (sample.ramp != level)
			return 0.0;
	}

	return level / kFullRamp;
}

std::string Lowered(const std::string& text)
{
	std::string out = text;

	for (char& letter : out)
	{
		if (letter >= 'A' && letter <= 'Z')
			letter = static_cast<char>(letter - 'A' + 'a');
	}

	return out;
}

std::string Leaf(const std::string& path)
{
	const size_t slash = path.find_last_of("/\\");

	return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool EndsWith(const std::string& text, const char* tail)
{
	const size_t length = strlen(tail);

	return text.size() >= length && text.compare(text.size() - length, length, tail) == 0;
}

void Place(const float point[3], double scale, bool mirror, float out[3])
{
	out[0] = static_cast<float>(point[0] * scale);
	out[1] = static_cast<float>(point[1] * scale);
	out[2] = static_cast<float>(mirror ? -point[2] * scale : point[2] * scale);
}

void Normalise(float vector[3])
{
	const double length = sqrt(static_cast<double>(vector[0]) * vector[0]
		+ static_cast<double>(vector[1]) * vector[1]
		+ static_cast<double>(vector[2]) * vector[2]);

	if (length < 1e-9)
	{
		vector[0] = 0.0f;
		vector[1] = 1.0f;
		vector[2] = 0.0f;
		return;
	}

	for (int i = 0; i < 3; ++i)
		vector[i] = static_cast<float>(vector[i] / length);
}

void Sides(const float points[4][3], float& wide, float& tall)
{
	std::vector<float> apart;

	for (int i = 0; i < 4; ++i)
	{
		for (int j = i + 1; j < 4; ++j)
		{
			float total = 0.0f;

			for (int k = 0; k < 3; ++k)
			{
				const float step = points[i][k] - points[j][k];
				total += step * step;
			}

			apart.push_back(sqrtf(total));
		}
	}

	std::sort(apart.begin(), apart.end());

	wide = apart[2];
	tall = apart[0];
}

void Corners(const std::vector<float>& vertices, float out[4][3])
{
	for (int i = 0; i < 4; ++i)
	{
		for (int k = 0; k < 3; ++k)
			out[i][k] = vertices[i * kFloats + k];
	}
}

void Divides(const std::vector<Plate>& siblings, float tall, int& count, int& taken)
{
	count = 1;
	taken = -1;

	for (const Plate& plate : siblings)
	{
		if (plate.span <= 0.0f || plate.span > kWindowBand || fabsf(plate.tall - tall) > kWindowMatch * tall)
			continue;

		const float bands = 1.0f / plate.span;
		const int whole = static_cast<int>(bands + 0.5f);

		if (whole < 2 || whole > kWindowBands || fabsf(bands - whole) > kWindowMatch)
			continue;

		count = whole;
		taken = static_cast<int>(plate.low / plate.span + 0.5f);
		return;
	}
}

void Bands(const std::vector<float>& vertices, const std::vector<FbxExWriter::Submesh>& submeshes,
	const BbtagArt::Size& size, const std::vector<Plate>& siblings, int& count, int& taken)
{
	count = 1;
	taken = -1;

	if (size.width <= 0 || size.height <= 0 || vertices.size() != 4 * kFloats)
		return;

	size_t indices = 0;

	for (const FbxExWriter::Submesh& submesh : submeshes)
		indices += submesh.indices.size();

	if (indices != 6)
		return;

	float low = vertices[11];
	float high = vertices[11];
	float leftU = vertices[10];
	float rightU = vertices[10];

	for (int i = 1; i < 4; ++i)
	{
		low = std::min(low, vertices[i * kFloats + 11]);
		high = std::max(high, vertices[i * kFloats + 11]);
		leftU = std::min(leftU, vertices[i * kFloats + 10]);
		rightU = std::max(rightU, vertices[i * kFloats + 10]);
	}

	if (high - low < kWindowCover)
		return;

	float points[4][3] = {};
	Corners(vertices, points);

	float wide = 0.0f;
	float tall = 0.0f;
	Sides(points, wide, tall);

	const float across = (rightU - leftU) * size.width;
	const float down = (high - low) * size.height;

	if (tall <= 0.0f || down <= 0.0f || across <= 0.0f)
		return;

	int whole = 1;
	int already = -1;
	Divides(siblings, tall, whole, already);

	if (whole < 2)
		return;

	const float stretch = (wide / tall) / (across / down);

	if (fabsf(stretch - whole) > kWindowMatch * whole)
		return;

	count = whole;
	taken = already;
}

std::vector<float> Banded(const std::vector<float>& vertices, int index, int count)
{
	float low = vertices[11];
	float high = vertices[11];
	const size_t rows = vertices.size() / kFloats;

	for (size_t i = 1; i < rows; ++i)
	{
		low = std::min(low, vertices[i * kFloats + 11]);
		high = std::max(high, vertices[i * kFloats + 11]);
	}

	const float step = (high - low) / count;
	std::vector<float> out = vertices;

	for (size_t i = 0; i < rows; ++i)
		out[i * kFloats + 11] = low + index * step + (vertices[i * kFloats + 11] - low) / count;

	return out;
}

std::vector<float> Framed(const std::vector<float>& vertices, const BbtagScript::Rect& rect,
	const BbtagArt::Size& size, bool flip)
{
	const size_t rows = vertices.size() / kFloats;
	std::vector<float> out = vertices;

	for (size_t i = 0; i < rows; ++i)
	{
		const double u = vertices[i * kFloats + 10];
		const double v = flip ? 1.0 - vertices[i * kFloats + 11] : vertices[i * kFloats + 11];
		const double across = (u * rect.w + rect.x) / size.width;
		const double down = (v * rect.h + rect.y) / size.height;

		out[i * kFloats + 10] = static_cast<float>(across);
		out[i * kFloats + 11] = static_cast<float>(flip ? 1.0 - down : down);
	}

	return out;
}

std::vector<float> Shown(int index, int count, int phase)
{
	const int frames = count * kWindowHold;
	std::vector<float> out;
	out.reserve(static_cast<size_t>(frames) * 16);

	for (int frame = 0; frame < frames; ++frame)
	{
		const bool up = (frame / kWindowHold + phase) % count == index;

		for (int i = 0; i < 16; ++i)
			out.push_back((i % 5) == 0 ? 1.0f : 0.0f);

		if (!up)
			out[out.size() - 3] = -1000.0f;
	}

	return out;
}

typedef std::map<int, std::pair<int, int> > Flowing;

int SingleKind(const BbtagMua::Flow& flow)
{
	return fabsf(flow.across) > fabsf(flow.down) ? kFlowAcross : kFlowDown;
}

std::vector<float> FlowRates(const BbtagMua::Flow& flow, int kind, bool flip)
{
	std::vector<float> out;

	if (kind != kFlowDown)
		out.push_back(-flow.across);

	if (kind != kFlowAcross)
		out.push_back(flip ? flow.down : -flow.down);

	return out;
}

int SharedSlot(const std::vector<float>& rates, const std::vector<float>& wanted)
{
	for (size_t at = 0; at + wanted.size() <= rates.size(); ++at)
	{
		if (std::equal(wanted.begin(), wanted.end(), rates.begin() + at))
			return static_cast<int>(at);
	}

	return -1;
}

void Flows(const BbtagMua::Model& model, bool flip, std::vector<float>& rates, Flowing& taken)
{
	rates.clear();
	taken.clear();

	const size_t slots = static_cast<size_t>(BbtagStage::kFlowSlots);

	for (size_t m = 0; m < model.Flows().size(); ++m)
	{
		const BbtagMua::Flow& flow = model.Flows()[m];

		if (!flow.known)
			continue;

		int kind = flow.across != 0.0f && flow.down != 0.0f ? kFlowBoth : SingleKind(flow);
		std::vector<float> wanted = FlowRates(flow, kind, flip);
		int slot = SharedSlot(rates, wanted);

		if (slot < 0 && kind == kFlowBoth && rates.size() + wanted.size() > slots)
		{
			kind = SingleKind(flow);
			wanted = FlowRates(flow, kind, flip);
			slot = SharedSlot(rates, wanted);
		}

		if (slot < 0 && rates.size() + wanted.size() > slots)
			continue;

		if (slot < 0)
		{
			slot = static_cast<int>(rates.size());
			rates.insert(rates.end(), wanted.begin(), wanted.end());
		}

		taken[static_cast<int>(m)] = std::make_pair(slot, kind);
	}
}

int BlendOf(const BbtagMua::Model& model, const BbtagMua::Mesh& mesh)
{
	const std::vector<BbtagMua::Skeleton>& skeletons = model.Skeletons();
	const int owners[] = { mesh.skeleton, mesh.partner };
	int mode = kBlendUnset;

	for (int owner : owners)
	{
		if (owner < 0 || owner >= static_cast<int>(skeletons.size()))
			continue;

		const int blend = skeletons[owner].blend;

		if (blend >= kBlendOpaque && blend <= kBlendSubtract)
			mode = blend;
	}

	return mode;
}

int Shelf(const std::vector<FbxExWriter::Material>& materials,
	const std::vector<FbxExWriter::Submesh>& submeshes)
{
	for (const FbxExWriter::Submesh& submesh : submeshes)
	{
		if (submesh.material >= 0 && submesh.material < static_cast<int>(materials.size()))
			return materials[submesh.material].textureIndex;
	}

	return 0;
}

void Strips(const BbtagMua::Model& model, double scale, bool mirror, Siblings& out)
{
	out.clear();

	for (const BbtagMua::Mesh& mesh : model.Meshes())
	{
		if (mesh.vertices != 4 || mesh.parts < 1)
			continue;

		float points[4][3] = {};
		float lowV = 0.0f;
		float highV = 0.0f;

		for (int v = 0; v < 4; ++v)
		{
			const BbtagMua::Vertex raw = model.At(mesh.firstVertex + v);
			Place(raw.position, scale, mirror, points[v]);

			lowV = v == 0 ? raw.uv[1] : std::min(lowV, raw.uv[1]);
			highV = v == 0 ? raw.uv[1] : std::max(highV, raw.uv[1]);
		}

		const int index = mesh.firstPart;

		if (index < 0 || index >= static_cast<int>(model.Parts().size()))
			continue;

		const int material = model.Parts()[index].material;

		if (material < 0 || material >= static_cast<int>(model.Materials().size())
			|| model.Materials()[material].empty())
		{
			continue;
		}

		float wide = 0.0f;
		float tall = 0.0f;
		Sides(points, wide, tall);

		const Plate plate = { highV - lowV, tall, 1.0f - highV };

		out[model.Materials()[material][0]].push_back(plate);
	}
}

void Split(const std::vector<float>& vertices, const std::vector<int>& rigged,
	const std::vector<FbxExWriter::Submesh>& submeshes, int unrigged,
	std::vector<std::pair<int, FbxExWriter::Node> >& out)
{
	out.clear();

	std::map<int, std::map<int, std::vector<int> > > groups;

	for (const FbxExWriter::Submesh& submesh : submeshes)
	{
		for (size_t i = 0; i + 2 < submesh.indices.size(); i += 3)
		{
			const int lead = submesh.indices[i];
			const int held = lead >= 0 && lead < static_cast<int>(rigged.size())
				? rigged[lead] : -1;
			const int bone = held < 0 ? unrigged : held;

			std::vector<int>& flat = groups[bone][submesh.material];

			flat.push_back(submesh.indices[i]);
			flat.push_back(submesh.indices[i + 1]);
			flat.push_back(submesh.indices[i + 2]);
		}
	}

	for (const std::pair<const int, std::map<int, std::vector<int> > >& group : groups)
	{
		std::map<int, int> remap;
		FbxExWriter::Node node = FbxExWriter::Leaf();

		for (const std::pair<const int, std::vector<int> >& part : group.second)
		{
			FbxExWriter::Submesh submesh;
			submesh.material = part.first;

			for (int index : part.second)
			{
				const std::map<int, int>::const_iterator known = remap.find(index);

				if (known == remap.end())
				{
					remap[index] = static_cast<int>(node.vertices.size() / kFloats);

					for (int k = 0; k < kFloats; ++k)
						node.vertices.push_back(vertices[index * kFloats + k]);
				}

				submesh.indices.push_back(remap[index]);
			}

			node.submeshes.push_back(submesh);
		}

		out.push_back(std::make_pair(group.first, node));
	}
}

void Chain(const BbtagMua::Model& model, int first, int local,
	const std::vector<std::vector<float> >& matrices, float out[16])
{
	BbtagMua::Identity(out);

	bool started = false;
	int index = local;
	int guard = 0;

	while (index >= 0 && index < static_cast<int>(matrices.size()) && guard++ < 256)
	{
		if (!started)
		{
			memcpy(out, matrices[index].data(), sizeof(float) * 16);
			started = true;
		}
		else
		{
			float composed[16] = {};
			BbtagMua::Multiply(out, matrices[index].data(), composed);
			memcpy(out, composed, sizeof(float) * 16);
		}

		const int bone = first + index;

		if (bone < 0 || bone >= static_cast<int>(model.Bones().size()))
			break;

		index = model.Bones()[bone].parent;
	}
}

int BonesOf(const BbtagMua::Model& model, int first)
{
	for (const BbtagMua::Skeleton& skeleton : model.Skeletons())
	{
		if (skeleton.firstBone == first)
			return skeleton.bones;
	}

	return 1;
}

void Deltas(const BbtagMua::Model& model, int first, const BbtagMmot::Motion& motion,
	double scale, bool mirror, std::vector<std::vector<float> >& out)
{
	const int bones = BonesOf(model, first);

	if (first < 0 || bones < 1 || first + bones > static_cast<int>(model.Bones().size()))
		return;

	std::vector<std::vector<float> > rest;

	for (int i = 0; i < bones; ++i)
	{
		const BbtagMua::Bone& held = model.Bones()[first + i];

		rest.push_back(std::vector<float>(held.matrix, held.matrix + 16));
	}

	std::vector<std::vector<float> > settled;

	for (int i = 0; i < bones; ++i)
	{
		float inverse[16] = {};

		if (i >= motion.Bones() || !motion.Unbind(i, inverse))
		{
			float composed[16] = {};
			Chain(model, first, i, rest, composed);
			BbtagMua::Invert(composed, inverse);
		}

		settled.push_back(std::vector<float>(inverse, inverse + 16));
	}

	float place[16] = {};
	BbtagMua::Identity(place);
	place[0] = static_cast<float>(scale);
	place[5] = static_cast<float>(scale);
	place[10] = static_cast<float>(mirror ? -scale : scale);

	float unplace[16] = {};
	BbtagMua::Invert(place, unplace);

	out.clear();

	for (int frame = 0; frame < motion.Frames(); ++frame)
	{
		std::vector<std::vector<float> > local;

		for (int i = 0; i < bones; ++i)
		{
			if (i >= motion.Bones())
			{
				local.push_back(rest[i]);
				continue;
			}

			const BbtagMua::Bone& held = model.Bones()[first + i];

			float composed[16] = {};
			motion.Pose(i, frame, held.translation, held.rotation, held.scale, composed);

			local.push_back(std::vector<float>(composed, composed + 16));
		}

		std::vector<float> step;

		for (int i = 0; i < bones; ++i)
		{
			float posed[16] = {};
			Chain(model, first, i, local, posed);

			float delta[16] = {};
			BbtagMua::Multiply(settled[i].data(), posed, delta);

			float lifted[16] = {};
			BbtagMua::Multiply(unplace, delta, lifted);

			float landed[16] = {};
			BbtagMua::Multiply(lifted, place, landed);

			step.insert(step.end(), landed, landed + 16);
		}

		out.push_back(step);
	}
}

typedef std::map<std::string, BbtagMmot::Motion> MotionFiles;

void Motions(const BbtagPac::Files& scene, MotionFiles& out)
{
	out.clear();

	for (const std::pair<const std::string, std::vector<uint8_t> >& file : scene)
	{
		const std::string leaf = Lowered(file.first);

		if (!EndsWith(leaf, ".mmot") || leaf.compare(0, 4, "mot/") != 0)
			continue;

		BbtagMmot::Motion motion;

		if (motion.Read(file.second) && motion.Frames() >= 2)
			out[Leaf(leaf)] = motion;
	}
}

std::string TakeName(const std::string& picked)
{
	return picked.empty() ? std::string(kModelTake) : Lowered(picked);
}

void Bind(const BbtagMua::Model& model, const MotionFiles& files, int first, const std::string& picked,
	double scale, bool mirror, std::map<int, Takes>& out)
{
	const std::string name = TakeName(picked);
	const std::map<int, Takes>::const_iterator known = out.find(first);

	if (picked.empty() || (known != out.end() && known->second.count(name) != 0))
		return;

	const MotionFiles::const_iterator file = files.find(name);

	if (file == files.end())
		return;

	Track track;
	track.motion = file->second;
	Deltas(model, first, track.motion, scale, mirror, track.frames);

	if (!track.frames.empty())
		out[first][name] = track;
}

bool Moves(const BbtagMua::Model& model, int first, int count)
{
	std::vector<BbtagMua::Key> keys;

	for (int i = 0; i < count; ++i)
	{
		const int index = first + i;

		if (index < 0 || index >= static_cast<int>(model.Bones().size()))
			break;

		const BbtagMua::Bone& bone = model.Bones()[index];

		if (bone.frames < 2)
			continue;

		for (int k = 0; k < 4; ++k)
		{
			model.Keys(bone.track[k], keys);

			for (size_t at = 1; at < keys.size(); ++at)
			{
				if (memcmp(keys[at].value, keys[0].value, sizeof(keys[0].value)) != 0)
					return true;
			}
		}
	}

	return false;
}

void Adopt(const BbtagMua::Model& model, int first, int count, BbtagMmot::Motion& out)
{
	int frames = 0;

	for (int i = 0; i < count; ++i)
	{
		const BbtagMua::Bone& bone = model.Bones()[first + i];

		if (bone.frames > 1)
			frames = frames < bone.frames ? bone.frames : frames;
	}

	out.Reset(count, frames);

	const BbtagMmot::Motion::Kind kinds[4] = { BbtagMmot::Motion::Kind::Translation,
		BbtagMmot::Motion::Kind::Rotation, BbtagMmot::Motion::Kind::Turn,
		BbtagMmot::Motion::Kind::Scale };

	std::vector<BbtagMua::Key> keys;

	for (int i = 0; i < count; ++i)
	{
		const BbtagMua::Bone& bone = model.Bones()[first + i];

		if (bone.frames < 2)
			continue;

		for (int k = 0; k < 4; ++k)
		{
			model.Keys(bone.track[k], keys);

			for (const BbtagMua::Key& key : keys)
				out.Add(i, kinds[k], key.value, key.frame);
		}
	}
}

void Internal(const BbtagMua::Model& model, double scale, bool mirror, std::map<int, Takes>& out)
{
	std::set<int> wanted;

	for (const BbtagMua::Mesh& mesh : model.Meshes())
	{
		if (mesh.vertices > 0)
			wanted.insert(mesh.bone);
	}

	for (const BbtagMua::Skeleton& skeleton : model.Skeletons())
	{
		const int first = skeleton.firstBone;
		const int count = skeleton.bones;

		if (wanted.count(first) == 0 || (skeleton.flags & kNoDefaultTake) != 0)
			continue;

		if (first < 0 || count < 1
			|| first + count > static_cast<int>(model.Bones().size()))
		{
			continue;
		}

		if (!Moves(model, first, count))
			continue;

		Track track;
		Adopt(model, first, count, track.motion);

		if (track.motion.Frames() < 2)
			continue;

		Deltas(model, first, track.motion, scale, mirror, track.frames);
		out[first][kModelTake] = track;
	}
}

bool Unculled(const std::string& stage)
{
	const std::string named = Lowered(stage);

	for (const char* one : kUnculled)
	{
		if (named == one)
			return true;
	}

	return false;
}

uint32_t SkeletonFlags(const BbtagMua::Model& model, const BbtagMua::Mesh& mesh)
{
	const std::vector<BbtagMua::Skeleton>& skeletons = model.Skeletons();

	if (mesh.skeleton < 0 || mesh.skeleton >= static_cast<int>(skeletons.size()))
		return 0;

	return skeletons[mesh.skeleton].flags;
}

Rank RankOf(const BbtagMua::Model& model, const BbtagMua::Mesh& mesh)
{
	const int lift = static_cast<int>(mesh.pivot[1]);
	Rank rank = { -static_cast<float>(static_cast<int>(mesh.pivot[2])),
		lift < 0 ? static_cast<float>(lift) : 0.0f };

	if ((SkeletonFlags(model, mesh) & kPivotMoves) == 0)
		return rank;

	float world[16] = {};
	model.World(mesh.bone, world);

	const float depth = mesh.pivot[2] * world[10] + world[14];
	const float w = mesh.pivot[2] * world[11] + world[15];

	rank.first = -(w != 0.0f ? depth / w : depth);

	return rank;
}

FbxExWriter::Material Painted(const std::vector<std::string>& textures, int index)
{
	FbxExWriter::Material material = {};
	material.filename = textures.empty() ? std::string() : textures[index];
	material.textureIndex = index;
	material.value[0] = 1.0f;
	material.value[1] = 1.0f;
	material.value[2] = 1.0f;
	material.value[3] = 1.0f;
	material.value[7] = 1.0f;
	material.value[11] = 1.0f;
	material.value[15] = 1.0f;
	material.value[16] = 20.0f;

	return material;
}

std::vector<uint8_t> MadeCard(const CardKind& kind)
{
	std::vector<uint8_t> out(kDdsSize + kCardBytes, 0);
	const uint32_t fields[] = { kDdsHeader, kDdsFlags, kCardSide, kCardSide, kCardBytes };
	const uint32_t format[] = { kDdsPixelFormat, kDdsFourCc };

	memcpy(out.data(), "DDS ", 4);
	memcpy(out.data() + 4, fields, sizeof(fields));
	memcpy(out.data() + StageCapture::kStampAt, kind.stamp, strlen(kind.stamp));
	memcpy(out.data() + kDdsPixelFormatAt, format, sizeof(format));
	memcpy(out.data() + kDdsFourCcAt, "DXT1", 4);
	memcpy(out.data() + kDdsCapsAt, &kDdsTexture, sizeof(kDdsTexture));

	return out;
}

class Atlas
{
public:
	Atlas(FbxExWriter::Model& built, const BbtagStage::Images& images)
		: m_built(built), m_images(images)
	{
	}

	int Add(const std::string& name)
	{
		const std::string leaf = Lowered(Leaf(name));
		const BbtagStage::Images::const_iterator image = m_images.find(leaf);
		const bool known = image != m_images.end();

		m_built.textures.push_back(known ? leaf : Lowered(name));
		m_pixels.push_back(known ? &image->second : nullptr);
		m_cutout.push_back(BbtagArt::Transparent(known ? image->second : m_empty));
		m_sheets.emplace_back(known ? image->second : m_empty);

		return static_cast<int>(m_built.textures.size()) - 1;
	}

	int Find(const std::string& name)
	{
		std::string leaf = Lowered(Leaf(name));

		if (!EndsWith(leaf, ".dds"))
			leaf += ".dds";

		for (size_t i = 0; i < m_built.textures.size(); ++i)
		{
			if (m_built.textures[i] == leaf && m_pixels[i] != nullptr)
				return static_cast<int>(i);
		}

		if (m_images.find(leaf) == m_images.end())
			return -1;

		return Add(leaf);
	}

	int Card(const std::string& sheet)
	{
		for (const CardKind& kind : kCards)
		{
			if (sheet != kind.sheet)
				continue;

			const std::map<const CardKind*, int>::const_iterator known = m_cardAt.find(&kind);

			if (known != m_cardAt.end())
				return known->second;

			m_cards.push_back(MadeCard(kind));
			m_built.textures.push_back(kind.texture);
			m_pixels.push_back(&m_cards.back());
			m_cutout.push_back(false);
			m_sheets.emplace_back(m_cards.back());
			m_cardAt[&kind] = static_cast<int>(m_built.textures.size()) - 1;

			return m_cardAt[&kind];
		}

		return -1;
	}

	int Material(int material, int sheet)
	{
		if (m_built.materials[material].textureIndex == sheet)
			return material;

		const std::pair<int, int> key(material, sheet);
		const std::map<std::pair<int, int>, int>::const_iterator known = m_retextured.find(key);

		if (known != m_retextured.end())
			return known->second;

		FbxExWriter::Material copy = m_built.materials[material];
		copy.filename = m_built.textures[sheet];
		copy.textureIndex = sheet;

		m_built.materials.push_back(copy);
		m_retextured[key] = static_cast<int>(m_built.materials.size()) - 1;

		return m_retextured[key];
	}

	void Emit(BbtagStage::Images& images) const
	{
		for (const std::pair<const CardKind* const, int>& one : m_cardAt)
			images[one.first->texture] = *m_pixels[one.second];
	}

	int Count() const { return static_cast<int>(m_built.textures.size()); }
	bool Cutout(int index) const { return m_cutout[index]; }
	const BbtagArt::Sheet& SheetAt(int index) const { return m_sheets[index]; }

	const std::vector<uint8_t>& PixelsAt(int index) const
	{
		return m_pixels[index] == nullptr ? m_empty : *m_pixels[index];
	}

private:
	FbxExWriter::Model& m_built;
	const BbtagStage::Images& m_images;
	const std::vector<uint8_t> m_empty;
	std::vector<const std::vector<uint8_t>*> m_pixels;
	std::vector<bool> m_cutout;
	std::deque<BbtagArt::Sheet> m_sheets;
	std::map<std::pair<int, int>, int> m_retextured;
	std::deque<std::vector<uint8_t> > m_cards;
	std::map<const CardKind*, int> m_cardAt;
};

std::string SheetName(const BbtagScript::Sprite& sprite, const BbtagScript::Rect& rect)
{
	if (rect.sheet < 0 || rect.sheet >= static_cast<int>(sprite.sheets.size()))
		return std::string();

	return sprite.sheets[rect.sheet];
}

struct Sheeted
{
	int sheet;
	BbtagArt::Size size;
	bool sized;
	BbtagScript::Rect rect;
};

Sheeted SheetFor(Atlas& atlas, const BbtagScript::Sprite& sprite, const BbtagScript::Rect& rect,
	const Sheeted& plate)
{
	const std::string name = Lowered(SheetName(sprite, rect));
	const int card = atlas.Card(name);

	if (card >= 0)
		return { card, { rect.w, rect.h }, true, { rect.sheet, 0, 0, rect.w, rect.h } };

	if (name == kBannerSheet)
		return { plate.sheet, plate.size, plate.sized,
			{ rect.sheet, 0, 0, plate.size.width, kBannerLines * rect.h } };

	const int named = name.empty() ? -1 : atlas.Find(name);

	if (named < 0)
		return { plate.sheet, plate.size, plate.sized, rect };

	Sheeted out = { named, {}, false, rect };
	out.sized = BbtagArt::Measure(atlas.PixelsAt(named), out.size);

	return out;
}

std::vector<std::pair<int, FbxExWriter::Node> > Grouped(const std::vector<float>& vertices,
	const std::vector<int>& rigged, const std::vector<FbxExWriter::Submesh>& submeshes, bool animated)
{
	std::vector<std::pair<int, FbxExWriter::Node> > out;

	if (animated)
	{
		Split(vertices, rigged, submeshes, 0, out);
		return out;
	}

	FbxExWriter::Node node = FbxExWriter::Leaf();
	node.vertices = vertices;
	node.submeshes = submeshes;
	out.push_back(std::make_pair(-1, node));

	return out;
}

std::string RectKey(const std::string& stem, size_t rect)
{
	return stem + "#" + std::to_string(rect);
}

int LampOf(const std::map<std::string, int>& slots, const std::string& stem, size_t rect)
{
	const std::map<std::string, int>::const_iterator found = slots.find(RectKey(stem, rect));

	return found == slots.end() ? -1 : found->second;
}

bool TakeRectLamps(const BbtagScript::Played& run, const BbtagScript::Sprite& sprite,
	const std::string& stem, std::map<std::string, int>& slots,
	std::vector<BbtagScript::Lamp>& lamps)
{
	if (slots.count(RectKey(stem, 0)) != 0)
		return true;

	std::vector<BbtagScript::Lamp> wanted(sprite.rect.size());
	std::vector<bool> fades(sprite.rect.size(), false);
	size_t needed = 0;

	for (size_t r = 0; r < sprite.rect.size(); ++r)
	{
		fades[r] = BbtagScript::Showing(run, sprite.rect[r], wanted[r]);
		needed += fades[r] ? 1 : 0;
	}

	if (lamps.size() + needed > static_cast<size_t>(BbtagStage::kLampSlots))
		return false;

	for (size_t r = 0; r < sprite.rect.size(); ++r)
	{
		if (!fades[r])
		{
			slots[RectKey(stem, r)] = -1;
			continue;
		}

		slots[RectKey(stem, r)] = static_cast<int>(lamps.size());
		lamps.push_back(wanted[r]);
	}

	return true;
}

void Shift(std::vector<float>& vertices, int column, float by)
{
	const size_t rows = vertices.size() / kFloats;

	for (size_t i = 0; i < rows; ++i)
		vertices[i * kFloats + column] += by;
}

void MarkFlow(std::vector<float>& vertices, float mark)
{
	if (mark == 0.0f)
		return;

	Shift(vertices, kU, mark);
}

std::vector<float> Squeezed(const std::vector<float>& vertices)
{
	std::vector<float> out = vertices;
	const size_t rows = out.size() / kFloats;

	for (size_t i = 0; i < rows; ++i)
	{
		out[i * kFloats + kU] = BbtagStage::kFlipInset + BbtagStage::kFlipSpan * out[i * kFloats + kU];
		out[i * kFloats + kV] = BbtagStage::kFlipInset + BbtagStage::kFlipSpan * out[i * kFloats + kV];
	}

	return out;
}

void Mark(std::vector<float>& vertices, int lamp)
{
	if (lamp < 0)
		return;

	Shift(vertices, kV, -kLampMark * (lamp + 1));
}

std::vector<float> Rest()
{
	std::vector<float> out;

	for (int i = 0; i < FbxExWriter::kMatrixFloats; ++i)
		out.push_back((i % 5) == 0 ? 1.0f : 0.0f);

	return out;
}

void Append(std::vector<float>& entry, const std::vector<float>& frame, size_t at)
{
	if (at + kMatrix > frame.size())
	{
		const std::vector<float> rest = Rest();
		entry.insert(entry.end(), rest.begin(), rest.end());
		return;
	}

	entry.insert(entry.end(), frame.begin() + at, frame.begin() + at + kMatrix);
}

std::vector<float> MotionOf(const Piece& piece, const std::map<int, Takes>& moving)
{
	std::vector<float> entry;
	const std::map<int, Takes>::const_iterator track = moving.find(piece.root);

	if (track == moving.end() || piece.bone < 0 || track->second.empty())
		return entry;

	const size_t at = static_cast<size_t>(piece.bone) * kMatrix;

	if (piece.run == nullptr)
	{
		const Takes::const_iterator own = track->second.find(kModelTake);

		if (own == track->second.end())
			return entry;

		for (const std::vector<float>& step : own->second.frames)
			Append(entry, step, at);

		return entry;
	}

	const std::vector<BbtagScript::Step>& steps = piece.run->frame;

	if (steps.empty())
		return entry;

	const BbtagScript::Step& last = steps.back();
	const Takes::const_iterator held = track->second.find(TakeName(last.take));
	const bool constant = std::all_of(steps.begin(), steps.end(),
		[&last](const BbtagScript::Step& one) { return one.take == last.take; });

	if (constant)
	{
		if (held == track->second.end() || held->second.frames.empty())
			return entry;

		const std::vector<std::vector<float> >& frames = held->second.frames;

		for (size_t i = 0; i < frames.size(); ++i)
			Append(entry, frames[(static_cast<size_t>(steps[0].at) + i) % frames.size()], at);

		return entry;
	}

	const int began = static_cast<int>(steps.size()) - 1 - last.at;
	size_t length = steps.size();

	if (piece.run->settled && held != track->second.end() && held->second.frames.size() > 1)
		length = std::max(length, static_cast<size_t>(std::max(0,
			began + static_cast<int>(held->second.frames.size()))));

	for (size_t i = 0; i < length; ++i)
	{
		const BbtagScript::Step step = i < steps.size() ? steps[i]
			: BbtagScript::Step{ last.take, static_cast<int>(i) - began };
		const Takes::const_iterator take = track->second.find(TakeName(step.take));

		if (take == track->second.end() || take->second.frames.empty())
		{
			Append(entry, std::vector<float>(), 0);
			continue;
		}

		const std::vector<std::vector<float> >& frames = take->second.frames;
		Append(entry, frames[static_cast<size_t>(step.at) % frames.size()], at);
	}

	return entry;
}

std::vector<float> Parked(const std::vector<int>& frames, int rect, const std::vector<float>& motion)
{
	const std::vector<float> rest = Rest();
	const std::vector<float>& source = motion.size() < kMatrix ? rest : motion;
	const size_t steps = source.size() / kMatrix;
	std::vector<float> out;
	out.reserve(frames.size() * kMatrix);

	for (size_t i = 0; i < frames.size(); ++i)
	{
		const size_t at = (i % steps) * kMatrix;
		out.insert(out.end(), source.begin() + at, source.begin() + at + kMatrix);

		if (frames[i] != rect)
			out[out.size() - 3] += kParked;
	}

	return out;
}

typedef std::map<std::string, const std::vector<uint8_t>*> EveryScript;

void SpawnOrigins(const BbtagMua::Model& model, const std::string& stem, int bone,
	std::vector<std::array<double, 3> >& out)
{
	const std::string wanted = Lowered(stem) + ".evb";
	const size_t before = out.size();

	for (const BbtagMua::Skeleton& skeleton : model.Skeletons())
	{
		const int which = skeleton.script;

		if (which < 0 || which >= static_cast<int>(model.Scripts().size())
			|| Lowered(model.Scripts()[which]) != wanted)
			continue;

		float world[kMatrix] = {};
		model.World(skeleton.firstBone + (bone >= 0 && bone < skeleton.bones ? bone : 0), world);
		out.push_back({ world[12], world[13], world[14] });
	}

	if (out.size() == before)
		out.push_back({ 0.0, 0.0, 0.0 });
}

std::vector<uint8_t> RawDds(int side, const std::vector<uint8_t>& rgba)
{
	const uint32_t pitch = static_cast<uint32_t>(side) * 4;
	std::vector<uint8_t> out(kDdsSize + rgba.size(), 0);
	const uint32_t fields[] = { kDdsHeader, kDdsRawFlags, static_cast<uint32_t>(side), static_cast<uint32_t>(side), pitch };
	const uint32_t format[] = { kDdsPixelFormat, kDdsRgbAlpha, 0, kDdsRawBits };

	memcpy(out.data(), "DDS ", 4);
	memcpy(out.data() + 4, fields, sizeof(fields));
	memcpy(out.data() + kDdsPixelFormatAt, format, sizeof(format));
	memcpy(out.data() + kDdsMasksAt, kDdsMasks, sizeof(kDdsMasks));
	memcpy(out.data() + kDdsCapsAt, &kDdsTexture, sizeof(kDdsTexture));

	for (size_t at = 0; at + 3 < rgba.size(); at += 4)
	{
		uint8_t* const texel = out.data() + kDdsSize + at;
		texel[0] = rgba[at + 2];
		texel[1] = rgba[at + 1];
		texel[2] = rgba[at];
		texel[3] = rgba[at + 3];
	}

	return out;
}

std::vector<float> PoseMatrix(const BbtagParticleBake::Pose& pose)
{
	std::vector<float> out = Rest();

	if (!pose.shown)
	{
		out[0] = 0.0f;
		out[5] = 0.0f;
		out[13] = kParked;
		return out;
	}

	float place[3] = {};
	const float position[3] = { static_cast<float>(pose.position[0]), static_cast<float>(pose.position[1]),
		static_cast<float>(pose.position[2]) };
	Place(position, kScale, kMirror, place);

	const double c = cos(-pose.turn);
	const double s = sin(-pose.turn);
	const double wide = pose.size[0] * kScale;
	const double tall = pose.size[1] * kScale;

	out[0] = static_cast<float>(wide * c);
	out[1] = static_cast<float>(wide * s);
	out[4] = static_cast<float>(-tall * s);
	out[5] = static_cast<float>(tall * c);
	out[12] = place[0];
	out[13] = place[1];
	out[14] = place[2];

	return out;
}

FbxExWriter::Node CardQuad(const std::array<double, 4>& cell, const double tint[3], int material)
{
	const float corners[4][2] = { { -0.5f, -0.5f }, { 0.5f, -0.5f }, { 0.5f, 0.5f }, { -0.5f, 0.5f } };
	FbxExWriter::Node node = FbxExWriter::Leaf();

	for (const float* corner : corners)
	{
		const double u = corner[0] < 0.0f ? cell[0] : cell[2];
		const double v = corner[1] > 0.0f ? cell[1] : cell[3];
		const float row[kFloats] = { corner[0], corner[1], 0.0f, 0.0f, 0.0f, kMirror ? -1.0f : 1.0f,
			static_cast<float>(tint[0]), static_cast<float>(tint[1]), static_cast<float>(tint[2]), 1.0f,
			static_cast<float>(u), static_cast<float>(kFlip ? 1.0 - v : v) };

		node.vertices.insert(node.vertices.end(), row, row + kFloats);
	}

	node.submeshes.push_back({ material, { 0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2 } });
	node.blendmode = 1;

	return node;
}

bool ParticleData(const BbtagStage::Source& source, std::vector<BbtagParticle::Effect>& effects,
	BbtagParticle::Surface& surface)
{
	return BbtagParticle::Read(source.particles, effects) && BbtagParticle::Atlas(source.particleArt, surface);
}

void Kicks(const BbtagStage::Source& source, const std::vector<BbtagScript::Zone>& zones,
	BbtagParticleBake::Cards& cards)
{
	const bool kicked = std::any_of(zones.begin(), zones.end(),
		[](const BbtagScript::Zone& zone) { return zone.kind == kKickKind; });

	std::vector<BbtagParticle::Effect> effects;
	BbtagParticle::Surface surface;

	if (!kicked || !ParticleData(source, effects, surface))
		return;

	BbtagParticleBake::Kicked(effects, kKickEffect, surface, kKickBursts, cards);
}

void Particles(const BbtagStage::Source& source, const BbtagMua::Model& model, const EveryScript& every,
	BbtagStage::Result& out, BbtagParticleBake::Cards& cards)
{
	std::vector<BbtagParticle::Effect> effects;
	BbtagParticle::Surface surface;

	if (!ParticleData(source, effects, surface))
		return;

	std::vector<BbtagParticleBake::Spawned> spawned;

	for (const std::pair<const std::string, const std::vector<uint8_t>*>& script : every)
	{
		std::vector<BbtagScript::Spawn> spawns;
		BbtagScript::Spawns(*script.second, spawns);

		for (const BbtagScript::Spawn& spawn : spawns)
		{
			std::vector<BbtagParticleBake::Spawned>::iterator entry = std::find_if(spawned.begin(), spawned.end(),
				[&spawn](const BbtagParticleBake::Spawned& one) { return one.effect == spawn.effect; });

			if (entry == spawned.end())
			{
				spawned.push_back({ spawn.effect, {} });
				entry = spawned.end() - 1;
			}

			SpawnOrigins(model, script.first, spawn.bone, entry->origins);
		}
	}

	if (spawned.empty())
		return;

	BbtagParticleBake::Emitted(effects, spawned, surface, cards);

	BbtagParticleBake::Layer layer;

	if (!BbtagParticleBake::Bake(effects, spawned, surface, source.stage, layer))
		return;

	out.layer[layer.patName] = layer.pat;
	out.layer[kObjectList] = layer.objects;
}

void Unpack(const std::vector<uint8_t>& art, BbtagStage::Images& out)
{
	BbtagPac::Files files;
	BbtagPac::Walk(art, files);

	for (const std::pair<const std::string, std::vector<uint8_t> >& file : files)
		out[Lowered(Leaf(file.first))] = file.second;
}

void Vertices(const BbtagMua::Model& model, const BbtagMua::Mesh& mesh, std::vector<float>& vertices,
	std::vector<int>& rigged)
{
	for (int v = 0; v < mesh.vertices; ++v)
	{
		const BbtagMua::Vertex raw = model.At(mesh.firstVertex + v);
		rigged.push_back(raw.bone);

		float position[3] = {};
		Place(raw.position, kScale, kMirror, position);

		float normal[3] = { raw.normal[0], raw.normal[1], kMirror ? -raw.normal[2] : raw.normal[2] };
		Normalise(normal);

		vertices.push_back(position[0]);
		vertices.push_back(position[1]);
		vertices.push_back(position[2]);
		vertices.push_back(normal[0]);
		vertices.push_back(normal[1]);
		vertices.push_back(normal[2]);

		for (int c = 0; c < 4; ++c)
			vertices.push_back(static_cast<float>(raw.colour[c] / 255.0));

		vertices.push_back(raw.uv[0]);
		vertices.push_back(kFlip ? static_cast<float>(1.0 - raw.uv[1]) : raw.uv[1]);
	}
}

void Wind(const BbtagMua::Triangle& triangle, bool twoSided, std::vector<int>& indices)
{
	indices.push_back(triangle.a);
	indices.push_back(kMirror ? triangle.c : triangle.b);
	indices.push_back(kMirror ? triangle.b : triangle.c);

	if (!twoSided)
		return;

	indices.push_back(triangle.a);
	indices.push_back(kMirror ? triangle.b : triangle.c);
	indices.push_back(kMirror ? triangle.c : triangle.b);
}

std::vector<FbxExWriter::Submesh> Submeshes(const BbtagMua::Model& model, const BbtagMua::Mesh& mesh,
	int materials, bool twoSided)
{
	std::vector<FbxExWriter::Submesh> out;

	for (int p = 0; p < mesh.parts; ++p)
	{
		const int index = mesh.firstPart + p;

		if (index < 0 || index >= static_cast<int>(model.Parts().size()))
			continue;

		const BbtagMua::Part& part = model.Parts()[index];

		std::vector<BbtagMua::Triangle> triangles;
		model.Triangles(part, triangles);

		FbxExWriter::Submesh submesh;
		submesh.material = part.material >= 0 && part.material < materials ? part.material : 0;

		for (const BbtagMua::Triangle& triangle : triangles)
		{
			if (triangle.a >= mesh.vertices || triangle.b >= mesh.vertices || triangle.c >= mesh.vertices)
				continue;

			Wind(triangle, twoSided, submesh.indices);
		}

		if (!submesh.indices.empty())
			out.push_back(submesh);
	}

	return out;
}

struct MeshBody
{
	std::vector<float> vertices;
	std::vector<int> rigged;
	std::vector<FbxExWriter::Submesh> submeshes;
	bool faded = false;
	bool clear = false;
};

struct Look
{
	int root;
	bool clear;
	bool adds;
	bool animated;
	const BbtagScript::Run* run;
	bool solid;
	Rank rank;
};

Piece Made(const Look& look, int bone, const FbxExWriter::Node& node)
{
	Piece piece;
	piece.clear = look.clear;
	piece.solid = look.solid;
	piece.rank = look.rank;
	piece.bone = bone;
	piece.root = look.root;
	piece.node = node;
	piece.node.blendmode = look.adds ? 1 : 0;

	return piece;
}

struct Placed
{
	FbxExWriter::Node node;
	std::vector<float> anime;
	bool clear;
	bool once;
};

int Midpoint(std::vector<float>& vertices, std::vector<int>& rigged,
	std::map<std::pair<int, int>, int>& made, int a, int b)
{
	const std::pair<int, int> key(std::min(a, b), std::max(a, b));
	const std::map<std::pair<int, int>, int>::const_iterator known = made.find(key);

	if (known != made.end())
		return known->second;

	const int index = static_cast<int>(vertices.size() / kFloats);

	for (int k = 0; k < kFloats; ++k)
		vertices.push_back(0.5f * (vertices[a * kFloats + k] + vertices[b * kFloats + k]));

	rigged.push_back(rigged[a]);
	made[key] = index;

	return index;
}

void Subdivided(std::vector<float>& vertices, std::vector<int>& rigged,
	std::vector<FbxExWriter::Submesh>& submeshes)
{
	for (int level = 0; level < kShineSplits; ++level)
	{
		std::map<std::pair<int, int>, int> made;

		for (FbxExWriter::Submesh& submesh : submeshes)
		{
			std::vector<int> finer;

			for (size_t i = 0; i + 2 < submesh.indices.size(); i += 3)
			{
				const int a = submesh.indices[i];
				const int b = submesh.indices[i + 1];
				const int c = submesh.indices[i + 2];
				const int ab = Midpoint(vertices, rigged, made, a, b);
				const int bc = Midpoint(vertices, rigged, made, b, c);
				const int ca = Midpoint(vertices, rigged, made, c, a);

				for (int one : { a, ab, ca, ab, b, bc, ca, bc, c, ab, bc, ca })
					finer.push_back(one);
			}

			submesh.indices.swap(finer);
		}
	}
}

void ScreenMapped(std::vector<float>& vertices)
{
	const double half = tan(BbtagCamera::kFov * BbtagCamera::kPi / 360.0);
	const double drift = BbtagCamera::kEyeHeight * kSkyDriftY;
	const size_t rows = vertices.size() / kFloats;

	for (size_t v = 0; v < rows; ++v)
	{
		const float* const row = vertices.data() + v * kFloats;
		const double x = row[0] / kScale;
		const double y = row[1] / kScale;
		const double z = (kMirror ? -row[2] : row[2]) / kScale;
		const double depth = std::max(kNearest, z + BbtagCamera::kEyeDistance);
		const double across = x / (depth * half * BbtagCamera::kAspect);
		const double down = (y - BbtagCamera::kEyeHeight) / (depth * half) + drift;

		vertices[v * kFloats + kU] = static_cast<float>(across);
		vertices[v * kFloats + kV] = static_cast<float>(kFlip ? 1.0 - down : down);
	}
}

bool Mergeable(const Placed& held, const FbxExWriter::Node& node, const std::vector<float>& anime,
	bool clear)
{
	if (held.anime.size() != kMatrix || held.anime != anime || held.clear != clear)
		return false;

	if (held.node.blendmode != node.blendmode
		|| !std::equal(held.node.matrix, held.node.matrix + kMatrix, node.matrix))
	{
		return false;
	}

	return held.node.vertices.size() + node.vertices.size() <= kMergedVertices * kFloats;
}

void Merge(FbxExWriter::Node& into, const FbxExWriter::Node& node)
{
	const int offset = static_cast<int>(into.vertices.size() / kFloats);

	into.vertices.insert(into.vertices.end(), node.vertices.begin(), node.vertices.end());

	for (const FbxExWriter::Submesh& submesh : node.submeshes)
	{
		if (into.submeshes.empty() || into.submeshes.back().material != submesh.material)
			into.submeshes.push_back({ submesh.material, {} });

		std::vector<int>& indices = into.submeshes.back().indices;

		for (int index : submesh.indices)
			indices.push_back(index + offset);
	}
}

std::vector<const Piece*> Ordered(const std::vector<Piece>& pieces)
{
	std::vector<const Piece*> out;
	std::vector<const Piece*> solid;
	std::vector<const Piece*> blended;

	for (const Piece& piece : pieces)
	{
		std::vector<const Piece*>& into = !piece.clear ? out : piece.solid ? solid : blended;
		into.push_back(&piece);
	}

	const auto backToFront = [](const Piece* one, const Piece* other) { return Before(one->rank, other->rank); };

	std::stable_sort(solid.begin(), solid.end(), backToFront);
	std::stable_sort(blended.begin(), blended.end(), backToFront);

	out.insert(out.end(), solid.begin(), solid.end());
	out.insert(out.end(), blended.begin(), blended.end());

	return out;
}

class Conversion
{
public:
	Conversion(const BbtagStage::Source& source, const BbtagMua::Model& model, BbtagStage::Result& out)
		: m_source(source)
		, m_model(model)
		, m_out(out)
		, m_unculled(Unculled(source.stage))
		, m_atlas(m_built, out.images)
	{
	}

	bool Run()
	{
		Paint();
		Flows(m_model, kFlip, m_out.flow, m_flowing);
		Strips(m_model, kScale, kMirror, m_siblings);
		ReadScene();
		Particles(m_source, m_model, m_every, m_out, m_cards);
		Kicks(m_source, m_zones, m_kicked);

		for (const BbtagMua::Mesh& mesh : m_model.Meshes())
			Classify(mesh);

		Internal(m_model, kScale, kMirror, m_moving);
		BindTakes();

		for (const BbtagMua::Mesh& mesh : m_model.Meshes())
			Build(mesh);

		if (m_pieces.empty())
			return false;

		Assemble();

		return !m_out.model.empty();
	}

private:
	void Paint()
	{
		for (const std::string& name : m_model.Textures())
			m_atlas.Add(name);

		for (size_t m = 0; m < m_model.Materials().size(); ++m)
		{
			const std::vector<int>& assigned = m_model.Materials()[m];
			int index = assigned.empty() ? 0 : assigned[0];

			if (index < 0 || index >= m_atlas.Count())
				index = 0;

			const int shine = m < m_model.Reflections().size() ? m_model.Reflections()[m] : -1;
			const bool reflects = shine >= 0 && shine < m_atlas.Count();

			if (reflects && m_atlas.SheetAt(index).Hidden())
				m_mirrored.insert(static_cast<int>(m));

			if (reflects && (m_atlas.SheetAt(index).Dark() || m_atlas.SheetAt(index).Hidden()))
				index = shine;

			m_built.materials.push_back(Painted(m_built.textures, index));
		}

		if (m_built.materials.empty())
			m_built.materials.push_back(Painted(m_built.textures, 0));
	}

	void ReadScene()
	{
		if (!BbtagPac::Walk(m_source.scene, m_scene))
			return;

		Motions(m_scene, m_files);

		for (const std::pair<const std::string, std::vector<uint8_t> >& file : m_scene)
		{
			const std::string lowered = Lowered(file.first);

			if (!EndsWith(lowered, ".evb") || lowered.compare(0, 4, "scr/") != 0)
				continue;

			const std::string leaf = Leaf(file.first);
			const std::string stem = leaf.substr(0, leaf.size() - 4);
			m_every[stem] = &file.second;

			if (stem == "base" || stem == "setting")
			{
				BbtagScript::Tilt(file.second, m_out.tilt);
				std::vector<BbtagScript::Zone> zones;

				if (BbtagScript::Zones(file.second, zones))
					m_zones.swap(zones);

				continue;
			}

			m_scripts[stem] = file.second;
		}
	}

	int IndexOf(const BbtagMua::Mesh& mesh) const
	{
		return static_cast<int>(&mesh - m_model.Meshes().data());
	}

	void BindTakes()
	{
		for (const BbtagMua::Mesh& mesh : m_model.Meshes())
		{
			const BbtagScript::Run* const run = Named(m_runs, IndexOf(mesh));

			if (run == nullptr)
				continue;

			std::set<std::string> picked;

			for (const BbtagScript::Step& step : run->frame)
				picked.insert(step.take);

			for (const std::string& take : picked)
				Bind(m_model, m_files, mesh.bone, take, kScale, kMirror, m_moving);
		}
	}

	void Classify(const BbtagMua::Mesh& mesh)
	{
		const std::string bound = ScriptOf(m_model, mesh);
		const std::string stem = Stem(m_scripts, bound);

		if (stem.empty())
			return;

		const BbtagScript::Played& run = PlayedFor(m_scripts, m_played, stem, mesh.skeleton);

		if (Unseen(run))
		{
			m_unseen.insert(IndexOf(mesh));
			return;
		}

		const double dim = ConstantRamp(run);

		if (dim > 0.0)
			m_dimmed[IndexOf(mesh)] = static_cast<float>(dim);

		BbtagScript::Sprite sprite;
		const bool drawn = BbtagScript::Sprites(run, sprite);

		if (drawn && Specks(sprite))
		{
			m_unseen.insert(IndexOf(mesh));
			return;
		}

		if (drawn)
			m_sprites[IndexOf(mesh)] = sprite;

		BbtagScript::Run motion;

		if (BbtagScript::Motions(run, motion))
			m_runs[IndexOf(mesh)] = motion;

		if (drawn && BbtagScript::Long(run) && sprite.rect.size() <= kLampRects
			&& TakeRectLamps(run, sprite, stem, m_slots, m_out.lamps))
		{
			m_lampedRects.insert(IndexOf(mesh));
			return;
		}

		BbtagScript::Lamp lamp;

		if (bound != stem || m_slots.count(bound) != 0
			|| static_cast<int>(m_out.lamps.size()) >= BbtagStage::kLampSlots || !BbtagScript::Lamps(run, lamp))
		{
			return;
		}

		m_slots[bound] = static_cast<int>(m_out.lamps.size());
		m_out.lamps.push_back(lamp);
	}

	static void Opaque(std::vector<float>& vertices)
	{
		const size_t rows = vertices.size() / kFloats;

		for (size_t i = 0; i < rows; ++i)
			vertices[i * kFloats + 9] = 1.0f;
	}

	void Dim(const BbtagMua::Mesh& mesh, std::vector<float>& vertices) const
	{
		const std::map<int, float>::const_iterator dim = m_dimmed.find(IndexOf(mesh));

		if (dim == m_dimmed.end())
			return;

		const size_t rows = vertices.size() / kFloats;

		for (size_t i = 0; i < rows; ++i)
			vertices[i * kFloats + 9] *= dim->second;
	}

	bool Body(const BbtagMua::Mesh& mesh, int mode, MeshBody& out) const
	{
		Vertices(m_model, mesh, out.vertices, out.rigged);

		if (out.vertices.empty())
			return false;

		Dim(mesh, out.vertices);

		if (mode == kBlendOpaque)
			Opaque(out.vertices);

		const size_t rows = out.vertices.size() / kFloats;

		for (size_t i = 0; i < rows; ++i)
			out.faded = out.faded || out.vertices[i * kFloats + 9] < 1.0f;

		const bool twoSided = m_unculled || (SkeletonFlags(m_model, mesh) & kCullNone) != 0;
		out.submeshes = Submeshes(m_model, mesh, static_cast<int>(m_built.materials.size()), twoSided);
		out.clear = out.faded;

		if (Mirrored(out.submeshes))
			Reflect(out.vertices);

		for (const FbxExWriter::Submesh& submesh : out.submeshes)
			out.clear = out.clear || m_atlas.Cutout(m_built.materials[submesh.material].textureIndex);

		return !out.submeshes.empty();
	}

	bool Mirrored(const std::vector<FbxExWriter::Submesh>& submeshes) const
	{
		return !submeshes.empty() && std::all_of(submeshes.begin(), submeshes.end(),
			[this](const FbxExWriter::Submesh& one) { return m_mirrored.count(one.material) != 0; });
	}

	static void Reflect(std::vector<float>& vertices)
	{
		const size_t rows = vertices.size() / kFloats;

		for (size_t i = 0; i < rows; ++i)
		{
			float* const row = vertices.data() + i * kFloats;
			row[kU] = 0.5f + 0.5f * row[kNormalX];
			row[kV] = 0.5f + 0.5f * row[kNormalY];
		}
	}

	int LampOfMesh(const BbtagMua::Mesh& mesh) const
	{
		const std::map<std::string, int>::const_iterator lit = m_slots.find(ScriptOf(m_model, mesh));

		return lit == m_slots.end() ? -1 : lit->second;
	}

	void Build(const BbtagMua::Mesh& mesh)
	{
		if (m_unseen.count(IndexOf(mesh)) != 0)
			return;

		MeshBody body;
		const int mode = BlendOf(m_model, mesh);

		if (!Body(mesh, mode, body))
			return;

		const int plate = Shelf(m_built.materials, body.submeshes);

		m_out.fading = m_out.fading || body.faded;

		const Look look = { mesh.bone, body.clear || mode != kBlendOpaque, mode == kBlendAdd,
			m_moving.find(mesh.bone) != m_moving.end(), Named(m_runs, IndexOf(mesh)),
			mode == kBlendOpaque, RankOf(m_model, mesh) };

		Sheeted plated = { plate, {}, false, {} };
		plated.sized = BbtagArt::Measure(m_atlas.PixelsAt(plate), plated.size);

		if (ShiningPieces(mesh, body, look))
			return;

		if (SpritePieces(mesh, body, look, plated))
			return;

		if (BandedPieces(body, look, plated))
			return;

		WholePieces(mesh, body, look);
	}

	int ShineOf(int material) const
	{
		const std::vector<int>& shines = m_model.Reflections();

		if (material < 0 || material >= static_cast<int>(shines.size())
			|| material >= static_cast<int>(m_model.Materials().size()) || m_mirrored.count(material) != 0)
		{
			return -1;
		}

		const std::vector<int>& assigned = m_model.Materials()[material];
		const int shine = shines[material];

		if (assigned.empty() || shine < 0 || shine >= m_atlas.Count()
			|| m_built.materials[material].textureIndex != assigned[0])
		{
			return -1;
		}

		return shine;
	}

	bool ShiningPieces(const BbtagMua::Mesh& mesh, const MeshBody& body, const Look& look)
	{
		if ((SkeletonFlags(m_model, mesh) & kShining) == 0 || body.submeshes.empty())
			return false;

		std::vector<FbxExWriter::Submesh> glowing = body.submeshes;

		for (FbxExWriter::Submesh& submesh : glowing)
		{
			const int shine = ShineOf(submesh.material);

			if (shine < 0)
				return false;

			submesh.material = m_atlas.Material(submesh.material, shine);
		}

		const int lamp = LampOfMesh(mesh);

		std::vector<float> mapped = body.vertices;
		std::vector<int> rigged = body.rigged;
		std::vector<FbxExWriter::Submesh> finer = body.submeshes;
		Subdivided(mapped, rigged, finer);
		ScreenMapped(mapped);
		Mark(mapped, lamp);
		Add(look, mapped, rigged, finer, nullptr, -1);

		Look glow = look;
		glow.clear = true;
		glow.adds = true;

		std::vector<float> lit = body.vertices;
		Mark(lit, lamp);
		Add(glow, lit, body.rigged, glowing, nullptr, -1);

		return true;
	}

	bool SpritePieces(const BbtagMua::Mesh& mesh, const MeshBody& body, const Look& look,
		const Sheeted& plated)
	{
		const BbtagScript::Sprite* const sprite = Named(m_sprites, IndexOf(mesh));

		if (sprite == nullptr)
			return false;

		const bool lamped = m_lampedRects.count(IndexOf(mesh)) != 0;

		if (!lamped && FlipPieces(mesh, *sprite, body, look, plated))
			return true;

		const size_t before = m_pieces.size();
		const std::string stem = Stem(m_scripts, ScriptOf(m_model, mesh));
		const int lamp = LampOfMesh(mesh);

		for (size_t r = 0; r < sprite->rect.size(); ++r)
		{
			const BbtagScript::Rect& rect = sprite->rect[r];

			if (BbtagScript::Speck(rect))
				continue;

			const Sheeted sheeted = SheetFor(m_atlas, *sprite, rect, plated);

			if (!sheeted.sized)
				continue;

			std::vector<float> framed = Framed(body.vertices, sheeted.rect, sheeted.size, kFlip);
			Mark(framed, lamped ? LampOf(m_slots, stem, r) : lamp);
			MarkFlow(framed, FlowMarkOf(body.submeshes));

			Add(look, framed, body.rigged, Retextured(body.submeshes, sheeted.sheet),
				lamped ? nullptr : &sprite->frame, static_cast<int>(r));
		}

		return m_pieces.size() != before;
	}

	bool FlipPieces(const BbtagMua::Mesh& mesh, const BbtagScript::Sprite& sprite, const MeshBody& body,
		const Look& look, const Sheeted& plated)
	{
		std::map<int, std::vector<size_t> > sheets;
		std::vector<Sheeted> placed(sprite.rect.size(), plated);

		for (size_t r = 0; r < sprite.rect.size(); ++r)
		{
			const BbtagScript::Rect& rect = sprite.rect[r];

			if (BbtagScript::Speck(rect))
				continue;

			placed[r] = SheetFor(m_atlas, sprite, rect, plated);

			if (placed[r].sized)
				sheets[placed[r].sheet].push_back(r);
		}

		if (sheets.empty())
			return false;

		std::vector<BbtagScript::Flip> wanted;
		size_t fresh = 0;
		const float flow = FlowMarkOf(body.submeshes);

		for (const std::pair<const int, std::vector<size_t> >& sheet : sheets)
		{
			wanted.push_back(FlipOf(sprite, sheet.second, placed, flow));
			fresh += SlotOf(wanted.back()) < 0 ? 1 : 0;
		}

		if (m_out.flips.size() + fresh > static_cast<size_t>(BbtagStage::kFlipSlots))
			return false;

		const int lamp = LampOfMesh(mesh);
		size_t next = 0;

		for (const std::pair<const int, std::vector<size_t> >& sheet : sheets)
		{
			const BbtagScript::Flip& flip = wanted[next++];
			int slot = SlotOf(flip);

			if (slot < 0)
			{
				slot = static_cast<int>(m_out.flips.size());
				m_out.flips.push_back(flip);
			}

			std::vector<float> marked = Squeezed(body.vertices);
			Shift(marked, kU, -BbtagStage::kFlipMark * (slot + 1));
			Mark(marked, lamp);

			Add(look, marked, body.rigged, Retextured(body.submeshes, sheet.first), nullptr, -1);
		}

		return true;
	}

	int SlotOf(const BbtagScript::Flip& flip) const
	{
		for (size_t i = 0; i < m_out.flips.size(); ++i)
		{
			if (m_out.flips[i].rects == flip.rects && m_out.flips[i].frame == flip.frame)
				return static_cast<int>(i);
		}

		return -1;
	}

	BbtagScript::Flip FlipOf(const BbtagScript::Sprite& sprite, const std::vector<size_t>& rects,
		const std::vector<Sheeted>& placed, float flow) const
	{
		BbtagScript::Flip flip;
		std::map<int, int> local;

		for (size_t r : rects)
		{
			const BbtagScript::Rect& rect = placed[r].rect;
			const BbtagArt::Size& size = placed[r].size;

			local[static_cast<int>(r)] = static_cast<int>(flip.rects.size() / 4);
			flip.rects.push_back(flow + static_cast<float>(rect.x) / size.width);
			flip.rects.push_back(static_cast<float>(rect.w) / size.width);
			flip.rects.push_back(static_cast<float>(rect.y) / size.height);
			flip.rects.push_back(static_cast<float>(rect.h) / size.height);
		}

		for (int shown : sprite.frame)
		{
			const std::map<int, int>::const_iterator at = local.find(shown);
			flip.frame.push_back(at == local.end() ? -1 : at->second);
		}

		return flip;
	}

	std::vector<FbxExWriter::Submesh> Retextured(const std::vector<FbxExWriter::Submesh>& submeshes,
		int sheet)
	{
		std::vector<FbxExWriter::Submesh> out = submeshes;

		for (FbxExWriter::Submesh& submesh : out)
			submesh.material = m_atlas.Material(submesh.material, sheet);

		return out;
	}

	void Add(const Look& look, const std::vector<float>& vertices, const std::vector<int>& rigged,
		const std::vector<FbxExWriter::Submesh>& submeshes, const std::vector<int>* frames, int rect)
	{
		for (const std::pair<int, FbxExWriter::Node>& group : Grouped(vertices, rigged, submeshes,
			look.animated))
		{
			Piece piece = Made(look, group.first, group.second);
			piece.run = look.animated ? look.run : nullptr;
			piece.frames = frames;
			piece.rect = rect;

			m_pieces.push_back(piece);
		}
	}

	bool BandedPieces(const MeshBody& body, const Look& look, const Sheeted& plated)
	{
		if (look.animated)
			return false;

		const Siblings::const_iterator group = m_siblings.find(plated.sheet);
		const std::vector<Plate> none;
		int window = 1;
		int taken = -1;

		Bands(body.vertices, body.submeshes, plated.size, group == m_siblings.end() ? none : group->second,
			window, taken);

		if (window <= 1)
			return false;

		std::vector<int> wanted;

		for (int i = 0; i < window; ++i)
		{
			if (i != taken)
				wanted.push_back(i);
		}

		const int phase = static_cast<int>(m_pieces.size()) % static_cast<int>(wanted.size());

		for (size_t step = 0; step < wanted.size(); ++step)
		{
			FbxExWriter::Node node = FbxExWriter::Leaf();
			node.vertices = Banded(body.vertices, wanted[step], window);
			node.submeshes = body.submeshes;

			Piece piece = Made(look, -1, node);
			piece.fixed = Shown(static_cast<int>(step), static_cast<int>(wanted.size()), phase);

			m_pieces.push_back(piece);
		}

		return true;
	}

	float FlowMarkOf(const std::vector<FbxExWriter::Submesh>& submeshes) const
	{
		for (const FbxExWriter::Submesh& submesh : submeshes)
		{
			const Flowing::const_iterator slot = m_flowing.find(submesh.material);

			if (slot == m_flowing.end())
				continue;

			const int which = slot->second.first;
			const int group = slot->second.second + BbtagStage::kFlowKinds * (which / BbtagStage::kFlowBank);

			return kFlowMark * (which % BbtagStage::kFlowBank + group * BbtagStage::kFlowBank + 1);
		}

		return 0.0f;
	}

	void WholePieces(const BbtagMua::Mesh& mesh, MeshBody& body, const Look& look)
	{
		Mark(body.vertices, LampOfMesh(mesh));
		MarkFlow(body.vertices, FlowMarkOf(body.submeshes));

		Add(look, body.vertices, body.rigged, body.submeshes, nullptr, -1);
	}

	std::vector<float> AnimeOf(const Piece& piece) const
	{
		if (!piece.fixed.empty())
			return piece.fixed;

		const std::vector<float> motion = MotionOf(piece, m_moving);

		if (piece.frames != nullptr)
			return Parked(*piece.frames, piece.rect, motion);

		return motion.empty() ? Rest() : motion;
	}

	void Assemble()
	{
		const std::vector<const Piece*> ordered = Ordered(m_pieces);

		FbxExWriter::Node root = FbxExWriter::Branch();
		root.child = ordered.empty() ? -1 : 1;

		m_built.nodes.push_back(root);
		m_built.animes.push_back(Rest());

		std::vector<Placed> placed;

		for (const Piece* piece : ordered)
		{
			const std::vector<float> anime = AnimeOf(*piece);

			if (!placed.empty() && Mergeable(placed.back(), piece->node, anime, piece->clear))
			{
				Merge(placed.back().node, piece->node);
				continue;
			}

			const bool once = piece->run != nullptr && piece->run->settled && piece->frames == nullptr
				&& anime.size() > kMatrix;

			placed.push_back({ piece->node, anime, piece->clear, once });
		}

		for (const Placed& one : placed)
		{
			if (one.once)
			{
				m_out.once.push_back(static_cast<int>(m_built.nodes.size()));
				m_out.once.push_back(static_cast<int>(one.anime.size() / kMatrix));
			}

			m_built.nodes.push_back(one.node);
			m_built.animes.push_back(one.anime);
		}

		CardNodes(m_cards, kSparkSuffix);
		KickNodes();

		for (size_t i = 1; i < m_built.nodes.size(); ++i)
			m_built.nodes[i].sibling = i + 1 < m_built.nodes.size() ? static_cast<int>(i) + 1 : -1;

		m_atlas.Emit(m_out.images);
		FbxExWriter::Build(m_built, m_out.model);
	}

	int CardNodes(const BbtagParticleBake::Cards& cards, const char* suffix)
	{
		const int first = static_cast<int>(m_built.nodes.size());

		if (cards.cards.empty())
			return first;

		const std::string name = Lowered(m_source.stage) + suffix;
		m_out.images[name] = RawDds(cards.side, cards.rgba);

		const int texture = m_atlas.Add(name);
		const int material = static_cast<int>(m_built.materials.size());
		m_built.materials.push_back(Painted(m_built.textures, texture));

		for (const BbtagParticleBake::Card& card : cards.cards)
		{
			std::vector<float> anime;

			for (const BbtagParticleBake::Pose& pose : card.frames)
			{
				const std::vector<float> step = PoseMatrix(pose);
				anime.insert(anime.end(), step.begin(), step.end());
			}

			m_built.nodes.push_back(CardQuad(cards.cells[static_cast<size_t>(card.cell)], card.tint, material));
			m_built.animes.push_back(anime);
		}

		return first;
	}

	void KickNodes()
	{
		if (m_kicked.cards.empty())
			return;

		const int first = CardNodes(m_kicked, kKickSuffix);
		const int perBurst = static_cast<int>(m_kicked.cards.size()) / kKickBursts;

		m_out.kick = { first, kKickBursts, perBurst, static_cast<int>(m_kicked.cards.front().frames.size()) };

		for (const BbtagScript::Zone& zone : m_zones)
		{
			m_out.kick.push_back(zone.limit);
			m_out.kick.push_back(zone.kind);
		}
	}

	const BbtagStage::Source& m_source;
	const BbtagMua::Model& m_model;
	BbtagStage::Result& m_out;
	const bool m_unculled;
	FbxExWriter::Model m_built;
	Atlas m_atlas;
	Flowing m_flowing;
	Siblings m_siblings;
	std::map<int, Takes> m_moving;
	MotionFiles m_files;
	BbtagPac::Files m_scene;
	EveryScript m_every;
	BbtagScript::Scripts m_scripts;
	PlayedScripts m_played;
	std::map<int, BbtagScript::Sprite> m_sprites;
	std::map<int, BbtagScript::Run> m_runs;
	std::map<std::string, int> m_slots;
	std::set<int> m_lampedRects;
	std::set<int> m_unseen;
	std::map<int, float> m_dimmed;
	std::set<int> m_mirrored;
	std::vector<Piece> m_pieces;
	BbtagParticleBake::Cards m_cards;
	BbtagParticleBake::Cards m_kicked;
	std::vector<BbtagScript::Zone> m_zones;
};

}

std::string BbtagStage::Block(const std::string& stage, float tilt)
{
	const BbtagDefaults::Look* const look = BbtagDefaults::Of(stage);
	const double size = look != nullptr ? look->size : 1.0;
	const double half = tan(BbtagCamera::kFov * BbtagCamera::kPi / 360.0);
	const double scale = size * (1.0 / half) / (BbtagCamera::kEyeDistance * kUni2Character / kCharacter);
	const double vanish = kUni2EyeHeight - BbtagCamera::kEyeHeight / (BbtagCamera::kEyeDistance * half);

	char camera[320] = {};
	sprintf_s(camera, "\tScale = [ %.4f, %.4f, %.4f ],\r\n"
		"\tPosition = [ 0.0, 0.0, 0.0 ],\r\n"
		"\tViewGrid = 0,\r\n"
		"\tFOV = %.1f,\r\n"
		"\tViewRotationX = %.1f,\r\n"
		"\tVanishingPoint = %.4f,\r\n", scale, scale, scale, BbtagCamera::kFov, tilt,
		vanish);

	return std::string(camera) + kBlock;
}

namespace {

bool ModelIn(const std::vector<uint8_t>& archive, BbtagMua::Model& out)
{
	BbtagPac::Files files;

	if (!BbtagPac::Walk(archive, files))
		return false;

	const std::vector<uint8_t>* const found = BbtagPac::Ending(files, ".mua");

	return found != nullptr && out.Read(*found);
}

}

bool BbtagStage::HoldsWholeModel(const std::vector<uint8_t>& archive)
{
	BbtagMua::Model model;

	return ModelIn(archive, model) && model.HasGeometry();
}

bool BbtagStage::Convert(const Source& source, Result& out)
{
	out = Result();

	BbtagMua::Model model;

	if (!ModelIn(source.geometry, model))
		return false;

	Unpack(source.art, out.images);

	if (model.Textures().empty())
		return false;

	Conversion conversion(source, model, out);

	return conversion.Run();
}
