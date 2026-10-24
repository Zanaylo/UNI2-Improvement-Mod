#include "Game/Display/FontPages.h"

#include "Core/logger.h"
#include "D3D9/Device/ScaledTargets.h"
#include "Game/Display/FontPageSet.h"
#include "Game/Files/DataArchive.h"

#include <Windows.h>

#include <mutex>
#include <string>
#include <vector>

namespace {

constexpr size_t kMostPages = 64;

std::once_flag g_learned;
FontPageSet g_pages;

SRWLOCK g_lock = SRWLOCK_INIT;
IDirect3DBaseTexture9* g_textures[kMostPages] = {};
size_t g_textureCount = 0;

void LearnFolder(const std::string& folder)
{
	std::vector<std::string> files;
	std::vector<uint8_t> head;
	size_t size = 0;

	if (!DataArchive::List(folder.c_str(), files))
		return;

	for (const std::string& file : files)
	{
		if (FontPageSet::IsPageFile(file) &&
			DataArchive::ReadHead(folder.c_str(), file.c_str(), FontPageSet::kHeadBytes, head, size))
			g_pages.Learn(head.data(), head.size(), size);
	}
}

void Learn()
{
	const DWORD began = GetTickCount();
	std::vector<std::string> folders;
	DataArchive::Folders(folders);

	for (const std::string& folder : folders)
	{
		if (FontPageSet::IsFontFolder(folder))
			LearnFolder(folder);
	}

	LOG("FontPages: %u font page(s) known in %lu ms", static_cast<unsigned>(g_pages.Count()), GetTickCount() - began);
}

void Remember(IDirect3DBaseTexture9* texture)
{
	AcquireSRWLockExclusive(&g_lock);

	if (g_textureCount < kMostPages)
		g_textures[g_textureCount++] = texture;

	ReleaseSRWLockExclusive(&g_lock);
}

}

void FontPages::OnTexture(const void* source, UINT bytes, IDirect3DBaseTexture9* texture)
{
	if (!ScaledTargets::ScalesSceneLayer())
		return;

	std::call_once(g_learned, Learn);

	if (!g_pages.Recognises(source, bytes))
		return;

	Remember(texture);
}

bool FontPages::IsPage(IDirect3DBaseTexture9* texture)
{
	AcquireSRWLockShared(&g_lock);

	bool found = false;

	for (size_t i = 0; i < g_textureCount && !found; ++i)
		found = g_textures[i] == texture;

	ReleaseSRWLockShared(&g_lock);
	return found;
}
