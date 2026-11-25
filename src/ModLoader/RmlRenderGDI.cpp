#include "ModLoader/RmlRenderGDI.h"

#include "Core/Formats/PngImage.h"

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FileInterface.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

inline uint8_t Mul255(uint32_t a, uint32_t b)
{
	return static_cast<uint8_t>((a * b + 127u) / 255u);
}

inline uint32_t BlendPremul(uint32_t dst, uint8_t sb, uint8_t sg, uint8_t sr, uint8_t sa)
{
	if (sa == 255)
		return static_cast<uint32_t>(sb) | (static_cast<uint32_t>(sg) << 8) |
			(static_cast<uint32_t>(sr) << 16) | 0xFF000000u;

	if (sa == 0 && sb == 0 && sg == 0 && sr == 0)
		return dst;

	const uint32_t inv = 255u - sa;
	const uint32_t db = dst & 0xFFu;
	const uint32_t dg = (dst >> 8) & 0xFFu;
	const uint32_t dr = (dst >> 16) & 0xFFu;

	const uint32_t ob = sb + ((db * inv + 127u) / 255u);
	const uint32_t og = sg + ((dg * inv + 127u) / 255u);
	const uint32_t or_ = sr + ((dr * inv + 127u) / 255u);

	return (ob > 255u ? 255u : ob) |
		((og > 255u ? 255u : og) << 8) |
		((or_ > 255u ? 255u : or_) << 16) |
		0xFF000000u;
}

inline float EdgeFn(float ax, float ay, float bx, float by, float cx, float cy)
{
	return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
}

inline bool NearlyEqual(float a, float b)
{
	return std::fabs(a - b) < 0.001f;
}

}

RmlRenderGDI::RmlRenderGDI()
{
	m_mat[0] = 1.0f;
	m_mat[5] = 1.0f;
	m_mat[10] = 1.0f;
	m_mat[15] = 1.0f;
}

RmlRenderGDI::~RmlRenderGDI()
{
	DestroySurface();
}

void RmlRenderGDI::DestroySurface()
{
	if (m_memDc != nullptr)
	{
		if (m_oldBmp != nullptr)
		{
			SelectObject(m_memDc, m_oldBmp);
			m_oldBmp = nullptr;
		}
		DeleteDC(m_memDc);
		m_memDc = nullptr;
	}

	if (m_dib != nullptr)
	{
		DeleteObject(m_dib);
		m_dib = nullptr;
	}

	m_pixels = nullptr;
	m_width = 0;
	m_height = 0;
}

void RmlRenderGDI::Resize(int width, int height)
{
	if (width <= 0 || height <= 0)
		return;

	if (width == m_width && height == m_height && m_pixels != nullptr)
		return;

	DestroySurface();

	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = -height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	HDC screen = GetDC(nullptr);
	m_memDc = CreateCompatibleDC(screen);
	void* bits = nullptr;
	m_dib = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
	ReleaseDC(nullptr, screen);

	if (m_memDc == nullptr || m_dib == nullptr || bits == nullptr)
	{
		DestroySurface();
		return;
	}

	m_oldBmp = static_cast<HBITMAP>(SelectObject(m_memDc, m_dib));
	m_pixels = static_cast<uint32_t*>(bits);
	m_width = width;
	m_height = height;
	m_stencil.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
}

void RmlRenderGDI::BeginFrame(uint32_t clearColorBgra)
{
	if (m_pixels == nullptr || m_width <= 0 || m_height <= 0)
		return;

	const size_t total = static_cast<size_t>(m_width) * static_cast<size_t>(m_height);

	for (size_t i = 0; i < total; ++i)
		m_pixels[i] = clearColorBgra;

	m_scissorEnabled = false;
	m_scissorL = 0;
	m_scissorT = 0;
	m_scissorR = m_width;
	m_scissorB = m_height;
	m_clipMaskEnabled = false;
	m_stencilRef = 1;
	m_hasTransform = false;
}

void RmlRenderGDI::Present(HDC targetDc)
{
	if (targetDc == nullptr || m_memDc == nullptr || m_width <= 0 || m_height <= 0)
		return;

	BitBlt(targetDc, 0, 0, m_width, m_height, m_memDc, 0, 0, SRCCOPY);
}

