#include "D3D9/Draw/DrawTrace.h"

#include "Core/logger.h"

#include <Windows.h>
#include <d3d9.h>

#include <cstdio>
#include <cstring>

namespace {

enum class State
{
	Idle,
	Pending,
	Capturing,
};

constexpr int kMaxLines = 900;
constexpr int kMaxVertices = 64;
constexpr int kMaxElements = 64;
constexpr int kMatrixRegisters = 12;
constexpr int kDumpedRegisters = 16;
constexpr unsigned kMaxLockedPrimitives = 16;
constexpr int kMaxShadersDumped = 12;

constexpr BYTE kUsagePosition = D3DDECLUSAGE_POSITION;
constexpr BYTE kUsagePositionT = D3DDECLUSAGE_POSITIONT;

State g_state = State::Idle;
int g_draws = 0;
int g_lines = 0;
const void* g_dumpedShaders[kMaxShadersDumped] = {};
int g_dumpedCount = 0;

struct Surface
{
	const void* identity;
	unsigned width;
	unsigned height;
};

struct Position
{
	int offset;
	int components;
	bool pretransformed;
	bool known;
	int texcoordOffset;
};

struct Range
{
	float minX;
	float maxX;
	float minY;
	float maxY;
	int count;
};

Surface Describe(IDirect3DSurface9* surface)
{
	Surface out = {};

	if (surface == nullptr)
		return out;

	D3DSURFACE_DESC desc = {};
	if (SUCCEEDED(surface->GetDesc(&desc)))
	{
		out.width = desc.Width;
		out.height = desc.Height;
	}

	out.identity = surface;
	return out;
}

Surface ReadTarget(IDirect3DDevice9* device, bool& outBackBuffer)
{
	outBackBuffer = false;

	IDirect3DSurface9* target = nullptr;
	if (FAILED(device->GetRenderTarget(0, &target)) || target == nullptr)
		return {};

	const Surface out = Describe(target);

	IDirect3DSurface9* backBuffer = nullptr;
	if (SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) &&
		backBuffer != nullptr)
	{
		outBackBuffer = backBuffer == target;
		backBuffer->Release();
	}

	target->Release();
	return out;
}

Surface ReadTexture(IDirect3DDevice9* device, DWORD stage)
{
	IDirect3DBaseTexture9* texture = nullptr;
	if (FAILED(device->GetTexture(stage, &texture)) || texture == nullptr)
		return {};

	Surface out = {};

	if (texture->GetType() == D3DRTYPE_TEXTURE)
	{
		IDirect3DSurface9* level = nullptr;
		if (SUCCEEDED(static_cast<IDirect3DTexture9*>(texture)->GetSurfaceLevel(0, &level)) &&
			level != nullptr)
		{
			out = Describe(level);
			level->Release();
		}
	}

	texture->Release();
	return out;
}

int ComponentsOf(BYTE type)
{
	switch (type)
	{
	case D3DDECLTYPE_FLOAT2:
		return 2;
	case D3DDECLTYPE_FLOAT3:
		return 3;
	case D3DDECLTYPE_FLOAT4:
		return 4;
	default:
		return 0;
	}
}

int FvfTexcoordOffset(DWORD fvf, int positionBytes)
{
	if ((fvf & D3DFVF_TEXCOUNT_MASK) == 0)
		return -1;

	int offset = positionBytes;
	offset += (fvf & D3DFVF_NORMAL) != 0 ? 12 : 0;
	offset += (fvf & D3DFVF_PSIZE) != 0 ? 4 : 0;
	offset += (fvf & D3DFVF_DIFFUSE) != 0 ? 4 : 0;
	offset += (fvf & D3DFVF_SPECULAR) != 0 ? 4 : 0;
	return offset;
}

Position FromFvf(DWORD fvf)
{
	const DWORD kind = fvf & D3DFVF_POSITION_MASK;

	if (kind == D3DFVF_XYZRHW)
		return { 0, 4, true, true, FvfTexcoordOffset(fvf, 16) };

	if (kind == D3DFVF_XYZW)
		return { 0, 4, false, true, FvfTexcoordOffset(fvf, 16) };

	if (kind == D3DFVF_XYZ)
		return { 0, 3, false, true, FvfTexcoordOffset(fvf, 12) };

	return {};
}

