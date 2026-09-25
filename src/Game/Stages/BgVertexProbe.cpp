#include "Game/Stages/BgVertexProbe.h"

#include "Core/logger.h"

#include <Windows.h>
#include <d3d9.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr long kReports = 6;
constexpr int kElements = 64;

const char* const kUsage[] = {
	"position", "blendweight", "blendindices", "normal", "psize", "texcoord", "tangent",
	"binormal", "tessfactor", "positiont", "colour", "fog", "depth", "sample",
};

const char* const kType[] = {
	"float1", "float2", "float3", "float4", "d3dcolor", "ubyte4", "short2", "short4",
	"ubyte4n", "short2n", "short4n", "ushort2n", "ushort4n", "udec3", "dec3n", "float16_2",
	"float16_4", "unused",
};

volatile long g_armed = 0;
volatile long g_left = kReports;

const char* Named(const char* const table[], size_t count, unsigned index)
{
	return index < count ? table[index] : "?";
}

void Elements(IDirect3DDevice9* device)
{
	IDirect3DVertexDeclaration9* declaration = nullptr;

	if (FAILED(device->GetVertexDeclaration(&declaration)) || declaration == nullptr)
	{
		DWORD fvf = 0;
		device->GetFVF(&fvf);
		LOG("BgVertexProbe: the draw takes no declaration, FVF 0x%08x", fvf);
		return;
	}

	D3DVERTEXELEMENT9 element[kElements] = {};
	UINT count = 0;
	
	if (SUCCEEDED(declaration->GetDeclaration(element, &count)))
	{
		char text[512] = {};

		for (UINT i = 0; i + 1 < count && strlen(text) < sizeof(text) - 48; ++i)
		{
			char one[64] = {};
			sprintf_s(one, "%s%s:%s@%u", i == 0 ? "" : ", ",
				Named(kUsage, sizeof(kUsage) / sizeof(kUsage[0]), element[i].Usage),
				Named(kType, sizeof(kType) / sizeof(kType[0]), element[i].Type),
				element[i].Offset);
			strcat_s(text, one);
		}

		LOG("BgVertexProbe: the stage vertex is %s", text);
	}

	declaration->Release();
}

void Blending(IDirect3DDevice9* device)
{
	DWORD enabled = 0;
	DWORD source = 0;
	DWORD destination = 0;

	device->GetRenderState(D3DRS_ALPHABLENDENABLE, &enabled);
	device->GetRenderState(D3DRS_SRCBLEND, &source);
	device->GetRenderState(D3DRS_DESTBLEND, &destination);

	LOG("BgVertexProbe: blend %s, source %u, destination %u (1 zero, 2 one, 5 srcalpha)",
		enabled ? "on" : "off", source, destination);
}

}

void BgVertexProbe::Arm()
{
	if (InterlockedCompareExchange(&g_left, 0, 0) > 0)
		InterlockedExchange(&g_armed, 1);
}

void BgVertexProbe::OnDraw(IDirect3DDevice9* device)
{
	if (InterlockedCompareExchange(&g_armed, 0, 0) == 0 || device == nullptr)
		return;

	InterlockedExchange(&g_armed, 0);

	if (InterlockedDecrement(&g_left) < 0)
		return;

	Elements(device);
	Blending(device);
}
