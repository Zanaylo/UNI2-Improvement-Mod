#include "Game/FbxExLocal.h"

#include <cmath>
#include <cstring>

namespace {

constexpr size_t kHeaderBytes = 16;
constexpr size_t kBlockHeaderBytes = 8;
constexpr size_t kBlockCount = 4;
constexpr size_t kNodeBlock = 2;
constexpr size_t kAnimBlock = 3;
constexpr size_t kRecordHeaderBytes = 16;
constexpr size_t kWorldOffset = kRecordHeaderBytes + 8;
constexpr size_t kMatrixFloats = 16;
constexpr size_t kMatrixBytes = kMatrixFloats * sizeof(float);
constexpr size_t kFrameCountBytes = 4;
constexpr int kMeshType = 1;
constexpr float kEpsilon = 0.001f;
constexpr float kSingular = 1e-12f;

struct Matrix
{
	float m[kMatrixFloats];
};

struct Node
{
	int child;
	int sibling;
	int parent;
	bool meshed;
	Matrix world;
};

struct Track
{
	size_t at;
	int frames;
};

struct Block
{
	size_t at;
	size_t end;
	int count;
};

int ReadInt(const std::vector<uint8_t>& data, size_t at)
{
	int value = 0;
	memcpy(&value, data.data() + at, sizeof(value));

	return value;
}

uint32_t ReadDword(const std::vector<uint8_t>& data, size_t at)
{
	uint32_t value = 0;
	memcpy(&value, data.data() + at, sizeof(value));

	return value;
}

Matrix ReadMatrix(const std::vector<uint8_t>& data, size_t at)
{
	Matrix out = {};
	memcpy(out.m, data.data() + at, kMatrixBytes);

	return out;
}

void WriteMatrix(std::vector<uint8_t>& data, size_t at, const Matrix& value)
{
	memcpy(data.data() + at, value.m, kMatrixBytes);
}

Matrix Multiply(const Matrix& left, const Matrix& right)
{
	Matrix out = {};

	for (size_t column = 0; column < 4; ++column)
	{
		for (size_t row = 0; row < 4; ++row)
		{
			out.m[column * 4 + row] =
				left.m[row] * right.m[column * 4] +
				left.m[4 + row] * right.m[column * 4 + 1] +
				left.m[8 + row] * right.m[column * 4 + 2] +
				left.m[12 + row] * right.m[column * 4 + 3];
		}
	}

	return out;
}

bool Inverse(const Matrix& value, Matrix& out)
{
	const float a[3][3] = {
		{ value.m[0], value.m[4], value.m[8] },
		{ value.m[1], value.m[5], value.m[9] },
		{ value.m[2], value.m[6], value.m[10] },
	};

	const float determinant =
		a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) -
		a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
		a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);

	if (fabsf(determinant) < kSingular)
		return false;

	float b[3][3] = {};

	b[0][0] = (a[1][1] * a[2][2] - a[1][2] * a[2][1]) / determinant;
	b[0][1] = (a[0][2] * a[2][1] - a[0][1] * a[2][2]) / determinant;
	b[0][2] = (a[0][1] * a[1][2] - a[0][2] * a[1][1]) / determinant;
	b[1][0] = (a[1][2] * a[2][0] - a[1][0] * a[2][2]) / determinant;
	b[1][1] = (a[0][0] * a[2][2] - a[0][2] * a[2][0]) / determinant;
	b[1][2] = (a[0][2] * a[1][0] - a[0][0] * a[1][2]) / determinant;
	b[2][0] = (a[1][0] * a[2][1] - a[1][1] * a[2][0]) / determinant;
	b[2][1] = (a[0][1] * a[2][0] - a[0][0] * a[2][1]) / determinant;
	b[2][2] = (a[0][0] * a[1][1] - a[0][1] * a[1][0]) / determinant;

	out = Matrix();

	for (size_t column = 0; column < 3; ++column)
	{
		for (size_t row = 0; row < 3; ++row)
			out.m[column * 4 + row] = b[row][column];
	}

	for (size_t row = 0; row < 3; ++row)
	{
		out.m[12 + row] = -(b[row][0] * value.m[12] + b[row][1] * value.m[13] +
			b[row][2] * value.m[14]);
	}

	out.m[15] = 1.0f;

	return true;
}

bool Same(const Matrix& left, const Matrix& right)
{
	for (size_t i = 0; i < kMatrixFloats; ++i)
	{
		if (fabsf(left.m[i] - right.m[i]) > kEpsilon)
			return false;
	}

	return true;
}

bool Blocks(const std::vector<uint8_t>& data, Block* out)
{
	if (data.size() < kHeaderBytes || memcmp(data.data(), "fbxex", 5) != 0)
		return false;

	size_t at = kHeaderBytes;

	for (size_t i = 0; i < kBlockCount; ++i)
	{
		if (at + kBlockHeaderBytes > data.size())
			return false;

		const uint32_t size = ReadDword(data, at);

		if (size < kBlockHeaderBytes || size > data.size() - at)
			return false;

		out[i].at = at;
		out[i].end = at + size;
		out[i].count = ReadInt(data, at + 4);
		at += size;
	}

	return true;
}