Position ReadPosition(IDirect3DDevice9* device)
{
	IDirect3DVertexDeclaration9* declaration = nullptr;

	if (FAILED(device->GetVertexDeclaration(&declaration)) || declaration == nullptr)
	{
		DWORD fvf = 0;
		device->GetFVF(&fvf);
		return FromFvf(fvf);
	}

	D3DVERTEXELEMENT9 elements[kMaxElements] = {};
	UINT count = 0;
	Position out = {};
	out.texcoordOffset = -1;
	int texcoordOffset = -1;

	if (SUCCEEDED(declaration->GetDeclaration(elements, &count)))
	{
		for (UINT i = 0; i + 1 < count && i < kMaxElements; ++i)
		{
			const D3DVERTEXELEMENT9& element = elements[i];

			if (element.Stream == 0 && element.UsageIndex == 0 &&
				element.Usage == D3DDECLUSAGE_TEXCOORD && ComponentsOf(element.Type) >= 2)
			{
				texcoordOffset = element.Offset;
			}

			if (element.Stream != 0 || element.UsageIndex != 0 ||
				(element.Usage != kUsagePosition && element.Usage != kUsagePositionT))
			{
				continue;
			}

			out = { element.Offset, ComponentsOf(element.Type),
				element.Usage == kUsagePositionT, ComponentsOf(element.Type) > 0, -1 };
		}
	}

	out.texcoordOffset = texcoordOffset;

	declaration->Release();
	return out;
}

unsigned VerticesFor(int primitiveType, unsigned primitiveCount)
{
	switch (primitiveType)
	{
	case D3DPT_POINTLIST:
		return primitiveCount;
	case D3DPT_LINELIST:
		return primitiveCount * 2;
	case D3DPT_LINESTRIP:
		return primitiveCount + 1;
	case D3DPT_TRIANGLELIST:
		return primitiveCount * 3;
	default:
		return primitiveCount + 2;
	}
}

void ReadVertex(const void* vertexData, unsigned stride, unsigned index, const Position& position,
	float out[4])
{
	out[0] = 0.0f;
	out[1] = 0.0f;
	out[2] = 0.0f;
	out[3] = 1.0f;

	const auto* base = static_cast<const unsigned char*>(vertexData) + index * stride +
		position.offset;
	memcpy(out, base, sizeof(float) * position.components);
}

void Dp4Block(const float registers[kMatrixRegisters][4], int block, float inOut[4])
{
	float result[4] = {};

	for (int row = 0; row < 4; ++row)
	{
		const float* c = registers[block * 4 + row];
		result[row] = inOut[0] * c[0] + inOut[1] * c[1] + inOut[2] * c[2] + inOut[3] * c[3];
	}

	memcpy(inOut, result, sizeof(result));
}

bool ProjectToPixel(const float registers[kMatrixRegisters][4], const D3DVIEWPORT9& viewport,
	const float vertex[4], float& outX, float& outY)
{
	float point[4] = { vertex[0], vertex[1], vertex[2], vertex[3] };

	for (int block = 0; block < 3; ++block)
		Dp4Block(registers, block, point);

	if (point[3] > -1e-6f && point[3] < 1e-6f)
		return false;

	const float ndcX = point[0] / point[3];
	const float ndcY = point[1] / point[3];

	outX = viewport.X + (ndcX + 1.0f) * 0.5f * viewport.Width;
	outY = viewport.Y + (1.0f - ndcY) * 0.5f * viewport.Height;
	return true;
}

void Include(Range& range, float x, float y)
{
	if (range.count == 0)
	{
		range = { x, x, y, y, 1 };
		return;
	}

	range.minX = x < range.minX ? x : range.minX;
	range.maxX = x > range.maxX ? x : range.maxX;
	range.minY = y < range.minY ? y : range.minY;
	range.maxY = y > range.maxY ? y : range.maxY;
	++range.count;
}

bool Emit()
{
	if (g_lines >= kMaxLines)
		return false;

	++g_lines;
	return true;
}

