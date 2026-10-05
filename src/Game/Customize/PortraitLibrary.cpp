#include "Game/Customize/PortraitLibrary.h"

#include "Core/Formats/PngImage.h"
#include "Core/utils.h"
#include "Game/Files/DataArchive.h"

#include <Windows.h>

#include <vector>

namespace {

constexpr const char* kRoot = "Portraits";
constexpr const char* kLibrary = "library";

bool Exists(const std::string& path)
{
	const DWORD attributes = GetFileAttributesA(path.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool LoadFromTheGame(const PortraitCatalog::Art& art, DdsImage::Image& out)
{
	std::vector<uint8_t> bytes;
	return DataArchive::Read(art.folder, art.file, bytes) && DdsImage::Decode(bytes, out);
}

bool LoadFromTheLibrary(const PortraitCatalog::Art& art, DdsImage::Image& out)
{
	std::vector<uint8_t> bytes;
	return ReadWholeFile(PortraitLibrary::PathOf(art), bytes) && PngImage::Decode(bytes, out.width, out.height, out.pixels);
}

}

std::string PortraitLibrary::Root()
{
	return GetModRootPath(kRoot);
}

std::string PortraitLibrary::Folder()
{
	return Root() + "\\" + kLibrary;
}

std::string PortraitLibrary::PathOf(const PortraitCatalog::Art& art)
{
	return Folder() + "\\" + art.file;
}

bool PortraitLibrary::IsReady(const PortraitCatalog::Art& art)
{
	return art.source == PortraitCatalog::Source_Game || Exists(PathOf(art));
}

bool PortraitLibrary::Load(const PortraitCatalog::Art& art, DdsImage::Image& out)
{
	if (art.source == PortraitCatalog::Source_Game)
		return LoadFromTheGame(art, out);

	return LoadFromTheLibrary(art, out);
}
