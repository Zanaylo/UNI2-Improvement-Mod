#include "D3D9/Device/PretransformedDraws.h"

#include "Core/Profiler.h"
#include "Core/logger.h"
#include "D3D9/Device/ScaledTargets.h"
#include "D3D9/Device/ScreenSpaceShader.h"
#include "D3D9/Draw/PrimitiveCount.h"
#include "Hooks/GameHook.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr int kSetVertexDeclarationIndex = 87;
constexpr int kSetFvfIndex = 89;
constexpr int kNoPosition = -1;
constexpr UINT kMaxElements = MAXD3DDECLLENGTH + 1;
constexpr BYTE kEndStream = 0xff;

using SetVertexDeclaration_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, IDirect3DVertexDeclaration9*);
using SetFvf_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD);

GameHook<SetVertexDeclaration_t> g_setVertexDeclarationHook("IDirect3DDevice9::SetVertexDeclaration");
GameHook<SetFvf_t> g_setFvfHook("IDirect3DDevice9::SetFVF");

int g_positionOffset = kNoPosition;
bool g_redrawing = false;
DWORD g_fvf = 0;
IDirect3DVertexDeclaration9* g_declaration = nullptr;

std::vector<unsigned char> g_vertices;
std::vector<unsigned char> g_indices;

long g_shaderDraws = 0;
long g_scaledDraws = 0;
long g_failedDraws = 0;
bool g_reportedFailure = false;

char g_status[128] = "off";

int PositionOffsetOf(IDirect3DVertexDeclaration9* declaration)
{
	if (declaration == nullptr)
		return kNoPosition;

	D3DVERTEXELEMENT9 elements[kMaxElements] = {};
	UINT count = kMaxElements;

	if (FAILED(declaration->GetDeclaration(elements, &count)))
		return kNoPosition;

	for (UINT i = 0; i < count && i < kMaxElements && elements[i].Stream != kEndStream; ++i)
	{
		const D3DVERTEXELEMENT9& element = elements[i];

		if (element.Stream == 0 && element.Usage == D3DDECLUSAGE_POSITIONT && element.UsageIndex == 0 &&
			element.Type == D3DDECLTYPE_FLOAT4)
		{
			return element.Offset;
		}
	}

	return kNoPosition;
}

HRESULT STDMETHODCALLTYPE HookedSetVertexDeclaration(IDirect3DDevice9* device,
	IDirect3DVertexDeclaration9* declaration)
{
	const HRESULT result = g_setVertexDeclarationHook.Original()(device, declaration);

	if (SUCCEEDED(result))
	{
		g_positionOffset = PositionOffsetOf(declaration);
		g_declaration = declaration;
	}

	return result;
}

HRESULT STDMETHODCALLTYPE HookedSetFvf(IDirect3DDevice9* device, DWORD fvf)
{
	const HRESULT result = g_setFvfHook.Original()(device, fvf);

	if (SUCCEEDED(result))
	{
		g_positionOffset = (fvf & D3DFVF_POSITION_MASK) == D3DFVF_XYZRHW ? 0 : kNoPosition;
		g_fvf = fvf;
		g_declaration = nullptr;
	}

	return result;
}

class BoundStream
{
public:
	explicit BoundStream(IDirect3DDevice9* device)
		: m_device(device)
	{
		if (FAILED(device->GetStreamSource(0, &m_buffer, &m_offset, &m_stride)))
			m_buffer = nullptr;
	}

	~BoundStream()
	{
		if (m_buffer == nullptr)
			return;

		m_device->SetStreamSource(0, m_buffer, m_offset, m_stride);
		m_buffer->Release();
	}

	BoundStream(const BoundStream&) = delete;
	BoundStream& operator=(const BoundStream&) = delete;

	bool IsUsable() const { return m_buffer != nullptr && m_stride != 0; }
	UINT Stride() const { return m_stride; }

