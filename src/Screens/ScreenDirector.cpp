#include "Screens/ScreenDirector.h"

#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "Core/utils.h"
#include "Core/logger.h"
#include "D3D9/QuadRenderer.h"
#include "Game/CharaSelectState.h"
#include "Game/SceneWatch.h"
#include "Screens/CharaGrid.h"
#include "Screens/PatFile.h"
#include "Screens/PatPainter.h"
#include "Screens/PatTextures.h"
#include "Screens/ScreenTheme.h"

#include <Windows.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr int kNoCharacter = CharaSelectState::kNoCharacter;

struct LayerState
{
	std::vector<int> frames;
	PatFile::Handle pat;
	int character;
};

struct Prepared
{
	const ScreenTheme::Screen* screen;
	PatFile::Handle pat;
	std::vector<LayerState> layers;
	bool usable;
};

struct SidePat
{
	std::string path;
	PatFile::Handle pat;
};

Prepared g_prepared = { nullptr, PatFile::kInvalid, {}, false };
std::vector<SidePat> g_sidePats;
uint64_t g_started = 0;
bool g_report = false;
char g_status[224] = "no theme";

std::string FrameName(const std::string& base, int frame)
{
	if (frame == 0)
		return base;

	size_t end = base.size();

	while (end > 0 && base[end - 1] >= '0' && base[end - 1] <= '9')
		--end;

	if (end == base.size())
		return base;

	const std::string digits = base.substr(end);
	const int value = atoi(digits.c_str()) + frame;

	char tail[16] = {};
	sprintf_s(tail, "%0*d", static_cast<int>(digits.size()), value);

	return base.substr(0, end) + tail;
}

std::string Expand(const std::string& form, const std::string& code, int side)
{
	std::string out;
	out.reserve(form.size() + code.size());

	for (size_t i = 0; i < form.size(); ++i)
	{
		if (form[i] != '%' || i + 1 >= form.size())
		{
			out.push_back(form[i]);
			continue;
		}

		const char what = form[i + 1];

		if (what == 's')
		{
			out.append(code);
			++i;
			continue;
		}

		if (what == 'p')
		{
			out.push_back(static_cast<char>('0' + side));
			++i;
			continue;
		}

		out.push_back(form[i]);
	}

	return out;
}

const std::string* CodeOf(const ScreenTheme::Screen& screen, int character)
{
	if (character < 0 || character >= static_cast<int>(screen.charaCodes.size()))
		return nullptr;

	const std::string& code = screen.charaCodes[static_cast<size_t>(character)];

	return code.empty() ? nullptr : &code;
}

PatFile::Handle SidePatOf(IDirect3DDevice9* device, const ScreenTheme::Theme& theme,
	const std::string& relative)
{
	for (const SidePat& known : g_sidePats)
	{
		if (known.path == relative)
			return known.pat;
	}

	SidePat entry = {};
	entry.path = relative;
	entry.pat = PatFile::Load(ScreenTheme::FilePath(theme, relative).c_str());

	if (entry.pat != PatFile::kInvalid && !PatTextures::Prepare(device, entry.pat))
		entry.pat = PatFile::kInvalid;

	if (entry.pat == PatFile::kInvalid)
		LOG("ScreenDirector: '%s' did not load - %s", relative.c_str(), PatFile::StatusText());

	g_sidePats.push_back(entry);
	return entry.pat;
}

int ResolveParts(PatFile::Handle pat, const std::string& base, int count, std::vector<int>& out)
{
	out.clear();

	if (pat == PatFile::kInvalid)
		return 0;

	for (int frame = 0; frame < count; ++frame)
	{
		const int part = PatFile::FindPartBySuffix(pat, FrameName(base, frame).c_str());

		if (part < 0)
			continue;

		out.push_back(part);
	}

	return static_cast<int>(out.size());
}

int ResolveFrames(PatFile::Handle pat, const std::string& base, int count, std::vector<int>& out)
{
	out.clear();

	if (pat == PatFile::kInvalid)
		return 0;

	for (int frame = 0; frame < count; ++frame)
	{
		const std::string name = FrameName(base, frame);
		int pattern = PatFile::FindPattern(pat, name.c_str());

		if (pattern < 0)
			pattern = PatFile::FindPattern(pat, (name + "_1").c_str());

		if (pattern < 0)
			continue;

		out.push_back(pattern);
	}

	return static_cast<int>(out.size());
}