void RmlRenderGDI::TransformPoint(float x, float y, float tx, float ty, float& outX, float& outY) const
{
	const float lx = x + tx;
	const float ly = y + ty;

	if (!m_hasTransform)
	{
		outX = lx;
		outY = ly;
		return;
	}

	outX = m_mat[0] * lx + m_mat[4] * ly + m_mat[12];
	outY = m_mat[1] * lx + m_mat[5] * ly + m_mat[13];
}

void RmlRenderGDI::DrawSolidAxisQuad(int x0, int y0, int x1, int y1,
	uint8_t b, uint8_t g, uint8_t r, uint8_t a)
{
	const int clipL = m_scissorEnabled ? (std::max)(0, m_scissorL) : 0;
	const int clipT = m_scissorEnabled ? (std::max)(0, m_scissorT) : 0;
	const int clipR = m_scissorEnabled ? (std::min)(m_width, m_scissorR) : m_width;
	const int clipB = m_scissorEnabled ? (std::min)(m_height, m_scissorB) : m_height;

	const int sx = (std::max)(x0, clipL);
	const int sy = (std::max)(y0, clipT);
	const int ex = (std::min)(x1, clipR);
	const int ey = (std::min)(y1, clipB);

	if (sx >= ex || sy >= ey)
		return;

	if (!m_clipMaskEnabled && a == 255)
	{
		const uint32_t packed = static_cast<uint32_t>(b) | (static_cast<uint32_t>(g) << 8) |
			(static_cast<uint32_t>(r) << 16) | 0xFF000000u;

		for (int y = sy; y < ey; ++y)
		{
			uint32_t* const row = m_pixels + static_cast<size_t>(y) * m_width;
			for (int x = sx; x < ex; ++x)
				row[x] = packed;
		}
		return;
	}

	for (int y = sy; y < ey; ++y)
	{
		const size_t base = static_cast<size_t>(y) * m_width;
		uint32_t* const row = m_pixels + base;
		const uint8_t* const st = m_stencil.data() + base;

		for (int x = sx; x < ex; ++x)
		{
			if (m_clipMaskEnabled && st[x] != m_stencilRef)
				continue;

			row[x] = BlendPremul(row[x], b, g, r, a);
		}
	}
}

void RmlRenderGDI::DrawTexturedAxisQuad(float x0, float y0, float x1, float y1,
	float u0, float v0, float u1, float v1,
	uint8_t mb, uint8_t mg, uint8_t mr, uint8_t ma, const Texture& tex)
{
	if (tex.width <= 0 || tex.height <= 0 || tex.bgra.empty())
		return;

	const int clipL = m_scissorEnabled ? (std::max)(0, m_scissorL) : 0;
	const int clipT = m_scissorEnabled ? (std::max)(0, m_scissorT) : 0;
	const int clipR = m_scissorEnabled ? (std::min)(m_width, m_scissorR) : m_width;
	const int clipB = m_scissorEnabled ? (std::min)(m_height, m_scissorB) : m_height;

	const int sx = (std::max)(static_cast<int>(std::floor(x0 + 0.5f)), clipL);
	const int sy = (std::max)(static_cast<int>(std::floor(y0 + 0.5f)), clipT);
	const int ex = (std::min)(static_cast<int>(std::floor(x1 + 0.5f)), clipR);
	const int ey = (std::min)(static_cast<int>(std::floor(y1 + 0.5f)), clipB);

	if (sx >= ex || sy >= ey)
		return;

	const float spanX = x1 - x0;
	const float spanY = y1 - y0;

	if (spanX <= 0.0001f || spanY <= 0.0001f)
		return;

	const float du = (u1 - u0) / spanX;
	const float dv = (v1 - v0) / spanY;
	const bool whiteMod = (mb == 255 && mg == 255 && mr == 255 && ma == 255);
	const int maxTx = tex.width - 1;
	const int maxTy = tex.height - 1;

	for (int y = sy; y < ey; ++y)
	{
		const float fy = (static_cast<float>(y) + 0.5f - y0) * dv + v0;
		int ty = static_cast<int>(std::floor(fy * tex.height));
		if (ty < 0) ty = 0;
		else if (ty > maxTy) ty = maxTy;

		const size_t base = static_cast<size_t>(y) * m_width;
		uint32_t* const row = m_pixels + base;
		const uint8_t* const st = m_stencil.data() + base;
		const uint8_t* const texRow = tex.bgra.data() + static_cast<size_t>(ty) * tex.width * 4;

		for (int x = sx; x < ex; ++x)
		{
			if (m_clipMaskEnabled && st[x] != m_stencilRef)
				continue;

			const float fx = (static_cast<float>(x) + 0.5f - x0) * du + u0;
			int tx = static_cast<int>(std::floor(fx * tex.width));
			if (tx < 0) tx = 0;
			else if (tx > maxTx) tx = maxTx;

			const uint8_t* const tp = texRow + static_cast<size_t>(tx) * 4;
			uint8_t sb = tp[0];
			uint8_t sg = tp[1];
			uint8_t sr = tp[2];
			uint8_t sa = tp[3];

			if (sa == 0 && sb == 0 && sg == 0 && sr == 0)
				continue;

			if (!whiteMod)
			{
				sb = Mul255(sb, mb);
				sg = Mul255(sg, mg);
				sr = Mul255(sr, mr);
				sa = Mul255(sa, ma);
			}

			row[x] = BlendPremul(row[x], sb, sg, sr, sa);
		}
	}
}

