#include "Overlay/CharaPreview.h"

#include "D3D9/DeviceHooks.h"
#include "Game/CgImage.h"

#include <d3d9.h>
#include <imgui.h>

#include <cstring>

namespace {

constexpr int kColours = 256;

// The panel backdrop, as an opaque ARGB pixel and as the matching ImGui colour.
constexpr uint32_t kBackground = 0xff18181cu;
constexpr ImU32 kBackdrop = IM_COL32(0x18, 0x18, 0x1c, 255);

}

CharaPreview::~CharaPreview()
{
	Release();
}

void CharaPreview::Release()
{
	if (m_texture != nullptr)
		m_texture->Release();

	m_texture = nullptr;
	m_chara = -1;
	m_frame = -1;
	m_width = 0;
	m_height = 0;
	m_painted = false;
}

int CharaPreview::GetFrameCount(int chara)
{
	if (!CgImage::Load(chara))
		return 0;

	return CgImage::GetFrameCount();
}

bool CharaPreview::Create(int width, int height)
{
	IDirect3DDevice9* const device = DeviceHooks::GetDevice();
	if (device == nullptr)
		return false;

	if (m_texture != nullptr)
	{
		m_texture->Release();
		m_texture = nullptr;
	}

	if (FAILED(device->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
		&m_texture, nullptr)))
	{
		m_texture = nullptr;
		return false;
	}

	m_width = width;
	m_height = height;
	return true;
}

// A face is two triangles, not a parallelogram. The three-corner inverse map is exact only when
// the fourth corner falls where the other three put it, which a plane satisfies and an annular
// segment does not - and the perspective divide breaks it even for a plane, since each corner is
// divided by its own depth. Interpolating across triangles is what the renderer itself does.
void CharaPreview::DrawTriangle(const PatParts::Quad& quad, const Vertex& a, const Vertex& b,
	const Vertex& c, const uint8_t* tint, uint8_t* pixels, int pitch) const
{
	const float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);

	if (area > -0.0001f && area < 0.0001f)
		return;

	float left = a.x < b.x ? a.x : b.x;
	float right = a.x > b.x ? a.x : b.x;
	float top = a.y < b.y ? a.y : b.y;
	float bottom = a.y > b.y ? a.y : b.y;

	left = c.x < left ? c.x : left;
	right = c.x > right ? c.x : right;
	top = c.y < top ? c.y : top;
	bottom = c.y > bottom ? c.y : bottom;

	int x0 = static_cast<int>(left) - 1;
	int y0 = static_cast<int>(top) - 1;
	int x1 = static_cast<int>(right) + 2;
	int y1 = static_cast<int>(bottom) + 2;

	x0 = x0 < 0 ? 0 : x0;
	y0 = y0 < 0 ? 0 : y0;
	x1 = x1 > m_width ? m_width : x1;
	y1 = y1 > m_height ? m_height : y1;

	for (int y = y0; y < y1; ++y)
	{
		uint32_t* const target =
			reinterpret_cast<uint32_t*>(pixels + static_cast<size_t>(y) * pitch);

		for (int x = x0; x < x1; ++x)
		{
			const float px = static_cast<float>(x);
			const float py = static_cast<float>(y);

			const float wa = ((b.x - px) * (c.y - py) - (c.x - px) * (b.y - py)) / area;
			const float wb = ((c.x - px) * (a.y - py) - (a.x - px) * (c.y - py)) / area;
			const float wc = 1.0f - wa - wb;

			// A shared edge must not drop a pixel, so admit the boundary generously.
			if (wa < -0.001f || wb < -0.001f || wc < -0.001f)
				continue;

			int u = static_cast<int>((wa * a.u + wb * b.u + wc * c.u) * quad.width);
			int v = static_cast<int>((wa * a.v + wb * b.v + wc * c.v) * quad.height);

			u = u >= quad.width ? quad.width - 1 : (u < 0 ? 0 : u);
			v = v >= quad.height ? quad.height - 1 : (v < 0 ? 0 : v);

			const uint8_t* const texel = quad.pixels + (static_cast<size_t>(v) * quad.width + u) * 4;

			// col = texel * tint; col.rgb += add; then blend by col.a.
			const uint32_t alpha = static_cast<uint32_t>(texel[3]) * quad.multiply[3] / 255;
			if (alpha == 0)
				continue;

			uint32_t source[3] = {};

			for (int k = 0; k < 3; ++k)
			{
				const uint32_t lit = static_cast<uint32_t>(texel[k]) * tint[k] / 255
					* quad.multiply[k] / 255 + quad.add[k];

				source[k] = lit > 255 ? 255 : lit;
			}

			const uint32_t behind = target[x];

			uint32_t mixed[3] = {};

			for (int k = 0; k < 3; ++k)
			{
				const uint32_t under = (behind >> (16 - k * 8)) & 0xff;

				const uint32_t blended = quad.additive
					? under + source[k] * alpha / 255
					: (under * (255 - alpha) + source[k] * alpha) / 255;

				mixed[k] = blended > 255 ? 255 : blended;
			}

			target[x] = 0xff000000u | (mixed[0] << 16) | (mixed[1] << 8) | mixed[2];
		}
	}
}

