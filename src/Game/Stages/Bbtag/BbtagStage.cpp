#include "Game/Stages/Bbtag/BbtagStage.h"

#include "Game/Stages/Bbtag/BbtagArt.h"
#include "Game/Stages/Bbtag/BbtagMmot.h"
#include "Game/Stages/Bbtag/BbtagMua.h"
#include "Game/Stages/Bbtag/BbtagPac.h"
#include "Game/Stages/Bbtag/BbtagScript.h"
#include "Game/Stages/FbxExWriter.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

namespace {

constexpr double kCharacter = 213.0;
constexpr double kUni2Character = 0.132;

constexpr float kFlowMark = 128.0f;
constexpr float kLampMark = 128.0f;
constexpr int kBlendOpaque = 0;
constexpr int kBlendAdd = 2;
constexpr int kBlendSubtract = 4;
constexpr int kBlendUnset = 0x7fffffff;
constexpr double kCardBlack = 8.0 / 255.0;

constexpr float kWindowCover = 0.9f;
constexpr float kWindowBand = 0.6f;
constexpr float kWindowMatch = 0.25f;
constexpr int kWindowBands = 8;
constexpr int kWindowHold = 180;
constexpr int kLeastRect = 4;
constexpr const char* kModelTake = "(the model's own)";

constexpr int kFloats = FbxExWriter::kVertexFloats;

const char* const kBlock =
	"\tScale = [ 10.0, 10.0, 10.0 ],\r\n"
	"\tPosition = [ 0.0, 0.0, 0.0 ],\r\n"
	"\tViewGrid = 0,\r\n"
	"\tFOV = 45.0,\r\n"
	"\tViewRotationX = 0.0,\r\n"
	"\tVanishingPoint = 0.0,\r\n"
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
	bool clear;
	int bone;
	int root;
	FbxExWriter::Node node;
	std::vector<float> fixed;
	const BbtagScript::Run* run;
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

std::vector<float> Framed(const std::vector<float>& vertices,
	const BbtagScript::Rect& rect, const BbtagArt::Size& size)
{
	const float left = static_cast<float>(rect.x) / size.width;
	const float right = static_cast<float>(rect.x + rect.w) / size.width;
	const float top = 1.0f - static_cast<float>(rect.y) / size.height;
	const float bottom = 1.0f - static_cast<float>(rect.y + rect.h) / size.height;

	const size_t rows = vertices.size() / kFloats;

	float lowU = vertices[10];
	float highU = vertices[10];
	float lowW = vertices[11];
	float highW = vertices[11];

	for (size_t i = 1; i < rows; ++i)
	{
		lowU = std::min(lowU, vertices[i * kFloats + 10]);
		highU = std::max(highU, vertices[i * kFloats + 10]);
		lowW = std::min(lowW, vertices[i * kFloats + 11]);
		highW = std::max(highW, vertices[i * kFloats + 11]);
	}

	std::vector<float> out = vertices;

	for (size_t i = 0; i < rows; ++i)
	{
		const float acrossU = highU == lowU ? 0.0f
			: (vertices[i * kFloats + 10] - lowU) / (highU - lowU);
		const float acrossW = highW == lowW ? 0.0f
			: (vertices[i * kFloats + 11] - lowW) / (highW - lowW);

		out[i * kFloats + 10] = left + (right - left) * acrossU;
		out[i * kFloats + 11] = bottom + (top - bottom) * acrossW;
	}

	return out;
}

std::vector<float> Held(const std::vector<int>& frames, int rect)
{
	std::vector<float> out;
	out.reserve(frames.size() * 16);

	for (int shown : frames)
	{
		for (int i = 0; i < 16; ++i)
			out.push_back((i % 5) == 0 ? 1.0f : 0.0f);

		if (shown != rect)
			out[out.size() - 3] = -1000.0f;
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
		rates.push_back(flow.across || flip ? -flow.rate : flow.rate);
	}
}

bool DrawnOn(const BbtagScript::Sprite& sprite, const BbtagScript::Rect& rect, const std::string& texture)
{
	if (sprite.sheets.empty())
		return true;

	if (rect.sheet < 0 || rect.sheet >= static_cast<int>(sprite.sheets.size()))
		return false;

	return Lowered(sprite.sheets[rect.sheet]) == texture;
}

int BlendOf(const BbtagMua::Model& model, const BbtagMua::Mesh& mesh)
{
	const std::vector<BbtagMua::Skeleton>& skeletons = model.Skeletons();

	if (mesh.skeleton < 0 || mesh.skeleton >= static_cast<int>(skeletons.size()))
		return kBlendUnset;

	return skeletons[mesh.skeleton].blend;
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

bool Carded(const std::vector<int>& used, const std::vector<BbtagArt::Sheet>& sheets,
	const std::vector<float>& vertices, bool flip)
{
	if (used.empty())
		return false;

	const size_t rows = vertices.size() / kFloats;

	for (int index : used)
	{
		if (index < 0 || index >= static_cast<int>(sheets.size()) || !sheets[index].Lit())
			return false;

		const BbtagArt::Sheet& sheet = sheets[index];

		for (size_t i = 0; i < rows; ++i)
		{
			const float* const one = &vertices[i * kFloats];
			const double w = flip ? 1.0 - one[11] : one[11];
			double shade = one[6];
			shade = one[7] > shade ? one[7] : shade;
			shade = one[8] > shade ? one[8] : shade;

			if (sheet.Peak(one[10], w) * shade > kCardBlack)
				return false;
		}
	}

	return true;
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

const std::vector<uint8_t>* Find(const BbtagPac::Files& files, const std::string& suffix)
{
	for (const std::pair<const std::string, std::vector<uint8_t> >& file : files)
	{
		if (EndsWith(Lowered(file.first), suffix.c_str()))
			return &file.second;
	}

	return nullptr;
}

}

std::string BbtagStage::Block()
{
	return kBlock;
}

bool BbtagStage::Convert(const Source& source, Result& out)
{
	out.model.clear();
	out.images.clear();
	out.flow.clear();
	out.fading = false;

	BbtagPac::Files geometry;

	if (!BbtagPac::Walk(source.geometry, geometry))
		return false;

	const std::vector<uint8_t>* const found = Find(geometry, ".mua");

	if (found == nullptr)
		return false;

	BbtagMua::Model model;

	if (!model.Read(*found))
		return false;

	BbtagPac::Files art;
	BbtagPac::Walk(source.art, art);

	for (const std::pair<const std::string, std::vector<uint8_t> >& file : art)
		out.images[Lowered(Leaf(file.first))] = file.second;

	if (model.Textures().empty())
		return false;

	const double scale = kUni2Character / kCharacter;
	const bool mirror = true;
	const bool flip = true;

	FbxExWriter::Model built;

	std::vector<bool> cutout;
	std::vector<const std::vector<uint8_t>*> pixels;
	std::vector<BbtagArt::Sheet> sheets;

	for (const std::string& name : model.Textures())
	{
		const std::string leaf = Lowered(Leaf(name));
		const Images::const_iterator image = out.images.find(leaf);
		const std::vector<uint8_t> empty;
		const std::vector<uint8_t>& blob = image == out.images.end() ? empty : image->second;

		built.textures.push_back(image == out.images.end() ? Lowered(name) : leaf);
		pixels.push_back(image == out.images.end() ? nullptr : &image->second);
		cutout.push_back(BbtagArt::Transparent(blob));
		sheets.emplace_back(blob);
	}

	for (const std::vector<int>& assigned : model.Materials())
	{
		int index = assigned.empty() ? 0 : assigned[0];

		if (index < 0 || index >= static_cast<int>(built.textures.size()))
			index = 0;

		FbxExWriter::Material material = {};
		material.filename = built.textures.empty() ? std::string() : built.textures[index];
		material.textureIndex = index;
		material.value[0] = 1.0f;
		material.value[1] = 1.0f;
		material.value[2] = 1.0f;
		material.value[3] = 1.0f;
		material.value[7] = 1.0f;
		material.value[11] = 1.0f;
		material.value[15] = 1.0f;
		material.value[16] = 20.0f;

		built.materials.push_back(material);
	}

	if (built.materials.empty())
	{
		FbxExWriter::Material material = {};
		material.filename = built.textures.empty() ? std::string() : built.textures[0];
		material.textureIndex = 0;
		material.value[16] = 20.0f;

		built.materials.push_back(material);
	}

	std::map<int, std::pair<int, bool> > flowing;
	Flows(model, flip, out.flow, flowing);

	Siblings siblings;
	Strips(model, scale, mirror, siblings);

	std::map<int, Takes> moving;
	BbtagScript::Scripts scripts;
	PlayedScripts played;
	std::map<std::string, BbtagScript::Sprite> sprites;
	std::map<std::string, BbtagScript::Run> runs;
	std::map<std::string, int> slots;
	BbtagPac::Files scene;

	if (BbtagPac::Walk(source.scene, scene))
	{
		Motions(model, scene, scale, mirror, moving);

		for (const std::pair<const std::string, std::vector<uint8_t> >& file : scene)
		{
			const std::string leaf = Lowered(file.first);

			if (!EndsWith(leaf, ".evb") || leaf.compare(0, 4, "scr/") != 0)
				continue;

			const std::string stem = Leaf(file.first).substr(0, Leaf(file.first).size() - 4);

			if (stem == "base" || stem == "setting")
				continue;

			scripts[stem] = file.second;
		}
	}

	out.lamps.clear();

	for (const BbtagMua::Mesh& mesh : model.Meshes())
	{
		const std::string bound = ScriptOf(model, mesh);
		const std::string stem = Stem(scripts, bound, mesh.name);

		if (stem.empty())
			continue;

		const BbtagScript::Played& run = PlayedFor(scripts, played, stem, mesh.skeleton);

		BbtagScript::Lamp lamp;

		if (BbtagScript::Lamps(run, lamp) && bound == stem && slots.count(bound) == 0
			&& static_cast<int>(slots.size()) < BbtagStage::kLampSlots)
		{
			slots[bound] = static_cast<int>(slots.size());
			out.lamps.push_back(lamp);
		}

		BbtagScript::Sprite sprite;

		if (BbtagScript::Sprites(run, sprite))
			sprites[mesh.name] = sprite;

		BbtagScript::Run motion;

		if (BbtagScript::Motions(run, motion))
			runs[mesh.name] = motion;
	}

	Internal(model, scale, mirror, moving);

	std::vector<Piece> pieces;

	for (const BbtagMua::Mesh& mesh : model.Meshes())
	{
		std::vector<float> vertices;
		std::vector<int> rigged;

		for (int v = 0; v < mesh.vertices; ++v)
		{
			const BbtagMua::Vertex raw = model.At(mesh.firstVertex + v);
			rigged.push_back(raw.bone);

			float position[3] = {};
			Place(raw.position, scale, mirror, position);

			float normal[3] = { raw.normal[0], raw.normal[1],
				mirror ? -raw.normal[2] : raw.normal[2] };
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
			vertices.push_back(flip ? static_cast<float>(1.0 - raw.uv[1]) : raw.uv[1]);
		}

		if (vertices.empty())
			continue;

		const size_t rows = vertices.size() / kFloats;
		bool faded = false;

		for (size_t i = 0; i < rows; ++i)
			faded = faded || vertices[i * kFloats + 9] < 1.0f;

		bool clear = faded;
		std::vector<FbxExWriter::Submesh> submeshes;
		std::vector<int> used;

		for (int p = 0; p < mesh.parts; ++p)
		{
			const int index = mesh.firstPart + p;

			if (index < 0 || index >= static_cast<int>(model.Parts().size()))
				continue;

			const BbtagMua::Part& part = model.Parts()[index];

			std::vector<BbtagMua::Triangle> triangles;
			model.Triangles(part, triangles);

			FbxExWriter::Submesh submesh;
			submesh.material = part.material >= 0
				&& part.material < static_cast<int>(built.materials.size()) ? part.material : 0;

			for (const BbtagMua::Triangle& triangle : triangles)
			{
				if (triangle.a >= mesh.vertices || triangle.b >= mesh.vertices
					|| triangle.c >= mesh.vertices)
				{
					continue;
				}

				submesh.indices.push_back(triangle.a);
				submesh.indices.push_back(mirror ? triangle.c : triangle.b);
				submesh.indices.push_back(mirror ? triangle.b : triangle.c);
			}

			if (submesh.indices.empty())
				continue;

			const int sheet = built.materials[submesh.material].textureIndex;
			clear = clear || cutout[sheet];

			if (std::find(used.begin(), used.end(), sheet) == used.end())
				used.push_back(sheet);

			submeshes.push_back(submesh);
		}

		if (submeshes.empty())
			continue;

		const int plate = Shelf(built.materials, submeshes);
		const int mode = BlendOf(model, mesh);
		const bool written = mode == kBlendOpaque || mode == kBlendAdd || mode == kBlendSubtract;
		const bool adds = mode == kBlendAdd
			|| (!written && !clear && Carded(used, sheets, vertices, flip));

		clear = clear || mode != kBlendOpaque;
		out.fading = out.fading || faded;

		const std::map<int, Takes>::const_iterator track = moving.find(mesh.bone);
		const bool animated = track != moving.end();
		const std::string bound = ScriptOf(model, mesh);
		const BbtagScript::Run* const run = Named(runs, mesh.name);

		const std::map<std::string, int>::const_iterator lit = slots.find(bound);
		const int lamp = lit == slots.end() ? -1 : lit->second;

		const std::vector<uint8_t> empty;
		const std::vector<uint8_t>& sheet = pixels[plate] == nullptr ? empty : *pixels[plate];

		BbtagArt::Size size = {};
		const bool measured = BbtagArt::Measure(sheet, size);

		const BbtagScript::Sprite* const sprite = animated ? nullptr : Named(sprites, mesh.name);

		std::vector<int> order;

		for (size_t i = 0; sprite != nullptr && i < sprite->rect.size(); ++i)
		{
			const BbtagScript::Rect& rect = sprite->rect[i];

			if (rect.w > kLeastRect && rect.h > kLeastRect
				&& DrawnOn(*sprite, rect, built.textures[plate]))
			{
				order.push_back(static_cast<int>(i));
			}
		}

		if (!order.empty() && measured)
		{
			std::sort(order.begin(), order.end(), [sprite](int one, int other)
			{
				const BbtagScript::Rect& a = sprite->rect[one];
				const BbtagScript::Rect& b = sprite->rect[other];

				if (a.x != b.x)
					return a.x < b.x;

				if (a.y != b.y)
					return a.y < b.y;

				if (a.w != b.w)
					return a.w < b.w;

				if (a.h != b.h)
					return a.h < b.h;

				return a.sheet < b.sheet;
			});

			for (int index : order)
			{
				Piece piece;
				piece.clear = clear;
				piece.bone = -1;
				piece.root = mesh.bone;
				piece.node = FbxExWriter::Leaf();
				piece.node.blendmode = adds ? 1 : 0;
				piece.node.vertices = Framed(vertices, sprite->rect[index], size);
				piece.node.submeshes = submeshes;
				piece.fixed = Held(sprite->frame, index);
				piece.run = nullptr;

				pieces.push_back(piece);
			}

			continue;
		}

		int window = 1;
		int taken = -1;

		if (!animated)
		{
			const Siblings::const_iterator group = siblings.find(plate);
			const std::vector<Plate> none;

			Bands(vertices, submeshes, size, group == siblings.end() ? none : group->second,
				window, taken);
		}

		if (window > 1)
		{
			std::vector<int> wanted;

			for (int i = 0; i < window; ++i)
			{
				if (i != taken)
					wanted.push_back(i);
			}

			const int phase = static_cast<int>(pieces.size()) % static_cast<int>(wanted.size());

			for (size_t step = 0; step < wanted.size(); ++step)
			{
				Piece piece;
				piece.clear = clear;
				piece.bone = -1;
				piece.root = mesh.bone;
				piece.node = FbxExWriter::Leaf();
				piece.node.blendmode = adds ? 1 : 0;
				piece.node.vertices = Banded(vertices, wanted[step], window);
				piece.node.submeshes = submeshes;
				piece.fixed = Shown(static_cast<int>(step), static_cast<int>(wanted.size()), phase);
				piece.run = nullptr;

				pieces.push_back(piece);
			}

			continue;
		}

		if (lamp >= 0)
		{
			const float mark = kLampMark * (lamp + 1);

			for (size_t i = 0; i < rows; ++i)
				vertices[i * kFloats + 11] += mark;
		}

		for (const FbxExWriter::Submesh& submesh : submeshes)
		{
			const std::map<int, std::pair<int, bool> >::const_iterator slot =
				flowing.find(submesh.material);

			if (slot == flowing.end())
				continue;

			const float mark = kFlowMark * (slot->second.first
				+ (slot->second.second ? BbtagStage::kFlowSlots : 0) + 1);

			for (size_t i = 0; i < rows; ++i)
				vertices[i * kFloats + 10] += mark;

			break;
		}

		std::vector<std::pair<int, FbxExWriter::Node> > groups;

		if (animated)
		{
			Split(vertices, rigged, submeshes, 0, groups);
		}
		else
		{
			FbxExWriter::Node node = FbxExWriter::Leaf();
			node.vertices = vertices;
			node.submeshes = submeshes;

			groups.push_back(std::make_pair(-1, node));
		}

		for (std::pair<int, FbxExWriter::Node>& group : groups)
		{
			Piece piece;
			piece.clear = clear;
			piece.bone = group.first;
			piece.root = mesh.bone;
			piece.node = group.second;
			piece.node.blendmode = adds ? 1 : 0;
			piece.run = animated ? run : nullptr;

			pieces.push_back(piece);
		}
	}

	if (pieces.empty())
		return false;

	std::vector<const Piece*> ordered;
	std::vector<std::pair<double, const Piece*> > clear;

	for (const Piece& piece : pieces)
	{
		if (!piece.clear)
		{
			ordered.push_back(&piece);
			continue;
		}

		clear.push_back(std::make_pair(Depth(piece.node), &piece));
	}

	std::stable_sort(clear.begin(), clear.end(),
		[](const std::pair<double, const Piece*>& one,
			const std::pair<double, const Piece*>& other)
	{
		return one.first < other.first;
	});

	for (const std::pair<double, const Piece*>& one : clear)
		ordered.push_back(one.second);

	FbxExWriter::Node root = FbxExWriter::Branch();
	root.child = ordered.empty() ? -1 : 1;

	built.nodes.push_back(root);

	std::vector<float> rest;

	for (int i = 0; i < FbxExWriter::kMatrixFloats; ++i)
		rest.push_back((i % 5) == 0 ? 1.0f : 0.0f);

	built.animes.push_back(rest);

	for (const Piece* piece : ordered)
	{
		built.nodes.push_back(piece->node);

		if (!piece->fixed.empty())
		{
			built.animes.push_back(piece->fixed);
			continue;
		}

		const std::map<int, Takes>::const_iterator track = moving.find(piece->root);

		if (track == moving.end() || piece->bone < 0 || track->second.empty())
		{
			built.animes.push_back(rest);
			continue;
		}

		const size_t at = static_cast<size_t>(piece->bone) * 16;
		std::vector<float> entry;

		if (piece->run == nullptr)
		{
			const Track* longest = nullptr;

			for (const std::pair<const std::string, Track>& take : track->second)
			{
				if (longest == nullptr || take.second.motion.Frames() > longest->motion.Frames())
					longest = &take.second;
			}

			for (const std::vector<float>& step : longest->frames)
			{
				if (at + 16 > step.size())
					entry.insert(entry.end(), rest.begin(), rest.end());
				else
					entry.insert(entry.end(), step.begin() + at, step.begin() + at + 16);
			}

			built.animes.push_back(entry);
			continue;
		}

		for (const BbtagScript::Step& step : piece->run->frame)
		{
			const Takes::const_iterator take = track->second.find(step.take);

			if (take == track->second.end() || take->second.frames.empty())
			{
				entry.insert(entry.end(), rest.begin(), rest.end());
				continue;
			}

			const std::vector<std::vector<float> >& frames = take->second.frames;
			const size_t which = static_cast<size_t>(step.at) < frames.size()
				? static_cast<size_t>(step.at) : frames.size() - 1;

			if (at + 16 > frames[which].size())
				entry.insert(entry.end(), rest.begin(), rest.end());
			else
				entry.insert(entry.end(), frames[which].begin() + at,
					frames[which].begin() + at + 16);
		}

		built.animes.push_back(entry);
	}

	for (size_t i = 1; i < built.nodes.size(); ++i)
		built.nodes[i].sibling = i + 1 < built.nodes.size() ? static_cast<int>(i) + 1 : -1;

	FbxExWriter::Build(built, out.model);

	return !out.model.empty();
}