void RmlRenderGDI::RasterizeTriangle(const SoftVertex& a, const SoftVertex& b, const SoftVertex& c,
	const Texture* tex, bool writeStencil, uint8_t stencilOp)
{
	float area = EdgeFn(a.x, a.y, b.x, b.y, c.x, c.y);

	if (std::fabs(area) < 0.0001f)
		return;

	SoftVertex v0 = a;
	SoftVertex v1 = b;
	SoftVertex v2 = c;

	if (area < 0.0f)
	{
		std::swap(v1, v2);
		area = -area;
	}

	const int clipL = m_scissorEnabled ? (std::max)(0, m_scissorL) : 0;
	const int clipT = m_scissorEnabled ? (std::max)(0, m_scissorT) : 0;
	const int clipR = m_scissorEnabled ? (std::min)(m_width, m_scissorR) : m_width;
	const int clipB = m_scissorEnabled ? (std::min)(m_height, m_scissorB) : m_height;

	const int minX = (std::max)(clipL, static_cast<int>(std::floor((std::min)({ v0.x, v1.x, v2.x }))));
	const int minY = (std::max)(clipT, static_cast<int>(std::floor((std::min)({ v0.y, v1.y, v2.y }))));
	const int maxX = (std::min)(clipR - 1, static_cast<int>(std::ceil((std::max)({ v0.x, v1.x, v2.x }))));
	const int maxY = (std::min)(clipB - 1, static_cast<int>(std::ceil((std::max)({ v0.y, v1.y, v2.y }))));

	if (minX > maxX || minY > maxY)
		return;

	const float invArea = 1.0f / area;
	const bool flatColor = (v0.b == v1.b && v1.b == v2.b &&
		v0.g == v1.g && v1.g == v2.g &&
		v0.r == v1.r && v1.r == v2.r &&
		v0.a == v1.a && v1.a == v2.a);

	for (int y = minY; y <= maxY; ++y)
	{
		const float py = static_cast<float>(y) + 0.5f;
		const size_t base = static_cast<size_t>(y) * m_width;
		uint32_t* const row = m_pixels + base;
		uint8_t* const st = m_stencil.data() + base;

		for (int x = minX; x <= maxX; ++x)
		{
			const float px = static_cast<float>(x) + 0.5f;
			const float w0 = EdgeFn(v1.x, v1.y, v2.x, v2.y, px, py);
			const float w1 = EdgeFn(v2.x, v2.y, v0.x, v0.y, px, py);
			const float w2 = EdgeFn(v0.x, v0.y, v1.x, v1.y, px, py);

			if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
				continue;

			if (writeStencil)
			{
				if (stencilOp == 0)
					st[x] = 1;
				else if (stencilOp == 1)
					st[x] = 0;
				else if (st[x] == m_stencilRef && st[x] < 255)
					++st[x];
				continue;
			}

			if (m_clipMaskEnabled && st[x] != m_stencilRef)
				continue;

			const float l0 = w0 * invArea;
			const float l1 = w1 * invArea;
			const float l2 = w2 * invArea;

			uint8_t sb = v0.b;
			uint8_t sg = v0.g;
			uint8_t sr = v0.r;
			uint8_t sa = v0.a;

			if (!flatColor)
			{
				sb = static_cast<uint8_t>(l0 * v0.b + l1 * v1.b + l2 * v2.b);
				sg = static_cast<uint8_t>(l0 * v0.g + l1 * v1.g + l2 * v2.g);
				sr = static_cast<uint8_t>(l0 * v0.r + l1 * v1.r + l2 * v2.r);
				sa = static_cast<uint8_t>(l0 * v0.a + l1 * v1.a + l2 * v2.a);
			}

			if (tex != nullptr && !tex->bgra.empty())
			{
				const float u = l0 * v0.u + l1 * v1.u + l2 * v2.u;
				const float v = l0 * v0.v + l1 * v1.v + l2 * v2.v;
				int tx = static_cast<int>(std::floor(u * tex->width));
				int ty = static_cast<int>(std::floor(v * tex->height));
				if (tx < 0) tx = 0; else if (tx >= tex->width) tx = tex->width - 1;
				if (ty < 0) ty = 0; else if (ty >= tex->height) ty = tex->height - 1;
				const uint8_t* const tp = tex->bgra.data() + (static_cast<size_t>(ty) * tex->width + tx) * 4;
				sb = Mul255(tp[0], sb);
				sg = Mul255(tp[1], sg);
				sr = Mul255(tp[2], sr);
				sa = Mul255(tp[3], sa);
			}

			row[x] = BlendPremul(row[x], sb, sg, sr, sa);
		}
	}
}

