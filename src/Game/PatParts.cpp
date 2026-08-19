#include "Game/PatParts.h"

#include "Core/logger.h"
#include "Game/CharaTables.h"
#include "Game/DataArchive.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

namespace {

constexpr const char* kFolder = "_coloredit";
constexpr const char* kMagic = "PAniDataFile";

constexpr size_t kMagicBytes = 12;
constexpr size_t kBodyStart = 0x20;

// The projection reduces to one divide: the frustum's camera sits 1024 * 1.3 from the plane.
constexpr float kCameraDistance = 1024.0f * 1.3f;

// UV is expressed against a 256-wide reference, so a 1024 atlas multiplies by four.
constexpr float kUvReference = 256.0f;

constexpr float kTau = 6.28318530718f;
constexpr float kPi = 3.14159265359f;

// A mesh is generated per segment; these bound what a malformed file can ask us to build.
constexpr int kMaxSegments = 64;

struct Shape
{
	int type = 1;
	int radius = 0;
	int width = 0;
	int vertexCount = 1;
	int vertexCount2 = 1;
	int length = 0;
	int length2 = 0;
	int dz = 0;
	int dRadius = 0;
};

struct CutOut
{
	int uv[4] = {};
	int pivot[2] = {};
	int size[2] = {};
	int texture = 0;
	int shape = 0;
	int colour = -1;
};

struct Property
{
	int cutOut = -1;
	int priority = 0;
	float x = 0.0f;
	float y = 0.0f;
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	float rotation[3] = {};
	bool additive = false;
	uint8_t multiply[4] = { 255, 255, 255, 255 };
	uint8_t add[3] = {};
};

struct Patch
{
	std::vector<uint8_t> pixels;
	int width = 0;
	int height = 0;
};

struct Atlas
{
	std::vector<uint8_t> blocks;
	int width = 0;
	int height = 0;
};

// A face before the model transform: four corners in the part's own 3D space and the slice of the
// cut-out laid over them.
struct Face
{
	float p[4][3];
	float u0;
	float v0;
	float u1;
	float v1;
};

std::vector<uint8_t> g_file;
std::map<int, CutOut> g_cutOuts;
std::map<int, std::vector<Property>> g_groups;
std::map<int, Patch> g_patches;
std::map<int, std::vector<PatParts::Quad>> g_quads;
std::map<int, Atlas> g_atlases;
std::vector<Shape> g_shapes;

int g_chara = -1;

uint32_t Dword(size_t at)
{
	uint32_t value = 0;
	if (at + sizeof(value) <= g_file.size())
		memcpy(&value, g_file.data() + at, sizeof(value));

	return value;
}

int Int(size_t at)
{
	return static_cast<int>(Dword(at));
}

float Float(size_t at)
{
	float value = 0.0f;
	if (at + sizeof(value) <= g_file.size())
		memcpy(&value, g_file.data() + at, sizeof(value));

	return value;
}

bool Tag(size_t at, const char* tag)
{
	return at + 4 <= g_file.size() && memcmp(g_file.data() + at, tag, 4) == 0;
}

// The format has no lengths, so an unrecognised chunk cannot be stepped over by size. Walking
// forward to the next tag that does belong at this level resynchronises instead of desyncing -
// which is what `PRAS`, four records in the whole cast, would otherwise do to Uzuki.
size_t Resync(size_t at, const char* const* tags, int count)
{
	for (size_t probe = at + 4; probe + 4 <= g_file.size(); ++probe)
	{
		for (int i = 0; i < count; ++i)
		{
			if (Tag(probe, tags[i]))
				return probe;
		}
	}

	return g_file.size();
}

void DecodeAlpha(const uint8_t* block, uint8_t* out)
{
	uint8_t table[8] = { block[0], block[1] };

	if (table[0] > table[1])
	{
		for (int i = 1; i < 7; ++i)
			table[i + 1] = static_cast<uint8_t>(((7 - i) * table[0] + i * table[1]) / 7);
	}
	else
	{
		for (int i = 1; i < 5; ++i)
			table[i + 1] = static_cast<uint8_t>(((5 - i) * table[0] + i * table[1]) / 5);

		table[6] = 0;
		table[7] = 255;
	}

	uint64_t bits = 0;
	memcpy(&bits, block + 2, 6);

	for (int i = 0; i < 16; ++i)
		out[i] = table[(bits >> (3 * i)) & 7];
}

void DecodeColour(const uint8_t* block, uint8_t* out)
{
	uint16_t packed[2] = {};
	memcpy(packed, block, sizeof(packed));

	uint8_t colour[4][3] = {};

	for (int i = 0; i < 2; ++i)
	{
		colour[i][0] = static_cast<uint8_t>(((packed[i] >> 11) & 31) * 255 / 31);
		colour[i][1] = static_cast<uint8_t>(((packed[i] >> 5) & 63) * 255 / 63);
		colour[i][2] = static_cast<uint8_t>((packed[i] & 31) * 255 / 31);
	}

	for (int c = 0; c < 3; ++c)
	{
		colour[2][c] = static_cast<uint8_t>((2 * colour[0][c] + colour[1][c]) / 3);
		colour[3][c] = static_cast<uint8_t>((colour[0][c] + 2 * colour[1][c]) / 3);
	}

	uint32_t bits = 0;
	memcpy(&bits, block + 4, sizeof(bits));

	for (int i = 0; i < 16; ++i)
	{
		const int pick = (bits >> (2 * i)) & 3;
		memcpy(out + i * 3, colour[pick], 3);
	}
}

void Texel(const Atlas& atlas, int x, int y, uint8_t* out)
{
	const size_t block =
		(static_cast<size_t>(y >> 2) * (atlas.width >> 2) + (x >> 2)) * 16;

	if (x < 0 || y < 0 || x >= atlas.width || y >= atlas.height
		|| block + 16 > atlas.blocks.size())
	{
		memset(out, 0, 4);
		return;
	}

	uint8_t alpha[16] = {};
	uint8_t colour[48] = {};

	DecodeAlpha(atlas.blocks.data() + block, alpha);
	DecodeColour(atlas.blocks.data() + block + 8, colour);

	const int index = (y & 3) * 4 + (x & 3);

	memcpy(out, colour + index * 3, 3);
	out[3] = alpha[index];
}

// `00 value count` writes a run, anything else is a literal byte.
void Decompress(size_t at, size_t packed, std::vector<uint8_t>& out)
{
	size_t write = 0;
	size_t read = at;
	const size_t end = at + packed;

	while (read < end && write < out.size())
	{
		const uint8_t byte = g_file[read];

		if (byte != 0)
		{
			out[write++] = byte;
			++read;
			continue;
		}

		if (read + 2 >= end)
			break;

		const uint8_t value = g_file[read + 1];
		size_t count = g_file[read + 2];

		if (write + count > out.size())
			count = out.size() - write;

		memset(out.data() + write, value, count);
		write += count;
		read += 3;
	}
}

size_t ReadAtlas(size_t at, int id)
{
	static const char* const kTags[] = { "PGST", "PPST", "P_ST", "VEST", "_END" };

	Atlas atlas;

	while (at + 4 <= g_file.size() && !Tag(at, "PGED"))
	{
		if (Tag(at, "PGNM"))
		{
			at += 4 + 0x20;
			continue;
		}

		if (!Tag(at, "PGT2"))
		{
			at += 8;
			continue;
		}

		const uint32_t packed = Dword(at + 4);
		atlas.width = Int(at + 8);
		atlas.height = Int(at + 12);

		if (!Tag(at + 16, "DXT5") || atlas.width <= 0 || atlas.height <= 0)
			break;

		const size_t surface = static_cast<size_t>(atlas.width) * atlas.height;

		if (packed == surface + 128)
		{
			const size_t start = at + 28 + 128;

			if (start + surface <= g_file.size())
				atlas.blocks.assign(g_file.begin() + start, g_file.begin() + start + surface);

			break;
		}

		const uint32_t compressed = Dword(at + 36);
		const uint32_t plain = Dword(at + 40);

		if (plain <= 128)
			break;

		std::vector<uint8_t> dds(plain);
		Decompress(at + 44, compressed, dds);

		atlas.blocks.assign(dds.begin() + 128, dds.end());
		break;
	}

	if (!atlas.blocks.empty())
		g_atlases[id] = std::move(atlas);

	return Resync(at, kTags, 5);
}

size_t ReadCutOut(size_t at, int id)
{
	static const char* const kTags[] = {
		"PPNA", "PPNM", "PPUV", "PPCC", "PPSS", "PPPA", "PPTP", "PPPP", "PPTE", "PPJP", "PPED",
	};

	CutOut cut;

	while (at + 4 <= g_file.size())
	{
		if (Tag(at, "PPED"))
		{
			at += 4;
			break;
		}

		if (Tag(at, "PPNA"))
		{
			at += 4 + 1 + g_file[at + 4];
			continue;
		}

		if (Tag(at, "PPNM"))
		{
			at += 4 + 0x20;
			continue;
		}

		if (Tag(at, "PPUV"))
		{
			for (int i = 0; i < 4; ++i)
				cut.uv[i] = Int(at + 4 + i * 4);

			at += 20;
			continue;
		}

		if (Tag(at, "PPCC"))
		{
			cut.pivot[0] = Int(at + 4);
			cut.pivot[1] = Int(at + 8);
			at += 12;
			continue;
		}

		if (Tag(at, "PPSS"))
		{
			cut.size[0] = Int(at + 4);
			cut.size[1] = Int(at + 8);
			at += 12;
			continue;
		}

		if (Tag(at, "PPPA"))
		{
			cut.colour = g_file[at + 4];
			at += 8;
			continue;
		}

		if (Tag(at, "PPTP"))
		{
			cut.texture = Int(at + 4);
			at += 8;
			continue;
		}

		if (Tag(at, "PPPP"))
		{
			cut.shape = Int(at + 4);
			at += 8;
			continue;
		}

		if (Tag(at, "PPTE"))
		{
			at += 8;
			continue;
		}

		if (Tag(at, "PPJP"))
		{
			at += 12;
			continue;
		}

		at = Resync(at, kTags, 11);
	}

	g_cutOuts[id] = cut;
	return at;
}

size_t ReadProperty(size_t at, std::vector<Property>& out)
{
	static const char* const kTags[] = {
		"PRXY", "PRZM", "PRA3", "PRPR", "PRID", "PRAL", "PRRV", "PRFL", "PRCL", "PRSP", "PRED",
	};

	Property property;

	while (at + 4 <= g_file.size())
	{
		if (Tag(at, "PRED"))
		{
			at += 4;
			break;
		}

		if (Tag(at, "PRXY"))
		{
			property.x = static_cast<float>(Int(at + 4));
			property.y = static_cast<float>(Int(at + 8));
			at += 12;
			continue;
		}

		if (Tag(at, "PRZM"))
		{
			property.scaleX = Float(at + 4);
			property.scaleY = Float(at + 8);
			at += 12;
			continue;
		}

		// Four floats, of which the first is not an angle.
		if (Tag(at, "PRA3"))
		{
			for (int i = 0; i < 3; ++i)
				property.rotation[i] = Float(at + 8 + i * 4);

			at += 20;
			continue;
		}

		if (Tag(at, "PRPR"))
		{
			property.priority = Int(at + 4);
			at += 8;
			continue;
		}

		if (Tag(at, "PRID"))
		{
			property.cutOut = Int(at + 4);
			at += 8;
			continue;
		}

		if (Tag(at, "PRAL"))
		{
			property.additive = g_file[at + 4] != 0;
			at += 5;
			continue;
		}

		if (Tag(at, "PRRV") || Tag(at, "PRFL"))
		{
			at += 5;
			continue;
		}

		// Both are stored blue first.
		if (Tag(at, "PRCL"))
		{
			property.multiply[0] = g_file[at + 6];
			property.multiply[1] = g_file[at + 5];
			property.multiply[2] = g_file[at + 4];
			property.multiply[3] = g_file[at + 7];
			at += 8;
			continue;
		}

		if (Tag(at, "PRSP"))
		{
			property.add[0] = g_file[at + 6];
			property.add[1] = g_file[at + 5];
			property.add[2] = g_file[at + 4];
			at += 8;
			continue;
		}

		at = Resync(at, kTags, 11);
	}

	out.push_back(property);
	return at;
}

size_t ReadGroup(size_t at, int id)
{
	static const char* const kTags[] = { "PANA", "PANM", "PRST", "P_ED" };

	std::vector<Property> properties;

	while (at + 4 <= g_file.size())
	{
		if (Tag(at, "P_ED"))
		{
			at += 4;
			break;
		}

		if (Tag(at, "PANA"))
		{
			at += 4 + 1 + g_file[at + 4];
			continue;
		}

		if (Tag(at, "PANM"))
		{
			at += 4 + 0x20;
			continue;
		}

		if (Tag(at, "PRST"))
		{
			at = ReadProperty(at + 8, properties);
			continue;
		}

		at = Resync(at, kTags, 4);
	}

	if (!properties.empty())
		g_groups[id] = std::move(properties);

	return at;
}

// VEST is `count, strideInDwords` then the shapes; the cut-outs index into it.
size_t ReadShapes(size_t at)
{
	static const char* const kTags[] = { "PGST", "PPST", "P_ST", "_END" };

	const int count = Int(at);
	const int stride = Int(at + 4);

	if (count > 0 && count < 4096 && stride > 8 && stride < 256)
	{
		g_shapes.assign(count, Shape{});

		for (int i = 0; i < count; ++i)
		{
			const size_t base = at + 8 + static_cast<size_t>(i) * stride * 4;

			Shape& shape = g_shapes[i];
			shape.type = Int(base);

			switch (shape.type)
			{
			case 3:
			case 4:
				shape.radius = Int(base + 12);
				shape.width = Int(base + 16);
				shape.vertexCount = Int(base + 20);
				shape.length = Int(base + 24);
				shape.dz = Int(base + 28);
				shape.dRadius = Int(base + 32);
				break;

			case 5:
				shape.radius = Int(base + 12);
				shape.vertexCount = Int(base + 16);
				shape.vertexCount2 = Int(base + 20);
				shape.length = Int(base + 24);
				shape.length2 = Int(base + 28);
				break;

			case 6:
				shape.radius = Int(base + 12);
				shape.dz = Int(base + 16);
				shape.vertexCount = Int(base + 20);
				shape.vertexCount2 = Int(base + 24);
				shape.length = Int(base + 28);
				break;

			default:
				break;
			}
		}
	}

	return Resync(at, kTags, 4);
}

void Parse()
{
	static const char* const kTags[] = { "P_ST", "PPST", "PGST", "VEST", "_END" };

	size_t at = kBodyStart;

	while (at + 4 <= g_file.size())
	{
		if (Tag(at, "_END"))
			return;

		if (Tag(at, "P_ST"))
		{
			at = ReadGroup(at + 8, Int(at + 4));
			continue;
		}

		if (Tag(at, "PPST"))
		{
			at = ReadCutOut(at + 8, Int(at + 4));
			continue;
		}

		if (Tag(at, "PGST"))
		{
			at = ReadAtlas(at + 8, Int(at + 4));
			continue;
		}

		if (Tag(at, "VEST"))
		{
			at = ReadShapes(at + 4);
			continue;
		}

		at = Resync(at, kTags, 5);
	}
}

int Segments(int requested)
{
	if (requested < 1)
		return 1;

	return requested > kMaxSegments ? kMaxSegments : requested;
}

// The meshes the game lays a cut-out over, ported from the reference renderer. A plane is the
// common case; the rest are the rings, arcs, spheres and cones that twelve of the cast use.
void BuildFaces(const Shape& shape, std::vector<Face>& out)
{
	const int n = Segments(shape.vertexCount);
	const int m = Segments(shape.vertexCount2);

	if (shape.type == 3 || shape.type == 4)
	{
		const float delta = kPi * shape.length / 5000.0f / n;

		for (int i = 0; i < n; ++i)
		{
			Face face = {};
			face.u0 = 0.0f;
			face.u1 = 1.0f;
			face.v0 = static_cast<float>(i) / n;
			face.v1 = static_cast<float>(i + 1) / n;

			const int cornerX[4] = { 0, 1, 1, 0 };
			const int cornerY[4] = { 0, 0, 1, 1 };

			for (int c = 0; c < 4; ++c)
			{
				const int k = i + cornerY[c];
				const float angle = delta * k;

				const bool closes = k == n && shape.length == 10000;
				const float taper = closes ? 0.0f : static_cast<float>(shape.dRadius) * k / n;

				const float radius = shape.type == 3
					? shape.radius - taper
					: shape.radius - (1 - cornerX[c]) * shape.width - taper;

				face.p[c][0] = radius * sinf(angle);
				face.p[c][1] = -radius * cosf(angle);
				face.p[c][2] = shape.type == 3
					? shape.width * -(cornerX[c] * 2 - 1) - static_cast<float>(shape.dz) * k / n
					: -static_cast<float>(shape.dz) * k / n;
			}

			out.push_back(face);
		}

		return;
	}

	if (shape.type == 5)
	{
		const float delta = kPi * shape.length / 5000.0f / n;
		const float delta2 = kPi * shape.length2 / 10000.0f / m;

		for (int i = 0; i < n; ++i)
		{
			for (int j = 0; j < m; ++j)
			{
				Face face = {};
				face.u0 = static_cast<float>(i) / n;
				face.u1 = static_cast<float>(i + 1) / n;
				face.v0 = static_cast<float>(j) / m;
				face.v1 = static_cast<float>(j + 1) / m;

				const int cornerX[4] = { 0, 1, 1, 0 };
				const int cornerY[4] = { 0, 0, 1, 1 };

				for (int c = 0; c < 4; ++c)
				{
					const float a = delta * (i + cornerY[c]);
					const float b = delta2 * (j + cornerX[c]);

					face.p[c][0] = shape.radius * sinf(b) * sinf(a);
					face.p[c][1] = shape.radius * sinf(b) * cosf(a);
					face.p[c][2] = shape.radius * cosf(b);
				}

				out.push_back(face);
			}
		}

		return;
	}

	if (shape.type == 6)
	{
		const float delta = kPi * shape.length / 5000.0f / n;
		const float width = static_cast<float>(shape.radius) / m;

		for (int i = 0; i < n; ++i)
		{
			for (int j = 0; j < m; ++j)
			{
				Face face = {};
				face.u0 = static_cast<float>(i) / n;
				face.u1 = static_cast<float>(i + 1) / n;
				face.v0 = static_cast<float>(m - j) / m;
				face.v1 = static_cast<float>(m - j - 1) / m;

				const int cornerX[4] = { 0, 1, 1, 0 };
				const int cornerY[4] = { 0, 0, 1, 1 };

				for (int c = 0; c < 4; ++c)
				{
					const float a = delta * (i + cornerY[c]);
					const float reach = width * (m - 1 - j + cornerX[c]);

					face.p[c][0] = -reach * sinf(a);
					face.p[c][1] = -reach * cosf(a);
					face.p[c][2] = -static_cast<float>(shape.dz) * (j + 1 - cornerX[c]) / m;
				}

				out.push_back(face);
			}
		}

		return;
	}

	out.push_back(Face{});
}

const uint8_t* BuildPatch(int id, const CutOut& cut, const Atlas& atlas, int& outWidth,
	int& outHeight)
{
	const auto found = g_patches.find(id);
	if (found != g_patches.end())
	{
		outWidth = found->second.width;
		outHeight = found->second.height;
		return found->second.pixels.data();
	}

	// UV is normalised against 256 on each axis independently, so the two scales are the atlas's
	// own dimensions - not its width twice. Seven of the cast ship a 2048x1024 or 1024x2048 sheet,
	// and using one scale for both stretches every cut-out they own to double or half height.
	const float scaleX = atlas.width / kUvReference;
	const float scaleY = atlas.height / kUvReference;

	const int u = static_cast<int>(cut.uv[0] * scaleX);
	const int v = static_cast<int>(cut.uv[1] * scaleY);
	const int width = static_cast<int>(cut.uv[2] * scaleX);
	const int height = static_cast<int>(cut.uv[3] * scaleY);

	// A desynced parse would ask for a nonsense rect rather than fail outright, so bound it.
	if (width <= 0 || height <= 0 || width > atlas.width || height > atlas.height)
		return nullptr;

	Patch patch;
	patch.width = width;
	patch.height = height;
	patch.pixels.assign(static_cast<size_t>(width) * height * 4, 0);

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			Texel(atlas, u + x, v + y,
				patch.pixels.data() + (static_cast<size_t>(y) * width + x) * 4);
		}
	}

	Patch& stored = g_patches[id];
	stored = std::move(patch);

	outWidth = stored.width;
	outHeight = stored.height;
	return stored.pixels.data();
}

