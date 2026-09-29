#include "Game/Stages/Bbtag/BbtagParticle.h"

#include "Game/Stages/Bbtag/BbtagArt.h"
#include "Game/Stages/Bbtag/BbtagPac.h"

#include <cstring>

namespace {

constexpr uint32_t kVersion = 0x102;
constexpr size_t kHeader = 16;
constexpr size_t kName = 32;
constexpr size_t kEntry = 0x48;
constexpr size_t kSprite = 0xb0;
constexpr size_t kShape = 0x38;
constexpr size_t kMove = 0xa0;
constexpr size_t kDdsPixels = 128;
constexpr size_t kDdsBitsAt = 88;
constexpr size_t kDdsMasksAt = 92;
constexpr uint32_t kDdsBits = 32;
constexpr uint32_t kDdsMasks[4] = { 0x00ff0000u, 0x0000ff00u, 0x000000ffu, 0xff000000u };

class Reader
{
public:
	Reader(const std::vector<uint8_t>& blob, size_t base) : m_blob(blob), m_base(base) {}

	bool Fits(size_t size) const { return m_base + size <= m_blob.size(); }

	uint32_t Dword(size_t at) const
	{
		uint32_t value = 0;
		memcpy(&value, m_blob.data() + m_base + at, 4);

		return value;
	}

	int Int(size_t at) const { return static_cast<int32_t>(Dword(at)); }

	double Float(size_t at) const
	{
		float value = 0.0f;
		memcpy(&value, m_blob.data() + m_base + at, 4);

		return value;
	}

	BbtagParticle::Range Pair(size_t maxAt, size_t minAt) const
	{
		return { Float(minAt), Float(maxAt) };
	}

private:
	const std::vector<uint8_t>& m_blob;
	size_t m_base;
};

bool Counted(const std::vector<uint8_t>& blob, uint32_t& count)
{
	if (blob.size() < kHeader || memcmp(blob.data(), "LTP ", 4) != 0)
		return false;

	const Reader header(blob, 0);

	if (header.Dword(4) != kVersion)
		return false;

	count = header.Dword(8);
	return true;
}

bool Names(const std::vector<uint8_t>& blob, std::vector<std::string>& out)
{
	uint32_t count = 0;

	if (!Counted(blob, count) || kHeader + count * kName > blob.size())
		return false;

	for (uint32_t i = 0; i < count; ++i)
	{
		const char* const text = reinterpret_cast<const char*>(blob.data() + kHeader + i * kName);
		out.push_back(std::string(text, strnlen(text, kName)));
	}

	return true;
}

BbtagParticle::Size SizeAt(const Reader& at, size_t base)
{
	return { at.Pair(base, base + 8), at.Pair(base + 4, base + 12) };
}

BbtagParticle::Sprite SpriteAt(const Reader& at)
{
	BbtagParticle::Sprite out = {};
	out.flags = at.Dword(0x00);
	out.flags2 = at.Dword(0x04);
	out.rotationStart = at.Pair(0x10, 0x14);
	out.rotationSpeed = at.Pair(0x18, 0x1c);
	out.rotationAccel = at.Pair(0x20, 0x24);
	out.rotationDamping = at.Float(0x28);
	out.colourPeriodMax = at.Int(0x3c);
	out.colourPeriodMin = at.Int(0x40);
	out.colourStart = at.Dword(0x44);
	out.colourEnd = at.Dword(0x48);
	out.colourMid = at.Dword(0x4c);
	out.colourMidAt = at.Float(0x50);
	out.scalePeriodMax = at.Int(0x54);
	out.scalePeriodMin = at.Int(0x58);
	out.scaleStart = SizeAt(at, 0x5c);
	out.scaleMid = SizeAt(at, 0x6c);
	out.scaleEnd = SizeAt(at, 0x7c);
	out.scaleMidAt = at.Float(0x8c);
	out.uvanime = at.Int(0x94);

	for (int i = 0; i < 4; ++i)
		out.uv[i] = at.Float(0x98 + i * 4);

	out.sharedAtlas = at.Int(0xac);
	return out;
}

BbtagParticle::Shape ShapeAt(const Reader& at)
{
	BbtagParticle::Shape out = {};
	out.flags = at.Dword(0x00);
	out.radius = at.Float(0x08);
	out.angleRange = at.Float(0x0c);
	out.angleOffset = at.Float(0x10);

	for (int axis = 0; axis < 3; ++axis)
	{
		out.boxMin[axis] = at.Float(0x14 + axis * 4);
		out.boxMax[axis] = at.Float(0x20 + axis * 4);
	}

	out.sizeScale = at.Float(0x30);
	return out;
}

BbtagParticle::Move MoveAt(const Reader& at)
{
	BbtagParticle::Move out = {};
	out.flags = at.Dword(0x00);

	for (int axis = 0; axis < 3; ++axis)
	{
		out.velocity[axis] = at.Pair(0x08 + axis * 4, 0x14 + axis * 4);
		out.accel[axis] = at.Pair(0x40 + axis * 4, 0x4c + axis * 4);
	}

	out.damping = at.Float(0x58);
	return out;
}

int EntryIndex(const std::vector<uint8_t>& blob, uint32_t offset)
{
	if (offset < kHeader || (offset - kHeader) % kEntry != 0 || offset + kEntry > blob.size())
		return -1;

	return static_cast<int>((offset - kHeader) / kEntry);
}

bool EffectAt(const std::vector<uint8_t>& blob, uint32_t index, BbtagParticle::Effect& out)
{
	const Reader entry(blob, kHeader + index * kEntry);

	if (!entry.Fits(kEntry))
		return false;

	out.group.flags = entry.Dword(0x00);
	out.group.countMax = entry.Int(0x08);
	out.group.countMin = entry.Int(0x0c);
	out.group.lifeMax = entry.Int(0x10);
	out.group.lifeMin = entry.Int(0x14);
	out.group.delayMax = entry.Int(0x18);
	out.group.delayMin = entry.Int(0x1c);
	out.group.childStart = EntryIndex(blob, entry.Dword(0x38));

	const Reader sprite(blob, entry.Dword(0x2c));
	const Reader shape(blob, entry.Dword(0x30));
	const Reader move(blob, entry.Dword(0x34));

	if (!sprite.Fits(kSprite) || !shape.Fits(kShape) || !move.Fits(kMove))
		return false;

	out.sprite = SpriteAt(sprite);
	out.shape = ShapeAt(shape);
	out.move = MoveAt(move);
	return true;
}

}

