#include "Game/Stages/Bbtag/BbtagStage.h"

#include "Game/Stages/Bbtag/BbtagArt.h"
#include "Game/Stages/Bbtag/BbtagCamera.h"
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
constexpr int kLeastRect = 4;
constexpr const char* kModelTake = "(the model's own)";
constexpr const char* kObjectList = "object.txt";

constexpr double kUni2EyeHeight = 280.0 / 360.0;
constexpr float kParked = -1000.0f;
constexpr size_t kLampRects = 4;
constexpr uint32_t kCullNone = 0x400;
constexpr const char* kCaptureSheet = "capture";
constexpr const char* kCaptureTexture = "uni2im_capture.dds";
constexpr uint32_t kCaptureSide = 64;
constexpr uint32_t kCaptureBytes = kCaptureSide * kCaptureSide / 2;
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

struct Piece
{
	bool clear = false;
	int bone = -1;
	int root = -1;
	FbxExWriter::Node node;
	std::vector<float> fixed;
	const BbtagScript::Run* run = nullptr;
	const std::vector<int>* frames = nullptr;
	int rect = -1;
};

double Depth(const FbxExWriter::Node& node)
{
	const size_t count = node.vertices.size() / FbxExWriter::kVertexFloats;

	if (count == 0)
		return 0.0;

	double total = 0.0;

	for (size_t i = 0; i < count; ++i)
		total += node.vertices[i * FbxExWriter::kVertexFloats + 2];

	return total / static_cast<double>(count);
}

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