void Prepare(IDirect3DDevice9* device, const ScreenTheme::Screen& screen)
{
	g_prepared.screen = &screen;
	g_prepared.layers.clear();
	g_prepared.usable = false;
	g_prepared.pat = PatFile::kInvalid;
	g_sidePats.clear();

	const ScreenTheme::Theme* const theme = ScreenTheme::Active();

	if (theme == nullptr || screen.pat.empty())
	{
		sprintf_s(g_status, "screen '%s' names no .pat", screen.name.c_str());
		return;
	}

	CharaSelectState::Layout layout = {};
	layout.pointer = screen.statePointer;
	layout.stride = screen.stateStride;
	layout.field = screen.stateField;
	CharaSelectState::Describe(layout);

	g_prepared.pat = PatFile::Load(ScreenTheme::FilePath(*theme, screen.pat).c_str());

	if (g_prepared.pat == PatFile::kInvalid)
	{
		sprintf_s(g_status, "%s", PatFile::StatusText());
		LOG("ScreenDirector: %s", g_status);
		return;
	}

	if (!PatTextures::Prepare(device, g_prepared.pat))
	{
		sprintf_s(g_status, "no atlas of %s became a texture", screen.pat.c_str());
		LOG("ScreenDirector: %s", g_status);
		return;
	}

	int resolved = 0;
	int missing = 0;

	for (const ScreenTheme::Layer& layer : screen.layers)
	{
		LayerState state = {};
		state.pat = g_prepared.pat;
		state.character = kNoCharacter;

		if (layer.pattern[0] == '@')
		{
			if (CharaGrid::Prepare(device))
			{
				CharaGrid::Style style = {};
				style.pat = g_prepared.pat;
				style.codes = &screen.charaCodes;
				style.cursor = layer.cursor;
				style.tag[0] = layer.tag[0];
				style.tag[1] = layer.tag[1];
				style.tagX[0] = layer.tagX[0];
				style.tagY[0] = layer.tagY[0];
				style.tagX[1] = layer.tagX[1];
				style.tagY[1] = layer.tagY[1];
				style.zoom = layer.zoom;
				style.spreadX = layer.spreadX;
				style.spreadY = layer.spreadY;
				style.fps = layer.fps;
				style.tint[0][0] = layer.tint[0][0];
				style.tint[0][1] = layer.tint[0][1];
				style.tint[1][0] = layer.tint[1][0];
				style.tint[1][1] = layer.tint[1][1];
				CharaGrid::SetStyle(style);
				++resolved;
			}
			else
			{
				++missing;
			}

			g_prepared.layers.push_back(state);
			continue;
		}

		if (layer.side > 0)
		{
			++resolved;
			g_prepared.layers.push_back(state);
			continue;
		}

		const int found = layer.part
			? ResolveParts(g_prepared.pat, layer.pattern, layer.count, state.frames)
			: ResolveFrames(g_prepared.pat, layer.pattern, layer.count, state.frames);

		resolved += found;
		missing += layer.count - found;

		if (found == 0)
			LOG("ScreenDirector: '%s' has no pattern '%s'", screen.pat.c_str(),
				layer.pattern.c_str());

		g_prepared.layers.push_back(state);
	}

	g_prepared.usable = resolved > 0;
	g_started = GetTickCount64();
	g_report = true;

	sprintf_s(g_status, "%s: %d layer(s), %d pattern(s), %d missing", screen.name.c_str(),
		static_cast<int>(screen.layers.size()), resolved, missing);

	LOG("ScreenDirector: %s", g_status);
}

void FeedCursors(const ScreenTheme::Screen& screen)
{
	for (int side = 0; side < CharaGrid::kSideCount; ++side)
	{
		const int character = CharaSelectState::CharacterOf(side);

		if (character >= 0)
		{
			CharaGrid::SetCursorFromId(side, static_cast<uint32_t>(character));
			continue;
		}

		if (screen.cursor[side] == 0)
			continue;

		uint32_t slot = 0;

		if (TryReadDword(reinterpret_cast<const void*>(RvaToAddress(screen.cursor[side])), slot))
			CharaGrid::SetCursorFromId(side, slot);
	}
}

int FrameOf(const ScreenTheme::Layer& layer, size_t count)
{
	if (count <= 1 || layer.fps <= 0)
		return 0;

	const uint64_t elapsed =
		(GetTickCount64() - g_started) * static_cast<uint64_t>(layer.fps) / 1000u;

	if (!layer.hold)
		return static_cast<int>(elapsed % count);

	return elapsed + 1 >= count ? static_cast<int>(count - 1) : static_cast<int>(elapsed);
}

int DrawSideLayer(IDirect3DDevice9* device, const ScreenTheme::Theme& theme,
	const ScreenTheme::Screen& screen, const ScreenTheme::Layer& layer, LayerState& state,
	const PatPainter::Placement& where)
{
	const int character = CharaSelectState::CharacterOf(layer.side - 1);
	const std::string* const code = CodeOf(screen, character);

	if (code == nullptr)
	{
		state.character = kNoCharacter;
		state.frames.clear();
		return 0;
	}

	if (state.character != character)
	{
		state.character = character;
		state.pat = layer.file.empty()
			? g_prepared.pat
			: SidePatOf(device, theme, Expand(layer.file, *code, layer.side));

		ResolveFrames(state.pat, Expand(layer.pattern, *code, layer.side), layer.count,
			state.frames);
	}

	if (state.frames.empty())
		return 0;

	const int pattern = state.frames[FrameOf(layer, state.frames.size())];

	return PatPainter::Draw(state.pat, pattern, where, layer.x, layer.y);
}

}