bool BbtagParticle::Read(const std::vector<uint8_t>& pac, std::vector<Effect>& out)
{
	out.clear();

	BbtagPac::Files files;

	if (!BbtagPac::Walk(pac, files))
		return false;

	const std::vector<uint8_t>* const table = BbtagPac::Named(files, "particle.bin");
	const std::vector<uint8_t>* const named = BbtagPac::Named(files, "ptlname.bin");

	if (table == nullptr || named == nullptr)
		return false;

	std::vector<std::string> names;
	uint32_t count = 0;

	if (!Names(*named, names) || !Counted(*table, count) || count != names.size())
		return false;

	for (uint32_t i = 0; i < count; ++i)
	{
		Effect effect = {};
		effect.name = names[i];

		if (!EffectAt(*table, i, effect))
			return false;

		out.push_back(effect);
	}

	return true;
}

bool BbtagParticle::Atlas(const std::vector<uint8_t>& pac, Surface& out)
{
	out = Surface();

	BbtagPac::Files files;

	if (!BbtagPac::Walk(pac, files))
		return false;

	const std::vector<uint8_t>* const dds = BbtagPac::Named(files, "particle.dds");
	BbtagArt::Size size = {};

	if (dds == nullptr || !BbtagArt::Measure(*dds, size))
		return false;

	const Reader header(*dds, 0);

	if (header.Dword(kDdsBitsAt) != kDdsBits)
		return false;

	for (int i = 0; i < 4; ++i)
	{
		if (header.Dword(kDdsMasksAt + i * 4) != kDdsMasks[i])
			return false;
	}

	const size_t bytes = static_cast<size_t>(size.width) * size.height * 4;

	if (kDdsPixels + bytes > dds->size())
		return false;

	out.width = size.width;
	out.height = size.height;
	out.bgra.assign(dds->begin() + kDdsPixels, dds->begin() + kDdsPixels + bytes);
	return true;
}

const BbtagParticle::Effect* BbtagParticle::Find(const std::vector<Effect>& effects,
	const std::string& name)
{
	for (const Effect& effect : effects)
	{
		if (effect.name == name)
			return &effect;
	}

	return nullptr;
}
