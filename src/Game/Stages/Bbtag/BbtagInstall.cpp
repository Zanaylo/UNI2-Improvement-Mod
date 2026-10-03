#include "Game/Stages/Bbtag/BbtagInstall.h"

#include "Game/Stages/Bbtag/BbtagPac.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr const char* kModelExtension = ".MUA";
constexpr const char* kSharedEntries[] = { "mdl.pac", "scr.pac" };
constexpr const char* kBbcfEntries[] = { "mot.pac", "cammot.pac" };

bool Holds(const std::vector<std::string>& names, const char* wanted)
{
	return std::any_of(names.begin(), names.end(),
		[wanted](const std::string& name) { return _stricmp(name.c_str(), wanted) == 0; });
}

template <size_t N>
bool HoldsAll(const std::vector<std::string>& names, const char* const (&wanted)[N])
{
	return std::all_of(std::begin(wanted), std::end(wanted),
		[&names](const char* one) { return Holds(names, one); });
}

std::string Leaf(const std::string& path)
{
	const size_t slash = path.find_last_of("/\\");

	return slash == std::string::npos ? path : path.substr(slash + 1);
}

}

std::string BbtagInstall::ModelName(const std::vector<uint8_t>& scene)
{
	BbtagPac::Files files;

	if (!BbtagPac::Walk(scene, files))
		return std::string();

	for (const std::pair<const std::string, std::vector<uint8_t> >& file : files)
	{
		const std::string leaf = Leaf(file.first);
		const size_t extension = strlen(kModelExtension);

		if (leaf.size() > extension && _stricmp(leaf.c_str() + leaf.size() - extension, kModelExtension) == 0)
			return leaf.substr(0, leaf.size() - extension);
	}

	return std::string();
}

bool BbtagInstall::Loadable(FbGameFolder::Game game, const std::vector<uint8_t>& scene)
{
	const std::vector<std::string> names = BbtagPac::Names(scene);

	if (!HoldsAll(names, kSharedEntries))
		return false;

	return game != FbGameFolder::Game_BBCF || HoldsAll(names, kBbcfEntries);
}