void BuildQuads(int group)
{
	const auto found = g_groups.find(group);
	if (found == g_groups.end())
		return;

	std::vector<Property> properties = found->second;

	// A higher priority draws first, so it ends up behind.
	std::stable_sort(properties.begin(), properties.end(),
		[](const Property& a, const Property& b) { return a.priority > b.priority; });

	std::vector<PatParts::Quad> quads;
	std::vector<Face> faces;

	for (const Property& property : properties)
	{
		const auto cut = g_cutOuts.find(property.cutOut);
		if (cut == g_cutOuts.end())
			continue;

		// A cut-out belongs to this pose only if it reads an atlas the file carries; the rest
		// point at the character's own sprite sheets, which the sprite layers already draw. A
		// cut-out with no colour slot is still drawn - untinted, which is what the white cores
		// of the bursts are.
		const auto atlas = g_atlases.find(cut->second.texture);
		if (atlas == g_atlases.end())
			continue;

		int width = 0;
		int height = 0;
		const uint8_t* const pixels =
			BuildPatch(property.cutOut, cut->second, atlas->second, width, height);
		if (pixels == nullptr)
			continue;

		const Shape& shape = cut->second.shape >= 0
			&& cut->second.shape < static_cast<int>(g_shapes.size())
			? g_shapes[cut->second.shape] : Shape{};

		faces.clear();
		BuildFaces(shape, faces);

		const float left = static_cast<float>(-cut->second.pivot[0]);
		const float top = static_cast<float>(-cut->second.pivot[1]);
		const float right = left + cut->second.size[0];
		const float bottom = top + cut->second.size[1];

		const float planeX[4] = { left, right, right, left };
		const float planeY[4] = { top, top, bottom, bottom };

		const bool plane = shape.type != 3 && shape.type != 4 && shape.type != 5
			&& shape.type != 6;

		for (const Face& face : faces)
		{
			PatParts::Quad quad = {};
			quad.pixels = pixels;
			quad.width = width;
			quad.height = height;
			quad.colour = cut->second.colour;
			quad.additive = property.additive;
			quad.u0 = plane ? 0.0f : face.u0;
			quad.v0 = plane ? 0.0f : face.v0;
			quad.u1 = plane ? 1.0f : face.u1;
			quad.v1 = plane ? 1.0f : face.v1;

			memcpy(quad.multiply, property.multiply, sizeof(quad.multiply));
			memcpy(quad.add, property.add, sizeof(quad.add));

			for (int i = 0; i < 4; ++i)
			{
				float x = (plane ? planeX[i] : face.p[i][0]) * property.scaleX;
				float y = (plane ? planeY[i] : face.p[i][1]) * property.scaleY;
				float z = plane ? 0.0f : face.p[i][2];

				float angle = property.rotation[2] * kTau;
				float sn = sinf(angle);
				float cs = cosf(angle);
				const float rx = x * cs - y * sn;
				y = x * sn + y * cs;
				x = rx;

				angle = -property.rotation[0] * kTau;
				sn = sinf(angle);
				cs = cosf(angle);
				const float ry = y * cs - z * sn;
				z = y * sn + z * cs;
				y = ry;

				angle = -property.rotation[1] * kTau;
				sn = sinf(angle);
				cs = cosf(angle);
				const float rz = x * cs + z * sn;
				z = -x * sn + z * cs;
				x = rz;

				x += property.x;
				y += property.y;

				const float divide = kCameraDistance - z;
				const float k = divide != 0.0f ? kCameraDistance / divide : 1.0f;

				quad.x[i] = x * k;
				quad.y[i] = y * k;
			}

			quads.push_back(quad);
		}
	}

	g_quads[group] = std::move(quads);
}

}