	bool Copy(UINT firstVertex, UINT vertexCount) const
	{
		const UINT bytes = vertexCount * m_stride;
		void* data = nullptr;

		if (FAILED(m_buffer->Lock(m_offset + firstVertex * m_stride, bytes, &data, D3DLOCK_READONLY)) ||
			data == nullptr)
		{
			return false;
		}

		g_vertices.resize(bytes);
		memcpy(g_vertices.data(), data, bytes);
		m_buffer->Unlock();
		return true;
	}

private:
	IDirect3DDevice9* m_device;
	IDirect3DVertexBuffer9* m_buffer = nullptr;
	UINT m_offset = 0;
	UINT m_stride = 0;
};

class BoundIndices
{
public:
	explicit BoundIndices(IDirect3DDevice9* device)
		: m_device(device)
	{
		D3DINDEXBUFFER_DESC desc = {};

		if (FAILED(device->GetIndices(&m_buffer)) || m_buffer == nullptr || FAILED(m_buffer->GetDesc(&desc)))
			return;

		m_format = desc.Format;
		m_size = desc.Format == D3DFMT_INDEX32 ? sizeof(UINT) : sizeof(WORD);
	}

	~BoundIndices()
	{
		if (m_buffer == nullptr)
			return;

		m_device->SetIndices(m_buffer);
		m_buffer->Release();
	}

	BoundIndices(const BoundIndices&) = delete;
	BoundIndices& operator=(const BoundIndices&) = delete;

	bool IsUsable() const { return m_buffer != nullptr && m_size != 0; }
	D3DFORMAT Format() const { return m_format; }

	bool CopyRebased(UINT startIndex, UINT indexCount, UINT minVertex, UINT vertexCount) const
	{
		const UINT bytes = indexCount * m_size;
		void* data = nullptr;

		if (FAILED(m_buffer->Lock(startIndex * m_size, bytes, &data, D3DLOCK_READONLY)) || data == nullptr)
			return false;

		g_indices.resize(bytes);
		memcpy(g_indices.data(), data, bytes);
		m_buffer->Unlock();

		return m_size == sizeof(UINT) ? Rebase<UINT>(indexCount, minVertex, vertexCount)
			: Rebase<WORD>(indexCount, minVertex, vertexCount);
	}

private:
	template <typename Index>
	static bool Rebase(UINT indexCount, UINT minVertex, UINT vertexCount)
	{
		Index* const indices = reinterpret_cast<Index*>(g_indices.data());

		for (UINT i = 0; i < indexCount; ++i)
		{
			if (indices[i] < minVertex || indices[i] - minVertex >= vertexCount)
				return false;

			indices[i] = static_cast<Index>(indices[i] - minVertex);
		}

		return true;
	}

	IDirect3DDevice9* m_device;
	IDirect3DIndexBuffer9* m_buffer = nullptr;
	D3DFORMAT m_format = D3DFMT_INDEX16;
	UINT m_size = 0;
};

void CopyVertices(const void* vertices, UINT vertexCount, UINT stride)
{
	const UINT bytes = vertexCount * stride;

	g_vertices.resize(bytes);
	memcpy(g_vertices.data(), vertices, bytes);
}

void ScaleVertices(UINT vertexCount, UINT stride)
{
	const float scaleX = ScaledTargets::ScaleX();
	const float scaleY = ScaledTargets::ScaleY();

	for (UINT i = 0; i < vertexCount; ++i)
	{
		float* const position = reinterpret_cast<float*>(g_vertices.data() + i * stride + g_positionOffset);
		position[0] *= scaleX;
		position[1] *= scaleY;
	}

	++g_scaledDraws;
}

bool FitsStride(UINT stride)
{
	return stride >= static_cast<UINT>(g_positionOffset) + 2 * sizeof(float);
}

void NoteFailure(const char* what)
{
	++g_failedDraws;

	if (g_reportedFailure)
		return;

	g_reportedFailure = true;
	LOG("[InternalResolution] a pretransformed %s could not be read back, drawn unscaled", what);
}

class ShaderSwap
{
public:
	explicit ShaderSwap(IDirect3DDevice9* device)
		: m_device(device)
	{
	}

