#include "D3D9/Device/ScreenSpaceShader.h"

#include "Core/logger.h"
#include "D3D9/Post/Shaders/ScreenSpaceShader.h"
#include "D3D9/Post/Shaders/ScreenSpaceShader3.h"

#include <vector>

namespace {

constexpr int kMaxLayouts = 16;
constexpr int kTexcoordSlots = 4;
constexpr int kElementCount = 3 + kTexcoordSlots + 1;
constexpr WORD kPositionBytes = 16;
constexpr WORD kColourBytes = 4;
constexpr WORD kPointSizeBytes = 4;
constexpr WORD kAbsentOffset = 0;
constexpr int kTexcoordSizeShift = 16;
constexpr int kTexcoordSizeBits = 2;

struct Layout
{
	DWORD fvf;
	IDirect3DVertexDeclaration9* declaration;
	float present[ScreenSpaceShader::kVectorWidth];
};

Layout g_layouts[kMaxLayouts] = {};
int g_layoutCount = 0;

enum Model
{
	Model_Two = 0,
	Model_Three = 1,
	Model_COUNT
};

constexpr int kMaxPixelShaders = 64;
constexpr DWORD kVersionMajorShift = 8;
constexpr DWORD kVersionMajorMask = 0xff;
constexpr DWORD kModelThreeMajor = 3;

const void* const kBytecode[Model_COUNT] = { kScreenSpaceShader, kScreenSpaceShader3 };

IDirect3DVertexShader9* g_shaders[Model_COUNT] = {};
bool g_shaderFailed[Model_COUNT] = {};

struct PixelShaderModel
{
	const void* shader;
	Model model;
};

PixelShaderModel g_pixelShaders[kMaxPixelShaders] = {};
int g_pixelShaderCount = 0;

Model ReadModel(IDirect3DPixelShader9* shader)
{
	UINT size = 0;
	if (FAILED(shader->GetFunction(nullptr, &size)) || size < sizeof(DWORD))
		return Model_Two;

	std::vector<DWORD> function(size / sizeof(DWORD) + 1);
	if (FAILED(shader->GetFunction(function.data(), &size)))
		return Model_Two;

	return ((function[0] >> kVersionMajorShift) & kVersionMajorMask) >= kModelThreeMajor ? Model_Three : Model_Two;
}

Model BoundModel(IDirect3DDevice9* device)
{
	IDirect3DPixelShader9* shader = nullptr;
	if (FAILED(device->GetPixelShader(&shader)) || shader == nullptr)
		return Model_Two;

	shader->Release();

	for (int i = 0; i < g_pixelShaderCount; ++i)
	{
		if (g_pixelShaders[i].shader == shader)
			return g_pixelShaders[i].model;
	}

	const Model model = ReadModel(shader);

	if (g_pixelShaderCount < kMaxPixelShaders)
		g_pixelShaders[g_pixelShaderCount++] = { shader, model };

	return model;
}

struct Texcoord
{
	BYTE type;
	WORD bytes;
};

Texcoord TexcoordFormat(DWORD fvf, int index)
{
	switch ((fvf >> (kTexcoordSizeShift + index * kTexcoordSizeBits)) & 3)
	{
	case D3DFVF_TEXTUREFORMAT1:
		return { D3DDECLTYPE_FLOAT1, 4 };
	case D3DFVF_TEXTUREFORMAT3:
		return { D3DDECLTYPE_FLOAT3, 12 };
	case D3DFVF_TEXTUREFORMAT4:
		return { D3DDECLTYPE_FLOAT4, 16 };
	default:
		return { D3DDECLTYPE_FLOAT2, 8 };
	}
}

D3DVERTEXELEMENT9 Element(WORD offset, BYTE type, BYTE usage, BYTE usageIndex)
{
	return { 0, offset, type, D3DDECLMETHOD_DEFAULT, usage, usageIndex };
}

bool BuildElements(DWORD fvf, D3DVERTEXELEMENT9 (&elements)[kElementCount],
	float (&present)[ScreenSpaceShader::kVectorWidth])
{
	if ((fvf & D3DFVF_POSITION_MASK) != D3DFVF_XYZRHW || (fvf & D3DFVF_NORMAL) != 0)
		return false;

	WORD offset = kPositionBytes + ((fvf & D3DFVF_PSIZE) != 0 ? kPointSizeBytes : 0);
	const bool diffuse = (fvf & D3DFVF_DIFFUSE) != 0;
	const bool specular = (fvf & D3DFVF_SPECULAR) != 0;

	elements[0] = Element(0, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_POSITION, 0);
	elements[1] = Element(diffuse ? offset : kAbsentOffset, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 0);
	offset += diffuse ? kColourBytes : 0;
	elements[2] = Element(specular ? offset : kAbsentOffset, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 1);
	offset += specular ? kColourBytes : 0;

	const int texcoords = static_cast<int>((fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT);

	for (int i = 0; i < kTexcoordSlots; ++i)
	{
		const BYTE usageIndex = static_cast<BYTE>(i);

		if (i >= texcoords)
		{
			elements[3 + i] = Element(kAbsentOffset, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, usageIndex);
			continue;
		}

		const Texcoord format = TexcoordFormat(fvf, i);
		elements[3 + i] = Element(offset, format.type, D3DDECLUSAGE_TEXCOORD, usageIndex);
		offset += format.bytes;
	}

	elements[kElementCount - 1] = D3DDECL_END();

	present[0] = diffuse ? 1.0f : 0.0f;
	present[1] = specular ? 1.0f : 0.0f;
	present[2] = 0.0f;
	present[3] = 0.0f;
	return true;
}

IDirect3DVertexShader9* EnsureShader(IDirect3DDevice9* device, Model model)
{
	if (g_shaders[model] != nullptr || g_shaderFailed[model])
		return g_shaders[model];

	if (SUCCEEDED(device->CreateVertexShader(static_cast<const DWORD*>(kBytecode[model]), &g_shaders[model])))
		return g_shaders[model];

	g_shaders[model] = nullptr;
	g_shaderFailed[model] = true;
	LOG("[InternalResolution] the device refused screen space vertex shader model %d, those draws are scaled on "
		"the CPU", model == Model_Three ? 3 : 2);
	return nullptr;
}

const Layout* LayoutFor(IDirect3DDevice9* device, DWORD fvf)
{
	for (int i = 0; i < g_layoutCount; ++i)
	{
		if (g_layouts[i].fvf == fvf)
			return g_layouts[i].declaration != nullptr ? &g_layouts[i] : nullptr;
	}

	if (g_layoutCount >= kMaxLayouts)
		return nullptr;

	Layout& layout = g_layouts[g_layoutCount++];
	layout.fvf = fvf;
	layout.declaration = nullptr;

	D3DVERTEXELEMENT9 elements[kElementCount] = {};

	if (!BuildElements(fvf, elements, layout.present) ||
		FAILED(device->CreateVertexDeclaration(elements, &layout.declaration)))
	{
		layout.declaration = nullptr;
		LOG("[InternalResolution] screen space fvf 0x%lx has no shader layout, it is scaled on the CPU", fvf);
		return nullptr;
	}

	return &layout;
}

}

bool ScreenSpaceShader::ProgramFor(IDirect3DDevice9* device, DWORD fvf, const D3DVIEWPORT9& viewport, float scaleX,
	float scaleY, Program& out)
{
	if (device == nullptr || viewport.Width == 0 || viewport.Height == 0)
		return false;

	IDirect3DVertexShader9* const shader = EnsureShader(device, BoundModel(device));
	if (shader == nullptr)
		return false;

	const Layout* const layout = LayoutFor(device, fvf);
	if (layout == nullptr)
		return false;

	const float width = static_cast<float>(viewport.Width);
	const float height = static_cast<float>(viewport.Height);

	out.declaration = layout->declaration;
	out.shader = shader;
	out.constants[0][0] = 2.0f * scaleX / width;
	out.constants[0][1] = -2.0f * scaleY / height;
	out.constants[0][2] = -1.0f - 2.0f * static_cast<float>(viewport.X) / width;
	out.constants[0][3] = 1.0f + 2.0f * static_cast<float>(viewport.Y) / height;

	for (int i = 0; i < kVectorWidth; ++i)
		out.constants[1][i] = layout->present[i];

	return true;
}