bool PatParts::Load(int chara)
{
	if (chara == g_chara)
		return !g_groups.empty();

	g_file.clear();
	g_cutOuts.clear();
	g_groups.clear();
	g_patches.clear();
	g_quads.clear();
	g_atlases.clear();
	g_shapes.clear();

	g_chara = chara;

	if (chara < 0 || chara >= CharaTables::GetCharaCount())
		return false;

	char file[32] = {};
	sprintf_s(file, "edit_chr%03d.pat", chara);

	if (!DataArchive::Read(kFolder, file, g_file)
		|| g_file.size() < kBodyStart
		|| memcmp(g_file.data(), kMagic, kMagicBytes) != 0)
	{
		LOG("coloredit: %s could not be read", file);
		g_file.clear();
		return false;
	}

	Parse();

	const bool ready = !g_atlases.empty() && !g_groups.empty();

	// Everything downstream reads the parsed tables and the decoded atlases, never the file again.
	g_file.clear();
	g_file.shrink_to_fit();

	if (!ready)
	{
		LOG("coloredit: %s parsed to %d atlases and %d groups", file,
			static_cast<int>(g_atlases.size()), static_cast<int>(g_groups.size()));
		g_groups.clear();
		return false;
	}

	return true;
}

int PatParts::GetQuadCount(int group)
{
	if (g_quads.find(group) == g_quads.end())
		BuildQuads(group);

	const auto found = g_quads.find(group);
	return found == g_quads.end() ? 0 : static_cast<int>(found->second.size());
}

const PatParts::Quad* PatParts::GetQuads(int group)
{
	if (GetQuadCount(group) == 0)
		return nullptr;

	return g_quads[group].data();
}

bool PatParts::GetBounds(int group, float& outX0, float& outY0, float& outX1, float& outY1)
{
	const int count = GetQuadCount(group);
	if (count == 0)
		return false;

	const Quad* const quads = GetQuads(group);

	outX0 = outX1 = quads[0].x[0];
	outY0 = outY1 = quads[0].y[0];

	for (int i = 0; i < count; ++i)
	{
		for (int k = 0; k < 4; ++k)
		{
			outX0 = quads[i].x[k] < outX0 ? quads[i].x[k] : outX0;
			outY0 = quads[i].y[k] < outY0 ? quads[i].y[k] : outY0;
			outX1 = quads[i].x[k] > outX1 ? quads[i].x[k] : outX1;
			outY1 = quads[i].y[k] > outY1 ? quads[i].y[k] : outY1;
		}
	}

	return true;
}