	~ShaderSwap()
	{
		if (!m_applied)
			return;

		m_device->SetVertexShader(m_shader);
		m_device->SetVertexShaderConstantF(ScreenSpaceShader::kConstantRegister, &m_constants[0][0],
			ScreenSpaceShader::kConstantCount);
		g_setFvfHook.Original()(m_device, g_fvf);

		if (m_shader != nullptr)
			m_shader->Release();
	}

	ShaderSwap(const ShaderSwap&) = delete;
	ShaderSwap& operator=(const ShaderSwap&) = delete;

	bool Apply(float scaleX, float scaleY)
	{
		if (g_declaration != nullptr)
			return false;

		D3DVIEWPORT9 viewport = {};
		ScreenSpaceShader::Program program = {};

		if (FAILED(m_device->GetViewport(&viewport)) ||
			!ScreenSpaceShader::ProgramFor(m_device, g_fvf, viewport, scaleX, scaleY, program))
		{
			return false;
		}

		m_device->GetVertexShader(&m_shader);
		m_device->GetVertexShaderConstantF(ScreenSpaceShader::kConstantRegister, &m_constants[0][0],
			ScreenSpaceShader::kConstantCount);

		g_setVertexDeclarationHook.Original()(m_device, program.declaration);
		m_device->SetVertexShader(program.shader);
		m_device->SetVertexShaderConstantF(ScreenSpaceShader::kConstantRegister, &program.constants[0][0],
			ScreenSpaceShader::kConstantCount);

		m_applied = true;
		++g_shaderDraws;
		return true;
	}

private:
	IDirect3DDevice9* m_device;
	IDirect3DVertexShader9* m_shader = nullptr;
	float m_constants[ScreenSpaceShader::kConstantCount][ScreenSpaceShader::kVectorWidth] = {};
	bool m_applied = false;
};

class Redraw
{
public:
	Redraw() { g_redrawing = true; }
	~Redraw() { g_redrawing = false; }

	Redraw(const Redraw&) = delete;
	Redraw& operator=(const Redraw&) = delete;
};

}

void PretransformedDraws::Attach(IDirect3DDevice9* device)
{
	if (device == nullptr || !ScaledTargets::IsActive() || g_setFvfHook.IsLive())
		return;

	void** const vtable = *reinterpret_cast<void***>(device);

	const bool declaration = g_setVertexDeclarationHook.Install(vtable[kSetVertexDeclarationIndex],
		&HookedSetVertexDeclaration);
	const bool fvf = g_setFvfHook.Install(vtable[kSetFvfIndex], &HookedSetFvf);

	if (!declaration || !fvf)
		LOG("[InternalResolution] vertex format hooks incomplete (declaration %d, fvf %d)", declaration, fvf);
}

bool PretransformedDraws::IsScreenSpace()
{
	return g_positionOffset != kNoPosition && g_declaration == nullptr;
}

HRESULT PretransformedDraws::ReplayIndexed(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT baseVertex,
	UINT minVertex, UINT vertexCount, UINT startIndex, UINT primitiveCount, float scaleX, float scaleY,
	DrawIndexedPrimitive_t original)
{
	ShaderSwap swap(device);

	if (!swap.Apply(scaleX, scaleY))
		return D3DERR_INVALIDCALL;

	return original(device, type, baseVertex, minVertex, vertexCount, startIndex, primitiveCount);
}

bool PretransformedDraws::Applies()
{
	return !g_redrawing && g_positionOffset != kNoPosition && ScaledTargets::BoundIsScaled();
}

HRESULT PretransformedDraws::DrawPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE type,
	UINT startVertex, UINT primitiveCount, DrawPrimitive_t unscaled)
{
	Profiler::Scope scope(Profiler::Section_DrawScaled);
	ShaderSwap swap(device);

	if (swap.Apply(ScaledTargets::ScaleX(), ScaledTargets::ScaleY()))
		return unscaled(device, type, startVertex, primitiveCount);

	const UINT vertexCount = PrimitiveVertexCount(type, primitiveCount);
	BoundStream stream(device);

	if (!stream.IsUsable() || !FitsStride(stream.Stride()) || !stream.Copy(startVertex, vertexCount))
	{
		NoteFailure("draw");
		return unscaled(device, type, startVertex, primitiveCount);
	}

	ScaleVertices(vertexCount, stream.Stride());

	Redraw redraw;
	return device->DrawPrimitiveUP(type, primitiveCount, g_vertices.data(), stream.Stride());
}