void ScreenDirector::Render(IDirect3DDevice9* device)
{
	if (kOnHold || device == nullptr || g_settings.screenThemeDrawn == 0)
		return;

	const ScreenTheme::Screen* const screen = ScreenTheme::ScreenForScene(SceneWatch::Current());

	if (screen == nullptr)
	{
		g_prepared.screen = nullptr;
		return;
	}

	if (screen != g_prepared.screen)
		Prepare(device, *screen);

	if (!g_prepared.usable)
		return;

	const ScreenTheme::Theme* const theme = ScreenTheme::Active();

	if (theme == nullptr)
		return;

	D3DVIEWPORT9 viewport = {};

	if (FAILED(device->GetViewport(&viewport)) || viewport.Width == 0 || viewport.Height == 0)
		return;

	const float width = static_cast<float>(viewport.Width);
	const float height = static_cast<float>(viewport.Height);
	const float byWidth = width / screen->designWidth;
	const float byHeight = height / screen->designHeight;

	PatPainter::Placement where = {};
	where.scale = byWidth < byHeight ? byWidth : byHeight;
	where.originX = width * 0.5f;

	const float drawn = screen->designHeight * where.scale;
	const float top = (height - drawn) * 0.5f;

	where.originY = screen->originTop ? top : top + drawn * 0.5f;

	if (!QuadRenderer::Begin(device))
		return;

	if (screen->cover)
		QuadRenderer::FillRect(0.0f, 0.0f, width, height, 0xFF000000);

	const bool report = g_report;
	g_report = false;

	if (report)
	{
		LOG("ScreenDirector: viewport %ux%u, scale %.3f, origin %.1f,%.1f", viewport.Width,
			viewport.Height, where.scale, where.originX, where.originY);
		LOG("ScreenDirector: 1P is character %d, 2P is character %d - %s",
			CharaSelectState::CharacterOf(0), CharaSelectState::CharacterOf(1),
			CharaSelectState::StatusText());
	}

	const unsigned elapsed = static_cast<unsigned>(GetTickCount64() - g_started);

	for (size_t i = 0; i < g_prepared.layers.size() && i < screen->layers.size(); ++i)
	{
		const ScreenTheme::Layer& layer = screen->layers[i];
		LayerState& state = g_prepared.layers[i];

		if (layer.pattern[0] == '@')
		{
			FeedCursors(*screen);

			const int painted = CharaGrid::Draw(where, elapsed);

			if (report)
				LOG("ScreenDirector: grid drew %d sprite(s) - %s", painted, CharaGrid::StatusText());

			continue;
		}

		if (layer.side > 0)
		{
			const int painted = DrawSideLayer(device, *theme, *screen, layer, state, where);

			if (report)
				LOG("ScreenDirector: side layer '%s' for %dP drew %d sprite(s) over %d frame(s)",
					layer.pattern.c_str(), layer.side, painted,
					static_cast<int>(state.frames.size()));

			continue;
		}

		if (state.frames.empty())
			continue;

		const int chosen = state.frames[FrameOf(layer, state.frames.size())];

		if (layer.part)
		{
			if (report)
				LOG("ScreenDirector: part layer '%s' at %.0f,%.0f", layer.pattern.c_str(), layer.x,
					layer.y);

			PatPainter::DrawPart(g_prepared.pat, chosen, where, layer.x, layer.y,
				layer.zoom > 0.0f ? layer.zoom : 1.0f, layer.tint[0][0]);

			continue;
		}

		const int painted = PatPainter::Draw(g_prepared.pat, chosen, where, layer.x, layer.y);

		if (report)
		{
			LOG("ScreenDirector: layer '%s' drew %d of %d sprite(s) - %s", layer.pattern.c_str(),
				painted, PatFile::SpriteCount(g_prepared.pat, chosen), PatPainter::LastReport());
		}
	}

	QuadRenderer::End();
}

void ScreenDirector::OnDeviceLost()
{
	Invalidate();
}

void ScreenDirector::Invalidate()
{
	PatTextures::OnDeviceLost();
	PatFile::UnloadAll();
	CharaGrid::Invalidate();

	g_sidePats.clear();
	g_prepared.screen = nullptr;
	g_prepared.pat = PatFile::kInvalid;
	g_prepared.layers.clear();
	g_prepared.usable = false;
}

bool ScreenDirector::IsDrawn()
{
	return !kOnHold && g_settings.screenThemeDrawn != 0;
}

void ScreenDirector::SetDrawn(bool drawn)
{
	if (kOnHold)
		return;

	g_settings.screenThemeDrawn = drawn ? 1 : 0;
	Settings::SaveInt("Theme", "ScreensDrawn", g_settings.screenThemeDrawn);
}

const char* ScreenDirector::StatusText()
{
	return g_status;
}