void DumpShaderOnce(IDirect3DDevice9* device, const void* shader)
{
	for (int i = 0; i < g_dumpedCount; ++i)
	{
		if (g_dumpedShaders[i] == shader)
			return;
	}

	if (g_dumpedCount >= kMaxShadersDumped || !Emit())
		return;

	float registers[kDumpedRegisters][4] = {};
	if (FAILED(device->GetVertexShaderConstantF(0, &registers[0][0], kDumpedRegisters)))
		return;

	g_dumpedShaders[g_dumpedCount++] = shader;
	LOG_RAW("vs %p c0..c15 at its first draw:", shader);

	for (int i = 0; i < kDumpedRegisters && Emit(); ++i)
	{
		LOG_RAW("  c%-2d %12.6f %12.6f %12.6f %12.6f", i, registers[i][0], registers[i][1],
			registers[i][2], registers[i][3]);
	}
}

void DescribeVertices(IDirect3DDevice9* device, const void* shader, const void* vertexData,
	unsigned stride, unsigned vertexCount, const D3DVIEWPORT9& viewport, char* out, size_t outSize)
{
	out[0] = '\0';

	const Position position = ReadPosition(device);
	if (!position.known || stride == 0)
	{
		sprintf_s(out, outSize, "  (position layout unknown)");
		return;
	}

	float registers[kMatrixRegisters][4] = {};
	const bool haveRegisters = !position.pretransformed &&
		SUCCEEDED(device->GetVertexShaderConstantF(0, &registers[0][0], kMatrixRegisters));

	if (haveRegisters && shader != nullptr)
		DumpShaderOnce(device, shader);

	Range raw = {};
	Range pixel = {};
	Range uv = {};
	const unsigned limit = vertexCount < kMaxVertices ? vertexCount : kMaxVertices;

	for (unsigned i = 0; i < limit; ++i)
	{
		float vertex[4] = {};
		ReadVertex(vertexData, stride, i, position, vertex);
		Include(raw, vertex[0], vertex[1]);

		if (position.texcoordOffset >= 0 &&
			static_cast<unsigned>(position.texcoordOffset) + 8 <= stride)
		{
			float texcoord[2] = {};
			memcpy(texcoord, static_cast<const unsigned char*>(vertexData) + i * stride +
				position.texcoordOffset, sizeof(texcoord));
			Include(uv, texcoord[0], texcoord[1]);
		}

		float x = 0.0f;
		float y = 0.0f;
		if (haveRegisters && ProjectToPixel(registers, viewport, vertex, x, y))
			Include(pixel, x, y);
	}

	int written = sprintf_s(out, outSize, "  %s x %.3f..%.3f y %.3f..%.3f",
		position.pretransformed ? "xyzrhw" : "pos", raw.minX, raw.maxX, raw.minY, raw.maxY);

	if (written > 0 && uv.count > 0)
	{
		const int more = sprintf_s(out + written, outSize - written, "  uv %.4f..%.4f %.4f..%.4f",
			uv.minX, uv.maxX, uv.minY, uv.maxY);
		written = more > 0 ? written + more : written;
	}

	if (pixel.count == 0 || written < 0)
		return;

	sprintf_s(out + written, outSize - written, "  -> rt px x %.1f..%.1f y %.1f..%.1f",
		pixel.minX, pixel.maxX, pixel.minY, pixel.maxY);
}

}

void DrawTrace::Arm()
{
	if (g_state == State::Idle)
		g_state = State::Pending;
}

void DrawTrace::OnPresentBegin(const RECT* sourceRect, const RECT* destRect)
{
	if (g_state != State::Capturing)
		return;

	g_state = State::Idle;

	if (sourceRect != nullptr)
	{
		LOG_RAW("present source rect %ld,%ld..%ld,%ld", sourceRect->left, sourceRect->top,
			sourceRect->right, sourceRect->bottom);
	}

	if (destRect != nullptr)
	{
		LOG_RAW("present dest rect %ld,%ld..%ld,%ld", destRect->left, destRect->top,
			destRect->right, destRect->bottom);
	}

	if (sourceRect == nullptr && destRect == nullptr)
		LOG_RAW("present takes the whole back buffer");
	LOG_RAW("draw trace end: %d draws, %d lines%s", g_draws, g_lines,
		g_lines >= kMaxLines ? " (cut at the line limit)" : "");
}

