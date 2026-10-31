#include "Game/Customize/PortraitLayer.h"

#include "Core/FileIndex.h"
#include "Game/Customize/PortraitCompose.h"

void PortraitLayer::Layer(FileIndex& into)
{
	FileIndex composed;
	composed.Walk(PortraitCompose::Folder());

	for (const FileIndex::Map::value_type& entry : composed.Entries())
		into.Add(entry.first, entry.second);
}