Rml::CompiledGeometryHandle RmlRenderGDI::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
	Rml::Span<const int> indices)
{
	if (vertices.empty() || indices.empty())
		return 0;

	Geometry geom;
	geom.vertices.resize(vertices.size());

	for (size_t i = 0; i < vertices.size(); ++i)
	{
		const Rml::Vertex& src = vertices[i];
		SoftVertex& dst = geom.vertices[i];
		dst.x = src.position.x;
		dst.y = src.position.y;
		dst.b = src.colour.blue;
		dst.g = src.colour.green;
		dst.r = src.colour.red;
		dst.a = src.colour.alpha;
		dst.u = src.tex_coord.x;
		dst.v = src.tex_coord.y;
	}

	geom.indices.assign(indices.begin(), indices.end());

	const uintptr_t handle = m_nextHandle++;
	m_geometries.emplace(handle, std::move(geom));
	return static_cast<Rml::CompiledGeometryHandle>(handle);
}

void RmlRenderGDI::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation,
	Rml::TextureHandle texture)
{
	if (m_pixels == nullptr)
		return;

	const auto it = m_geometries.find(static_cast<uintptr_t>(geometry));

	if (it == m_geometries.end() || it->second.indices.size() < 3)
		return;

	const Texture* tex = nullptr;

	if (texture != 0)
	{
		const auto texIt = m_textures.find(static_cast<uintptr_t>(texture));
		if (texIt != m_textures.end())
			tex = &texIt->second;
	}

	const Geometry& geom = it->second;
	const size_t nIdx = geom.indices.size();

	if (!m_hasTransform && (nIdx % 6) == 0)
	{
		bool allAxisQuads = true;

		for (size_t i = 0; i < nIdx; i += 6)
		{
			const int i0 = geom.indices[i + 0];
			const int i1 = geom.indices[i + 1];
			const int i2 = geom.indices[i + 2];
			const int i3 = geom.indices[i + 3];
			const int i4 = geom.indices[i + 4];
			const int i5 = geom.indices[i + 5];

			if (i0 < 0 || i1 < 0 || i2 < 0 || i3 < 0 || i4 < 0 || i5 < 0)
			{
				allAxisQuads = false;
				break;
			}

			const SoftVertex& a = geom.vertices[i0];
			const SoftVertex& b = geom.vertices[i1];
			const SoftVertex& c = geom.vertices[i2];
			const SoftVertex& d = geom.vertices[i4];

			if (!(i0 == i3 && i2 == i5 &&
				a.r == b.r && b.r == c.r && c.r == d.r &&
				a.g == b.g && b.g == c.g && c.g == d.g &&
				a.b == b.b && b.b == c.b && c.b == d.b &&
				a.a == b.a && b.a == c.a && c.a == d.a))
			{
				allAxisQuads = false;
				break;
			}

			const bool horizFirst = NearlyEqual(a.y, d.y) && NearlyEqual(b.y, c.y) &&
				NearlyEqual(a.x, b.x) && NearlyEqual(d.x, c.x);
			const bool vertFirst = NearlyEqual(a.y, b.y) && NearlyEqual(d.y, c.y) &&
				NearlyEqual(a.x, d.x) && NearlyEqual(b.x, c.x);

			if (!horizFirst && !vertFirst)
			{
				allAxisQuads = false;
				break;
			}
		}

		if (allAxisQuads)
		{
			for (size_t i = 0; i < nIdx; i += 6)
			{
				const SoftVertex& a = geom.vertices[geom.indices[i + 0]];
				const SoftVertex& b = geom.vertices[geom.indices[i + 1]];
				const SoftVertex& c = geom.vertices[geom.indices[i + 2]];
				const SoftVertex& d = geom.vertices[geom.indices[i + 4]];

				const float x0 = (std::min)({ a.x, b.x, c.x, d.x }) + translation.x;
				const float y0 = (std::min)({ a.y, b.y, c.y, d.y }) + translation.y;
				const float x1 = (std::max)({ a.x, b.x, c.x, d.x }) + translation.x;
				const float y1 = (std::max)({ a.y, b.y, c.y, d.y }) + translation.y;

				if (tex == nullptr)
				{
					DrawSolidAxisQuad(
						static_cast<int>(std::floor(x0 + 0.5f)),
						static_cast<int>(std::floor(y0 + 0.5f)),
						static_cast<int>(std::floor(x1 + 0.5f)),
						static_cast<int>(std::floor(y1 + 0.5f)),
						a.b, a.g, a.r, a.a);
				}
				else
				{
					const float u0 = (std::min)({ a.u, b.u, c.u, d.u });
					const float v0 = (std::min)({ a.v, b.v, c.v, d.v });
					const float u1 = (std::max)({ a.u, b.u, c.u, d.u });
					const float v1 = (std::max)({ a.v, b.v, c.v, d.v });
					DrawTexturedAxisQuad(x0, y0, x1, y1, u0, v0, u1, v1, a.b, a.g, a.r, a.a, *tex);
				}
			}
			return;
		}
	}

	for (size_t i = 0; i + 2 < nIdx; i += 3)
	{
		SoftVertex v0 = geom.vertices[geom.indices[i + 0]];
		SoftVertex v1 = geom.vertices[geom.indices[i + 1]];
		SoftVertex v2 = geom.vertices[geom.indices[i + 2]];
		TransformPoint(v0.x, v0.y, translation.x, translation.y, v0.x, v0.y);
		TransformPoint(v1.x, v1.y, translation.x, translation.y, v1.x, v1.y);
		TransformPoint(v2.x, v2.y, translation.x, translation.y, v2.x, v2.y);
		RasterizeTriangle(v0, v1, v2, tex, false, 0);
	}
}

