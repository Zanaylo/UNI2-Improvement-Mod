#include "D3D9/Device/SampledStages.h"

#include "D3D9/Device/ShaderSamplers.h"

#include <vector>

namespace {

constexpr int kMaxShaders = 128;
constexpr unsigned kMaxStages = 8;

struct KnownShader
{
	const void* shader;
	uint32_t stages;
};

KnownShader g_known[kMaxShaders] = {};
int g_knownCount = 0;
int g_nextEvicted = 0;

uint32_t ReadDeclared(IDirect3DPixelShader9* shader)
{
	UINT bytes = 0;

	if (FAILED(shader->GetFunction(nullptr, &bytes)) || bytes < sizeof(uint32_t))
		return ShaderSamplers::kEveryStage;

	std::vector<uint32_t> function(bytes / sizeof(uint32_t));

	if (FAILED(shader->GetFunction(function.data(), &bytes)))
		return ShaderSamplers::kEveryStage;

	return ShaderSamplers::Declared(function.data(), function.size());
}

KnownShader& Slot()
{
	if (g_knownCount < kMaxShaders)
		return g_known[g_knownCount++];

	KnownShader& evicted = g_known[g_nextEvicted];
	g_nextEvicted = (g_nextEvicted + 1) % kMaxShaders;
	return evicted;
}

uint32_t Declared(IDirect3DPixelShader9* shader)
{
	for (int i = 0; i < g_knownCount; ++i)
	{
		if (g_known[i].shader == shader)
			return g_known[i].stages;
	}

	KnownShader& known = Slot();
	known = { shader, ReadDeclared(shader) };
	return known.stages;
}

uint32_t FixedFunction(IDirect3DDevice9* device, unsigned stages)
{
	uint32_t colourOps[kMaxStages] = {};
	const unsigned count = stages < kMaxStages ? stages : kMaxStages;

	for (unsigned stage = 0; stage < count; ++stage)
	{
		DWORD op = D3DTOP_DISABLE;
		device->GetTextureStageState(stage, D3DTSS_COLOROP, &op);
		colourOps[stage] = op;
	}

	return ShaderSamplers::Enabled(colourOps, count);
}

}

uint32_t SampledStages::Of(IDirect3DDevice9* device, unsigned stages)
{
	IDirect3DPixelShader9* shader = nullptr;

	if (FAILED(device->GetPixelShader(&shader)) || shader == nullptr)
		return FixedFunction(device, stages);

	shader->Release();
	return Declared(shader);
}
