#include "Game/Customize/PortraitCatalog.h"

namespace {

const PortraitCatalog::Art kArts[] = {
#include "Game/Customize/PortraitCatalog.inc"
};

}

const PortraitCatalog::Art* PortraitCatalog::Find(const std::string& id)
{
	for (const Art& art : kArts)
	{
		if (id == art.id)
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

std::vector<const PortraitCatalog::Art*> PortraitCatalog::FromTheWiki()
{
	std::vector<const Art*> out;

	for (const Art& art : kArts)
	{
		if (art.source == Source_Wiki)
			out.push_back(&art);
	}

	return out;
}
