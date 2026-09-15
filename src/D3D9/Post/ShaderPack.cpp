#include "D3D9/Post/ShaderPack.h"

#include "Core/Settings.h"
#include "Core/info.h"
#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/Post/BundledShaders.h"
#include "D3D9/Post/ShaderSource.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int kMaxPacks = 64;
constexpr const char* kEntryPoint = "main";
constexpr const char* kTarget = "ps_3_0";
constexpr const char* kReadmeName = "README.txt";
constexpr const char* kInstallMarker = "installed.txt";
constexpr const char* kInstallStamp = UNI2_IM_VERSION " packs 3";

using D3DCompile_t = HRESULT(WINAPI*)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*,
	ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);

HMODULE g_compilerModule = nullptr;
D3DCompile_t g_compile = nullptr;
bool g_compilerTried = false;

std::vector<std::string> g_names;
std::string g_folder;
int g_selected = -1;

IDirect3DPixelShader9* g_shader = nullptr;
int g_compiled = -1;
bool g_compileFailed = false;

char g_status[512] = "no shader pack selected";
bool g_installed = false;

void Report(const char* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	vsnprintf(g_status, sizeof(g_status), format, arguments);
	va_end(arguments);
}

bool EnsureCompiler()
{
	if (g_compilerTried)
		return g_compile != nullptr;

	g_compilerTried = true;
	g_compilerModule = LoadLibraryA("d3dcompiler_47.dll");

	if (g_compilerModule == nullptr)
		g_compilerModule = LoadLibraryA("d3dcompiler_43.dll");

	if (g_compilerModule == nullptr)
	{
		LOG("[ShaderPack] no d3dcompiler on this system, user shaders are unavailable");
		return false;
	}

	g_compile = reinterpret_cast<D3DCompile_t>(GetProcAddress(g_compilerModule, "D3DCompile"));

	if (g_compile == nullptr)
		LOG("[ShaderPack] d3dcompiler loaded but has no D3DCompile export");

	return g_compile != nullptr;
}

void ReleaseShader()
{
	if (g_shader != nullptr)
		g_shader->Release();

	g_shader = nullptr;
	g_compiled = -1;
	g_compileFailed = false;
}

int IndexOf(const std::string& name)
{
	for (size_t i = 0; i < g_names.size(); ++i)
	{
		if (_stricmp(g_names[i].c_str(), name.c_str()) == 0)
			return static_cast<int>(i);
	}

	return -1;
}

bool HasExtension(const char* name, const char* extension)
{
	const size_t nameLength = strlen(name);
	const size_t extensionLength = strlen(extension);

	if (nameLength <= extensionLength)
		return false;

	return _stricmp(name + nameLength - extensionLength, extension) == 0;
}

void ScanExtension(const char* extension)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((g_folder + "*" + extension).c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		if (!HasExtension(found.cFileName, extension))
			continue;

		if (static_cast<int>(g_names.size()) >= kMaxPacks)
			break;

		g_names.push_back(found.cFileName);
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
}

void Scan()
{
	g_names.clear();

	for (int i = 0; i < ShaderSource::ExtensionCount(); ++i)
		ScanExtension(ShaderSource::ExtensionAt(i));

	std::sort(g_names.begin(), g_names.end());
}

bool WriteBytes(const std::string& path, const void* data, size_t size);
void SaveTranslated(const std::string& name, const std::string& hlsl);

