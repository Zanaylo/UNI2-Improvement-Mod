#include "Overlay/OverlayFont.h"

#include "Core/interfaces.h"
#include "Core/logger.h"

#include <imgui.h>

#include <Windows.h>

#include <cstdio>
#include <string>

namespace {

const char* const kCandidates[] = {
	"segoeui.ttf",
	"tahoma.ttf",
	"verdana.ttf",
	"arial.ttf",
	"DejaVuSans.ttf",
	"LiberationSans-Regular.ttf",
	"FreeSans.ttf",
};

constexpr int kCandidateCount = sizeof(kCandidates) / sizeof(kCandidates[0]);

char g_status[320] = "not loaded";

std::string FontsDirectory()
{
	char buffer[MAX_PATH] = {};
	const UINT length = GetWindowsDirectoryA(buffer, MAX_PATH);

	if (length == 0 || length >= MAX_PATH)
		return std::string();

	return std::string(buffer, length) + "\\Fonts\\";
}

bool FileExists(const std::string& path)
{
	const DWORD attributes = GetFileAttributesA(path.c_str());

	return attributes != INVALID_FILE_ATTRIBUTES &&
		(attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool AddFace(const std::string& path, float size)
{
	if (path.empty() || !FileExists(path))
		return false;

	ImFontConfig config;
	config.SizePixels = size;

	return ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), size, &config) != nullptr;
}

}

void OverlayFont::Load()
{
	const float size = g_modVals.fontSize;

	if (AddFace(g_settings.fontPath, size))
	{
		snprintf(g_status, sizeof(g_status), "%s at %.0fpx", g_settings.fontPath.c_str(),
			static_cast<double>(size));
		LOG("[OverlayFont] %s", g_status);
		return;
	}

	if (!g_settings.fontPath.empty())
	{
		LOG("[OverlayFont] %s could not be loaded, falling back to a system face",
			g_settings.fontPath.c_str());
	}

	const std::string directory = FontsDirectory();

	for (int i = 0; i < kCandidateCount; ++i)
	{
		if (!AddFace(directory + kCandidates[i], size))
			continue;

		snprintf(g_status, sizeof(g_status), "%s at %.0fpx", kCandidates[i],
			static_cast<double>(size));
		LOG("[OverlayFont] %s", g_status);
		return;
	}

	ImGui::GetIO().Fonts->AddFontDefault();

	snprintf(g_status, sizeof(g_status), "no scalable face found in %s - using the stock bitmap "
		"font, which will look soft above 1080p. Set [Overlay] FontPath to a .ttf.",
		directory.c_str());

	LOG("[OverlayFont] %s", g_status);
}

const char* OverlayFont::GetStatusText()
{
	return g_status;
}
