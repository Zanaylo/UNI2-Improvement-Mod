#include "D3D9/DgVoodoo.h"

#include "Core/Settings.h"
#include "Core/logger.h"
#include "Core/utils.h"

#include <cstdio>
#include <cstring>
#include <mutex>

namespace {

constexpr const char* kSection = "dgVoodoo";
constexpr const char* kFolder = "dgVoodoo\\";
constexpr const char* kRuntime = "D3D9.dll";
constexpr const char* kConfig = "dgVoodoo.conf";
constexpr const char* const kSources[] = { "MS\\x86\\", "x86\\", "" };
constexpr const char* const kLibraries[] = { "D3D9.dll", "D3D8.dll", "D3DImm.dll", "DDraw.dll" };
constexpr int kMostFps = 360;

constexpr const char* const kResolutionLabels[] = { "The game's own", "1920x1080", "2560x1440",
	"3840x2160", "Desktop" };
constexpr const char* const kResolutionValues[] = { "unforced", "h:1920, v:1080", "h:2560, v:1440",
	"h:3840, v:2160", "desktop" };

constexpr const char* const kScalingLabels[] = { "Unspecified", "Centered", "Stretched",
	"Centered, keep aspect", "Stretched, keep aspect" };
constexpr const char* const kScalingValues[] = { "unspecified", "centered", "stretched",
	"centered_ar", "stretched_ar" };

constexpr const char* const kFilteringLabels[] = { "The game's own", "Point", "Bilinear",
	"Trilinear", "Anisotropic 16x" };
constexpr const char* const kFilteringValues[] = { "appdriven", "pointsampled", "bilinear",
	"trilinear", "16" };

constexpr const char* const kAntialiasingLabels[] = { "Off", "2x", "4x", "8x" };
constexpr const char* const kAntialiasingValues[] = { "off", "2x", "4x", "8x" };

constexpr const char* const kOutputLabels[] = { "Best available", "Direct3D 11", "Direct3D 12" };
constexpr const char* const kOutputValues[] = { "bestavailable", "d3d11_fl11_0", "d3d12_fl12_0" };

struct ChoiceSpec
{
	const char* name;
	const char* key;
	int fallback;
	const char* const* labels;
	const char* const* values;
	int count;
};

constexpr ChoiceSpec kChoices[DgVoodoo::Choice_COUNT] = {
	{ "Resolution", "Resolution", 2, kResolutionLabels, kResolutionValues,
		static_cast<int>(_countof(kResolutionValues)) },
	{ "Scaling", "ScalingMode", 0, kScalingLabels, kScalingValues,
		static_cast<int>(_countof(kScalingValues)) },
	{ "Texture filtering", "Filtering", 0, kFilteringLabels, kFilteringValues,
		static_cast<int>(_countof(kFilteringValues)) },
	{ "Anti-aliasing", "Antialiasing", 0, kAntialiasingLabels, kAntialiasingValues,
		static_cast<int>(_countof(kAntialiasingValues)) },
	{ "Output", "OutputAPI", 0, kOutputLabels, kOutputValues,
		static_cast<int>(_countof(kOutputValues)) },
};

struct FlagSpec
{
	const char* name;
	const char* key;
	bool fallback;
};

constexpr FlagSpec kFlags[DgVoodoo::Flag_COUNT] = {
	{ "Fake fullscreen", "FakeFullscreen", true },
	{ "Force vsync", "VSync", false },
	{ "Keep the window's aspect ratio", "KeepAspect", false },
	{ "Keep the mouse inside the window", "CaptureMouse", false },
	{ "Show the dgVoodoo watermark", "Watermark", false },
};

std::mutex g_loadLock;
bool g_read = false;
bool g_enabled = false;
bool g_loadTried = false;
bool g_failed = false;
int g_chosen[DgVoodoo::Choice_COUNT] = {};
bool g_set[DgVoodoo::Flag_COUNT] = {};
int g_fps = 0;
HMODULE g_module = nullptr;

char g_status[320] = "not asked yet";

int Clamp(int value, int low, int high)
{
	if (value < low)
		return low;

	return value > high ? high : value;
}

int ReadInt(const char* key, int fallback)
{
	return static_cast<int>(GetPrivateProfileIntA(kSection, key, fallback,
		Settings::GetIniPath().c_str()));
}

void Read()
{
	if (g_read)
		return;

	for (int i = 0; i < DgVoodoo::Choice_COUNT; ++i)
		g_chosen[i] = Clamp(ReadInt(kChoices[i].key, kChoices[i].fallback), 0, kChoices[i].count - 1);

	for (int i = 0; i < DgVoodoo::Flag_COUNT; ++i)
		g_set[i] = ReadInt(kFlags[i].key, kFlags[i].fallback ? 1 : 0) != 0;

	g_fps = Clamp(ReadInt("FpsLimit", 0), 0, kMostFps);
	g_enabled = ReadInt("Enabled", 0) != 0;
	g_read = true;
}

bool Exists(const std::string& path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

const char* Bool(bool value)
{
	return value ? "true" : "false";
}

const char* Value(DgVoodoo::Choice choice)
{
	return kChoices[choice].values[g_chosen[choice]];
}

std::string Config()
{
	char text[6144] = {};

	sprintf_s(text,
		"Version                              = 0x280\n"
		"\n"
		"[General]\n"
		"\n"
		"OutputAPI                            = %s\n"
		"Adapters                             = all\n"
		"FullScreenOutput                     = default\n"
		"FullScreenMode                       = true\n"
		"ScalingMode                          = %s\n"
		"ProgressiveScanlineOrder             = false\n"
		"EnumerateRefreshRates                = false\n"
		"\n"
		"Brightness                           = 100\n"
		"Color                                = 100\n"
		"Contrast                             = 100\n"
		"InheritColorProfileInFullScreenMode  = true\n"
		"\n"
		"KeepWindowAspectRatio                = %s\n"
		"CaptureMouse                         = %s\n"
		"CenterAppWindow                      = false\n"
		"\n"
		"[GeneralExt]\n"
		"\n"
		"DesktopResolution                    = \n"
		"DesktopBitDepth                      = \n"
		"DeframerSize                         = 1\n"
		"ImageScaleFactor                     = 1\n"
		"CursorScaleFactor                    = 0\n"
		"DisplayROI                           = \n"
		"Resampling                           = pointsampled\n"
		"PresentationModel                    = auto\n"
		"FreeMouse                            = true\n"
		"WindowedAttributes                   = \n"
		"FullscreenAttributes                 = %s\n"
		"FPSLimit                             = %d\n"
		"Environment                          = \n"
		"SystemHookFlags                      = \n"
		"\n"
		"[DirectX]\n"
		"\n"
		"DisableAndPassThru                  = false\n"
		"\n"
		"VideoCard                           = internal3D\n"
		"VRAM                                = 2048\n"
		"Filtering                           = %s\n"
		"KeepFilterIfPointSampled            = true\n"
		"DisableMipmapping                   = false\n"
		"Resolution                          = %s\n"
		"Antialiasing                        = %s\n"
		"\n"
		"AppControlledScreenMode             = true\n"
		"DisableAltEnterToToggleScreenMode   = false\n"
		"\n"
		"Bilinear2DOperations                = false\n"
		"PhongShadingWhenPossible            = false\n"
		"ForceVerticalSync                   = %s\n"
		"dgVoodooWatermark                   = %s\n"
		"FastVideoMemoryAccess               = true\n"
		"\n"
		"[DirectXExt]\n"
		"\n"
		"AdapterIDType                       = \n"
		"VendorID                            = \n"
		"DeviceID                            = \n"
		"SubsystemID                         = \n"
		"RevisionID                          = \n"
		"\n"
		"DefaultEnumeratedResolutions        = none\n"
		"ExtraEnumeratedResolutions          = 1280x720@60\n"
		"EnumeratedResolutionBitdepths       = all\n"
		"\n"
		"DitheringEffect                     = pure32bit\n"
		"Dithering                           = appdriven\n"
		"DitherOrderedMatrixSizeScale        = 0\n"
		"DepthBuffersBitDepth                = appdriven\n"
		"Default3DRenderFormat               = argb8888\n"
		"\n"
		"MaxVSConstRegisters                 = 256\n"
		"NPatchTesselationLevel              = 0\n"
		"DisplayOutputEnableMask             = 0xffffffff\n"
		"\n"
		"MSD3DDeviceNames                    = false\n"
		"RTTexturesForceScaleAndMSAA         = true\n"
		"SmoothedDepthSampling               = true\n"
		"DeferredScreenModeSwitch            = false\n"
		"PrimarySurfaceBatchedUpdate         = false\n",
		Value(DgVoodoo::Choice_Output), Value(DgVoodoo::Choice_Scaling),
		Bool(g_set[DgVoodoo::Flag_KeepAspect]), Bool(g_set[DgVoodoo::Flag_CaptureMouse]),
		g_set[DgVoodoo::Flag_FakeFullscreen] ? "Fake" : "", g_fps,
		Value(DgVoodoo::Choice_Filtering), Value(DgVoodoo::Choice_Resolution),
		Value(DgVoodoo::Choice_Antialiasing), Bool(g_set[DgVoodoo::Flag_VSync]),
		Bool(g_set[DgVoodoo::Flag_Watermark]));

	return text;
}

bool WriteConfig()
{
	const std::string folder = DgVoodoo::Folder();

	if (!Exists(folder + kRuntime))
		return false;

	const std::string path = folder + kConfig;
	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "w") != 0 || handle == nullptr)
	{
		LOG("DgVoodoo: %s could not be written", path.c_str());
		return false;
	}