void DrawTrace::OnPresentEnd()
{
	if (g_state != State::Pending)
		return;

	g_state = State::Capturing;
	g_draws = 0;
	g_lines = 0;
	g_dumpedCount = 0;

	LOG_SECTION("draw trace");
	LOG_RAW("one game frame, every draw call in order; rt/tex are surface identities, a tex "
		"matching an earlier rt means that draw samples what was rendered there");
}

void DrawTrace::OnIndexedDraw(IDirect3DDevice9* device, int primitiveType,
	unsigned primitiveCount, int baseVertex, unsigned minVertex, unsigned vertexCount)
{
	if (g_state != State::Capturing || device == nullptr)
		return;

	if (primitiveCount > kMaxLockedPrimitives || vertexCount == 0)
	{
		OnDraw(device, "dip", primitiveType, primitiveCount);
		return;
	}

	IDirect3DVertexBuffer9* buffer = nullptr;
	UINT offset = 0;
	UINT stride = 0;

	if (FAILED(device->GetStreamSource(0, &buffer, &offset, &stride)) || buffer == nullptr ||
		stride == 0)
	{
		if (buffer != nullptr)
			buffer->Release();

		OnDraw(device, "dip", primitiveType, primitiveCount);
		return;
	}

	const UINT first = offset + static_cast<UINT>(baseVertex + static_cast<int>(minVertex)) * stride;
	void* data = nullptr;

	if (FAILED(buffer->Lock(first, vertexCount * stride, &data, D3DLOCK_READONLY)) ||
		data == nullptr)
	{
		buffer->Release();
		OnDraw(device, "dip", primitiveType, primitiveCount);
		return;
	}

	OnDraw(device, "dip", primitiveType, primitiveCount, data, stride, vertexCount);

	buffer->Unlock();
	buffer->Release();
}

void DrawTrace::OnDraw(IDirect3DDevice9* device, const char* kind, int primitiveType,
	unsigned primitiveCount, const void* vertexData, unsigned stride, unsigned vertexCount)
{
	if (g_state != State::Capturing || device == nullptr)
		return;

	const int index = g_draws++;

	if (!Emit())
		return;

	bool backBuffer = false;
	const Surface target = ReadTarget(device, backBuffer);
	const Surface texture0 = ReadTexture(device, 0);
	const Surface texture1 = ReadTexture(device, 1);

	D3DVIEWPORT9 viewport = {};
	device->GetViewport(&viewport);

	DWORD scissorOn = 0;
	device->GetRenderState(D3DRS_SCISSORTESTENABLE, &scissorOn);
	RECT scissor = {};
	if (scissorOn != 0)
		device->GetScissorRect(&scissor);

	IDirect3DVertexShader9* shader = nullptr;
	device->GetVertexShader(&shader);
	if (shader != nullptr)
		shader->Release();

	char vertices[256] = {};
	if (vertexData != nullptr)
	{
		const unsigned count = vertexCount != 0 ? vertexCount
			: VerticesFor(primitiveType, primitiveCount);
		DescribeVertices(device, shader, vertexData, stride, count, viewport, vertices,
			sizeof(vertices));
	}

	char clip[64] = {};
	if (scissorOn != 0)
	{
		sprintf_s(clip, "  scissor %ld,%ld..%ld,%ld", scissor.left, scissor.top, scissor.right,
			scissor.bottom);
	}

	LOG_RAW("#%03d %-5s t%d n%-4u rt %p %ux%u%s vp %lu,%lu %lux%lu tex0 %p %ux%u tex1 %ux%u "
		"vs %p%s%s", index, kind, primitiveType, primitiveCount, target.identity, target.width,
		target.height, backBuffer ? " (bb)" : "", viewport.X, viewport.Y, viewport.Width,
		viewport.Height, texture0.identity, texture0.width, texture0.height, texture1.width,
		texture1.height, static_cast<const void*>(shader), clip, vertices);
}