void CharaPreview::DrawQuad(const PatParts::Quad& quad, float offsetX, float offsetY,
	const uint8_t* palette, uint8_t* pixels, int pitch) const
{
	// No colour slot means no tint, which the shader expresses as multiplying by white.
	constexpr uint8_t kWhite[4] = { 255, 255, 255, 255 };
	const uint8_t* const tint = quad.colour >= 0 ? palette + quad.colour * 4 : kWhite;

	const float u[4] = { quad.u0, quad.u1, quad.u1, quad.u0 };
	const float v[4] = { quad.v0, quad.v0, quad.v1, quad.v1 };

	Vertex corner[4] = {};

	for (int i = 0; i < 4; ++i)
	{
		corner[i].x = quad.x[i] + offsetX;
		corner[i].y = quad.y[i] + offsetY;
		corner[i].u = u[i];
		corner[i].v = v[i];
	}

	DrawTriangle(quad, corner[0], corner[1], corner[2], tint, pixels, pitch);
	DrawTriangle(quad, corner[0], corner[2], corner[3], tint, pixels, pitch);
}

void CharaPreview::DrawParts(bool behind, const uint8_t* palette, uint8_t* pixels, int pitch) const
{
	const CgImage::PartLayer* layers = nullptr;
	int count = 0;

	if (!CgImage::GetFrameParts(m_frame, layers, count))
		return;

	for (int i = 0; i < count; ++i)
	{
		if (layers[i].behind != behind)
			continue;

		for (int k = 0; k < layers[i].count; ++k)
			DrawQuad(layers[i].quads[k], layers[i].offsetX, layers[i].offsetY, palette, pixels, pitch);
	}
}

bool CharaPreview::Paint(const uint8_t* indices, const uint8_t* palette)
{
	if (m_texture == nullptr)
		return false;

	uint32_t lookup[kColours] = {};

	for (int colour = 1; colour < kColours; ++colour)
	{
		const uint8_t* const entry = palette + colour * 4;
		const uint32_t alpha = entry[3] != 0 ? 0xffu : 0u;

		lookup[colour] = (alpha << 24) | (static_cast<uint32_t>(entry[0]) << 16)
			| (static_cast<uint32_t>(entry[1]) << 8) | entry[2];
	}

	D3DLOCKED_RECT locked = {};
	if (FAILED(m_texture->LockRect(0, &locked, nullptr, 0)))
		return false;

	uint8_t* const pixels = static_cast<uint8_t*>(locked.pBits);

	// Additive parts add light to what is under them, so the canvas has to be the opaque backdrop
	// they are seen against rather than transparent - otherwise the glow is blended twice.
	for (int y = 0; y < m_height; ++y)
	{
		uint32_t* const row =
			reinterpret_cast<uint32_t*>(pixels + static_cast<size_t>(y) * locked.Pitch);

		for (int x = 0; x < m_width; ++x)
			row[x] = kBackground;
	}

	DrawParts(true, palette, pixels, locked.Pitch);

	for (int y = 0; y < m_height; ++y)
	{
		uint32_t* const target =
			reinterpret_cast<uint32_t*>(pixels + static_cast<size_t>(y) * locked.Pitch);
		const uint8_t* const source = indices + static_cast<size_t>(y) * m_width;

		for (int x = 0; x < m_width; ++x)
		{
			if (source[x] != 0)
				target[x] = lookup[source[x]];
		}
	}

	DrawParts(false, palette, pixels, locked.Pitch);

	m_texture->UnlockRect(0);

	memcpy(m_palette, palette, sizeof(m_palette));
	m_painted = true;
	return true;
}

bool CharaPreview::Ensure(int chara, int frame, const uint8_t* palette)
{
	if (!CgImage::Load(chara))
		return false;

	int width = 0;
	int height = 0;
	const uint8_t* indices = nullptr;

	if (!CgImage::GetFrame(frame, width, height, indices))
		return false;

	const bool sameFrame = m_texture != nullptr && m_chara == chara && m_frame == frame;

	if (sameFrame && m_painted && memcmp(m_palette, palette, sizeof(m_palette)) == 0)
		return true;

	if (!sameFrame)
	{
		m_painted = false;

		if (!Create(width, height))
			return false;

		m_chara = chara;
		m_frame = frame;
	}

	return Paint(indices, palette);
}

bool CharaPreview::Draw(int chara, int frame, const uint8_t* palette, float maxWidth,
	float maxHeight)
{
	if (palette == nullptr || !Ensure(chara, frame, palette))
		return false;

	float width = static_cast<float>(m_width);
	float height = static_cast<float>(m_height);

	const float wide = maxWidth > 0.0f ? maxWidth / width : 1.0f;
	const float tall = maxHeight > 0.0f ? maxHeight / height : 1.0f;

	const float fit = wide < tall ? wide : tall;

	if (fit < 1.0f)
	{
		width *= fit;
		height *= fit;
	}

	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const ImVec2 corner = ImVec2(origin.x + width, origin.y + height);

	ImDrawList* const draw = ImGui::GetWindowDrawList();

	draw->AddRectFilled(origin, corner, kBackdrop);

	const ImTextureID handle = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(m_texture));
	draw->AddImage(handle, origin, corner);

	draw->AddRect(origin, corner, IM_COL32(120, 120, 130, 255));

	ImGui::Dummy(ImVec2(width, height));
	return true;
}