	const std::string text = Config();

	fwrite(text.data(), 1, text.size(), handle);
	fclose(handle);

	return true;
}

std::string SourceIn(const std::string& picked)
{
	std::string root = picked;

	if (!root.empty() && root.back() != '\\')
		root += '\\';

	for (const char* const sub : kSources)
	{
		const std::string candidate = root + sub;

		if (Exists(candidate + kRuntime))
			return candidate;
	}

	return std::string();
}

}

HMODULE DgVoodoo::Load()
{
	std::lock_guard<std::mutex> lock(g_loadLock);

	if (g_loadTried)
		return g_module;

	g_loadTried = true;

	Read();

	if (!g_enabled)
		return nullptr;

	const std::string runtime = Folder() + kRuntime;

	if (!Exists(runtime))
	{
		g_failed = true;
		sprintf_s(g_status, "On, but %s is missing. Pick the dgVoodoo folder again.",
			runtime.c_str());
		LOG("DgVoodoo: %s", g_status);
		return nullptr;
	}

	WriteConfig();

	g_module = LoadLibraryA(runtime.c_str());

	if (g_module == nullptr)
	{
		g_failed = true;
		sprintf_s(g_status, "%s failed to load (error %lu), so the game uses the normal "
			"Direct3D 9", runtime.c_str(), GetLastError());
		LOG("DgVoodoo: %s", g_status);
		return nullptr;
	}

	sprintf_s(g_status, "Running: the game draws through %s", runtime.c_str());
	LOG("DgVoodoo: %s", g_status);

	return g_module;
}

