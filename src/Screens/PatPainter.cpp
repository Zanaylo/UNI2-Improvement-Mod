#include "Screens/PatPainter.h"

#include "D3D9/QuadRenderer.h"
#include "Screens/PatTextures.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

constexpr float k2Pi = 6.2831853f;

std::vector<int> g_order;
int g_noPart = 0;
int g_noRect = 0;
int g_noTexture = 0;
int g_noSize = 0;
int g_firstPart = -1;
int g_firstAtlas = -1;
float g_firstRect[4] = {};
uint32_t g_firstTint = 0;
char g_report[256] = "nothing drawn yet";

bool Paint(PatFile::Handle handle, const PatFile::Part& part, const PatPainter::Placement& where,
	float x, float y, float zoomX, float zoomY, float turns, uint32_t tint)
{
	IDirect3DTexture9* const texture = PatTextures::Get(handle, part.atlas);

	if (texture == nullptr)
	{
		++g_noTexture;

		if (g_firstAtlas < 0)
			g_firstAtlas = part.atlas;

		return false;
	}

	const float width = part.width * zoomX * where.scale;
	const float height = part.height * zoomY * where.scale;

	if (width <= 0.0f || height <= 0.0f)
	{
		++g_noSize;
		return false;
	}

	int atlasWidth = 0;
	int atlasHeight = 0;

	if (!PatFile::AtlasSize(handle, part.atlas, atlasWidth, atlasHeight) || atlasWidth <= 0 ||
		atlasHeight <= 0)
	{
		return false;
	}

	const float insetU = 0.5f / atlasWidth;
	const float insetV = 0.5f / atlasHeight;
	const float u0 = part.u / 256.0f + insetU;
	const float v0 = part.v / 256.0f + insetV;
	const float u1 = (part.u + part.w) / 256.0f - insetU;
	const float v1 = (part.v + part.h) / 256.0f - insetV;

	QuadRenderer::TexturedRectRotated(texture, x, y, width, height,
		part.pivotX * zoomX * where.scale, part.pivotY * zoomY * where.scale, -turns * k2Pi,
		u0, v0, u1, v1, tint);

	if (g_firstRect[2] == 0.0f)
	{
		g_firstRect[0] = x;
		g_firstRect[1] = y;
		g_firstRect[2] = width;
		g_firstRect[3] = height;
		g_firstTint = tint;
	}

	return true;
}

}

int PatPainter::Draw(PatFile::Handle handle, int pattern, const Placement& where, float offsetX,
	float offsetY)
{
	const int count = PatFile::SpriteCount(handle, pattern);
	const PatFile::Sprite* sprites = PatFile::GetSprites(handle, pattern);

	if (count <= 0 || sprites == nullptr)
		return 0;

	g_order.resize(static_cast<size_t>(count));

	for (int i = 0; i < count; ++i)
		g_order[i] = i;

	std::stable_sort(g_order.begin(), g_order.end(), [sprites](int a, int b) {
		return sprites[a].priority > sprites[b].priority;
	});

	int drawn = 0;

	g_noPart = 0;
	g_noRect = 0;
	g_noTexture = 0;
	g_noSize = 0;
	g_firstPart = -1;
	g_firstAtlas = -1;
	g_firstRect[0] = 0.0f;
	g_firstRect[1] = 0.0f;
	g_firstRect[2] = 0.0f;
	g_firstRect[3] = 0.0f;
	g_firstTint = 0;

	for (int index : g_order)
	{
		const PatFile::Sprite& sprite = sprites[index];

		PatFile::Part part = {};

		if (!PatFile::GetPart(handle, sprite.part, part))
		{
			++g_noPart;

			if (g_firstPart < 0)
				g_firstPart = sprite.part;

			continue;
		}

		if (part.width <= 0 || part.height <= 0 || part.w <= 0 || part.h <= 0)
		{
			++g_noRect;
			continue;
		}

		const float zoomX = sprite.zoomX > 0.0f ? sprite.zoomX : 1.0f;
		const float zoomY = sprite.zoomY > 0.0f ? sprite.zoomY : 1.0f;
		const float x = where.originX + (offsetX + sprite.x - part.pivotX * zoomX) * where.scale;
		const float y = where.originY + (offsetY + sprite.y - part.pivotY * zoomY) * where.scale;

		QuadRenderer::SetBlend(sprite.blend);

		if (Paint(handle, part, where, x, y, zoomX, zoomY, sprite.turns, sprite.tint))
			++drawn;
	}

	QuadRenderer::SetBlend(QuadRenderer::Blend_Normal);

	sprintf_s(g_report, "first at %.0f,%.0f %.0fx%.0f tint %08X; skipped %d no part (first %d), "
		"%d no rect, %d no texture (first atlas %d), %d no size", g_firstRect[0], g_firstRect[1],
		g_firstRect[2], g_firstRect[3], g_firstTint, g_noPart, g_firstPart, g_noRect, g_noTexture,
		g_firstAtlas, g_noSize);

	return drawn;
}

bool PatPainter::DrawPart(PatFile::Handle handle, int id, const Placement& where, float atX,
	float atY, float zoom, uint32_t tint)
{
	PatFile::Part part = {};

	if (!PatFile::GetPart(handle, id, part) || part.width <= 0 || part.height <= 0)
		return false;

	const float x = where.originX + (atX - part.pivotX * zoom) * where.scale;
	const float y = where.originY + (atY - part.pivotY * zoom) * where.scale;

	QuadRenderer::SetBlend(QuadRenderer::Blend_Normal);

	return Paint(handle, part, where, x, y, zoom, zoom, 0.0f, tint);
}

const char* PatPainter::LastReport()
{
	return g_report;
}