bool CompileSelected(IDirect3DDevice9* device)
{
	std::vector<uint8_t> source;
	const std::string& name = g_names[g_selected];
	const std::string path = g_folder + name;

	if (!ReadWholeFile(path, source, 1))
	{
		Report("%s could not be read", name.c_str());
		return false;
	}

	const ShaderSource::Format format = ShaderSource::DetectFormat(name.c_str());

	std::string hlsl;
	std::string note;

	if (!ShaderSource::Translate(format,
		std::string(reinterpret_cast<const char*>(source.data()), source.size()), hlsl, note))
	{
		Report("%s is a %s shader and %s", name.c_str(), ShaderSource::FormatName(format),
			note.c_str());

		LOG("[ShaderPack] %s", g_status);
		return false;
	}

	if (!ShaderSource::IsNative(format))
		SaveTranslated(name, hlsl);

	ID3DBlob* bytecode = nullptr;
	ID3DBlob* errors = nullptr;

	const HRESULT compiled = g_compile(hlsl.data(), hlsl.size(), path.c_str(), nullptr,
		nullptr, kEntryPoint, kTarget, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &bytecode, &errors);

	if (FAILED(compiled) || bytecode == nullptr)
	{
		Report("%s did not compile: %s%s", name.c_str(),
			errors != nullptr ? static_cast<const char*>(errors->GetBufferPointer())
			: "no message",
			ShaderSource::IsNative(format)
				? "" : " (line numbers refer to the translated copy in the Translated "
					"folder)");

		LOG("[ShaderPack] %s", g_status);

		if (errors != nullptr)
			errors->Release();

		if (bytecode != nullptr)
			bytecode->Release();

		return false;
	}

	if (errors != nullptr)
		errors->Release();

	const HRESULT created = device->CreatePixelShader(
		static_cast<const DWORD*>(bytecode->GetBufferPointer()), &g_shader);

	bytecode->Release();

	if (FAILED(created))
	{
		g_shader = nullptr;
		Report("the device refused %s (it needs pixel shader 3.0)", name.c_str());
		LOG("[ShaderPack] %s", g_status);
		return false;
	}

	g_compiled = g_selected;

	if (ShaderSource::IsNative(format))
		Report("%s is running", name.c_str());
	else
		Report("%s is running (%s)", name.c_str(), note.c_str());
	LOG("[ShaderPack] %s", g_status);
	return true;
}

bool WriteBytes(const std::string& path, const void* data, size_t size)
{
	FILE* file = nullptr;

	if (fopen_s(&file, path.c_str(), "wb") != 0 || file == nullptr)
		return false;

	const size_t written = fwrite(data, 1, size, file);
	fclose(file);

	return written == size;
}

std::string TranslatedFolder()
{
	return g_folder + "Translated\\";
}

void SaveTranslated(const std::string& name, const std::string& hlsl)
{
	const std::string folder = TranslatedFolder();

	CreateDirectoryA(folder.c_str(), nullptr);
	WriteBytes(folder + name + ".hlsl", hlsl.data(), hlsl.size());
}