bool TakeNodes(const std::vector<uint8_t>& data, const Block& block, std::vector<Node>& out)
{
	if (block.count <= 0)
		return false;

	size_t at = block.at + kBlockHeaderBytes;

	for (int i = 0; i < block.count; ++i)
	{
		if (at + kRecordHeaderBytes > block.end)
			return false;

		const int size = ReadInt(data, at);

		if (size < static_cast<int>(kRecordHeaderBytes) ||
			static_cast<size_t>(size) > block.end - at)
		{
			return false;
		}

		Node node = {};
		node.parent = -1;
		node.child = ReadInt(data, at + 8);
		node.sibling = ReadInt(data, at + 12);
		node.meshed = ReadInt(data, at + 4) == kMeshType;

		if (node.meshed)
		{
			if (static_cast<size_t>(size) < kWorldOffset + kMatrixBytes)
				return false;

			node.world = ReadMatrix(data, at + kWorldOffset);
		}

		out.push_back(node);
		at += static_cast<size_t>(size);
	}

	return true;
}

bool TakeParents(std::vector<Node>& nodes)
{
	for (size_t i = 0; i < nodes.size(); ++i)
	{
		for (int child = nodes[i].child; child >= 0; child = nodes[child].sibling)
		{
			if (static_cast<size_t>(child) >= nodes.size() || nodes[child].parent >= 0)
				return false;

			nodes[child].parent = static_cast<int>(i);
		}
	}

	return true;
}

bool TakeTracks(const std::vector<uint8_t>& data, const Block& block, size_t nodes,
	std::vector<Track>& out)
{
	if (block.count < 0 || static_cast<size_t>(block.count) != nodes)
		return false;

	size_t at = block.at + kBlockHeaderBytes;

	for (int i = 0; i < block.count; ++i)
	{
		if (at + kFrameCountBytes > block.end)
			return false;

		const int frames = ReadInt(data, at);
		at += kFrameCountBytes;

		if (frames <= 0 || static_cast<size_t>(frames) > (block.end - at) / kMatrixBytes)
			return false;

		const Track track = { at, frames };
		out.push_back(track);
		at += static_cast<size_t>(frames) * kMatrixBytes;
	}

	return true;
}

bool Parse(const std::vector<uint8_t>& data, std::vector<Node>& nodes, std::vector<Track>& tracks)
{
	Block blocks[kBlockCount] = {};

	if (!Blocks(data, blocks))
		return false;

	if (!TakeNodes(data, blocks[kNodeBlock], nodes) || !TakeParents(nodes))
		return false;

	return TakeTracks(data, blocks[kAnimBlock], nodes.size(), tracks);
}

bool Baked(const std::vector<uint8_t>& data, const std::vector<Node>& nodes,
	const std::vector<Track>& tracks)
{
	int meshes = 0;

	for (size_t i = 0; i < nodes.size(); ++i)
	{
		if (!nodes[i].meshed)
			continue;

		++meshes;

		if (!Same(ReadMatrix(data, tracks[i].at), nodes[i].world))
			return false;
	}

	return meshes != 0;
}

void Rebase(std::vector<uint8_t>& data, const std::vector<Node>& nodes,
	const std::vector<Track>& tracks, FbxExLocal::Report& report)
{
	const std::vector<uint8_t> source(data);

	for (size_t i = 0; i < nodes.size(); ++i)
	{
		const int parent = nodes[i].parent;

		if (parent <= 0)
			continue;

		const Track& track = tracks[i];
		const Track& above = tracks[parent];
		bool moved = false;

		for (int frame = 0; frame < track.frames; ++frame)
		{
			const int taken = frame < above.frames ? frame : above.frames - 1;
			Matrix inverse = {};

			if (!Inverse(ReadMatrix(source, above.at + static_cast<size_t>(taken) * kMatrixBytes),
				inverse))
			{
				continue;
			}

			const size_t at = track.at + static_cast<size_t>(frame) * kMatrixBytes;
			const Matrix world = ReadMatrix(source, at);
			const Matrix local = Multiply(inverse, world);

			if (Same(local, world))
				continue;

			WriteMatrix(data, at, local);
			++report.frames;
			moved = true;
		}

		if (moved)
			++report.nodes;
	}
}

}

bool FbxExLocal::Apply(std::vector<uint8_t>& data, Report& report)
{
	report = Report();

	std::vector<Node> nodes;
	std::vector<Track> tracks;

	if (!Parse(data, nodes, tracks))
		return false;

	if (!Baked(data, nodes, tracks))
		return false;

	Rebase(data, nodes, tracks, report);

	return true;
}