HRESULT PretransformedDraws::DrawIndexedPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE type,
	INT baseVertex, UINT minVertex, UINT vertexCount, UINT startIndex, UINT primitiveCount,
	DrawIndexedPrimitive_t unscaled)
{
	Profiler::Scope scope(Profiler::Section_DrawScaled);
	ShaderSwap swap(device);

	if (swap.Apply(ScaledTargets::ScaleX(), ScaledTargets::ScaleY()))
		return unscaled(device, type, baseVertex, minVertex, vertexCount, startIndex, primitiveCount);

	const INT firstVertex = baseVertex + static_cast<INT>(minVertex);
	BoundStream stream(device);
	BoundIndices indices(device);

	if (firstVertex < 0 || !stream.IsUsable() || !indices.IsUsable() || !FitsStride(stream.Stride()) ||
		!indices.CopyRebased(startIndex, PrimitiveVertexCount(type, primitiveCount), minVertex, vertexCount) ||
		!stream.Copy(static_cast<UINT>(firstVertex), vertexCount))
	{
		NoteFailure("indexed draw");
		return unscaled(device, type, baseVertex, minVertex, vertexCount, startIndex, primitiveCount);
	}

	ScaleVertices(vertexCount, stream.Stride());

	Redraw redraw;
	return device->DrawIndexedPrimitiveUP(type, 0, vertexCount, primitiveCount, g_indices.data(),
		indices.Format(), g_vertices.data(), stream.Stride());
}

HRESULT PretransformedDraws::DrawPrimitiveUP(IDirect3DDevice9* device, D3DPRIMITIVETYPE type,
	UINT primitiveCount, const void* vertices, UINT stride, DrawPrimitiveUP_t original)
{
	Profiler::Scope scope(Profiler::Section_DrawScaled);
	ShaderSwap swap(device);

	if (swap.Apply(ScaledTargets::ScaleX(), ScaledTargets::ScaleY()) || vertices == nullptr || !FitsStride(stride))
		return original(device, type, primitiveCount, vertices, stride);

	const UINT vertexCount = PrimitiveVertexCount(type, primitiveCount);

	CopyVertices(vertices, vertexCount, stride);
	ScaleVertices(vertexCount, stride);

	return original(device, type, primitiveCount, g_vertices.data(), stride);
}

HRESULT PretransformedDraws::DrawIndexedPrimitiveUP(IDirect3DDevice9* device, D3DPRIMITIVETYPE type,
	UINT minVertex, UINT vertexCount, UINT primitiveCount, const void* indices, D3DFORMAT indexFormat,
	const void* vertices, UINT stride, DrawIndexedPrimitiveUP_t original)
{
	Profiler::Scope scope(Profiler::Section_DrawScaled);
	ShaderSwap swap(device);

	if (swap.Apply(ScaledTargets::ScaleX(), ScaledTargets::ScaleY()) || vertices == nullptr || !FitsStride(stride))
	{
		return original(device, type, minVertex, vertexCount, primitiveCount, indices, indexFormat, vertices,
			stride);
	}

	const UINT copied = minVertex + vertexCount;

	CopyVertices(vertices, copied, stride);
	ScaleVertices(copied, stride);

	return original(device, type, minVertex, vertexCount, primitiveCount, indices, indexFormat,
		g_vertices.data(), stride);
}

const char* PretransformedDraws::GetStatusText()
{
	snprintf(g_status, sizeof(g_status), "screen space draws: %ld on the GPU, %ld on the CPU, %ld unscaled",
		g_shaderDraws, g_scaledDraws, g_failedDraws);
	return g_status;
}