bool DgVoodoo::IsInstalled()
{
	return Exists(Folder() + kRuntime);
}

bool DgVoodoo::IsRunning()
{
	return g_module != nullptr;
}

bool DgVoodoo::IsEnabled()
{
	Read();
	return g_enabled;
}

void DgVoodoo::SetEnabled(bool enabled)
{
	Read();
	g_enabled = enabled;
	Save();
}

int DgVoodoo::ChoiceCount(Choice choice)
{
	return kChoices[choice].count;
}

const char* DgVoodoo::ChoiceName(Choice choice)
{
	return kChoices[choice].name;
}

const char* DgVoodoo::ChoiceLabel(Choice choice, int option)
{
	return kChoices[choice].labels[Clamp(option, 0, kChoices[choice].count - 1)];
}

int DgVoodoo::Chosen(Choice choice)
{
	Read();
	return g_chosen[choice];
}

void DgVoodoo::Choose(Choice choice, int option)
{
	Read();
	g_chosen[choice] = Clamp(option, 0, kChoices[choice].count - 1);
	Save();
}

const char* DgVoodoo::FlagName(Flag flag)
{
	return kFlags[flag].name;
}

bool DgVoodoo::IsSet(Flag flag)
{
	Read();
	return g_set[flag];
}

void DgVoodoo::Set(Flag flag, bool set)
{
	Read();
	g_set[flag] = set;
	Save();
}

int DgVoodoo::FpsLimit()
{
	Read();
	return g_fps;
}

void DgVoodoo::SetFpsLimit(int fps)
{
	Read();
	g_fps = Clamp(fps, 0, kMostFps);
}

void DgVoodoo::Save()
{
	Read();

	Settings::SaveInt(kSection, "Enabled", g_enabled ? 1 : 0);

	for (int i = 0; i < Choice_COUNT; ++i)
		Settings::SaveInt(kSection, kChoices[i].key, g_chosen[i]);

	for (int i = 0; i < Flag_COUNT; ++i)
		Settings::SaveInt(kSection, kFlags[i].key, g_set[i] ? 1 : 0);

	Settings::SaveInt(kSection, "FpsLimit", g_fps);

	WriteConfig();
}

bool DgVoodoo::InstallFrom(const std::string& folder, char* status, int statusSize)
{
	const std::string source = SourceIn(folder);

	if (source.empty())
	{
		sprintf_s(status, statusSize, "No %s in that folder or in its MS\\x86 folder",
			kRuntime);
		return false;
	}

	const std::string target = Folder();

	CreateDirectoryA(target.c_str(), nullptr);

	int copied = 0;

	for (const char* const library : kLibraries)
	{
		if (!Exists(source + library))
			continue;

		if (CopyFileA((source + library).c_str(), (target + library).c_str(), FALSE))
		{
			++copied;
			continue;
		}

		LOG("DgVoodoo: %s could not be copied into %s (error %lu)", library, target.c_str(),
			GetLastError());
	}

	if (!IsInstalled())
	{
		sprintf_s(status, statusSize, "Could not copy %s into %s", kRuntime, target.c_str());
		return false;
	}

	Read();
	WriteConfig();

	sprintf_s(status, statusSize, "%d file(s) copied from %s", copied, source.c_str());
	LOG("DgVoodoo: %s", status);

	return true;
}

std::string DgVoodoo::Folder()
{
	return GetModRootPath(kFolder);
}

const char* DgVoodoo::StatusText()
{
	Read();

	if (g_module != nullptr || g_failed)
		return g_status;

	if (!IsInstalled())
		return "Not installed. Pick the folder you extracted dgVoodoo2 into.";

	if (g_enabled)
		return "On. Restart the game to use it.";

	return "Installed but off. The game uses the normal Direct3D 9.";
}
