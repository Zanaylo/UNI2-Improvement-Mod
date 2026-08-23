#include "D3D9/Post/ShaderPack.h"

#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr int kMaxPacks = 64;
constexpr const char* kEntryPoint = "main";
constexpr const char* kTarget = "ps_3_0";

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

void Scan()
{
	g_names.clear();

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((g_folder + "*.hlsl").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		if (static_cast<int>(g_names.size()) >= kMaxPacks)
			break;

		g_names.push_back(found.cFileName);
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);

	std::sort(g_names.begin(), g_names.end());
}

bool CompileSelected(IDirect3DDevice9* device)
{
	std::vector<uint8_t> source;
	const std::string path = g_folder + g_names[g_selected];

	if (!ReadWholeFile(path, source, 1))
	{
		Report("%s could not be read", g_names[g_selected].c_str());
		return false;
	}

	ID3DBlob* bytecode = nullptr;
	ID3DBlob* errors = nullptr;

	const HRESULT compiled = g_compile(source.data(), source.size(), path.c_str(), nullptr,
		nullptr, kEntryPoint, kTarget, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &bytecode, &errors);

	if (FAILED(compiled) || bytecode == nullptr)
	{
		Report("%s did not compile: %s", g_names[g_selected].c_str(),
			errors != nullptr ? static_cast<const char*>(errors->GetBufferPointer())
			: "no message");

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
		Report("the device refused %s - it needs pixel shader 3.0",
			g_names[g_selected].c_str());
		LOG("[ShaderPack] %s", g_status);
		return false;
	}

	g_compiled = g_selected;
	Report("%s compiled and running", g_names[g_selected].c_str());
	LOG("[ShaderPack] %s", g_status);
	return true;
}

void WriteReadme()
{
	const std::string path = g_folder + "README.txt";

	if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES)
		return;

	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "wb") != 0 || file == nullptr)
		return;

	fputs(
		"UNI2-Improvement-Mod shader packs\r\n"
		"\r\n"
		"Drop a .hlsl file in this folder and pick it in the Shaders tab. It is compiled when you\r\n"
		"select it and runs last in the chain, over the finished frame.\r\n"
		"\r\n"
		"The entry point is main, the target is ps_3_0, and the mod binds:\r\n"
		"\r\n"
		"  sampler2D Frame  : register(s0);   the frame so far\r\n"
		"  float4 FrameSize : register(c0);   xy = 1/width, 1/height   zw = width, height\r\n"
		"  float4 FrameTime : register(c1);   x  = seconds since load  y = frames since load\r\n"
		"\r\n"
		"Smallest pack that does anything:\r\n"
		"\r\n"
		"  sampler2D Frame : register(s0);\r\n"
		"  float4 FrameSize : register(c0);\r\n"
		"\r\n"
		"  float4 main(float2 uv : TEXCOORD0) : COLOR0\r\n"
		"  {\r\n"
		"      float3 colour = tex2D(Frame, uv).rgb;\r\n"
		"      return float4(colour.bgr, 1.0f);\r\n"
		"  }\r\n"
		"\r\n"
		"Compilation needs d3dcompiler_47.dll. It ships with Windows and with Proton; if the tab\r\n"
		"says the compiler is missing, nothing here is loaded and the rest of the chain is\r\n"
		"unaffected. A pack that fails to compile prints its errors in the tab and is skipped.\r\n",
		file);

	fclose(file);
}

}

void ShaderPack::Refresh()
{
	g_folder = GetModShaderPath();
	WriteReadme();
	Scan();

	const int wanted = IndexOf(g_settings.shaderPack);

	if (wanted != g_selected)
		ReleaseShader();

	g_selected = wanted;

	if (g_names.empty())
	{
		Report("no .hlsl files in %s", g_folder.c_str());
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
		Report("d3dcompiler_47.dll is not on this system, so user shaders cannot be compiled");
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