void WriteReadme()
{
	const char* const text =
		"UNI2 Improvement Mod: shader packs\r\n"
		"\r\n"
		"A shader pack is a single file in this folder. It runs over the finished frame, after everything\r\n"
		"else the mod draws. This file explains how to install one and what to do when one does not work.\r\n"
		"\r\n"
		"\r\n"
		"HOW TO INSTALL A SHADER\r\n"
		"\r\n"
		"  1. Put the file in this folder (the one this README is in):\r\n"
		"\r\n"
		"         <the game folder>\\UNI2-IM\\Shaders\r\n"
		"\r\n"
		"     Subfolders are not scanned. The file name does not matter, only the extension.\r\n"
		"\r\n"
		"  2. Start the game, open the mod overlay (F1 by default) and go to Graphics -> Shaders.\r\n"
		"\r\n"
		"  3. Find \"Shader pack\" at the bottom of that tab. If the game was already running when you\r\n"
		"     added the file, press Rescan. Then pick your file from the drop down.\r\n"
		"\r\n"
		"  4. The shader compiles as soon as you pick it. The line under the drop down shows the result:\r\n"
		"\r\n"
		"         crt-lottes.slang is running (GLSL fragment stage, one pass)\r\n"
		"\r\n"
		"     and the picture changes. To turn it off, pick \"Off\" in the same drop down.\r\n"
		"\r\n"
		"  5. Your pick is saved in UNI2_IM.ini as [Graphics] ShaderPack and comes back next launch.\r\n"
		"\r\n"
		"Nothing gets installed, unpacked or registered. To remove a shader, delete its file.\r\n"
		"\r\n"
		"\r\n"
		"SUPPORTED FILES\r\n"
		"\r\n"
		"  .hlsl .ps      HLSL, compiled as written\r\n"
		"  .fx            effect format: HLSL with annotations and techniques\r\n"
		"  .slang         Vulkan GLSL with #pragma parameters\r\n"
		"  .glsl          OpenGL GLSL, modern or legacy\r\n"
		"  .frag .fsh     also GLSL\r\n"
		"\r\n"
		"Every format except .hlsl and .ps is TRANSLATED into HLSL when you pick it. The translation is\r\n"
		"saved as:\r\n"
		"\r\n"
		"    UNI2-IM\\Shaders\\Translated\\<the file name>.hlsl\r\n"
		"\r\n"
		"That is the file the compiler actually gets. It is worth opening at least once.\r\n"
		"\r\n"
		"\r\n"
		"WHY SHADERS ARE TRANSLATED\r\n"
		"\r\n"
		"This game uses Direct3D 9. It can only run HLSL compiled to pixel shader 3.0, and there is one\r\n"
		"slot for it, over the finished frame.\r\n"
		"\r\n"
		"  * A .fx file is HLSL, but not a plain pixel shader. It also describes uniforms, annotations,\r\n"
		"    textures, samplers and techniques made of passes, and all of that has to be resolved\r\n"
		"    before anything compiles. The maths inside the pass is fine as it is.\r\n"
		"  * A .slang file is Vulkan GLSL and a .glsl file is OpenGL GLSL. Direct3D cannot compile GLSL\r\n"
		"    at all, and a preset usually chains several passes.\r\n"
		"\r\n"
		"So neither can be given to Direct3D 9 as it is, and the mod rewrites them: uniforms become their\r\n"
		"default values, samplers read the frame, resolution and time uniforms use the two constants\r\n"
		"listed below, and GLSL is rewritten as HLSL. The other option would be a full effect runtime (a\r\n"
		"GLSL compiler, multi-pass rendering, its own render targets), which is too much for a training\r\n"
		"mod.\r\n"
		"\r\n"
		"That is also the limit: ONE PASS OVER THE FINISHED FRAME. A shader that needs a second pass, a\r\n"
		"lookup texture, the depth buffer or the previous frame will translate but look wrong. The big\r\n"
		"multi-pass CRT shaders are like that and will not work. The single pass ones do.\r\n"
		"\r\n"
		"\r\n"
		"WHEN IT DOES NOT WORK\r\n"
		"\r\n"
		"The tab shows the compiler's error. For a translated file, the line numbers refer to the\r\n"
		"translated copy, so:\r\n"
		"\r\n"
		"  1. Open UNI2-IM\\Shaders\\Translated\\<your file>.hlsl.\r\n"
		"  2. Go to the line in the error. The #define lines at the top were added by the mod and show\r\n"
		"     what each uniform of the original was replaced with.\r\n"
		"  3. Fix it there, save it into the Shaders folder as your own .hlsl file, and pick that one\r\n"
		"     instead. From then on it is compiled as written.\r\n"
		"\r\n"
		"Common causes, most likely first:\r\n"
		"\r\n"
		"  * the shader needs a second pass or a texture, which is now a constant 0\r\n"
		"  * it uses a GLSL feature that HLSL does not have\r\n"
		"  * a #include the mod removed defined something the shader needs\r\n"
		"\r\n"
		"\r\n"
		"WHAT THE MOD PROVIDES\r\n"
		"\r\n"
		"  sampler2D Frame  : register(s0);   the frame so far\r\n"
		"  float4 FrameSize : register(c0);   xy = 1/width, 1/height   zw = width, height\r\n"
		"  float4 FrameTime : register(c1);   x  = seconds since load  y = frames since load\r\n"
		"\r\n"
		"Nothing else is set: no vertex shader of your own, no second pass, no previous frame and no\r\n"
		"depth buffer. uv goes from 0 to 1 across the window.\r\n"
		"\r\n"
		"\r\n"
		"WHAT A TRANSLATED SHADER GETS\r\n"
		"\r\n"
		"  .fx        BUFFER_WIDTH, BUFFER_HEIGHT, BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT, BUFFER_PIXEL_SIZE,\r\n"
		"             BUFFER_SCREEN_SIZE, BUFFER_ASPECT_RATIO, and the back buffer, pixel size and screen\r\n"
		"             size symbols. A uniform becomes its default value, and one with a source of\r\n"
		"             timer or framecount becomes FrameTime. Every sampler reads the frame and the depth\r\n"
		"             buffer reads 1.0. The first technique's PixelShader is the pass that runs\r\n"
		"  .slang     SourceSize, OriginalSize, OutputSize and FinalViewportSize as float4(w, h, 1/w, 1/h),\r\n"
		"             plus FrameCount, FrameDirection and MVP. Every #pragma parameter becomes its default.\r\n"
		"             Every sampler reads the frame, and the varying that carried the coordinate becomes the\r\n"
		"             coordinate this pass draws with. Only the fragment stage is kept\r\n"
		"  image      iResolution, iTime, iTimeDelta, iFrame, iMouse, iDate and iChannel0 to iChannel3.\r\n"
		"  shader     mainImage is called with fragCoord in pixels, y up\r\n"
		"  plain GLSL a sampler uniform becomes the frame, an in or varying becomes the coordinate, an out\r\n"
		"             or gl_FragColor becomes the result, and a uniform whose name looks like a resolution\r\n"
		"             or a time is answered from FrameSize and FrameTime. Everything else becomes 0\r\n"
		"\r\n"
		"\r\n"
		"WRITING YOUR OWN\r\n"
		"\r\n"
		"The smallest pack that works, saved as anything.hlsl:\r\n"
		"\r\n"
		"  sampler2D Frame : register(s0);\r\n"
		"\r\n"
		"  float4 main(float2 uv : TEXCOORD0) : COLOR0\r\n"
		"  {\r\n"
		"      return float4(tex2D(Frame, uv).rgb, 1.0f);\r\n"
		"  }\r\n"
		"\r\n"
		"Entry point main, target ps_3_0, one pass, pixel shader only. Copy 01_passthrough.hlsl to start.\r\n"
		"\r\n"
		"Watch out: Frame is sampled with POINT filtering, not linear. Reading straight at uv is exact,\r\n"
		"which suits pixel art. But a pack that moves the coordinates (curvature, wobble, zoom) has to\r\n"
		"filter on its own, or the picture shimmers. 12_crt.hlsl has a four tap bilinear filter for this,\r\n"
		"in SampleFrame.\r\n"
		"\r\n"
		"\r\n"
		"THE FILES IN THIS FOLDER\r\n"
		"\r\n"
		"  01_passthrough.hlsl       the skeleton. Copy it to start your own\r\n"
		"  02_grayscale.hlsl         black and white, by the weights the eye uses\r\n"
		"  03_sepia.hlsl             grayscale with a paper tint\r\n"
		"  04_invert.hlsl            a photographic negative\r\n"
		"  05_posterize.hlsl         a few colour steps per channel, with a 2x2 dither\r\n"
		"  06_pixelate.hlsl          snaps the picture to a coarser grid\r\n"
		"  07_chromatic.hlsl         red and blue pull apart towards the corners\r\n"
		"  08_film_grain.hlsl        moving noise, mostly in the shadows\r\n"
		"  09_vhs.hlsl               tape wobble, colour split and a head switching band\r\n"
		"  10_lcd_grid.hlsl          the dot grid of a handheld screen\r\n"
		"  11_outline.hlsl           a Sobel edge detector inking the picture\r\n"
		"  12_crt.hlsl               curvature, scanlines, phosphor mask, bleed and vignette\r\n"
		"\r\n"
		"One example per supported format, so you can compare each file with its translation:\r\n"
		"\r\n"
		"  13_reshade_tonemap.fx     .fx: annotated uniforms, a sampler and a technique\r\n"
		"  14_slang_scanlines.slang  .slang: #pragma parameters, a UBO, two stages\r\n"
		"  15_shadertoy_ripple.glsl  image shader: one mainImage, iResolution and iTime\r\n"
		"  16_bleach_bypass.ps       HLSL again, under the other extension\r\n"
		"  17_dot_matrix.frag        modern GLSL: in, out, texture(), a sampler uniform\r\n"
		"  18_bloom_glow.fsh         old GLSL: varying, gl_FragColor, texture2D, precision qualifiers\r\n"
		"\r\n"
		"The twelve .hlsl files keep their settings as #define lines at the top. Edit those, then pick the\r\n"
		"pack again on the tab to recompile it. The last six are meant to be read side by side with their\r\n"
		"copies in the Translated folder.\r\n"
		"\r\n"
		"Files you add or edit are never overwritten. A file you delete stays deleted until the mod\r\n"
		"updates.\r\n"
		"\r\n"
		"\r\n"
		"IF NOTHING COMPILES AT ALL\r\n"
		"\r\n"
		"Compiling needs d3dcompiler_47.dll, which comes with Windows and with Proton. Without it the tab\r\n"
		"says so, the folder is still listed and nothing compiles. The rest of the tab still works.\r\n";

	WriteBytes(g_folder + kReadmeName, text, strlen(text));
}