std::string Stem(const BbtagScript::Scripts& scripts, const std::string& bound, const std::string& mesh)
{
	if (scripts.find(bound) != scripts.end())
		return bound;

	if (!bound.empty())
		return std::string();

	for (const BbtagScript::Scripts::value_type& one : scripts)
	{
		if (mesh.compare(0, one.first.size(), one.first) == 0)
			return one.first;
	}

	return std::string();
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
const Held* Named(const std::map<std::string, Held>& held, const std::string& mesh)
{
	const typename std::map<std::string, Held>::const_iterator found = held.find(mesh);

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
		if (plate.span > kWindowBand || fabsf(plate.tall - tall) > kWindowMatch * tall)
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

void Flows(const BbtagMua::Model& model, bool flip, std::vector<float>& rates,
	std::map<int, std::pair<int, bool> >& taken)
{
	rates.clear();
	taken.clear();

	for (size_t m = 0; m < model.Flows().size(); ++m)
	{
		const BbtagMua::Flow& flow = model.Flows()[m];

		if (!flow.known || static_cast<int>(rates.size()) >= BbtagStage::kFlowSlots)
			continue;

		taken[static_cast<int>(m)] = std::make_pair(static_cast<int>(rates.size()), flow.across);
		rates.push_back(flow.across || !flip ? -flow.rate : flow.rate);
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

void Deltas(const BbtagMua::Model& model, int first, const BbtagMmot::Motion& motion,
	double scale, bool mirror, std::vector<std::vector<float> >& out)
{
	const int bones = motion.Bones();

	if (first < 0 || first + bones > static_cast<int>(model.Bones().size()))
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
		float composed[16] = {};
		Chain(model, first, i, rest, composed);

		float inverse[16] = {};
		BbtagMua::Invert(composed, inverse);

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
			const BbtagMua::Bone& held = model.Bones()[first + i];

			float translate[3] = {};
			float rotate[3] = {};
			float size[3] = {};

			motion.Sample(i, frame, held.translation, held.rotation, held.scale, translate, rotate,
				size);

			float composed[16] = {};
			BbtagMmot::Compose(translate, rotate, size, composed);

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

void Motions(const BbtagMua::Model& model, const BbtagPac::Files& scene, double scale,
	bool mirror, std::map<int, Takes>& out)
{
	out.clear();

	std::map<std::string, int> named;

	for (size_t i = 0; i < model.Bones().size(); ++i)
		named.insert(std::make_pair(model.Bones()[i].name, static_cast<int>(i)));

	for (const std::pair<const std::string, std::vector<uint8_t> >& file : scene)
	{
		const std::string leaf = Lowered(file.first);

		if (!EndsWith(leaf, ".mmot") || leaf.compare(0, 4, "mot/") != 0)
			continue;

		BbtagMmot::Motion motion;

		if (!motion.Read(file.second) || motion.Frames() < 2)
			continue;

		const std::map<std::string, int>::const_iterator root = named.find(motion.Target());

		if (root == named.end())
			continue;

		int first = root->second;
		int count = 1;

		for (const BbtagMua::Skeleton& skeleton : model.Skeletons())
		{
			if (skeleton.firstBone != first)
				continue;

			count = skeleton.bones;
			break;
		}

		if (count != motion.Bones())
			continue;

		Track track;
		track.motion = motion;

		out[first][Leaf(file.first)] = track;
	}

	for (std::pair<const int, Takes>& root : out)
	{
		for (std::pair<const std::string, Track>& chosen : root.second)
			Deltas(model, root.first, chosen.second.motion, scale, mirror, chosen.second.frames);
	}
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
		BbtagMmot::Motion::Kind::Rotation, BbtagMmot::Motion::Kind::Translation,
		BbtagMmot::Motion::Kind::Scale };

	std::vector<BbtagMua::Key> keys;

	for (int i = 0; i < count; ++i)
	{
		const BbtagMua::Bone& bone = model.Bones()[first + i];

		if (bone.frames < 2)
			continue;

		for (int k = 0; k < 4; ++k)
		{
			if (k == 2)
				continue;

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

		if (wanted.count(first) == 0 || out.count(first) != 0)
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

std::vector<uint8_t> CaptureCard()
{
	std::vector<uint8_t> out(kDdsSize + kCaptureBytes, 0);
	const uint32_t fields[] = { kDdsHeader, kDdsFlags, kCaptureSide, kCaptureSide, kCaptureBytes };
	const uint32_t format[] = { kDdsPixelFormat, kDdsFourCc };

	memcpy(out.data(), "DDS ", 4);
	memcpy(out.data() + 4, fields, sizeof(fields));
	memcpy(out.data() + StageCapture::kStampAt, StageCapture::kStamp, strlen(StageCapture::kStamp));
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

	int Capture()
	{
		if (m_capture >= 0)
			return m_capture;

		m_card = CaptureCard();
		m_built.textures.push_back(kCaptureTexture);
		m_pixels.push_back(&m_card);
		m_cutout.push_back(false);
		m_sheets.emplace_back(m_card);
		m_capture = static_cast<int>(m_built.textures.size()) - 1;

		return m_capture;
	}

	bool Captures(int index) const
	{
		return index >= 0 && index == m_capture;
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
		if (m_capture >= 0)
			images[kCaptureTexture] = m_card;
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
	std::vector<uint8_t> m_card;
	int m_capture = -1;
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
};

Sheeted SheetFor(Atlas& atlas, const BbtagScript::Sprite& sprite, const BbtagScript::Rect& rect,
	const Sheeted& plate)
{
	const std::string name = SheetName(sprite, rect);

	if (Lowered(name) == kCaptureSheet)
		return { atlas.Capture(), { rect.w, rect.h }, true };

	const int named = name.empty() ? -1 : atlas.Find(name);

	if (named < 0)
		return plate;

	Sheeted out = { named, {}, false };
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
		const Track* longest = nullptr;

		for (const std::pair<const std::string, Track>& take : track->second)
		{
			if (longest == nullptr || take.second.motion.Frames() > longest->motion.Frames())
				longest = &take.second;
		}

		for (const std::vector<float>& step : longest->frames)
			Append(entry, step, at);

		return entry;
	}

	const std::vector<BbtagScript::Step>& steps = piece.run->frame;

	if (steps.empty())
		return entry;

	const BbtagScript::Step& last = steps.back();
	const int began = static_cast<int>(steps.size()) - 1 - last.at;
	const Takes::const_iterator held = track->second.find(last.take);
	size_t length = steps.size();

	if (piece.run->settled && held != track->second.end() && held->second.frames.size() > 1)
		length = std::max(length, static_cast<size_t>(began) + held->second.frames.size());

	for (size_t i = 0; i < length; ++i)
	{
		const BbtagScript::Step step = i < steps.size() ? steps[i]
			: BbtagScript::Step{ last.take, static_cast<int>(i) - began };
		const Takes::const_iterator take = track->second.find(step.take);

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

void Particles(const BbtagStage::Source& source, const BbtagMua::Model& model, const EveryScript& every,
	BbtagStage::Result& out)
{
	std::vector<BbtagParticle::Effect> effects;
	BbtagParticle::Surface surface;

	if (!BbtagParticle::Read(source.particles, effects) || !BbtagParticle::Atlas(source.particleArt, surface))
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

	BbtagParticleBake::Layer layer;

	if (spawned.empty() || !BbtagParticleBake::Bake(effects, spawned, surface, source.stage, layer))
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
};

Piece Made(const Look& look, int bone, const FbxExWriter::Node& node)
{
	Piece piece;
	piece.clear = look.clear;
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
};

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
	std::vector<std::pair<double, const Piece*> > clear;

	for (const Piece& piece : pieces)
	{
		if (!piece.clear)
		{
			out.push_back(&piece);
			continue;
		}

		clear.push_back(std::make_pair(Depth(piece.node), &piece));
	}

	std::stable_sort(clear.begin(), clear.end(),
		[](const std::pair<double, const Piece*>& one, const std::pair<double, const Piece*>& other)
	{
		return one.first < other.first;
	});

	for (const std::pair<double, const Piece*>& one : clear)
		out.push_back(one.second);

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
		Particles(m_source, m_model, m_every, m_out);

		for (const BbtagMua::Mesh& mesh : m_model.Meshes())
			Classify(mesh);

		Internal(m_model, kScale, kMirror, m_moving);

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

			if (shine >= 0 && shine < m_atlas.Count() && m_atlas.SheetAt(index).Dark())
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

		Motions(m_model, m_scene, kScale, kMirror, m_moving);

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
				continue;
			}

			m_scripts[stem] = file.second;
		}
	}

	void Classify(const BbtagMua::Mesh& mesh)
	{
		const std::string bound = ScriptOf(m_model, mesh);
		const std::string stem = Stem(m_scripts, bound, mesh.name);

		if (stem.empty())
			return;

		const BbtagScript::Played& run = PlayedFor(m_scripts, m_played, stem, mesh.skeleton);

		if (Unseen(run))
		{
			m_unseen.insert(mesh.name);
			return;
		}

		const double dim = ConstantRamp(run);

		if (dim > 0.0)
			m_dimmed[mesh.name] = static_cast<float>(dim);

		BbtagScript::Sprite sprite;
		const bool drawn = BbtagScript::Sprites(run, sprite);

		if (drawn)
			m_sprites[mesh.name] = sprite;

		BbtagScript::Run motion;

		if (BbtagScript::Motions(run, motion))
			m_runs[mesh.name] = motion;

		if (drawn && BbtagScript::Long(run) && sprite.rect.size() <= kLampRects
			&& TakeRectLamps(run, sprite, stem, m_slots, m_out.lamps))
		{
			m_lampedRects.insert(mesh.name);
			return;
		}

		BbtagScript::Lamp lamp;

		if (bound != stem || m_slots.count(bound) != 0
			|| static_cast<int>(m_slots.size()) >= BbtagStage::kLampSlots || !BbtagScript::Lamps(run, lamp))
		{
			return;
		}

		m_slots[bound] = static_cast<int>(m_out.lamps.size());
		m_out.lamps.push_back(lamp);
	}

	void Dim(const BbtagMua::Mesh& mesh, std::vector<float>& vertices) const
	{
		const std::map<std::string, float>::const_iterator dim = m_dimmed.find(mesh.name);

		if (dim == m_dimmed.end())
			return;

		const size_t rows = vertices.size() / kFloats;

		for (size_t i = 0; i < rows; ++i)
			vertices[i * kFloats + 9] *= dim->second;
	}

	bool Body(const BbtagMua::Mesh& mesh, MeshBody& out) const
	{
		Vertices(m_model, mesh, out.vertices, out.rigged);

		if (out.vertices.empty())
			return false;

		Dim(mesh, out.vertices);

		const size_t rows = out.vertices.size() / kFloats;

		for (size_t i = 0; i < rows; ++i)
			out.faded = out.faded || out.vertices[i * kFloats + 9] < 1.0f;

		const bool twoSided = m_unculled || (SkeletonFlags(m_model, mesh) & kCullNone) != 0;
		out.submeshes = Submeshes(m_model, mesh, static_cast<int>(m_built.materials.size()), twoSided);
		out.clear = out.faded;

		for (const FbxExWriter::Submesh& submesh : out.submeshes)
			out.clear = out.clear || m_atlas.Cutout(m_built.materials[submesh.material].textureIndex);

		return !out.submeshes.empty();
	}

	int LampOfMesh(const BbtagMua::Mesh& mesh) const
	{
		const std::map<std::string, int>::const_iterator lit = m_slots.find(ScriptOf(m_model, mesh));

		return lit == m_slots.end() ? -1 : lit->second;
	}

	void Build(const BbtagMua::Mesh& mesh)
	{
		if (m_unseen.count(mesh.name) != 0)
			return;

		MeshBody body;

		if (!Body(mesh, body))
			return;

		const int plate = Shelf(m_built.materials, body.submeshes);
		const int mode = BlendOf(m_model, mesh);

		m_out.fading = m_out.fading || body.faded;

		const Look look = { mesh.bone, body.clear || mode != kBlendOpaque, mode == kBlendAdd,
			m_moving.find(mesh.bone) != m_moving.end(), Named(m_runs, mesh.name) };

		Sheeted plated = { plate, {}, false };
		plated.sized = BbtagArt::Measure(m_atlas.PixelsAt(plate), plated.size);

		if (SpritePieces(mesh, body, look, plated))
			return;

		if (BandedPieces(body, look, plated))
			return;

		WholePieces(mesh, body, look);
	}

	bool SpritePieces(const BbtagMua::Mesh& mesh, const MeshBody& body, const Look& look,
		const Sheeted& plated)
	{
		const BbtagScript::Sprite* const sprite = Named(m_sprites, mesh.name);

		if (sprite == nullptr)
			return false;

		const bool lamped = m_lampedRects.count(mesh.name) != 0;

		if (!lamped && FlipPieces(mesh, *sprite, body, look, plated))
			return true;

		const size_t before = m_pieces.size();
		const std::string stem = Stem(m_scripts, ScriptOf(m_model, mesh), mesh.name);
		const int lamp = LampOfMesh(mesh);

		for (size_t r = 0; r < sprite->rect.size(); ++r)
		{
			const BbtagScript::Rect& rect = sprite->rect[r];

			if (rect.w <= kLeastRect || rect.h <= kLeastRect)
				continue;

			const Sheeted sheeted = SheetFor(m_atlas, *sprite, rect, plated);

			if (!sheeted.sized)
				continue;

			BbtagScript::Rect placed = rect;

			if (m_atlas.Captures(sheeted.sheet))
			{
				placed.x = 0;
				placed.y = 0;
			}

			std::vector<float> framed = Framed(body.vertices, placed, sheeted.size, kFlip);
			Mark(framed, lamped ? LampOf(m_slots, stem, r) : lamp);

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

			if (rect.w <= kLeastRect || rect.h <= kLeastRect)
				continue;

			placed[r] = SheetFor(m_atlas, sprite, rect, plated);

			if (placed[r].sized)
				sheets[placed[r].sheet].push_back(r);
		}

		if (sheets.empty())
			return false;

		std::vector<BbtagScript::Flip> wanted;
		size_t fresh = 0;

		for (const std::pair<const int, std::vector<size_t> >& sheet : sheets)
		{
			wanted.push_back(FlipOf(sprite, sheet.second, placed));
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
		const std::vector<Sheeted>& placed) const
	{
		BbtagScript::Flip flip;
		std::map<int, int> local;

		for (size_t r : rects)
		{
			const BbtagScript::Rect& rect = sprite.rect[r];
			const BbtagArt::Size& size = placed[r].size;
			const bool captured = m_atlas.Captures(placed[r].sheet);

			local[static_cast<int>(r)] = static_cast<int>(flip.rects.size() / 4);
			flip.rects.push_back(captured ? 0.0f : static_cast<float>(rect.x) / size.width);
			flip.rects.push_back(static_cast<float>(rect.w) / size.width);
			flip.rects.push_back(captured ? 0.0f : static_cast<float>(rect.y) / size.height);
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

	void MarkFlow(std::vector<float>& vertices, const std::vector<FbxExWriter::Submesh>& submeshes) const
	{
		for (const FbxExWriter::Submesh& submesh : submeshes)
		{
			const std::map<int, std::pair<int, bool> >::const_iterator slot = m_flowing.find(submesh.material);

			if (slot == m_flowing.end())
				continue;

			Shift(vertices, kU, kFlowMark * (slot->second.first
				+ (slot->second.second ? BbtagStage::kFlowSlots : 0) + 1));

			return;
		}
	}

	void WholePieces(const BbtagMua::Mesh& mesh, MeshBody& body, const Look& look)
	{
		Mark(body.vertices, LampOfMesh(mesh));
		MarkFlow(body.vertices, body.submeshes);

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

			placed.push_back({ piece->node, anime, piece->clear });
		}

		for (const Placed& one : placed)
		{
			m_built.nodes.push_back(one.node);
			m_built.animes.push_back(one.anime);
		}

		for (size_t i = 1; i < m_built.nodes.size(); ++i)
			m_built.nodes[i].sibling = i + 1 < m_built.nodes.size() ? static_cast<int>(i) + 1 : -1;

		m_atlas.Emit(m_out.images);
		FbxExWriter::Build(m_built, m_out.model);
	}

	const BbtagStage::Source& m_source;
	const BbtagMua::Model& m_model;
	BbtagStage::Result& m_out;
	const bool m_unculled;
	FbxExWriter::Model m_built;
	Atlas m_atlas;
	std::map<int, std::pair<int, bool> > m_flowing;
	Siblings m_siblings;
	std::map<int, Takes> m_moving;
	BbtagPac::Files m_scene;
	EveryScript m_every;
	BbtagScript::Scripts m_scripts;
	PlayedScripts m_played;
	std::map<std::string, BbtagScript::Sprite> m_sprites;
	std::map<std::string, BbtagScript::Run> m_runs;
	std::map<std::string, int> m_slots;
	std::set<std::string> m_lampedRects;
	std::set<std::string> m_unseen;
	std::map<std::string, float> m_dimmed;
	std::vector<Piece> m_pieces;
};

}

std::string BbtagStage::Block(float tilt)
{
	const double half = tan(BbtagCamera::kFov * BbtagCamera::kPi / 360.0);
	const double scale = (1.0 / half) / (BbtagCamera::kEyeDistance * kUni2Character / kCharacter);
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

bool BbtagStage::Convert(const Source& source, Result& out)
{
	out = Result();

	BbtagPac::Files geometry;

	if (!BbtagPac::Walk(source.geometry, geometry))
		return false;

	const std::vector<uint8_t>* const found = BbtagPac::Ending(geometry, ".mua");

	if (found == nullptr)
		return false;

	BbtagMua::Model model;

	if (!model.Read(*found))
		return false;

	Unpack(source.art, out.images);

	if (model.Textures().empty())
		return false;

	Conversion conversion(source, model, out);

	return conversion.Run();
}
