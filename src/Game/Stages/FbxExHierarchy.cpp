#include "Game/Stages/FbxExHierarchy.h"

#include "Game/Stages/Bbtag/BbtagMua.h"

#include <cmath>
#include <cstring>

namespace {

constexpr int kNone = -1;
constexpr int kMatrix = FbxExWriter::kMatrixFloats;
constexpr float kStill = 1e-6f;

bool Moves(const std::vector<float>& track)
{
	for (size_t at = kMatrix; at + kMatrix <= track.size(); at += kMatrix)
	{
		for (int k = 0; k < kMatrix; ++k)
		{
			if (fabsf(track[at + k] - track[k]) > kStill)
				return true;
		}
	}

	return false;
}

}

FbxExHierarchy::FbxExHierarchy(const FbxExWriter::Model& model)
	: m_model(model), m_parent(model.nodes.size(), kNone), m_varies(model.nodes.size(), false)
{
	const int count = static_cast<int>(model.nodes.size());

	for (int i = 0; i < count; ++i)
	{
		int guard = 0;

		for (int child = model.nodes[i].child; child >= 0 && child < count && guard++ < count;
			child = model.nodes[child].sibling)
		{
			m_parent[child] = i;
		}
	}

	for (int i = 0; i < count && i < static_cast<int>(model.animes.size()); ++i)
		m_varies[i] = Moves(model.animes[i]);
}

int FbxExHierarchy::Parent(int node) const
{
	return node >= 0 && node < static_cast<int>(m_parent.size()) ? m_parent[node] : kNone;
}

int FbxExHierarchy::Frames(int node) const
{
	if (node < 0 || node >= static_cast<int>(m_model.animes.size()))
		return 0;

	return static_cast<int>(m_model.animes[node].size() / kMatrix);
}

bool FbxExHierarchy::Varies(int node) const
{
	return node >= 0 && node < static_cast<int>(m_varies.size()) && m_varies[node];
}

void FbxExHierarchy::Local(int node, int frame, float out[16]) const
{
	const int frames = Frames(node);

	if (frames == 0)
	{
		BbtagMua::Identity(out);
		return;
	}

	memcpy(out, m_model.animes[node].data() + (frame % frames) * kMatrix, sizeof(float) * kMatrix);
}

void FbxExHierarchy::World(int node, int frame, float out[16]) const
{
	Local(node, frame, out);

	int guard = 0;

	for (int parent = Parent(node); parent >= 0 && guard++ < static_cast<int>(m_parent.size());
		parent = Parent(parent))
	{
		float local[16] = {};
		Local(parent, frame, local);

		float composed[16] = {};
		BbtagMua::Multiply(out, local, composed);
		memcpy(out, composed, sizeof(composed));
	}
}