void RmlRenderGDI::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
{
	m_geometries.erase(static_cast<uintptr_t>(geometry));
}

Rml::TextureHandle RmlRenderGDI::LoadTexture(Rml::Vector2i& textureDimensions, const Rml::String& source)
{
	Rml::FileInterface* const files = Rml::GetFileInterface();

	if (files == nullptr)
		return 0;

	const Rml::FileHandle file = files->Open(source);

	if (file == 0)
		return 0;

	const size_t size = files->Length(file);
	std::vector<uint8_t> bytes(size);

	if (size > 0)
		files->Read(bytes.data(), size, file);

	files->Close(file);

	Texture tex;

	if (!PngImage::Decode(bytes, tex.width, tex.height, tex.bgra) || tex.width <= 0 || tex.height <= 0)
		return 0;

	const size_t pixels = static_cast<size_t>(tex.width) * static_cast<size_t>(tex.height);

	for (size_t i = 0; i < pixels; ++i)
	{
		uint8_t* const p = &tex.bgra[i * 4];
		const uint32_t a = p[3];
		p[0] = static_cast<uint8_t>((p[0] * a + 127u) / 255u);
		p[1] = static_cast<uint8_t>((p[1] * a + 127u) / 255u);
		p[2] = static_cast<uint8_t>((p[2] * a + 127u) / 255u);
	}

	textureDimensions.x = tex.width;
	textureDimensions.y = tex.height;

	const uintptr_t handle = m_nextHandle++;
	m_textures.emplace(handle, std::move(tex));
	return static_cast<Rml::TextureHandle>(handle);
}