bool AlreadyInstalled()
{
	std::vector<uint8_t> stamp;

	if (!ReadWholeFile(g_folder + kInstallMarker, stamp))
		return false;

	const std::string text(stamp.begin(), stamp.end());

	return text.compare(kInstallStamp) == 0;
}

void InstallBundled()
{
	if (g_installed)
		return;

	g_installed = true;

	if (AlreadyInstalled())
		return;

	WriteReadme();

	int written = 0;

	for (int i = 0; i < BundledShaders::Count(); ++i)
	{
		const uint8_t* data = nullptr;
		size_t size = 0;

		if (!BundledShaders::Get(i, data, size))
			continue;

		const std::string path = g_folder + BundledShaders::Name(i);

		if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES)
			continue;

		if (WriteBytes(path, data, size))
			++written;
	}

	WriteBytes(g_folder + kInstallMarker, kInstallStamp, strlen(kInstallStamp));

	LOG("[ShaderPack] wrote %d bundled example%s into %s", written, written == 1 ? "" : "s",
		g_folder.c_str());
}

}

void ShaderPack::Refresh()
{
	g_folder = GetModShaderPath();
	InstallBundled();
	Scan();

	const int wanted = IndexOf(g_settings.shaderPack);

	if (wanted != g_selected)
		ReleaseShader();

	g_selected = wanted;

	if (g_names.empty())
	{
		Report("no shader files in %s", g_folder.c_str());
		return;
	}

	if (g_selected < 0)
	{
		Report("%d pack%s available, none selected", static_cast<int>(g_names.size()),
			g_names.size() == 1 ? "" : "s");
		return;
	}

	Report("%s selected", g_names[g_selected].c_str());
}

