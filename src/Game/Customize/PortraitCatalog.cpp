#include "Game/Customize/PortraitCatalog.h"

#include <cstring>

namespace {

constexpr char kPrefixEnd = ':';

const PortraitCatalog::Art kArts[] = {
#include "Game/Customize/PortraitCatalog.inc"
};

const char* Unprefixed(const char* id)
{
	const char* const end = strchr(id, kPrefixEnd);
	return end == nullptr ? id : end + 1;
}

}

const PortraitCatalog::Art* PortraitCatalog::Find(const std::string& id)
{
	const char* const wanted = Unprefixed(id.c_str());

	for (const Art& art : kArts)
	{
		if (strcmp(wanted, Unprefixed(art.id)) == 0)
			return &art;
	}

	return nullptr;
}

std::vector<const PortraitCatalog::Art*> PortraitCatalog::Of(int chara)
{
	std::vector<const Art*> out;

	for (const Art& art : kArts)
	{
		if (art.chara == chara)
			out.push_back(&art);
	}

	return out;
}

std::vector<const PortraitCatalog::Art*> PortraitCatalog::FromThePack()
{
	std::vector<const Art*> out;

	for (const Art& art : kArts)
	{
		if (art.source == Source_Pack)
			out.push_back(&art);
	}

	return out;
}