Rml::TextureHandle RmlRenderGDI::GenerateTexture(Rml::Span<const Rml::byte> source,
	Rml::Vector2i sourceDimensions)
{
	const int width = sourceDimensions.x;
	const int height = sourceDimensions.y;

	if (width <= 0 || height <= 0 || source.size() < static_cast<size_t>(width) * height * 4)
		return 0;

	Texture tex;
	tex.width = width;
	tex.height = height;
	const size_t pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
	tex.bgra.resize(pixels * 4);

	for (size_t i = 0; i < pixels; ++i)
	{
		tex.bgra[i * 4 + 0] = source[i * 4 + 2];
		tex.bgra[i * 4 + 1] = source[i * 4 + 1];
		tex.bgra[i * 4 + 2] = source[i * 4 + 0];
		tex.bgra[i * 4 + 3] = source[i * 4 + 3];
	}

	const uintptr_t handle = m_nextHandle++;
	m_textures.emplace(handle, std::move(tex));
	return static_cast<Rml::TextureHandle>(handle);
}

void RmlRenderGDI::ReleaseTexture(Rml::TextureHandle texture)
{
	m_textures.erase(static_cast<uintptr_t>(texture));
}

void RmlRenderGDI::EnableScissorRegion(bool enable)
{
	m_scissorEnabled = enable;
}

void RmlRenderGDI::SetScissorRegion(Rml::Rectanglei region)
{
	m_scissorL = region.Left();
	m_scissorT = region.Top();
	m_scissorR = region.Right();
	m_scissorB = region.Bottom();
}

void RmlRenderGDI::EnableClipMask(bool enable)
{
	m_clipMaskEnabled = enable;
}

void RmlRenderGDI::RenderToClipMask(Rml::ClipMaskOperation operation,
	Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation)
{
	if (m_stencil.empty())
		return;

	const auto it = m_geometries.find(static_cast<uintptr_t>(geometry));

	if (it == m_geometries.end() || it->second.indices.size() < 3)
		return;

	uint8_t op = 0;

	if (operation == Rml::ClipMaskOperation::Set)
	{
		std::fill(m_stencil.begin(), m_stencil.end(), 0);
		m_stencilRef = 1;
		op = 0;
	}
	else if (operation == Rml::ClipMaskOperation::SetInverse)
	{
		std::fill(m_stencil.begin(), m_stencil.end(), 1);
		m_stencilRef = 1;
		op = 1;
	}
	else
	{
		op = 2;
	}

	const Geometry& geom = it->second;

	for (size_t i = 0; i + 2 < geom.indices.size(); i += 3)
	{
		SoftVertex v0 = geom.vertices[geom.indices[i + 0]];
		SoftVertex v1 = geom.vertices[geom.indices[i + 1]];
		SoftVertex v2 = geom.vertices[geom.indices[i + 2]];
		TransformPoint(v0.x, v0.y, translation.x, translation.y, v0.x, v0.y);
		TransformPoint(v1.x, v1.y, translation.x, translation.y, v1.x, v1.y);
		TransformPoint(v2.x, v2.y, translation.x, translation.y, v2.x, v2.y);
		RasterizeTriangle(v0, v1, v2, nullptr, true, op);
	}

	if (operation == Rml::ClipMaskOperation::Intersect && m_stencilRef < 255)
		++m_stencilRef;
}

void RmlRenderGDI::SetTransform(const Rml::Matrix4f* transform)
{
	if (transform == nullptr)
	{
		m_hasTransform = false;
		return;
	}

	m_hasTransform = true;
	memcpy(m_mat, transform->data(), sizeof(m_mat));
}