int ShaderPack::Count()
{
	return static_cast<int>(g_names.size());
}

const char* ShaderPack::GetName(int index)
{
	if (index < 0 || index >= Count())
		return "";

	return g_names[index].c_str();
}

int ShaderPack::GetSelected()
{
	return g_selected;
}

void ShaderPack::Select(int index)
{
	ReleaseShader();

	g_selected = index >= 0 && index < Count() ? index : -1;

	const char* const name = g_selected < 0 ? "" : g_names[g_selected].c_str();

	g_settings.shaderPack = name;
	Settings::SaveString("Graphics", "ShaderPack", name);

	Report(g_selected < 0 ? "no shader pack selected" : "%s selected", name);
}

IDirect3DPixelShader9* ShaderPack::Acquire(IDirect3DDevice9* device)
{
	if (device == nullptr || g_selected < 0 || g_compileFailed)
		return nullptr;

	if (g_shader != nullptr && g_compiled == g_selected)
		return g_shader;

	if (!EnsureCompiler())
	{
		Report("d3dcompiler_47.dll is missing, so shaders cannot be compiled");
		g_compileFailed = true;
		return nullptr;
	}

	if (CompileSelected(device))
		return g_shader;

	g_compileFailed = true;
	return nullptr;
}

void ShaderPack::OnDeviceLost()
{
	ReleaseShader();
}

void ShaderPack::Shutdown()
{
	ReleaseShader();

	if (g_compilerModule == nullptr)
		return;

	FreeLibrary(g_compilerModule);
	g_compilerModule = nullptr;
	g_compile = nullptr;
	g_compilerTried = false;
}

bool ShaderPack::IsCompilerAvailable()
{
	return EnsureCompiler();
}

const char* ShaderPack::GetFolderPath()
{
	return g_folder.c_str();
}

const char* ShaderPack::GetStatusText()
{
	return g_status;
}
